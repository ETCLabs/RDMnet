/******************************************************************************
 * Copyright 2019 ETC Inc.
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

#ifndef RDMNET_CORE_CLIENT_H_
#define RDMNET_CORE_CLIENT_H_

#include "etcpal/uuid.h"
#include "etcpal/inet.h"
#include "rdm/uid.h"
#include "rdm/message.h"
#include "rdmnet/defs.h"
#include "rdmnet/client.h"
#include "rdmnet/message.h"
#include "rdmnet/core/broker_prot.h"

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

#ifdef __cplusplus
extern "C" {
#endif

typedef int rdmnet_client_t;
#define RDMNET_CLIENT_INVALID -1

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
    RdmnetRdmCommand cmd;
    RdmnetRdmResponse resp;
    RdmnetRptStatus status;
  } payload;
} RptClientMessage;

#define RDMNET_GET_RDM_COMMAND(rptclimsgptr) (&(rptclimsgptr)->payload.cmd)
#define RDMNET_GET_RDM_RESPONSE(rptclimsgptr) (&(rptclimsgptr)->payload.resp)
#define RDMNET_GET_RPT_STATUS(rptclimsgptr) (&(rptclimsgptr)->payload.status)

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
    RdmnetEptData data;
  } payload;
} EptClientMessage;

/*!
 * \name Client Callback Functions
 * \brief Function types used as callbacks for RPT and EPT clients.
 * @{
 */

/*!
 * \brief An RDMnet client has connected successfully to a broker on a scope.
 *
 * Messages may now be sent using the relevant API functions, and messages may be received via
 * the msg_received callback.
 *
 * \param[in] handle Handle to client which has connected.
 * \param[in] scope_handle Handle to scope on which the client has connected.
 * \param[in] info More information about the successful connection.
 * \param[in] context Context pointer that was given at the creation of the client.
 */
typedef void (*RdmnetClientConnectedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                        const RdmnetClientConnectedInfo* info, void* context);

/*!
 * \brief An RDMnet client experienced a failure while attempting to connect to a broker on a
 *        scope.
 *
 * Connection failures can be fatal or non-fatal; the will_retry member of the info struct
 * indicates whether the connection will be retried automatically. If will_retry is false, it
 * usually indicates a misconfiguration that needs to be resolved by an application user.
 *
 * \param[in] handle Handle to client on which connection has failed.
 * \param[in] scope_handle Handle to scope on which connection has failed.
 * \param[in] info More information about the failed connection.
 * \param[in] context Context pointer that was given at the creation of the client.
 */
typedef void (*RdmnetClientConnectFailedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                            const RdmnetClientConnectFailedInfo* info, void* context);

/*! \brief An RDMnet client disconnected from a broker on a scope.
 *
 *  Disconnection can be fatal or non-fatal; the will_retry member of the info struct indicates
 *  whether the connection will be retried automatically. If will_retry is false, it usually
 *  indicates a misconfiguration that needs to be resolved by an application user.
 *
 *  \param[in] handle Handle to client which has disconnected.
 *  \param[in] scope_handle Handle to scope on which the disconnect occurred.
 *  \param[in] info More information about the disconnect.
 *  \param[in] context Context pointer that was given at the creation of the client.
 */
typedef void (*RdmnetClientDisconnectedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                           const RdmnetClientDisconnectedInfo* info, void* context);

/*!
 * \brief A broker message has been received on an RDMnet client connection.
 *
 * Broker messages are exchanged between an RDMnet client and broker to setup and faciliate RDMnet
 * communication. If using the \ref rdmnet_device "Device" or \ref rdmnet_controller "Controller"
 * API, this callback will be consumed internally and propagated to callbacks specific to those
 * client types.
 *
 * \param[in] handle Handle to client which has received a broker message.
 * \param[in] scope_handle Handle to scope on which the broker message was received.
 * \param[in] msg The broker message.
 * \param[in] context Context pointer that was given at the creation of the client.
 */
typedef void (*RdmnetClientBrokerMsgReceivedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                const BrokerMessage* msg, void* context);

/*!
 * \brief An LLRP message has been received by an RDMnet client.
 *
 * RPT clients (controllers and devices) automatically listen for LLRP messages as required by
 * E1.33. This callback is called when an LLRP RDM command is received.
 *
 * \param[in] handle Handle to client which has received an LLRP message.
 * \param[in] cmd The LLRP RDM command.
 * \param[in] response A response object to be used for responding synchronously.
 * \param[in] context Context pointer that was given at the creation of the client.
 */
typedef void (*RdmnetClientLlrpMsgReceivedCb)(rdmnet_client_t handle, const LlrpRdmCommand* cmd,
                                              RdmnetSyncRdmResponse* response, void* context);

/*!
 * \brief An RPT message was received on an RPT client connection.
 *
 * RPT messages include Request and Notification, which wrap RDM commands and responses, as well as
 * Status, which informs of exceptional conditions in response to a Request. If using the
 * \ref rdmnet_device "Device" or \ref rdmnet_controller "Controller" API, this callback will be
 * consumed internally and propagated to callbacks specific to those client types.
 *
 * \param[in] handle Handle to client which has received an RPT message.
 * \param[in] scope_handle Handle to scope on which the RPT message was received.
 * \param[in] msg The RPT message.
 * \param[in] context Context pointer that was given at the creation of the client.
 */
typedef void (*RptClientMsgReceivedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                       const RptClientMessage* msg, RdmnetSyncRdmResponse* response, void* context);

/*!
 * \brief An EPT message was received on an EPT client connection.
 *
 * EPT messages include Data, which wraps opaque data, and Status, which informs of exceptional
 * conditions in response to Data.
 *
 * \param[in] handle Handle to client which has received an EPT message.
 * \param[in] scope_handle Handle to scope on which the EPT message was received.
 * \param[in] msg The EPT message.
 * \param[in] context Context pointer that was given at the creation of the client.
 */
typedef void (*EptClientMsgReceivedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope, const EptClientMessage* msg,
                                       EptClientMessage* response, void* context);

/*!
 * @}
 */

/*! The set of possible callbacks that are delivered to an RPT client. */
typedef struct RptClientCallbacks
{
  RdmnetClientConnectedCb connected;
  RdmnetClientConnectFailedCb connect_failed;
  RdmnetClientDisconnectedCb disconnected;
  RdmnetClientBrokerMsgReceivedCb broker_msg_received;
  RdmnetClientLlrpMsgReceivedCb llrp_msg_received;
  RptClientMsgReceivedCb msg_received;
} RptClientCallbacks;

/*! The set of possible callbacks that are delivered to an EPT client. */
typedef struct EptClientCallbacks
{
  RdmnetClientConnectedCb connected;
  RdmnetClientConnectFailedCb connect_failed;
  RdmnetClientDisconnectedCb disconnected;
  RdmnetClientBrokerMsgReceivedCb broker_msg_received;
  EptClientMsgReceivedCb msg_received;
} EptClientCallbacks;

/*! A set of information that defines the startup parameters of an RPT RDMnet Client. */
typedef struct RdmnetRptClientConfig
{
  /****** Required Values ******/

  /*! The client type, either controller or device. */
  rpt_client_type_t type;
  /*! The client's CID. */
  EtcPalUuid cid;
  /*! A set of callbacks for the client to receive RDMnet notifications. */
  RptClientCallbacks callbacks;

  /****** Optional Values ******/

  /*! (optional) Pointer to opaque data passed back with each callback. */
  void* callback_context;
  /*!
   * (optional) The client's UID. This will be initialized with a Dynamic UID request using the
   * initialization functions/macros for this structure. If you want to use a static UID instead,
   * just fill this in with the static UID after initializing.
   */
  RdmUid uid;
  /*! (optional) The client's configured search domain for discovery. */
  const char* search_domain;
  /*!
   * (optional) A set of network interfaces to use for the LLRP target associated with this client.
   * If NULL, the set passed to rdmnet_core_init() will be used, or all network interfaces on the
   * system if that was not provided.
   */
  RdmnetMcastNetintId* llrp_netint_arr;
  /*! (optional) The size of llrp_netint_arr. */
  size_t num_llrp_netints;
} RdmnetRptClientConfig;

/*!
 * \brief A default-value initializer for an RdmnetRptClientConfig struct.
 *
 * Usage:
 * \code
 * RdmnetRptClientConfig config = RDMNET_RPT_CLIENT_CONFIG_DEFAULT_INIT(MY_ESTA_MANUFACTURER_ID);
 * // Now fill in the required portions as necessary with your data...
 * \endcode
 *
 * \param manu_id Your ESTA manufacturer ID.
 */
#define RDMNET_RPT_CLIENT_CONFIG_DEFAULT_INIT(manu_id)                                                          \
  {                                                                                                             \
    kRPTClientTypeUnknown, kEtcPalNullUuid, {NULL, NULL, NULL, NULL, NULL, NULL}, NULL, {0x8000u | manu_id, 0}, \
        E133_DEFAULT_DOMAIN, NULL, 0                                                                            \
  }

/*! A set of information that defines the startup parameters of an EPT RDMnet Client. */
typedef struct RdmnetEptClientConfig
{
  /*! An array of EPT sub-protocols that this EPT client uses. */
  RdmnetEptSubProtocol* protocols;
  /*! The size of the protocols array. */
  size_t num_protocols;
  /*! A set of callbacks for the client to receive RDMnet notifications. */
  EptClientCallbacks callbacks;
  /*! (optional) Pointer to opaque data passed back with each callback. */
  void* callback_context;
} RdmnetEptClientConfig;

/*!
 * \brief A default-value initializer for an RdmnetEptClientConfig struct.
 *
 * Usage:
 * \code
 * RdmnetEptClientConfig config = RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT;
 * // Now fill in the required portions as necessary with your data...
 * \endcode
 */
#define RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT     \
  {                                               \
    NULL, 0, {NULL, NULL, NULL, NULL, NULL}, NULL \
  }

typedef enum
{
  kScopeStateDiscovery,
  kScopeStateConnecting,
  kScopeStateConnected
} scope_state_t;

typedef struct RdmnetClient RdmnetClient;

typedef struct ClientScopeListEntry ClientScopeListEntry;
struct ClientScopeListEntry
{
  rdmnet_client_scope_t handle;
  char id[E133_SCOPE_STRING_PADDED_LENGTH];
  bool has_static_broker_addr;
  EtcPalSockAddr static_broker_addr;
  scope_state_t state;
  RdmUid uid;
  uint32_t send_seq_num;

  rdmnet_scope_monitor_t monitor_handle;
  bool broker_found;
  const EtcPalIpAddr* listen_addrs;
  size_t num_listen_addrs;
  size_t current_listen_addr;
  uint16_t port;

  RdmnetClient* client;
  ClientScopeListEntry* next;
};

typedef struct RptClientData
{
  rpt_client_type_t type;
  bool has_static_uid;
  RdmUid uid;
  RptClientCallbacks callbacks;
} RptClientData;

typedef struct EptClientData
{
  EptClientCallbacks callbacks;
} EptClientData;

struct RdmnetClient
{
  rdmnet_client_t handle;
  client_protocol_t type;
  EtcPalUuid cid;
  void* callback_context;
  ClientScopeListEntry* scope_list;
  char search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];

  llrp_target_t llrp_handle;

  union
  {
    RptClientData rpt;
    EptClientData ept;
  } data;
};

typedef enum
{
  kClientCallbackNone,
  kClientCallbackConnected,
  kClientCallbackConnectFailed,
  kClientCallbackDisconnected,
  kClientCallbackBrokerMsgReceived,
  kClientCallbackLlrpMsgReceived,
  kClientCallbackMsgReceived
} client_callback_t;

typedef struct ConnectedArgs
{
  rdmnet_client_scope_t scope_handle;
  RdmnetClientConnectedInfo info;
} ConnectedArgs;

typedef struct ConnectFailedArgs
{
  rdmnet_client_scope_t scope_handle;
  RdmnetClientConnectFailedInfo info;
} ConnectFailedArgs;

typedef struct DisconnectedArgs
{
  rdmnet_client_scope_t scope_handle;
  RdmnetClientDisconnectedInfo info;
} DisconnectedArgs;

typedef struct BrokerMsgReceivedArgs
{
  rdmnet_client_scope_t scope_handle;
  const BrokerMessage* msg;
} BrokerMsgReceivedArgs;

typedef struct LlrpMsgReceivedArgs
{
  const LlrpRemoteRdmCommand* cmd;
} LlrpMsgReceivedArgs;

typedef struct RptMsgReceivedArgs
{
  rdmnet_client_scope_t scope_handle;
  RptClientMessage msg;
} RptMsgReceivedArgs;

typedef struct EptMsgReceivedArgs
{
  rdmnet_client_scope_t scope_handle;
  EptClientMessage msg;
} EptMsgReceivedArgs;

typedef struct RptCallbackDispatchInfo
{
  RptClientCallbacks cbs;
  union
  {
    RptMsgReceivedArgs msg_received;
    LlrpMsgReceivedArgs llrp_msg_received;
  } args;
} RptCallbackDispatchInfo;

typedef struct EptCallbackDispatchInfo
{
  EptClientCallbacks cbs;
  EptMsgReceivedArgs msg_received;
} EptCallbackDispatchInfo;

typedef struct ClientCallbackDispatchInfo
{
  rdmnet_client_t handle;
  client_protocol_t type;
  client_callback_t which;
  void* context;
  union
  {
    RptCallbackDispatchInfo rpt;
    EptCallbackDispatchInfo ept;
  } prot_info;
  union
  {
    ConnectedArgs connected;
    ConnectFailedArgs connect_failed;
    DisconnectedArgs disconnected;
    BrokerMsgReceivedArgs broker_msg_received;
  } common_args;
} ClientCallbackDispatchInfo;

/*! An unsolicited RDM response generated by a local component, to be sent over RDMnet. */
typedef struct RdmnetSourceAddr
{
  /*! The endpoint from which this response is being sent. */
  uint16_t source_endpoint;
  /*! The UID of the RDM responder from which this response is being sent. */
  RdmUid rdm_source_uid;
  /*! The sub-device from which this response is being sent, or 0 for the root device. */
  uint16_t subdevice;
} RdmnetSourceAddr;

etcpal_error_t rdmnet_client_init(void);
void rdmnet_client_deinit(void);

void rdmnet_address_to_default_responder(RdmnetDestinationAddr* addr, const RdmUid* responder_uid);
void rdmnet_address_to_default_responder_subdevice(RdmnetDestinationAddr* addr, const RdmUid* responder_uid,
                                                   uint16_t subdevice);
void rdmnet_address_to_sub_responder(RdmnetDestinationAddr* addr, const RdmUid* rdmnet_uid, uint16_t endpoint,
                                     const RdmUid* responder_uid);
void rdmnet_address_to_sub_responder_subdevice(RdmnetDestinationAddr* addr, const RdmUid* rdmnet_uid, uint16_t endpoint,
                                               const RdmUid* responder_uid, uint16_t subdevice);

void rdmnet_rpt_client_config_init(RdmnetRptClientConfig* config, uint16_t manufacturer_id);
void rdmnet_ept_client_config_init(RdmnetEptClientConfig* config);

etcpal_error_t rdmnet_rpt_client_create(const RdmnetRptClientConfig* config, rdmnet_client_t* handle);
etcpal_error_t rdmnet_ept_client_create(const RdmnetEptClientConfig* config, rdmnet_client_t* handle);
etcpal_error_t rdmnet_client_destroy(rdmnet_client_t handle, rdmnet_disconnect_reason_t disconnect_reason);

etcpal_error_t rdmnet_client_add_scope(rdmnet_client_t handle, const RdmnetScopeConfig* scope_config,
                                       rdmnet_client_scope_t* scope_handle);
etcpal_error_t rdmnet_client_remove_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                          rdmnet_disconnect_reason_t reason);
etcpal_error_t rdmnet_client_get_scope_string(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                              char* scope_str_buf);
etcpal_error_t rdmnet_client_get_static_broker_config(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                      bool* has_static_broker_addr, EtcPalSockAddr* static_broker_addr);

etcpal_error_t rdmnet_client_change_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                          const RdmnetScopeConfig* new_scope_config,
                                          rdmnet_disconnect_reason_t disconnect_reason);
etcpal_error_t rdmnet_client_change_search_domain(rdmnet_client_t handle, const char* new_search_domain,
                                                  rdmnet_disconnect_reason_t reason);

etcpal_error_t rdmnet_client_request_client_list(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle);
etcpal_error_t rdmnet_client_request_dynamic_uids(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                  const BrokerDynamicUidRequest* requests, size_t num_requests);
etcpal_error_t rdmnet_client_request_dynamic_uid_mappings(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                          const RdmUid* uids, size_t num_uids);

etcpal_error_t rdmnet_client_send_rdm_command(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetDestinationAddr* destination,
                                              rdmnet_client_command_class_t command_class, uint16_t param_id,
                                              const uint8_t* data, uint8_t data_len, uint32_t* seq_num);
etcpal_error_t rdmnet_client_send_get_command(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetDestinationAddr* destination, uint16_t param_id,
                                              const uint8_t* data, uint8_t data_len, uint32_t* seq_num);
etcpal_error_t rdmnet_client_send_set_command(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetDestinationAddr* destination, uint16_t param_id,
                                              const uint8_t* data, uint8_t data_len, uint32_t* seq_num);

etcpal_error_t rdmnet_rpt_client_send_rdm_ack(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetRdmCommand* received_cmd, const uint8_t* response_data,
                                              size_t response_data_len);
etcpal_error_t rdmnet_rpt_client_send_rdm_nack(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                               const RdmnetRdmCommand* received_cmd, rdm_nack_reason_t nack_reason);
etcpal_error_t rdmnet_rpt_client_send_unsolicited_response(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                           const RdmnetSourceAddr* source_addr, uint16_t param_id,
                                                           const uint8_t* data, size_t data_len);

etcpal_error_t rdmnet_rpt_client_send_status(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                             const RdmnetRdmCommand* received_cmd, rpt_status_code_t status_code,
                                             const char* status_string);

etcpal_error_t rdmnet_rpt_client_send_llrp_ack(rdmnet_client_t handle, const LlrpSavedRdmCommand* received_cmd,
                                               const uint8_t* response_data, uint8_t response_data_len);
etcpal_error_t rdmnet_rpt_client_send_llrp_nack(rdmnet_client_t handle, const LlrpSavedRdmCommand* received_cmd,
                                                rdm_nack_reason_t nack_reason);

etcpal_error_t rdmnet_ept_client_send_data(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                           const EtcPalUuid* dest_cid, const EptDataMsg* data);
etcpal_error_t rdmnet_ept_client_send_status(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                             const EtcPalUuid* dest_cid, ept_status_code_t status_code,
                                             const char* status_string);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_CLIENT_H_ */
