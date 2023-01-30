/******************************************************************************
 * Copyright 2020 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

/*
 * rdmnet/core/client.h: Implementation of RDMnet client functionality
 *
 * RDMnet clients encompass controllers (which originate RDM commands on the network and receive
 * responses) and devices (which receive RDM commands and respond to them), as well as EPT clients,
 * which use the Extensible Packet Transport feature of RDMnet to transport opaque data through a
 * broker.
 *
 * Clients and the scopes they participate in are tracked by handles. Management of connections to
 * brokers, as well as LLRP functionality, is handled under the hood.
 */

#ifndef RDMNET_CORE_CLIENT_H_
#define RDMNET_CORE_CLIENT_H_

#include "etcpal/uuid.h"
#include "etcpal/handle_manager.h"
#include "etcpal/inet.h"
#include "etcpal/mutex.h"
#include "rdm/uid.h"
#include "rdm/message.h"
#include "rdmnet/defs.h"
#include "rdmnet/discovery.h"
#include "rdmnet/client.h"
#include "rdmnet/message.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/core/broker_prot.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/llrp_target.h"

#ifdef __cplusplus
extern "C" {
#endif

#if RDMNET_MAX_CONTROLLERS != 0
#define RDMNET_MAX_SCOPES_PER_CLIENT RDMNET_MAX_SCOPES_PER_CONTROLLER
#else
#define RDMNET_MAX_SCOPES_PER_CLIENT 1
#endif

typedef struct RCClient RCClient;

typedef enum
{
  kRptClientMsgRdmCmd,
  kRptClientMsgRdmResp,
  kRptClientMsgStatus
} rpt_client_msg_t;

typedef struct RptClientMessage
{
  rpt_client_msg_t type;
  union
  {
    RdmnetRdmCommand  cmd;
    RdmnetRdmResponse resp;
    RdmnetRptStatus   status;
  } payload;
} RptClientMessage;

#define RDMNET_GET_RDM_COMMAND(rptclimsgptr) (RDMNET_ASSERT_VERIFY(rptclimsgptr) ? &(rptclimsgptr)->payload.cmd : NULL)
#define RDMNET_GET_RDM_RESPONSE(rptclimsgptr) \
  (RDMNET_ASSERT_VERIFY(rptclimsgptr) ? &(rptclimsgptr)->payload.resp : NULL)
#define RDMNET_GET_RPT_STATUS(rptclimsgptr) \
  (RDMNET_ASSERT_VERIFY(rptclimsgptr) ? &(rptclimsgptr)->payload.status : NULL)

typedef enum
{
  kEptClientMsgData,
  kEptClientMsgStatus
} ept_client_msg_t;

typedef struct EptClientMessage
{
  ept_client_msg_t type;
  union
  {
    RdmnetEptStatus status;
    RdmnetEptData   data;
  } payload;
} EptClientMessage;

/**************************************************************************************************
 * Client callback functions: Function types used as callbacks for RPT and EPT clients.
 *************************************************************************************************/

// A client has connected successfully to a broker on a scope.
typedef void (*RCClientConnectedCb)(RCClient*                        client,
                                    rdmnet_client_scope_t            scope_handle,
                                    const RdmnetClientConnectedInfo* info);

// A client experienced a failure while attempting to connect to a broker on a scope.
typedef void (*RCClientConnectFailedCb)(RCClient*                            client,
                                        rdmnet_client_scope_t                scope_handle,
                                        const RdmnetClientConnectFailedInfo* info);

// A client disconnected from a broker on a scope.
typedef void (*RCClientDisconnectedCb)(RCClient*                           client,
                                       rdmnet_client_scope_t               scope_handle,
                                       const RdmnetClientDisconnectedInfo* info);

// A broker message has been received on a client connection.
//
// Broker messages are exchanged between an RDMnet client and broker to setup and faciliate RDMnet
// communication. Use the macros from broker_prot.h to inspect the BrokerMessage.
typedef void (*RCClientBrokerMsgReceivedCb)(RCClient*             client,
                                            rdmnet_client_scope_t scope_handle,
                                            const BrokerMessage*  msg);

// An LLRP RDM command has been received by a client.
//
// use_internal_buf_for_response specifies whether to use the internal RCClient response buffer
// (obtained using rc_client_get_internal_response_buf()) or the external buffer provided as part
// of the client object.
typedef void (*RCClientLlrpMsgReceivedCb)(RCClient*              client,
                                          const LlrpRdmCommand*  cmd,
                                          RdmnetSyncRdmResponse* response,
                                          bool*                  use_internal_buf_for_response);

// An RPT message was received on an RPT client connection.
//
// RPT messages include Request and Notification, which wrap RDM commands and responses, as well as
// Status, which informs of exceptional conditions in response to a Request. Use the macros from
// this header to inspect the RptClientMessage.
//
// use_internal_buf_for_response specifies whether to use the internal RCClient response buffer
// (obtained using rc_client_get_internal_response_buf()) or the external buffer provided as part
// of the client object.
typedef void (*RCClientRptMsgReceivedCb)(RCClient*               client,
                                         rdmnet_client_scope_t   scope_handle,
                                         const RptClientMessage* msg,
                                         RdmnetSyncRdmResponse*  response,
                                         bool*                   use_internal_buf_for_response);

// An EPT message was received on an EPT client connection.
//
// EPT messages include Data, which wraps opaque data, and Status, which informs of exceptional
// conditions in response to Data.
//
// use_internal_buf_for_response specifies whether to use the internal RCClient response buffer
// (obtained using rc_client_get_internal_response_buf()) or the external buffer provided as part
// of the client object.
typedef void (*RCClientEptMsgReceivedCb)(RCClient*               client,
                                         rdmnet_client_scope_t   scope_handle,
                                         const EptClientMessage* msg,
                                         RdmnetSyncRdmResponse*  response,
                                         bool*                   use_internal_buf_for_response);

// An RDMnet client has been destroyed and unregistered. This is called from the background thread,
// after the resources associated with the client (e.g. other module structs, sockets, etc) have
// been cleaned up. It is safe to deallocate the client from this callback.
typedef void (*RCClientDestroyedCb)(RCClient* client);

// The set of callbacks shared between RPT and EPT clients.
typedef struct RCClientCommonCallbacks
{
  RCClientConnectedCb         connected;
  RCClientConnectFailedCb     connect_failed;
  RCClientDisconnectedCb      disconnected;
  RCClientBrokerMsgReceivedCb broker_msg_received;
  RCClientDestroyedCb         destroyed;
} RCClientCommonCallbacks;

// The set of possible callbacks that are delivered to an RPT client.
typedef struct RCRptClientCallbacks
{
  RCClientLlrpMsgReceivedCb llrp_msg_received;
  RCClientRptMsgReceivedCb  rpt_msg_received;
} RCRptClientCallbacks;

// The set of possible callbacks that are delivered to an EPT client.
typedef struct RCEptClientCallbacks
{
  RCClientEptMsgReceivedCb msg_received;
} RCEptClientCallbacks;

typedef enum
{
  kRCScopeStateInactive,
  kRCScopeStateDiscovery,
  kRCScopeStateConnecting,
  kRCScopeStateConnected,
  kRCScopeStateMarkedForDestruction,
} rc_scope_state_t;

typedef struct RCClientScope
{
  rdmnet_client_scope_t handle;
  rc_scope_state_t      state;

  char           id[E133_SCOPE_STRING_PADDED_LENGTH];
  EtcPalSockAddr static_broker_addr;
  RdmUid         uid;
  uint32_t       send_seq_num;

  rdmnet_scope_monitor_t monitor_handle;
  bool                   broker_found;
  char                   broker_name[E133_SERVICE_NAME_STRING_PADDED_LENGTH];
#if RDMNET_DYNAMIC_MEM
  EtcPalIpAddr* broker_listen_addrs;
#else
  EtcPalIpAddr  broker_listen_addrs[RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER];
#endif
  size_t   num_broker_listen_addrs;
  size_t   current_listen_addr;
  uint16_t port;

  // TCP comms status tracking
  EtcPalSockAddr current_broker_addr;
  uint16_t       unhealthy_counter;

  RCConnection conn;

  RCClient* client;
} RCClientScope;

typedef struct RCRptClientData
{
  rpt_client_type_t    type;
  RdmUid               uid;
  RCRptClientCallbacks callbacks;
} RCRptClientData;

typedef struct RCEptClientData
{
  RC_DECLARE_BUF(RdmnetEptSubProtocol, protocols, RDMNET_MAX_PROTOCOLS_PER_EPT_CLIENT);
  RCEptClientCallbacks callbacks;
} RCEptClientData;

// RDMNET_MAX_SCOPES_PER_CLIENT dictates the length of the TCP_COMMS_STATUS response, which the
// client module handles internally.
#if RDMNET_MAX_SCOPES_PER_CLIENT > RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES
#define RC_CLIENT_STATIC_RESP_BUF_LEN (RDMNET_MAX_SCOPES_PER_CLIENT + 2)
#else
#define RC_CLIENT_STATIC_RESP_BUF_LEN (RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES + 2)
#endif

struct RCClient
{
  /////////////////////////////////////////////////////////////////////////////
  // Fill in these items before initializing the RCClient.

  etcpal_mutex_t*         lock;
  client_protocol_t       type;
  EtcPalUuid              cid;
  RCClientCommonCallbacks callbacks;
  union
  {
    RCRptClientData rpt;
    RCEptClientData ept;
  } data;
  char     search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];
  uint8_t* sync_resp_buf;

  /////////////////////////////////////////////////////////////////////////////

  bool marked_for_destruction;

  IntHandleManager scope_handle_manager;
#if RDMNET_DYNAMIC_MEM
  RCClientScope** scopes;
  size_t          num_scopes;
#else
  RCClientScope scopes[RDMNET_MAX_SCOPES_PER_CLIENT];
#endif

#if !RDMNET_DYNAMIC_MEM
  RdmBuffer resp_buf[RC_CLIENT_STATIC_RESP_BUF_LEN];
#endif

  RCLlrpTarget llrp_target;
  bool         target_valid;
};

#define RC_RPT_CLIENT_DATA(clientptr) (RDMNET_ASSERT_VERIFY(clientptr) ? &(clientptr)->data.rpt : NULL)
#define RC_EPT_CLIENT_DATA(clientptr) (RDMNET_ASSERT_VERIFY(clientptr) ? &(clientptr)->data.ept : NULL)

#define RC_CLIENT_INIT_SCOPES(clientptr, initial_capacity) \
  RC_INIT_BUF(clientptr, RCClientScope, scopes, initial_capacity, RDMNET_MAX_SCOPES_PER_CLIENT)

#define RC_CLIENT_DEINIT_SCOPES(clientptr) RC_DEINIT_BUF(clientptr, scopes)

#define RC_CLIENT_CHECK_SCOPES_CAPACITY(clientptr, num_additional) \
  RC_CHECK_BUF_CAPACITY(clientptr, RCClientScope, scopes, RDMNET_MAX_SCOPES_PER_CLIENT, num_additional)

etcpal_error_t rc_client_module_init(const RdmnetNetintConfig* netint_config);
void           rc_client_module_deinit(void);

etcpal_error_t rc_rpt_client_register(RCClient* client, bool create_llrp_target);
etcpal_error_t rc_ept_client_register(RCClient* client);
bool           rc_client_unregister(RCClient* client, rdmnet_disconnect_reason_t disconnect_reason);
etcpal_error_t rc_client_add_scope(RCClient*                client,
                                   const RdmnetScopeConfig* scope_config,
                                   rdmnet_client_scope_t*   scope_handle);
etcpal_error_t rc_client_remove_scope(RCClient*                  client,
                                      rdmnet_client_scope_t      scope_handle,
                                      rdmnet_disconnect_reason_t reason);
etcpal_error_t rc_client_change_scope(RCClient*                  client,
                                      rdmnet_client_scope_t      scope_handle,
                                      const RdmnetScopeConfig*   new_scope_config,
                                      rdmnet_disconnect_reason_t disconnect_reason);
etcpal_error_t rc_client_get_scope(RCClient*             client,
                                   rdmnet_client_scope_t scope_handle,
                                   char*                 scope_str_buf,
                                   EtcPalSockAddr*       static_broker_addr);
etcpal_error_t rc_client_change_search_domain(RCClient*                  client,
                                              const char*                new_search_domain,
                                              rdmnet_disconnect_reason_t reason);
etcpal_error_t rc_client_request_client_list(RCClient* client, rdmnet_client_scope_t scope_handle);
etcpal_error_t rc_client_request_dynamic_uids(RCClient*             client,
                                              rdmnet_client_scope_t scope_handle,
                                              const EtcPalUuid*     responder_ids,
                                              size_t                num_responders);
etcpal_error_t rc_client_request_responder_ids(RCClient*             client,
                                               rdmnet_client_scope_t scope_handle,
                                               const RdmUid*         uids,
                                               size_t                num_uids);
etcpal_error_t rc_client_send_rdm_command(RCClient*                    client,
                                          rdmnet_client_scope_t        scope_handle,
                                          const RdmnetDestinationAddr* destination,
                                          rdmnet_command_class_t       command_class,
                                          uint16_t                     param_id,
                                          const uint8_t*               data,
                                          uint8_t                      data_len,
                                          uint32_t*                    seq_num);
etcpal_error_t rc_client_send_rdm_ack(RCClient*                    client,
                                      rdmnet_client_scope_t        scope_handle,
                                      const RdmnetSavedRdmCommand* received_cmd,
                                      const uint8_t*               response_data,
                                      size_t                       response_data_len);
etcpal_error_t rc_client_send_rdm_nack(RCClient*                    client,
                                       rdmnet_client_scope_t        scope_handle,
                                       const RdmnetSavedRdmCommand* received_cmd,
                                       rdm_nack_reason_t            nack_reason);
etcpal_error_t rc_client_send_rdm_update(RCClient*             client,
                                         rdmnet_client_scope_t scope_handle,
                                         uint16_t              subdevice,
                                         uint16_t              param_id,
                                         const uint8_t*        data,
                                         size_t                data_len);
etcpal_error_t rc_client_send_rdm_update_from_responder(RCClient*               client,
                                                        rdmnet_client_scope_t   scope_handle,
                                                        const RdmnetSourceAddr* source_addr,
                                                        uint16_t                param_id,
                                                        const uint8_t*          data,
                                                        size_t                  data_len);
etcpal_error_t rc_client_send_rpt_status(RCClient*                    client,
                                         rdmnet_client_scope_t        scope_handle,
                                         const RdmnetSavedRdmCommand* received_cmd,
                                         rpt_status_code_t            status_code,
                                         const char*                  status_string);
etcpal_error_t rc_client_send_llrp_ack(RCClient*                  client,
                                       const LlrpSavedRdmCommand* received_cmd,
                                       const uint8_t*             response_data,
                                       uint8_t                    response_data_len);
etcpal_error_t rc_client_send_llrp_nack(RCClient*                  client,
                                        const LlrpSavedRdmCommand* received_cmd,
                                        rdm_nack_reason_t          nack_reason);
etcpal_error_t rc_client_send_ept_data(RCClient*             client,
                                       rdmnet_client_scope_t scope_handle,
                                       const EtcPalUuid*     dest_cid,
                                       uint16_t              manufacturer_id,
                                       uint16_t              protocol_id,
                                       const uint8_t*        data,
                                       size_t                data_len);
etcpal_error_t rc_client_send_ept_status(RCClient*             handle,
                                         rdmnet_client_scope_t scope_handle,
                                         const EtcPalUuid*     dest_cid,
                                         ept_status_code_t     status_code,
                                         const char*           status_string);

uint8_t* rc_client_get_internal_response_buf(size_t size);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_CLIENT_H_ */
