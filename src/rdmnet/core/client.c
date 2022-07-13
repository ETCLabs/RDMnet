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

#include "rdmnet/core/client.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "etcpal/common.h"
#include "etcpal/pack.h"
#include "rdm/defs.h"
#include "rdm/message.h"
#include "rdmnet/core/broker_prot.h"
#include "rdmnet/core/client_entry.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/rpt_prot.h"
#include "rdmnet/core/util.h"
#include "rdmnet/defs.h"
#include "rdmnet/discovery.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/core/util.h"

/***************************** Private types *********************************/

// Represents a supported parameter to add to a SUPPORTED_PARAMETERS RDM response.
typedef struct SupportedParameter
{
  uint16_t          pid;
  rpt_client_type_t client_type;
  bool              found;
} SupportedParameter;

/*************************** Private constants *******************************/

#define INITIAL_SCOPES_CAPACITY 5

#define RDM_RESP_BUF_STATIC_SIZE (RDMNET_PARSER_MAX_ACK_OVERFLOW_RESPONSES * RDM_MAX_PDL)

// Calculation of the internal response buffer size

// TODO change to defined value when it is available from the RDM library.
#define TCP_COMMS_STATUS_PD_SIZE 87
#define INTERNAL_PD_BUF_STATIC_SIZE (RDMNET_MAX_SCOPES_PER_CLIENT * TCP_COMMS_STATUS_PD_SIZE)

#if E133_DOMAIN_STRING_PADDED_LENGTH > INTERNAL_PD_BUF_STATIC_SIZE
#undef INTERNAL_PD_BUF_STATIC_SIZE
#define INTERNAL_PD_BUF_STATIC_SIZE E133_DOMAIN_STRING_PADDED_LENGTH
#endif

#define ENDPOINT_RESPONDERS_BUF_SIZE ((RDMNET_MAX_RESPONDERS_PER_DEVICE * 6) + 6)
#if ENDPOINT_RESPONDERS_BUF_SIZE > INTERNAL_PD_BUF_STATIC_SIZE
#undef INTERNAL_PD_BUF_STATIC_SIZE
#define INTERNAL_PD_BUF_STATIC_SIZE ENDPOINT_RESPONDERS_BUF_SIZE
#endif

#define ENDPOINT_LIST_BUF_SIZE ((RDMNET_MAX_ENDPOINTS_PER_DEVICE * 3) + 4)
#if ENDPOINT_LIST_BUF_SIZE > INTERNAL_PD_BUF_STATIC_SIZE
#undef INTERNAL_PD_BUF_STATIC_SIZE
#define INTERNAL_PD_BUF_STATIC_SIZE ENDPOINT_LIST_BUF_SIZE
#endif

#define INTERNAL_PD_BUF_INITIAL_CAPACITY 32

/***************************** Private macros ********************************/

#define GET_CLIENT_FROM_LLRP_TARGET(targetptr) (RCClient*)((char*)(targetptr)-offsetof(RCClient, llrp_target))
#define GET_CLIENT_SCOPE_FROM_CONN(connptr) (RCClientScope*)((char*)(connptr)-offsetof(RCClientScope, conn))

#define RC_CLIENT_LOCK(client_ptr) etcpal_mutex_lock((client_ptr)->lock)
#define RC_CLIENT_UNLOCK(client_ptr) etcpal_mutex_unlock((client_ptr)->lock)

#define RDM_CC_IS_NON_DISC_RESPONSE(cc) (((cc) == E120_GET_COMMAND_RESPONSE) || ((cc) == E120_SET_COMMAND_RESPONSE))
#define RDM_CC_IS_NON_DISC_COMMAND(cc) (((cc) == E120_GET_COMMAND) || ((cc) == E120_SET_COMMAND))

#define CHECK_SCOPE_HANDLE(scope_handle) \
  if ((scope_handle) < 0)                \
    return kEtcPalErrInvalid;

#if RDMNET_DYNAMIC_MEM
#define BEGIN_FOR_EACH_CLIENT_SCOPE(client_ptr)                                                                       \
  for (RCClientScope** scope_ptr = (client_ptr)->scopes; scope_ptr < (client_ptr)->scopes + (client_ptr)->num_scopes; \
       ++scope_ptr)                                                                                                   \
  {                                                                                                                   \
    RCClientScope* scope = *scope_ptr;
#else
#define BEGIN_FOR_EACH_CLIENT_SCOPE(client_ptr)                                                                  \
  for (RCClientScope* scope = (client_ptr)->scopes; scope < (client_ptr)->scopes + RDMNET_MAX_SCOPES_PER_CLIENT; \
       ++scope)                                                                                                  \
  {
#endif
#define END_FOR_EACH_CLIENT_SCOPE(client_ptr) }

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
static uint8_t internal_pd_buf[INTERNAL_PD_BUF_STATIC_SIZE];
static uint8_t received_rdm_response_buf[RDM_RESP_BUF_STATIC_SIZE];
#else
static uint8_t* internal_pd_buf;
size_t          internal_pd_buf_size;
#endif

static void monitorcb_broker_found(rdmnet_scope_monitor_t      handle,
                                   const RdmnetBrokerDiscInfo* broker_info,
                                   void*                       context);
static void monitorcb_broker_updated(rdmnet_scope_monitor_t      handle,
                                     const RdmnetBrokerDiscInfo* broker_info,
                                     void*                       context);
static void monitorcb_broker_lost(rdmnet_scope_monitor_t handle,
                                  const char*            scope_str,
                                  const char*            service_name,
                                  void*                  context);

// clang-format off
static const RdmnetScopeMonitorCallbacks disc_callbacks =
{
  monitorcb_broker_found,
  monitorcb_broker_updated,
  monitorcb_broker_lost,
  NULL
};
// clang-format on

static void                conncb_connected(RCConnection* conn, const RCConnectedInfo* connected_info);
static void                conncb_connect_failed(RCConnection* conn, const RCConnectFailedInfo* failed_info);
static void                conncb_disconnected(RCConnection* conn, const RCDisconnectedInfo* disconn_info);
static rc_message_action_t conncb_msg_received(RCConnection* conn, const RdmnetMessage* message);
static void                conncb_destroyed(RCConnection* conn);

// clang-format off
static const RCConnectionCallbacks kConnCallbacks =
{
  conncb_connected,
  conncb_connect_failed,
  conncb_disconnected,
  conncb_msg_received,
  conncb_destroyed
};
// clang-format on

static void llrpcb_rdm_cmd_received(RCLlrpTarget*                target,
                                    const LlrpRdmCommand*        cmd,
                                    RCLlrpTargetSyncRdmResponse* response);
static void llrpcb_target_destroyed(RCLlrpTarget* target);

// clang-format off
static const RCLlrpTargetCallbacks kLlrpTargetCallbacks =
{
  llrpcb_rdm_cmd_received,
  llrpcb_target_destroyed
};
// clang-format on

// clang-format off
// This list must be kept sorted by numeric value of PID.
static SupportedParameter kSupportedParametersChecklist[] = {
  {E120_SUPPORTED_PARAMETERS, kRPTClientTypeUnknown, false},
  {E120_DEVICE_MODEL_DESCRIPTION, kRPTClientTypeUnknown, false},
  {E120_MANUFACTURER_LABEL, kRPTClientTypeUnknown, false},
  {E120_DEVICE_LABEL, kRPTClientTypeUnknown, false},
  {E120_SOFTWARE_VERSION_LABEL, kRPTClientTypeUnknown, false},
  {E133_COMPONENT_SCOPE, kRPTClientTypeUnknown, false},
  {E133_SEARCH_DOMAIN, kRPTClientTypeUnknown, false},
  {E133_TCP_COMMS_STATUS, kRPTClientTypeUnknown, false},
  {E137_7_ENDPOINT_LIST, kRPTClientTypeDevice, false},
  {E137_7_ENDPOINT_LIST_CHANGE, kRPTClientTypeDevice, false},
  {E137_7_ENDPOINT_RESPONDERS, kRPTClientTypeDevice, false},
  {E137_7_ENDPOINT_RESPONDER_LIST_CHANGE, kRPTClientTypeDevice, false},
  {E120_IDENTIFY_DEVICE, kRPTClientTypeUnknown, false},
};
#define SUPPORTED_PARAMETERS_CHECKLIST_SIZE \
  (sizeof(kSupportedParametersChecklist) / sizeof(kSupportedParametersChecklist[0]))
// clang-format on

/*********************** Private function prototypes *************************/

// Create and destroy clients and scopes
static etcpal_error_t create_and_add_scope_entry(RCClient*                client,
                                                 const RdmnetScopeConfig* config,
                                                 RCClientScope**          new_entry);
static RCClientScope* get_scope(RCClient* client, rdmnet_client_scope_t scope_handle);
static RCClientScope* get_scope_by_id(RCClient* client, const char* scope_str);
static RCClientScope* get_unused_scope_entry(RCClient* client);
static bool           scope_handle_in_use(int handle_val, void* context);
static void mark_scope_for_destruction(RCClientScope* scope, const rdmnet_disconnect_reason_t* disconnect_reason);
static bool client_fully_destroyed(RCClient* client);

static etcpal_error_t start_scope_discovery(RCClientScope* scope, const char* search_domain);
static void           attempt_connection_on_listen_addrs(RCClientScope* scope);
static etcpal_error_t start_connection_for_scope(RCClientScope*              scope,
                                                 const EtcPalSockAddr*       broker_addr,
                                                 rdmnet_disconnect_reason_t* disconnect_reason);
static void           clear_discovered_broker_info(RCClientScope* scope);

// Helpers for send functions
static etcpal_error_t send_rdm_ack_internal(RCClient*               client,
                                            RCClientScope*          scope,
                                            const RptHeader*        rpt_header,
                                            const RdmCommandHeader* received_cmd_header,
                                            const uint8_t*          received_cmd_data,
                                            uint8_t                 received_cmd_data_len,
                                            const uint8_t*          resp_data,
                                            size_t                  resp_data_len);
static etcpal_error_t send_rdm_nack_internal(RCClient*               client,
                                             RCClientScope*          scope,
                                             const RptHeader*        rpt_header,
                                             const RdmCommandHeader* received_cmd_header,
                                             const uint8_t*          received_cmd_data,
                                             uint8_t                 received_cmd_data_len,
                                             rdm_nack_reason_t       nack_reason);
static etcpal_error_t client_send_update_internal(RCClient*               client,
                                                  RCClientScope*          scope,
                                                  const RdmnetSourceAddr* source_addr,
                                                  uint16_t                param_id,
                                                  const uint8_t*          data,
                                                  size_t                  data_len);

// Some special functions for handling RDM responses from the application
static void append_to_supported_parameters(RCClient*               client,
                                           const RdmCommandHeader* received_cmd_header,
                                           RdmBuffer*              param_resp_buf,
                                           size_t                  param_num_buffers,
                                           size_t*                 total_resp_size);
static void change_destination_to_broadcast(RdmBuffer* resp_buf, size_t total_resp_size);

// Manage callbacks
static bool connect_failed_will_retry(rdmnet_connect_fail_event_t event, rdmnet_connect_status_t status);
static bool disconnected_will_retry(rdmnet_disconnect_event_t event, rdmnet_disconnect_reason_t reason);

// Message handling
static void free_rpt_client_message(RptClientMessage* msg);
#if 0
static void free_ept_client_message(EptClientMessage* msg);
#endif
static bool parse_rpt_message(const RCClientScope* scope, const RptMessage* rmsg, RptClientMessage* msg_out);
static bool parse_rpt_request(const RptMessage* rmsg, RptClientMessage* msg_out);
static bool parse_rpt_notification(const RCClientScope* scope, const RptMessage* rmsg, RptClientMessage* msg_out);
static bool parse_rpt_status(const RptMessage* rmsg, RptClientMessage* msg_out);
static bool unpack_notification_rdm_buffer(const RdmBuffer*   buffer,
                                           RdmnetRdmResponse* resp,
                                           uint8_t*           resp_data_buf,
                                           bool*              is_first_resp);
static void send_rdm_response_if_requested(RCClient*               client,
                                           RCClientScope*          scope,
                                           const RptClientMessage* msg,
                                           RdmnetSyncRdmResponse*  response,
                                           bool                    use_internal_buf);

static bool handle_rdm_command_internally(RCClient*               client,
                                          RCClientScope*          scope,
                                          const RptClientMessage* cmd,
                                          RdmnetSyncRdmResponse*  resp);
static void handle_tcp_comms_status(RCClient* client, const RdmnetRdmCommand* cmd, RdmnetSyncRdmResponse* resp);

// Memory for holding response data
static bool get_rdm_response_data_buf(const RptRdmBufList* buf_list, uint8_t** buf_ptr);
static void free_rdm_response_data_buf(uint8_t* buf);

/*************************** Function definitions ****************************/

etcpal_error_t rc_client_module_init(void)
{
#if RDMNET_DYNAMIC_MEM
  internal_pd_buf = (uint8_t*)malloc(INTERNAL_PD_BUF_INITIAL_CAPACITY);
  if (internal_pd_buf)
    internal_pd_buf_size = INTERNAL_PD_BUF_INITIAL_CAPACITY;
  else
    return kEtcPalErrNoMem;
#endif

  return kEtcPalErrOk;
}

void rc_client_module_deinit(void)
{
#if RDMNET_DYNAMIC_MEM
  if (internal_pd_buf)
    free(internal_pd_buf);
  internal_pd_buf_size = 0;
#endif
}

/*
 * Initialize a new RCClient structure.
 *
 * Initialize the items marked in the struct before passing it to this function. The LLRP
 * information will be used to create an associated LLRP target.
 */
etcpal_error_t rc_rpt_client_register(RCClient*                  client,
                                      bool                       create_llrp_target,
                                      const EtcPalMcastNetintId* llrp_netints,
                                      size_t                     num_llrp_netints)
{
  RDMNET_ASSERT(client);
  client->marked_for_destruction = false;

  init_int_handle_manager(&client->scope_handle_manager, -1, scope_handle_in_use, client);
#if RDMNET_DYNAMIC_MEM
  client->scopes = NULL;
  client->num_scopes = 0;
#else
  for (RCClientScope* scope = client->scopes; scope < client->scopes + RDMNET_MAX_SCOPES_PER_CLIENT; ++scope)
  {
    scope->handle = RDMNET_CLIENT_SCOPE_INVALID;
  }
#endif

  if (create_llrp_target)
  {
    RCLlrpTarget* target = &client->llrp_target;
    target->cid = client->cid;
    target->uid = RC_RPT_CLIENT_DATA(client)->uid;
    target->component_type =
        (RC_RPT_CLIENT_DATA(client)->type == kRPTClientTypeController ? kLlrpCompRptController : kLlrpCompRptDevice);
    target->callbacks = kLlrpTargetCallbacks;
    target->lock = client->lock;

    etcpal_error_t res = rc_llrp_target_register(&client->llrp_target, llrp_netints, num_llrp_netints);
    if (res == kEtcPalErrOk)
    {
      client->target_valid = true;
    }
    else
    {
      RC_CLIENT_DEINIT_SCOPES(client);
      return res;
    }
  }
  else
  {
    client->target_valid = false;
  }

  return kEtcPalErrOk;
}

/*
 * Unregister an RCClient structure.
 *
 * Will disconnect from all brokers to which this client is currently connected, sending the
 * disconnect reason provided in the disconnect_reason parameter.
 *
 * If it returns true, the structure holding this client can be deallocated immediately after this
 * function returns. Otherwise, you must wait to receive the client's destroyed() callback.
 */
bool rc_client_unregister(RCClient* client, rdmnet_disconnect_reason_t disconnect_reason)
{
  RDMNET_ASSERT(client);

  BEGIN_FOR_EACH_CLIENT_SCOPE(client)
  {
    if (scope->handle != RDMNET_CLIENT_SCOPE_INVALID)
      mark_scope_for_destruction(scope, &disconnect_reason);
  }
  END_FOR_EACH_CLIENT_SCOPE(client)

  if (client->target_valid)
    rc_llrp_target_unregister(&client->llrp_target);

  client->marked_for_destruction = true;
  return client_fully_destroyed(client);
}

/*
 * Add a new scope to a client instance. The library will attempt to discover and connect to a
 * broker for the scope (or just connect if a static broker address is given); the status of these
 * attempts will be communicated via the callbacks associated with the client instance.
 */
etcpal_error_t rc_client_add_scope(RCClient*                client,
                                   const RdmnetScopeConfig* scope_config,
                                   rdmnet_client_scope_t*   scope_handle)
{
  RDMNET_ASSERT(client);
  RDMNET_ASSERT(scope_config);
  RDMNET_ASSERT(scope_handle);

  if (get_scope_by_id(client, scope_config->scope) != NULL)
    return kEtcPalErrExists;

  RCClientScope* new_entry;
  etcpal_error_t res = create_and_add_scope_entry(client, scope_config, &new_entry);
  if (res == kEtcPalErrOk)
  {
    // Start discovery or connection on the new scope (depending on whether a static broker was
    // configured)
    if (ETCPAL_IP_IS_INVALID(&scope_config->static_broker_addr.ip))
      res = start_scope_discovery(new_entry, client->search_domain);
    else
      res = start_connection_for_scope(new_entry, &new_entry->static_broker_addr, NULL);

    if (res == kEtcPalErrOk)
    {
      *scope_handle = new_entry->handle;
    }
    else
    {
      mark_scope_for_destruction(new_entry, NULL);
    }
  }

  return res;
}

/*
 * Remove a previously-added scope from a client instance. After this call completes, scope_handle
 * will no longer be valid. Sends the RDMnet protocol disconnect reason code provided in
 * disconnect_reason to the connected broker.
 */
etcpal_error_t rc_client_remove_scope(RCClient*                  client,
                                      rdmnet_client_scope_t      scope_handle,
                                      rdmnet_disconnect_reason_t disconnect_reason)
{
  RDMNET_ASSERT(client);

  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  mark_scope_for_destruction(scope, &disconnect_reason);
  return kEtcPalErrOk;
}

/*
 * Change the settings of a previously-added scope. Changed settings will cause the client to
 * disconnect from any connected broker for the old scope, sending the RDMnet-level disconnect
 * reason code provided in disconnect_reason.
 */
etcpal_error_t rc_client_change_scope(RCClient*                  client,
                                      rdmnet_client_scope_t      scope_handle,
                                      const RdmnetScopeConfig*   new_scope_config,
                                      rdmnet_disconnect_reason_t disconnect_reason)
{
  RDMNET_ASSERT(client);
  RDMNET_ASSERT(new_scope_config);
  RDMNET_ASSERT(new_scope_config->scope);

  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  if (strcmp(new_scope_config->scope, scope->id) == 0 &&
      etcpal_ip_and_port_equal(&new_scope_config->static_broker_addr, &scope->static_broker_addr))
  {
    // Scope is the same, do nothing
    return kEtcPalErrOk;
  }

  etcpal_error_t res = kEtcPalErrInvalid;
  if (scope->state != kRCScopeStateMarkedForDestruction)
  {
    rdmnet_safe_strncpy(scope->id, new_scope_config->scope, E133_SCOPE_STRING_PADDED_LENGTH);
    scope->static_broker_addr = new_scope_config->static_broker_addr;
    clear_discovered_broker_info(scope);

    if (scope->monitor_handle != RDMNET_SCOPE_MONITOR_INVALID)
    {
      rdmnet_disc_stop_monitoring(scope->monitor_handle);
      scope->monitor_handle = RDMNET_SCOPE_MONITOR_INVALID;
    }

    if (ETCPAL_IP_IS_INVALID(&new_scope_config->static_broker_addr.ip))
    {
      // New scope is a dynamic scope
      if (scope->state == kRCScopeStateConnecting || scope->state == kRCScopeStateConnected)
      {
        rc_conn_disconnect(&scope->conn, disconnect_reason);
      }
      res = start_scope_discovery(scope, client->search_domain);
    }
    else
    {
      // New scope is a static scope
      res = start_connection_for_scope(scope, &new_scope_config->static_broker_addr, &disconnect_reason);
    }
  }

  return res;
}

/*
 * Retrieve the scope string and static broker address of a previously-added scope. scope_str_buf
 * and static_broker_addr are filled in on success with the requested information. scope_str_buf
 * must be at least of length E133_SCOPE_STRING_PADDED_LENGTH.
 */
etcpal_error_t rc_client_get_scope(RCClient*             client,
                                   rdmnet_client_scope_t scope_handle,
                                   char*                 scope_str_buf,
                                   EtcPalSockAddr*       static_broker_addr)
{
  RDMNET_ASSERT(client);

  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  if (scope_str_buf)
    rdmnet_safe_strncpy(scope_str_buf, scope->id, E133_SCOPE_STRING_PADDED_LENGTH);
  if (static_broker_addr)
    *static_broker_addr = scope->static_broker_addr;

  return kEtcPalErrOk;
}

/*
 * Change the search domain setting of a client. A changed domain will cause the client to
 * disconnect from any connected broker for which dynamic discovery is configured and restart the
 * discovery process, sending the RDMnet-level disconnect reason code provided in disconnect_reason.
 */
etcpal_error_t rc_client_change_search_domain(RCClient*                  client,
                                              const char*                new_search_domain,
                                              rdmnet_disconnect_reason_t disconnect_reason)
{
  RDMNET_ASSERT(client);

  if (strcmp(new_search_domain, client->search_domain) == 0)
  {
    // Domain is the same, do nothing
    return kEtcPalErrOk;
  }

  rdmnet_safe_strncpy(client->search_domain, new_search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);

  BEGIN_FOR_EACH_CLIENT_SCOPE(client)
  {
    if (scope->handle != RDMNET_CLIENT_SCOPE_INVALID && scope->state != kRCScopeStateMarkedForDestruction &&
        scope->monitor_handle != RDMNET_SCOPE_MONITOR_INVALID)
    {
      if (scope->state == kRCScopeStateConnecting || scope->state == kRCScopeStateConnected)
      {
        rc_conn_disconnect(&scope->conn, disconnect_reason);
      }
      rdmnet_disc_stop_monitoring(scope->monitor_handle);
      scope->monitor_handle = RDMNET_SCOPE_MONITOR_INVALID;
      clear_discovered_broker_info(scope);
      start_scope_discovery(scope, new_search_domain);
    }
  }
  END_FOR_EACH_CLIENT_SCOPE(client)

  return kEtcPalErrOk;
}

/*
 * Send a message requesting an RDMnet client list from a broker on a given scope. The response
 * will be delivered via an RCClientBrokerMsgReceivedCb containing a ClientList broker message.
 */
etcpal_error_t rc_client_request_client_list(RCClient* client, rdmnet_client_scope_t scope_handle)
{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;
  return rc_broker_send_fetch_client_list(&scope->conn, &client->cid);
}

/*
 * Send a message requesting one or more dynamic UIDs from a broker on a given scope. The response
 * will be delivered via an RCClientBrokerMsgReceivedCb containing an
 * RdmnetDynamicUidAssignmentList broker message.
 */
etcpal_error_t rc_client_request_dynamic_uids(RCClient*             client,
                                              rdmnet_client_scope_t scope_handle,
                                              const EtcPalUuid*     responder_ids,
                                              size_t                num_responders)
{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;
  return rc_broker_send_request_dynamic_uids(&scope->conn, &client->cid, RC_RPT_CLIENT_DATA(client)->uid.manu,
                                             responder_ids, num_responders);
}

/*
 * Send a message requesting the mapping of one or more dynamic UIDs to RIDs from a broker on a
 * given scope. The response will be delivered via an RCClientBrokerMsgReceivedCb containing a
 * RdmnetDynamicUidAssignmentList broker message.
 */
etcpal_error_t rc_client_request_responder_ids(RCClient*             client,
                                               rdmnet_client_scope_t scope_handle,
                                               const RdmUid*         uids,
                                               size_t                num_uids)
{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;
  return rc_broker_send_fetch_uid_assignment_list(&scope->conn, &client->cid, uids, num_uids);
}

/*
 * Send an RDM command from an RPT client on a scope. The response will be delivered via an
 * RcClientRptMsgReceivedCb containing an RdmnetRdmResponse. seq_num is filled in on success with a
 * sequence number which can be used to match the command with a response.
 */
etcpal_error_t rc_client_send_rdm_command(RCClient*                    client,
                                          rdmnet_client_scope_t        scope_handle,
                                          const RdmnetDestinationAddr* destination,
                                          rdmnet_command_class_t       command_class,
                                          uint16_t                     param_id,
                                          const uint8_t*               data,
                                          uint8_t                      data_len,
                                          uint32_t*                    seq_num)
{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  RptHeader header;
  header.source_uid = scope->uid;
  header.source_endpoint_id = E133_NULL_ENDPOINT;
  header.dest_uid = destination->rdmnet_uid;
  header.dest_endpoint_id = destination->endpoint;
  header.seqnum = scope->send_seq_num;

  RdmCommandHeader rdm_header;
  rdm_header.source_uid = scope->uid;
  rdm_header.dest_uid = destination->rdm_uid;
  rdm_header.port_id = 1;
  rdm_header.transaction_num = (uint8_t)(header.seqnum & 0xffu);
  rdm_header.subdevice = destination->subdevice;
  rdm_header.command_class = (rdm_command_class_t)command_class;
  rdm_header.param_id = param_id;

  RdmBuffer      buf_to_send;
  etcpal_error_t res = rdm_pack_command(&rdm_header, data, data_len, &buf_to_send);
  if (res == kEtcPalErrOk)
  {
    res = rc_rpt_send_request(&scope->conn, &client->cid, &header, &buf_to_send);
    if (res == kEtcPalErrOk)
    {
      if (seq_num)
        *seq_num = scope->send_seq_num;
      ++scope->send_seq_num;
    }
  }

  return res;
}

/* Send an RDM ACK response from an RPT client. */
etcpal_error_t rc_client_send_rdm_ack(RCClient*                    client,
                                      rdmnet_client_scope_t        scope_handle,
                                      const RdmnetSavedRdmCommand* received_cmd,
                                      const uint8_t*               response_data,
                                      size_t                       response_data_len)
{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  RptHeader header;
  header.source_uid = scope->uid;
  header.source_endpoint_id = received_cmd->dest_endpoint;
  if (received_cmd->rdm_header.command_class == kRdmCCSetCommand)
    header.dest_uid = kRdmnetControllerBroadcastUid;
  else
    header.dest_uid = received_cmd->rdmnet_source_uid;
  header.dest_endpoint_id = E133_NULL_ENDPOINT;
  header.seqnum = received_cmd->seq_num;

  return send_rdm_ack_internal(client, scope, &header, &received_cmd->rdm_header, received_cmd->data,
                               received_cmd->data_len, response_data, response_data_len);
}

/* Send an RDM NACK response from an RPT client. */
etcpal_error_t rc_client_send_rdm_nack(RCClient*                    client,
                                       rdmnet_client_scope_t        scope_handle,
                                       const RdmnetSavedRdmCommand* received_cmd,
                                       rdm_nack_reason_t            nack_reason)
{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  RptHeader header;
  header.source_uid = scope->uid;
  header.source_endpoint_id = received_cmd->dest_endpoint;
  header.dest_uid = received_cmd->rdmnet_source_uid;
  header.dest_endpoint_id = E133_NULL_ENDPOINT;
  header.seqnum = received_cmd->seq_num;

  return send_rdm_nack_internal(client, scope, &header, &received_cmd->rdm_header, received_cmd->data,
                                received_cmd->data_len, nack_reason);
}

etcpal_error_t rc_client_send_rdm_update(RCClient*             client,
                                         rdmnet_client_scope_t scope_handle,
                                         uint16_t              subdevice,
                                         uint16_t              param_id,
                                         const uint8_t*        data,
                                         size_t                data_len)

{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  RdmnetSourceAddr source_addr;
  source_addr.rdm_source_uid = scope->uid;
  source_addr.source_endpoint = E133_NULL_ENDPOINT;
  source_addr.subdevice = subdevice;

  return client_send_update_internal(client, scope, &source_addr, param_id, data, data_len);
}

etcpal_error_t rc_client_send_rdm_update_from_responder(RCClient*               client,
                                                        rdmnet_client_scope_t   scope_handle,
                                                        const RdmnetSourceAddr* source_addr,
                                                        uint16_t                param_id,
                                                        const uint8_t*          data,
                                                        size_t                  data_len)
{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  return client_send_update_internal(client, scope, source_addr, param_id, data, data_len);
}

etcpal_error_t rc_client_send_rpt_status(RCClient*                    client,
                                         rdmnet_client_scope_t        scope_handle,
                                         const RdmnetSavedRdmCommand* received_cmd,
                                         rpt_status_code_t            status_code,
                                         const char*                  status_string)
{
  RDMNET_ASSERT(client);
  CHECK_SCOPE_HANDLE(scope_handle);
  RCClientScope* scope = get_scope(client, scope_handle);
  if (!scope)
    return kEtcPalErrNotFound;

  RptHeader header;
  header.source_uid = scope->uid;
  header.source_endpoint_id = received_cmd->dest_endpoint;
  header.dest_uid = received_cmd->rdmnet_source_uid;
  header.dest_endpoint_id = E133_NULL_ENDPOINT;
  header.seqnum = received_cmd->seq_num;

  RptStatusMsg status;
  status.status_code = status_code;
  status.status_string = status_string;

  return rc_rpt_send_status(&scope->conn, &client->cid, &header, &status);
}

etcpal_error_t rc_client_send_llrp_ack(RCClient*                  client,
                                       const LlrpSavedRdmCommand* received_cmd,
                                       const uint8_t*             response_data,
                                       uint8_t                    response_data_len)
{
  RDMNET_ASSERT(client);
  return rc_llrp_target_send_ack(&client->llrp_target, received_cmd, response_data, response_data_len);
}

etcpal_error_t rc_client_send_llrp_nack(RCClient*                  client,
                                        const LlrpSavedRdmCommand* received_cmd,
                                        rdm_nack_reason_t          nack_reason)
{
  RDMNET_ASSERT(client);
  return rc_llrp_target_send_nack(&client->llrp_target, received_cmd, nack_reason);
}

etcpal_error_t rc_client_send_ept_data(RCClient*             client,
                                       rdmnet_client_scope_t scope_handle,
                                       const EtcPalUuid*     dest_cid,
                                       uint16_t              manufacturer_id,
                                       uint16_t              protocol_id,
                                       const uint8_t*        data,
                                       size_t                data_len)
{
  ETCPAL_UNUSED_ARG(client);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(dest_cid);
  ETCPAL_UNUSED_ARG(manufacturer_id);
  ETCPAL_UNUSED_ARG(protocol_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

etcpal_error_t rc_client_send_ept_status(RCClient*             client,
                                         rdmnet_client_scope_t scope_handle,
                                         const EtcPalUuid*     dest_cid,
                                         ept_status_code_t     status_code,
                                         const char*           status_string)
{
  ETCPAL_UNUSED_ARG(client);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(dest_cid);
  ETCPAL_UNUSED_ARG(status_code);
  ETCPAL_UNUSED_ARG(status_string);
  return kEtcPalErrNotImpl;
}

uint8_t* rc_client_get_internal_response_buf(size_t size)
{
#if RDMNET_DYNAMIC_MEM
  if (size <= internal_pd_buf_size)
  {
    return internal_pd_buf;
  }
  else
  {
    size_t new_size = internal_pd_buf_size;
    while (new_size < size)
      new_size *= 2;
    uint8_t* new_buf = (uint8_t*)realloc(internal_pd_buf, new_size);
    if (new_buf)
    {
      internal_pd_buf = new_buf;
      return internal_pd_buf;
    }
    else
    {
      return NULL;
    }
  }
#else
  if (size <= INTERNAL_PD_BUF_STATIC_SIZE)
    return internal_pd_buf;
  else
    return NULL;
#endif
}

/******************************************************************************
 * Callback functions from the discovery interface
 *****************************************************************************/

static bool copy_broker_listen_addrs(RCClientScope* scope, const RdmnetBrokerDiscInfo* broker_info)
{
#if RDMNET_DYNAMIC_MEM
  if (scope->num_broker_listen_addrs != broker_info->num_listen_addrs)
  {
    if (scope->broker_listen_addrs)
    {
      EtcPalIpAddr* new_addr_array =
          (EtcPalIpAddr*)realloc(scope->broker_listen_addrs, broker_info->num_listen_addrs * sizeof(EtcPalIpAddr));
      if (new_addr_array)
        scope->broker_listen_addrs = new_addr_array;
      else
        return false;
    }
    else
    {
      scope->broker_listen_addrs = (EtcPalIpAddr*)calloc(broker_info->num_listen_addrs, sizeof(EtcPalIpAddr));
      if (!scope->broker_listen_addrs)
        return false;
    }
  }
  memcpy(scope->broker_listen_addrs, broker_info->listen_addrs, broker_info->num_listen_addrs * sizeof(EtcPalIpAddr));
#else
  size_t num_addrs_to_copy =
      (broker_info->num_listen_addrs > RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER ? RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER
                                                                                       : broker_info->num_listen_addrs);
  memcpy(scope->broker_listen_addrs, broker_info->listen_addrs, num_addrs_to_copy * sizeof(EtcPalIpAddr));
#endif
  scope->num_broker_listen_addrs = broker_info->num_listen_addrs;
  return true;
}

void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  RDMNET_LOG_INFO("Broker '%s' for scope '%s' discovered.", broker_info->service_instance_name, broker_info->scope);

  RCClientScope* scope = (RCClientScope*)context;
  if (scope)
  {
    if (RC_CLIENT_LOCK(scope->client))
    {
      if (scope->state == kRCScopeStateMarkedForDestruction)
      {
        RC_CLIENT_UNLOCK(scope->client);
        return;
      }

      RDMNET_ASSERT(handle == scope->monitor_handle);

      if (!scope->broker_found)
      {
        if (copy_broker_listen_addrs(scope, broker_info))
        {
          scope->broker_found = true;
          rdmnet_safe_strncpy(scope->broker_name, broker_info->service_instance_name,
                              E133_SERVICE_NAME_STRING_PADDED_LENGTH);
          scope->current_listen_addr = 0;
          scope->port = broker_info->port;

          attempt_connection_on_listen_addrs(scope);
        }
      }
      RC_CLIENT_UNLOCK(scope->client);
    }
  }
}

void monitorcb_broker_updated(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  RDMNET_LOG_DEBUG("Broker discovery info updated for broker '%s' on scope '%s'.", broker_info->service_instance_name,
                   broker_info->scope);

  RCClientScope* scope = (RCClientScope*)context;
  if (scope)
  {
    if (RC_CLIENT_LOCK(scope->client))
    {
      if (scope->state == kRCScopeStateMarkedForDestruction)
      {
        RC_CLIENT_UNLOCK(scope->client);
        return;
      }

      RDMNET_ASSERT(handle == scope->monitor_handle);

      if (scope->broker_found)
      {
        if (copy_broker_listen_addrs(scope, broker_info))
        {
          rdmnet_safe_strncpy(scope->broker_name, broker_info->service_instance_name,
                              E133_SERVICE_NAME_STRING_PADDED_LENGTH);
          scope->current_listen_addr = 0;
          scope->port = broker_info->port;
        }
      }
      RC_CLIENT_UNLOCK(scope->client);
    }
  }
}

void monitorcb_broker_lost(rdmnet_scope_monitor_t handle,
                           const char*            scope_str,
                           const char*            service_name,
                           void*                  context)
{
  ETCPAL_UNUSED_ARG(handle);
  RCClientScope* scope = (RCClientScope*)context;
  if (scope)
  {
    if (RC_CLIENT_LOCK(scope->client))
    {
      if (scope->state == kRCScopeStateMarkedForDestruction)
      {
        RC_CLIENT_UNLOCK(scope->client);
        return;
      }

      RDMNET_ASSERT(handle == scope->monitor_handle);

      clear_discovered_broker_info(scope);

      RC_CLIENT_UNLOCK(scope->client);
    }
  }
  RDMNET_LOG_INFO("Broker '%s' no longer discovered on scope '%s'", service_name, scope_str);
}

/******************************************************************************
 * Callback functions from the connection interface
 *****************************************************************************/

void conncb_connected(RCConnection* conn, const RCConnectedInfo* connected_info)
{
  RCClientScope* scope = GET_CLIENT_SCOPE_FROM_CONN(conn);
  RCClient*      client = scope->client;

  RDMNET_ASSERT(client->type == kClientProtocolRPT || client->type == kClientProtocolEPT);

  if (RC_CLIENT_LOCK(client))
  {
    scope->state = kRCScopeStateConnected;
    scope->current_broker_addr = connected_info->connected_addr;

    if (client->type == kClientProtocolRPT)
    {
      if (!RDMNET_UID_IS_STATIC(&RC_RPT_CLIENT_DATA(client)->uid))
        scope->uid = connected_info->client_uid;
    }

    RC_CLIENT_UNLOCK(client);
  }

  RdmnetClientConnectedInfo cli_conn_info;
  cli_conn_info.broker_addr = connected_info->connected_addr;
  cli_conn_info.broker_cid = connected_info->broker_cid;
  cli_conn_info.broker_name = scope->broker_name;
  cli_conn_info.broker_uid = connected_info->broker_uid;
  client->callbacks.connected(client, scope->handle, &cli_conn_info);
}

void conncb_connect_failed(RCConnection* conn, const RCConnectFailedInfo* failed_info)
{
  RCClientScope* scope = GET_CLIENT_SCOPE_FROM_CONN(conn);
  RCClient*      client = scope->client;

  RDMNET_ASSERT(client->type == kClientProtocolRPT || client->type == kClientProtocolEPT);

  RdmnetClientConnectFailedInfo cli_conn_failed_info;
  cli_conn_failed_info.event = failed_info->event;
  cli_conn_failed_info.socket_err = failed_info->socket_err;
  cli_conn_failed_info.rdmnet_reason = failed_info->rdmnet_reason;
  cli_conn_failed_info.will_retry =
      connect_failed_will_retry(cli_conn_failed_info.event, cli_conn_failed_info.rdmnet_reason);

  if (RC_CLIENT_LOCK(client))
  {
    if (cli_conn_failed_info.will_retry)
    {
      if (scope->monitor_handle != RDMNET_SCOPE_MONITOR_INVALID)
      {
        if (scope->broker_found)
        {
          // Attempt to connect on the next listen address.
          if (++scope->current_listen_addr == scope->num_broker_listen_addrs)
            scope->current_listen_addr = 0;
          attempt_connection_on_listen_addrs(scope);
        }
      }
      else
      {
        if (kEtcPalErrOk != start_connection_for_scope(scope, &scope->static_broker_addr, false))
        {
          // Some fatal error while attempting to connect to the statically-configured address.
          cli_conn_failed_info.will_retry = false;
        }
      }
    }

    if (!cli_conn_failed_info.will_retry)
    {
      if (scope->monitor_handle != RDMNET_SCOPE_MONITOR_INVALID)
      {
        rdmnet_disc_stop_monitoring(scope->monitor_handle);
        scope->monitor_handle = RDMNET_SCOPE_MONITOR_INVALID;
      }
      clear_discovered_broker_info(scope);
      scope->state = kRCScopeStateInactive;
    }
    RC_CLIENT_UNLOCK(client);
  }

  client->callbacks.connect_failed(client, scope->handle, &cli_conn_failed_info);
}

void conncb_disconnected(RCConnection* conn, const RCDisconnectedInfo* disconn_info)
{
  RCClientScope* scope = GET_CLIENT_SCOPE_FROM_CONN(conn);
  RCClient*      client = scope->client;

  RDMNET_ASSERT(client->type == kClientProtocolRPT || client->type == kClientProtocolEPT);

  RdmnetClientDisconnectedInfo cli_disconn_info;
  cli_disconn_info.event = disconn_info->event;
  cli_disconn_info.socket_err = disconn_info->socket_err;
  cli_disconn_info.rdmnet_reason = disconn_info->rdmnet_reason;
  cli_disconn_info.will_retry = disconnected_will_retry(cli_disconn_info.event, cli_disconn_info.rdmnet_reason);

  if (RC_CLIENT_LOCK(client))
  {
    ETCPAL_IP_SET_INVALID(&scope->current_broker_addr.ip);
    scope->current_broker_addr.port = 0;
    if (disconn_info->event == kRdmnetDisconnectNoHeartbeat && scope->unhealthy_counter < UINT16_MAX)
      ++scope->unhealthy_counter;

    if (cli_disconn_info.will_retry)
    {
      // Retry connection on the scope.
      if (scope->monitor_handle != RDMNET_SCOPE_MONITOR_INVALID)
      {
        if (scope->broker_found)
        {
          // Attempt to connect to the Broker on its reported listen addresses.
          attempt_connection_on_listen_addrs(scope);
        }
      }
      else
      {
        if (kEtcPalErrOk != start_connection_for_scope(scope, &scope->static_broker_addr, false))
        {
          // Some fatal error while attempting to connect to the statically-configured address.
          cli_disconn_info.will_retry = false;
        }
      }
    }

    if (!cli_disconn_info.will_retry)
    {
      if (scope->monitor_handle != RDMNET_SCOPE_MONITOR_INVALID)
      {
        rdmnet_disc_stop_monitoring(scope->monitor_handle);
        scope->monitor_handle = RDMNET_SCOPE_MONITOR_INVALID;
      }
      clear_discovered_broker_info(scope);
      scope->state = kRCScopeStateInactive;
    }
    RC_CLIENT_UNLOCK(client);
  }

  client->callbacks.disconnected(client, scope->handle, &cli_disconn_info);
}

rc_message_action_t conncb_msg_received(RCConnection* conn, const RdmnetMessage* message)
{
  rc_message_action_t action = kRCMessageActionProcessNext;

  RCClientScope* scope = GET_CLIENT_SCOPE_FROM_CONN(conn);
  RCClient*      client = scope->client;

  RDMNET_ASSERT(client->type == kClientProtocolRPT || client->type == kClientProtocolEPT);

  bool should_return = true;
  if (RC_CLIENT_LOCK(client))
  {
    should_return = (scope->state != kRCScopeStateConnected);
    RC_CLIENT_UNLOCK(client);
  }
  if (should_return)
    return kRCMessageActionProcessNext;

  switch (message->vector)
  {
    case ACN_VECTOR_ROOT_BROKER:
      client->callbacks.broker_msg_received(client, scope->handle, RDMNET_GET_BROKER_MSG(message));
      break;
    case ACN_VECTOR_ROOT_RPT:
      if (client->type == kClientProtocolRPT)
      {
        RptClientMessage client_msg;
        if (parse_rpt_message(scope, RDMNET_GET_RPT_MSG(message), &client_msg))
        {
          RdmnetSyncRdmResponse resp = RDMNET_SYNC_RDM_RESPONSE_INIT;
          bool                  use_internal_buf_for_response = false;

          if (handle_rdm_command_internally(client, scope, &client_msg, &resp))
          {
            use_internal_buf_for_response = true;
          }
          else
          {
            RC_RPT_CLIENT_DATA(client)->callbacks.rpt_msg_received(client, scope->handle, &client_msg, &resp,
                                                                   &use_internal_buf_for_response);

            if (resp.response_action == kRdmnetRdmResponseActionRetryLater)
              action = kRCMessageActionRetryLater;
          }
          send_rdm_response_if_requested(client, scope, &client_msg, &resp, use_internal_buf_for_response);
          free_rpt_client_message(&client_msg);
        }
      }
      else if (RDMNET_CAN_LOG(ETCPAL_LOG_WARNING))
      {
        char cid_str[ETCPAL_UUID_STRING_BYTES];
        etcpal_uuid_to_string(&client->cid, cid_str);
        RDMNET_LOG_WARNING("Incorrectly got RPT message for non-RPT client %s on scope %d", cid_str, scope->handle);
      }
      break;
    case ACN_VECTOR_ROOT_EPT:
      // TODO, for now fall through
    default:
      // RDMNET_LOG_WARNING("Got message with unhandled vector type %" PRIu32 " on scope %d", message->vector,
      // handle);
      break;
  }

  return action;
}

void conncb_destroyed(RCConnection* conn)
{
  RCClientScope* scope = GET_CLIENT_SCOPE_FROM_CONN(conn);
  RCClient*      client = scope->client;

  bool send_destroyed_cb = false;

  if (RC_CLIENT_LOCK(client))
  {
    scope->handle = RDMNET_CLIENT_SCOPE_INVALID;
    scope->state = kRCScopeStateInactive;
    if (client->marked_for_destruction)
      send_destroyed_cb = client_fully_destroyed(client);
    RC_CLIENT_UNLOCK(client);
  }

  if (send_destroyed_cb && client->callbacks.destroyed)
    client->callbacks.destroyed(client);
}

bool parse_rpt_message(const RCClientScope* scope, const RptMessage* rmsg, RptClientMessage* msg_out)
{
  switch (rmsg->vector)
  {
    case VECTOR_RPT_REQUEST:
      return parse_rpt_request(rmsg, msg_out);
    case VECTOR_RPT_NOTIFICATION:
      return parse_rpt_notification(scope, rmsg, msg_out);
    case VECTOR_RPT_STATUS:
      return parse_rpt_status(rmsg, msg_out);
    default:
      return false;
  }
}

bool parse_rpt_request(const RptMessage* rmsg, RptClientMessage* msg_out)
{
  RdmnetRdmCommand*    cmd = RDMNET_GET_RDM_COMMAND(msg_out);
  const RptRdmBufList* list = RPT_GET_RDM_BUF_LIST(rmsg);

  if (list->num_rdm_buffers == 1)  // Only one RDM command allowed in an RPT request
  {
    etcpal_error_t unpack_res = rdm_unpack_command(list->rdm_buffers, &cmd->rdm_header, &cmd->data, &cmd->data_len);
    if (unpack_res == kEtcPalErrOk)
    {
      msg_out->type = kRptClientMsgRdmCmd;
      cmd->rdmnet_source_uid = rmsg->header.source_uid;
      cmd->dest_endpoint = rmsg->header.dest_endpoint_id;
      cmd->seq_num = rmsg->header.seqnum;
      return true;
    }
  }
  return false;
}

bool parse_rpt_notification(const RCClientScope* scope, const RptMessage* rmsg, RptClientMessage* msg_out)
{
  RdmnetRdmResponse* resp = RDMNET_GET_RDM_RESPONSE(msg_out);

  // Do some initialization
  msg_out->type = kRptClientMsgRdmResp;
  memset(&resp->original_cmd_header, 0, sizeof(RdmCommandHeader));
  resp->original_cmd_data = NULL;
  resp->original_cmd_data_len = 0;
  resp->rdm_data_len = 0;
  resp->more_coming = RPT_GET_RDM_BUF_LIST(rmsg)->more_coming;

  const RptRdmBufList* list = RPT_GET_RDM_BUF_LIST(rmsg);
  uint8_t*             resp_data_buf = NULL;
  if (!get_rdm_response_data_buf(list, &resp_data_buf))
    return false;

  // Initialize some values
  resp->rdm_data = resp_data_buf;

  bool good_parse = true;
  bool first_msg = true;
  for (size_t i = 0; i < list->num_rdm_buffers; ++i)
  {
    good_parse = unpack_notification_rdm_buffer(&list->rdm_buffers[i], resp, resp_data_buf, &first_msg);
    if (!good_parse)
      break;
  }

  if (good_parse)
  {
    // Fill in the rest of the info
    resp->rdmnet_source_uid = rmsg->header.source_uid;
    resp->source_endpoint = rmsg->header.source_endpoint_id;
    resp->seq_num = rmsg->header.seqnum;
    if (RDM_UID_EQUAL(&scope->uid, &rmsg->header.dest_uid))
      resp->is_response_to_me = true;
    else
      resp->is_response_to_me = false;
    return true;
  }
  else
  {
    // Clean up
    free_rpt_client_message(msg_out);
    return false;
  }
}

bool parse_rpt_status(const RptMessage* rmsg, RptClientMessage* msg_out)
{
  RdmnetRptStatus*    status_out = RDMNET_GET_RPT_STATUS(msg_out);
  const RptStatusMsg* status = RPT_GET_STATUS_MSG(rmsg);

  // This one is quick and simple with no failure condition
  msg_out->type = kRptClientMsgStatus;
  status_out->source_uid = rmsg->header.source_uid;
  status_out->source_endpoint = rmsg->header.source_endpoint_id;
  status_out->seq_num = rmsg->header.seqnum;
  status_out->status_code = status->status_code;
  status_out->status_string = status->status_string;
  return true;
}

bool unpack_notification_rdm_buffer(const RdmBuffer*   buffer,
                                    RdmnetRdmResponse* resp,
                                    uint8_t*           resp_data_buf,
                                    bool*              is_first_resp)
{
  if (rdm_validate_msg(buffer))
  {
    if (*is_first_resp)
    {
      if (RDM_CC_IS_NON_DISC_COMMAND(buffer->data[RDM_OFFSET_COMMAND_CLASS]))
      {
        // The command is included.
        etcpal_error_t unpack_res = rdm_unpack_command(buffer, &resp->original_cmd_header, &resp->original_cmd_data,
                                                       &resp->original_cmd_data_len);
        return (unpack_res == kEtcPalErrOk);
      }
      else if (RDM_CC_IS_NON_DISC_RESPONSE(buffer->data[RDM_OFFSET_COMMAND_CLASS]))
      {
        const uint8_t* this_data;
        uint8_t        this_data_len;
        etcpal_error_t unpack_res = rdm_unpack_response(buffer, &resp->rdm_header, &this_data, &this_data_len);
        if (unpack_res == kEtcPalErrOk)
        {
          if (this_data && this_data_len)
          {
            memcpy(&resp_data_buf[resp->rdm_data_len], this_data, this_data_len);
            resp->rdm_data_len += this_data_len;
          }
          *is_first_resp = false;

          if (resp->rdm_header.resp_type == kRdmResponseTypeAckOverflow)
            resp->rdm_header.resp_type = kRdmResponseTypeAck;

          return true;
        }
      }
    }
    else if (RDM_CC_IS_NON_DISC_RESPONSE(buffer->data[RDM_OFFSET_COMMAND_CLASS]))
    {
      RdmResponseHeader new_header;
      const uint8_t*    this_data;
      uint8_t           this_data_len;
      etcpal_error_t    unpack_res = rdm_unpack_response(buffer, &new_header, &this_data, &this_data_len);
      if (unpack_res == kEtcPalErrOk)
      {
        if (!RDM_UID_EQUAL(&new_header.source_uid, &resp->rdm_header.source_uid) ||
            !RDM_UID_EQUAL(&new_header.dest_uid, &resp->rdm_header.dest_uid) ||
            new_header.subdevice != resp->rdm_header.subdevice ||
            new_header.command_class != resp->rdm_header.command_class ||
            new_header.param_id != resp->rdm_header.param_id)
        {
          return false;
        }
        if (this_data && this_data_len)
        {
          memcpy(&resp_data_buf[resp->rdm_data_len], this_data, this_data_len);
          resp->rdm_data_len += this_data_len;
        }
        return true;
      }
    }
  }
  return false;
}

void send_rdm_response_if_requested(RCClient*               client,
                                    RCClientScope*          scope,
                                    const RptClientMessage* msg,
                                    RdmnetSyncRdmResponse*  resp,
                                    bool                    use_internal_buf)
{
  if (RC_CLIENT_LOCK(client))
  {
    if (scope->state != kRCScopeStateConnected)
    {
      RC_CLIENT_UNLOCK(client);
      return;
    }

    const RdmnetRdmCommand* received_cmd = RDMNET_GET_RDM_COMMAND(msg);
    etcpal_error_t          res = kEtcPalErrOk;

    if (resp->response_action == kRdmnetRdmResponseActionSendAck)
    {
      RptHeader header;
      header.source_uid = scope->uid;
      header.source_endpoint_id = received_cmd->dest_endpoint;
      if (received_cmd->rdm_header.command_class == kRdmCCSetCommand)
        header.dest_uid = kRdmnetControllerBroadcastUid;
      else
        header.dest_uid = received_cmd->rdmnet_source_uid;
      header.dest_endpoint_id = E133_NULL_ENDPOINT;
      header.seqnum = received_cmd->seq_num;

      res = send_rdm_ack_internal(client, scope, &header, &received_cmd->rdm_header, received_cmd->data,
                                  received_cmd->data_len, use_internal_buf ? internal_pd_buf : client->sync_resp_buf,
                                  resp->response_data.response_data_len);
    }
    else if (resp->response_action == kRdmnetRdmResponseActionSendNack)
    {
      RptHeader header;
      header.source_uid = scope->uid;
      header.source_endpoint_id = received_cmd->dest_endpoint;
      header.dest_uid = received_cmd->rdmnet_source_uid;
      header.dest_endpoint_id = E133_NULL_ENDPOINT;
      header.seqnum = received_cmd->seq_num;

      res = send_rdm_nack_internal(client, scope, &header, &received_cmd->rdm_header, received_cmd->data,
                                   received_cmd->data_len, resp->response_data.nack_reason);
    }

    if (res != kEtcPalErrOk && RDMNET_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&client->cid, cid_str);
      RDMNET_LOG_WARNING("Error sending RDM response from client %s: '%s'", cid_str, etcpal_strerror(res));
    }

    RC_CLIENT_UNLOCK(client);
  }
}

bool handle_rdm_command_internally(RCClient*               client,
                                   RCClientScope*          scope,
                                   const RptClientMessage* msg,
                                   RdmnetSyncRdmResponse*  resp)
{
  bool res = false;
  if (RC_CLIENT_LOCK(client))
  {
    const RdmnetRdmCommand* cmd = RDMNET_GET_RDM_COMMAND(msg);
    if (scope->state == kRCScopeStateConnected && msg->type == kRptClientMsgRdmCmd &&
        cmd->dest_endpoint == E133_NULL_ENDPOINT)
    {
      res = true;
      switch (cmd->rdm_header.param_id)
      {
        case E133_TCP_COMMS_STATUS:
          handle_tcp_comms_status(client, cmd, resp);
          break;
        default:
          res = false;
          break;
      }
    }
    RC_CLIENT_UNLOCK(client);
  }
  return res;
}

void handle_tcp_comms_status(RCClient* client, const RdmnetRdmCommand* cmd, RdmnetSyncRdmResponse* resp)
{
  const RdmCommandHeader* cmd_header = &cmd->rdm_header;
  if (cmd_header->command_class == kRdmCCGetCommand)
  {
#if RDMNET_DYNAMIC_MEM
    size_t pd_len = TCP_COMMS_STATUS_PD_SIZE * client->num_scopes;
#else
    size_t pd_len = TCP_COMMS_STATUS_PD_SIZE * RDMNET_MAX_SCOPES_PER_CLIENT;
#endif
    uint8_t* buf = rc_client_get_internal_response_buf(pd_len);
    if (!buf)
    {
      RDMNET_SYNC_SEND_RDM_ACK(resp, kRdmNRHardwareFault);
      return;
    }

    uint8_t* cur_ptr = buf;
    BEGIN_FOR_EACH_CLIENT_SCOPE(client)
    {
      if (scope->handle == RDMNET_CLIENT_SCOPE_INVALID || scope->state == kRCScopeStateMarkedForDestruction)
      {
        pd_len -= TCP_COMMS_STATUS_PD_SIZE;
        continue;
      }

      rdmnet_safe_strncpy((char*)cur_ptr, scope->id, E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
      if (ETCPAL_IP_IS_V4(&scope->current_broker_addr.ip))
        etcpal_pack_u32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&scope->current_broker_addr.ip));
      else
        etcpal_pack_u32b(cur_ptr, 0);
      cur_ptr += 4;
      if (ETCPAL_IP_IS_V6(&scope->current_broker_addr.ip))
        memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&scope->current_broker_addr.ip), 16);
      else
        memset(cur_ptr, 0, 16);
      cur_ptr += 16;
      etcpal_pack_u16b(cur_ptr, scope->current_broker_addr.port);
      cur_ptr += 2;
      etcpal_pack_u16b(cur_ptr, scope->unhealthy_counter);
      cur_ptr += 2;
    }
    END_FOR_EACH_CLIENT_SCOPE(client)

    RDMNET_SYNC_SEND_RDM_ACK(resp, pd_len);
  }
  else if (cmd_header->command_class == kRdmCCSetCommand)
  {
    if (cmd->data_len != E133_SCOPE_STRING_PADDED_LENGTH || cmd->data[E133_SCOPE_STRING_PADDED_LENGTH - 1] != 0)
    {
      RDMNET_SYNC_SEND_RDM_NACK(resp, kRdmNRFormatError);
      return;
    }
    RCClientScope* scope = get_scope_by_id(client, (char*)cmd->data);
    if (!scope)
    {
      RDMNET_SYNC_SEND_RDM_NACK(resp, kRdmNRUnknownScope);
      return;
    }

    scope->unhealthy_counter = 0;
    RDMNET_SYNC_SEND_RDM_ACK(resp, 0);
  }
  else
  {
    RDMNET_SYNC_SEND_RDM_NACK(resp, kRdmNRUnsupportedCommandClass);
  }
}

void free_rpt_client_message(RptClientMessage* msg)
{
  if (msg->type == kRptClientMsgRdmResp)
    free_rdm_response_data_buf((uint8_t*)RDMNET_GET_RDM_RESPONSE(msg)->rdm_data);
}

#if 0
void free_ept_client_message(EptClientMessage* msg)
{
  ETCPAL_UNUSED_ARG(msg);
  // TODO
}
#endif

void llrpcb_rdm_cmd_received(RCLlrpTarget* target, const LlrpRdmCommand* cmd, RCLlrpTargetSyncRdmResponse* response)
{
  RDMNET_ASSERT(target);
  RCClient* client = GET_CLIENT_FROM_LLRP_TARGET(target);

  bool use_internal_buf_for_response = false;
  RC_RPT_CLIENT_DATA(client)->callbacks.llrp_msg_received(client, cmd, &response->resp, &use_internal_buf_for_response);

  if (use_internal_buf_for_response)
    response->response_buf = internal_pd_buf;
  else
    response->response_buf = client->sync_resp_buf;
}

void llrpcb_target_destroyed(RCLlrpTarget* target)
{
  RDMNET_ASSERT(target);
  RCClient* client = GET_CLIENT_FROM_LLRP_TARGET(target);

  bool send_destroy_cb = false;

  if (RC_CLIENT_LOCK(client))
  {
    client->target_valid = false;
    if (client->marked_for_destruction)
      send_destroy_cb = client_fully_destroyed(client);
    RC_CLIENT_UNLOCK(client);
  }

  if (send_destroy_cb && client->callbacks.destroyed)
    client->callbacks.destroyed(client);
}

bool connect_failed_will_retry(rdmnet_connect_fail_event_t event, rdmnet_connect_status_t status)
{
  switch (event)
  {
    case kRdmnetConnectFailSocketFailure:
      return false;
    case kRdmnetConnectFailRejected:
      return ((status == kRdmnetConnectCapacityExceeded) || (status == kRdmnetConnectDuplicateUid));
    case kRdmnetConnectFailTcpLevel:
    case kRdmnetConnectFailNoReply:
    default:
      return true;
  }
}

bool disconnected_will_retry(rdmnet_disconnect_event_t event, rdmnet_disconnect_reason_t reason)
{
  ETCPAL_UNUSED_ARG(event);
  ETCPAL_UNUSED_ARG(reason);
  // Currently all disconnects are retried.
  return true;
}

/*
 * Allocate a new scope list entry and append it to a client's scope list. If a scope string is
 * already in the list, fails with kEtcPalErrExists. Attempts to create a new connection handle to
 * accompany the scope. Returns kEtcPalErrOk on success, other error code otherwise. Fills in
 * new_entry with the newly-created entry on success.
 */
etcpal_error_t create_and_add_scope_entry(RCClient* client, const RdmnetScopeConfig* config, RCClientScope** new_entry)
{
  rdmnet_client_scope_t new_handle = get_next_int_handle(&client->scope_handle_manager);
  if (new_handle == RDMNET_CLIENT_SCOPE_INVALID)
    return kEtcPalErrNoMem;

  RCClientScope* new_scope = get_unused_scope_entry(client);
  if (!new_scope)
    return kEtcPalErrNoMem;

  new_scope->conn.local_cid = client->cid;
  new_scope->conn.lock = client->lock;
  new_scope->conn.callbacks = kConnCallbacks;
  etcpal_error_t res = rc_conn_register(&new_scope->conn);
  if (res != kEtcPalErrOk)
    return res;

  // Do the rest of the initialization
  new_scope->handle = new_handle;
  rdmnet_safe_strncpy(new_scope->id, config->scope, E133_SCOPE_STRING_PADDED_LENGTH);
  new_scope->static_broker_addr = config->static_broker_addr;
  if (!ETCPAL_IP_IS_INVALID(&new_scope->static_broker_addr.ip))
    new_scope->state = kRCScopeStateConnecting;
  else
    new_scope->state = kRCScopeStateDiscovery;
  new_scope->uid = RC_RPT_CLIENT_DATA(client)->uid;
  // uid init is done at connection time
  new_scope->send_seq_num = 1;
  new_scope->monitor_handle = RDMNET_SCOPE_MONITOR_INVALID;
  new_scope->broker_found = false;
#if RDMNET_DYNAMIC_MEM
  new_scope->broker_listen_addrs = NULL;
#endif
  new_scope->num_broker_listen_addrs = 0;
  new_scope->current_listen_addr = 0;
  new_scope->port = 0;
  ETCPAL_IP_SET_INVALID(&new_scope->current_broker_addr.ip);
  new_scope->current_broker_addr.port = 0;
  new_scope->unhealthy_counter = 0;
  new_scope->client = client;

  *new_entry = new_scope;
  return kEtcPalErrOk;
}

RCClientScope* get_scope(RCClient* client, rdmnet_client_scope_t scope_handle)
{
  BEGIN_FOR_EACH_CLIENT_SCOPE(client)
  {
    if (scope->handle == scope_handle)
    {
      // Found
      return scope;
    }
  }
  END_FOR_EACH_CLIENT_SCOPE(client)
  return NULL;
}

RCClientScope* get_scope_by_id(RCClient* client, const char* scope_str)
{
  BEGIN_FOR_EACH_CLIENT_SCOPE(client)
  {
    if ((scope->handle != RDMNET_CLIENT_SCOPE_INVALID && scope->state != kRCScopeStateMarkedForDestruction) &&
        (strcmp(scope->id, scope_str) == 0))
    {
      // Found
      return scope;
    }
  }
  END_FOR_EACH_CLIENT_SCOPE(client)
  return NULL;
}

RCClientScope* get_unused_scope_entry(RCClient* client)
{
#if RDMNET_DYNAMIC_MEM
  for (RCClientScope** scope_ptr = client->scopes; scope_ptr < client->scopes + client->num_scopes; ++scope_ptr)
  {
    if ((*scope_ptr)->handle == RDMNET_CLIENT_SCOPE_INVALID)
      return *scope_ptr;
  }

  RCClientScope* new_scope = (RCClientScope*)malloc(sizeof(RCClientScope));
  if (new_scope)
  {
    RCClientScope** new_scope_buf =
        (RCClientScope**)realloc(client->scopes, (client->num_scopes + 1) * sizeof(RCClientScope*));
    if (new_scope_buf)
    {
      client->scopes = new_scope_buf;
      client->scopes[client->num_scopes] = new_scope;
      new_scope->handle = RDMNET_CLIENT_SCOPE_INVALID;
      ++client->num_scopes;
      return new_scope;
    }
    else
    {
      free(new_scope);
      return NULL;
    }
  }
  else
  {
    return NULL;
  }
#else
  for (RCClientScope* scope = client->scopes; scope < client->scopes + RDMNET_MAX_SCOPES_PER_CLIENT; ++scope)
  {
    if (scope->handle == RDMNET_CLIENT_SCOPE_INVALID)
      return scope;
  }
  return NULL;
#endif
}

bool scope_handle_in_use(int handle_val, void* context)
{
  RCClient* client = (RCClient*)context;
  BEGIN_FOR_EACH_CLIENT_SCOPE(client)
  {
    if (scope->handle == handle_val)
      return true;
  }
  END_FOR_EACH_CLIENT_SCOPE(client)
  return false;
}

void mark_scope_for_destruction(RCClientScope* scope, const rdmnet_disconnect_reason_t* disconnect_reason)
{
  if (scope->monitor_handle != RDMNET_SCOPE_MONITOR_INVALID)
  {
    rdmnet_disc_stop_monitoring(scope->monitor_handle);
    scope->monitor_handle = RDMNET_SCOPE_MONITOR_INVALID;
  }
  clear_discovered_broker_info(scope);
  rc_conn_unregister(&scope->conn, disconnect_reason);
  scope->state = kRCScopeStateMarkedForDestruction;
}

bool client_fully_destroyed(RCClient* client)
{
  if (client->target_valid)
    return false;

  bool fully_destroyed = true;
  BEGIN_FOR_EACH_CLIENT_SCOPE(client)
  {
    if (scope->handle != RDMNET_CLIENT_SCOPE_INVALID)
    {
      fully_destroyed = false;
      break;
    }
  }
  END_FOR_EACH_CLIENT_SCOPE(client)

#if RDMNET_DYNAMIC_MEM
  if (fully_destroyed)
  {
    if (client->scopes)
    {
      for (RCClientScope** scope_ptr = client->scopes; scope_ptr < client->scopes + client->num_scopes; ++scope_ptr)
      {
        if (*scope_ptr)
          free(*scope_ptr);
      }
      free(client->scopes);
    }
    client->num_scopes = 0;
  }
#endif
  return fully_destroyed;
}

etcpal_error_t start_scope_discovery(RCClientScope* scope, const char* search_domain)
{
  RdmnetScopeMonitorConfig config;

  config.scope = scope->id;
  config.domain = search_domain;
  config.callbacks = disc_callbacks;
  config.callbacks.context = scope;

  int            platform_error;
  etcpal_error_t res = rdmnet_disc_start_monitoring(&config, &scope->monitor_handle, &platform_error);
  if (res == kEtcPalErrOk)
  {
    scope->state = kRCScopeStateDiscovery;
  }
  else
  {
    RDMNET_LOG_WARNING("Starting discovery failed on scope '%s' with error '%s' (platform-specific error code %d)",
                       scope->id, etcpal_strerror(res), platform_error);
  }
  return res;
}

void attempt_connection_on_listen_addrs(RCClientScope* scope)
{
  size_t listen_addr_index = scope->current_listen_addr;

  while (true)
  {
    char addr_str[ETCPAL_IP_STRING_BYTES] = {'\0'};

    if (RDMNET_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      etcpal_ip_to_string(&scope->broker_listen_addrs[listen_addr_index], addr_str);
    }

    RDMNET_LOG_DEBUG("Attempting broker connection on scope '%s' at address %s:%d...", scope->id, addr_str,
                     scope->port);

    EtcPalSockAddr connect_addr;
    connect_addr.ip = scope->broker_listen_addrs[listen_addr_index];
    connect_addr.port = scope->port;

    etcpal_error_t connect_res = start_connection_for_scope(scope, &connect_addr, false);
    if (connect_res == kEtcPalErrOk)
    {
      scope->current_listen_addr = listen_addr_index;
      break;
    }
    else
    {
      if (++listen_addr_index == scope->num_broker_listen_addrs)
        listen_addr_index = 0;
      if (listen_addr_index == scope->current_listen_addr)
      {
        // We've looped through all the addresses. This broker is no longer valid.
        clear_discovered_broker_info(scope);
      }

      RDMNET_LOG_WARNING("Connection to broker for scope '%s' at address %s:%d failed with error: '%s'. %s", scope->id,
                         addr_str, connect_addr.port, etcpal_strerror(connect_res),
                         scope->broker_found ? "Trying next address..." : "All addresses exhausted. Giving up.");

      if (!scope->broker_found)
      {
        scope->state = kRCScopeStateDiscovery;
        break;
      }
    }
  }
}

etcpal_error_t start_connection_for_scope(RCClientScope*              scope,
                                          const EtcPalSockAddr*       broker_addr,
                                          rdmnet_disconnect_reason_t* disconnect_reason)
{
  BrokerClientConnectMsg connect_msg;
  RCClient*              client = scope->client;

  if (client->type == kClientProtocolRPT)
  {
    RCRptClientData* rpt_data = RC_RPT_CLIENT_DATA(client);
    RdmUid           my_uid;
    if (RDMNET_UID_IS_STATIC(&rpt_data->uid))
    {
      my_uid = rpt_data->uid;
    }
    else
    {
      RDMNET_INIT_DYNAMIC_UID_REQUEST(&my_uid, rpt_data->uid.manu);
    }

    rdmnet_safe_strncpy(connect_msg.scope, scope->id, E133_SCOPE_STRING_PADDED_LENGTH);
    connect_msg.e133_version = E133_VERSION;
    rdmnet_safe_strncpy(connect_msg.search_domain, client->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
    if (rpt_data->type == kRPTClientTypeController)
      connect_msg.connect_flags = BROKER_CONNECT_FLAG_INCREMENTAL_UPDATES;
    else
      connect_msg.connect_flags = 0;
    connect_msg.client_entry.client_protocol = kClientProtocolRPT;
    rc_create_rpt_client_entry(&client->cid, &my_uid, rpt_data->type, NULL,
                               GET_RPT_CLIENT_ENTRY(&connect_msg.client_entry));
  }
  else
  {
    // TODO EPT
    return kEtcPalErrNotImpl;
  }

  etcpal_error_t res = kEtcPalErrOk;
  if (disconnect_reason)
  {
    // Scope was previously dynamic or inactive
    if (scope->state == kRCScopeStateDiscovery || scope->state == kRCScopeStateInactive)
      res = rc_conn_connect(&scope->conn, broker_addr, &connect_msg);
    else
      res = rc_conn_reconnect(&scope->conn, broker_addr, &connect_msg, *disconnect_reason);
  }
  else
  {
    res = rc_conn_connect(&scope->conn, broker_addr, &connect_msg);
  }
  if (res == kEtcPalErrOk)
    scope->state = kRCScopeStateConnecting;

  return res;
}

void clear_discovered_broker_info(RCClientScope* scope)
{
  scope->broker_found = false;
#if RDMNET_DYNAMIC_MEM
  if (scope->broker_listen_addrs)
  {
    free(scope->broker_listen_addrs);
    scope->broker_listen_addrs = NULL;
  }
#endif
  scope->num_broker_listen_addrs = 0;
  scope->current_listen_addr = 0;
  scope->port = 0;
}

etcpal_error_t send_rdm_ack_internal(RCClient*               client,
                                     RCClientScope*          scope,
                                     const RptHeader*        rpt_header,
                                     const RdmCommandHeader* received_cmd_header,
                                     const uint8_t*          received_cmd_data,
                                     uint8_t                 received_cmd_data_len,
                                     const uint8_t*          resp_data,
                                     size_t                  resp_data_len)
{
  // resp_size: The number of RDM command PDUs that make up the ACK or ACK_OVERFLOW response.
  // total_resp_size: resp_size + 1 more RDM command PDU for the original command.
  // We allocate total_resp_size + 1, to account for potentially adding more parameter data right
  // before sending.
  size_t     resp_size = rdm_get_num_responses_needed(received_cmd_header->param_id, resp_data_len);
  size_t     total_resp_size = resp_size + 1;
  RdmBuffer* resp_buf = NULL;

#if RDMNET_DYNAMIC_MEM
  resp_buf = (RdmBuffer*)calloc(total_resp_size + 1, sizeof(RdmBuffer));
  if (!resp_buf)
    return kEtcPalErrNoMem;
#else
  if (total_resp_size + 1 <= RC_CLIENT_STATIC_RESP_BUF_LEN)
    resp_buf = client->resp_buf;
  else
    return kEtcPalErrMsgSize;
#endif

  etcpal_error_t res = rdm_pack_command(received_cmd_header, received_cmd_data, received_cmd_data_len, &resp_buf[0]);

  if (res == kEtcPalErrOk)
  {
    res = rdm_pack_full_response(received_cmd_header, resp_data, resp_data_len, &resp_buf[1], resp_size);
  }
  if (res == kEtcPalErrOk)
  {
    if (received_cmd_header->param_id == E120_SUPPORTED_PARAMETERS)
      append_to_supported_parameters(client, received_cmd_header, &resp_buf[1], resp_size, &total_resp_size);
    if (received_cmd_header->command_class == kRdmCCSetCommand)
      change_destination_to_broadcast(resp_buf, total_resp_size);

    res = rc_rpt_send_notification(&scope->conn, &client->cid, rpt_header, resp_buf, total_resp_size);
  }

#if RDMNET_DYNAMIC_MEM
  free(resp_buf);
#endif
  return res;
}

etcpal_error_t send_rdm_nack_internal(RCClient*               client,
                                      RCClientScope*          scope,
                                      const RptHeader*        rpt_header,
                                      const RdmCommandHeader* received_cmd_header,
                                      const uint8_t*          received_cmd_data,
                                      uint8_t                 received_cmd_data_len,
                                      rdm_nack_reason_t       nack_reason)
{
  RdmBuffer* resp_buf = NULL;
#if RDMNET_DYNAMIC_MEM
  resp_buf = (RdmBuffer*)calloc(2, sizeof(RdmBuffer));
  if (!resp_buf)
    return kEtcPalErrNoMem;
#else
  resp_buf = client->resp_buf;
#endif

  etcpal_error_t res = rdm_pack_command(received_cmd_header, received_cmd_data, received_cmd_data_len, &resp_buf[0]);
  if (res == kEtcPalErrOk)
  {
    res = rdm_pack_nack_response(received_cmd_header, 0, nack_reason, &resp_buf[1]);
  }
  if (res == kEtcPalErrOk)
  {
    res = rc_rpt_send_notification(&scope->conn, &client->cid, rpt_header, resp_buf, 2);
  }

#if RDMNET_DYNAMIC_MEM
  free(resp_buf);
#endif
  return res;
}

etcpal_error_t client_send_update_internal(RCClient*               client,
                                           RCClientScope*          scope,
                                           const RdmnetSourceAddr* source_addr,
                                           uint16_t                param_id,
                                           const uint8_t*          data,
                                           size_t                  data_len)
{
  // resp_size: The number of RDM command PDUs that make up the ACK or ACK_OVERFLOW response.
  // We allocate resp_size + 1, to account for potentially adding more parameter data right
  // before sending.
  size_t     resp_size = rdm_get_num_responses_needed(param_id, data_len);
  RdmBuffer* resp_buf = NULL;

#if RDMNET_DYNAMIC_MEM
  resp_buf = (RdmBuffer*)calloc(resp_size + 1, sizeof(RdmBuffer));
  if (!resp_buf)
    return kEtcPalErrNoMem;
#else
  if (resp_size <= RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES)
    resp_buf = client->resp_buf;
  else
    return kEtcPalErrMsgSize;
#endif

  RptHeader header;
  header.source_uid = scope->uid;
  header.source_endpoint_id = source_addr->source_endpoint;
  header.dest_uid = kRdmnetControllerBroadcastUid;
  header.dest_endpoint_id = E133_NULL_ENDPOINT;
  header.seqnum = 0;

  RdmCommandHeader fake_rdm_header;
  fake_rdm_header.source_uid.manu = 0;
  fake_rdm_header.source_uid.id = 0;
  fake_rdm_header.dest_uid = source_addr->rdm_source_uid;
  fake_rdm_header.transaction_num = 0;
  fake_rdm_header.port_id = 1;
  fake_rdm_header.subdevice = source_addr->subdevice;
  fake_rdm_header.command_class = kRdmCCGetCommand;
  fake_rdm_header.param_id = param_id;

  etcpal_error_t res = rdm_pack_full_response(&fake_rdm_header, data, data_len, resp_buf, resp_size);
  if (res == kEtcPalErrOk)
  {
    if (param_id == E120_SUPPORTED_PARAMETERS)
      append_to_supported_parameters(client, &fake_rdm_header, resp_buf, resp_size, &resp_size);

    // Hack - need to add the destination broadcast UID in each response. This is needed here
    // because it's allowed in RDMnet but not RDM, so rdm_pack_full_response() will not work
    // otherwise.
    for (RdmBuffer* resp = resp_buf; resp < resp_buf + resp_size; ++resp)
    {
      memset(&resp->data[RDM_OFFSET_DEST_MANUFACTURER], 0xff, 6);
      uint8_t* checksum_offset = &resp->data[RDM_OFFSET_PARAM_DATA] + resp->data[RDM_OFFSET_PARAM_DATA_LEN];
      etcpal_pack_u16b(checksum_offset, etcpal_unpack_u16b(checksum_offset) + 0x5fa);
    }

    res = rc_rpt_send_notification(&scope->conn, &client->cid, &header, resp_buf, resp_size);
  }

#if RDMNET_DYNAMIC_MEM
  free(resp_buf);
#endif

  return res;
}

static int supported_param_compare(const void* a, const void* b)
{
  const SupportedParameter* param_a = (const SupportedParameter*)a;
  const SupportedParameter* param_b = (const SupportedParameter*)b;
  return (param_a->pid > param_b->pid) - (param_a->pid < param_b->pid);
}

void append_to_supported_parameters(RCClient*               client,
                                    const RdmCommandHeader* received_cmd_header,
                                    RdmBuffer*              param_resp_buf,
                                    size_t                  param_num_buffers,
                                    size_t*                 total_resp_size)
{
  RDMNET_ASSERT(param_resp_buf);
  RDMNET_ASSERT(param_num_buffers >= 1);
  RDMNET_ASSERT(total_resp_size);
  RDMNET_ASSERT(*total_resp_size >= 1);

  // Reset the checklist
  for (SupportedParameter* param = kSupportedParametersChecklist;
       param < kSupportedParametersChecklist + SUPPORTED_PARAMETERS_CHECKLIST_SIZE; ++param)
  {
    param->found = false;
  }

  size_t num_params_found = 0;

  for (RdmBuffer* resp = param_resp_buf; resp < param_resp_buf + param_num_buffers; ++resp)
  {
    uint8_t* pd_ptr = &resp->data[RDM_OFFSET_PARAM_DATA];
    while (pd_ptr < &resp->data[RDM_OFFSET_PARAM_DATA] + resp->data[RDM_OFFSET_PARAM_DATA_LEN])
    {
      uint16_t            pid = etcpal_unpack_u16b(pd_ptr);
      SupportedParameter* param =
          (SupportedParameter*)bsearch(&pid, kSupportedParametersChecklist, SUPPORTED_PARAMETERS_CHECKLIST_SIZE,
                                       sizeof(SupportedParameter), supported_param_compare);
      if (param)
      {
        ++num_params_found;
        param->found = true;
      }
      pd_ptr += 2;
    }
  }

  RdmBuffer* last_resp = &param_resp_buf[param_num_buffers - 1];
  uint8_t    last_resp_remaining_param_len = 230 - last_resp->data[RDM_OFFSET_PARAM_DATA_LEN];

  uint8_t params_to_append_buf[SUPPORTED_PARAMETERS_CHECKLIST_SIZE * 2];
  uint8_t params_to_append_size = 0;

  for (SupportedParameter* current_param = kSupportedParametersChecklist;
       current_param < kSupportedParametersChecklist + SUPPORTED_PARAMETERS_CHECKLIST_SIZE; ++current_param)
  {
    if (!current_param->found && (current_param->client_type == RC_RPT_CLIENT_DATA(client)->type ||
                                  current_param->client_type == kRPTClientTypeUnknown))
    {
      etcpal_pack_u16b(&params_to_append_buf[params_to_append_size], current_param->pid);
      params_to_append_size += 2;
    }
  }

  if (params_to_append_size <= last_resp_remaining_param_len)
  {
    rdm_append_parameter_data(last_resp, params_to_append_buf, params_to_append_size);
  }
  else
  {
    // We must split the additional parameters out into a new response.

    // Append what we can
    rdm_append_parameter_data(last_resp, params_to_append_buf, last_resp_remaining_param_len);
    // Convert the last ACK to an ACK_OVERFLOW
    last_resp->data[RDM_OFFSET_PORTID_RESPTYPE] = E120_RESPONSE_TYPE_ACK_OVERFLOW;
    // Update checksum
    uint8_t* checksum_offset = last_resp->data + RDM_OFFSET_PARAM_DATA + 230;
    etcpal_pack_u16b(checksum_offset, etcpal_unpack_u16b(checksum_offset) + 3);

    // Create a new response
    RdmBuffer* new_resp = &param_resp_buf[param_num_buffers];
    rdm_pack_response(received_cmd_header, 0, &params_to_append_buf[last_resp_remaining_param_len],
                      params_to_append_size - last_resp_remaining_param_len, new_resp);
    (*total_resp_size)++;
  }
}

void change_destination_to_broadcast(RdmBuffer* resp_buf, size_t total_resp_size)
{
  RDMNET_ASSERT(resp_buf);
  RDMNET_ASSERT(total_resp_size > 1);

  for (RdmBuffer* rdm_buf = resp_buf + 1; rdm_buf < resp_buf + total_resp_size; ++rdm_buf)
  {
    uint8_t* checksum_offset = rdm_buf->data + RDM_OFFSET_PARAM_DATA + rdm_buf->data[RDM_OFFSET_PARAM_DATA_LEN];
    uint32_t checksum = (uint32_t)etcpal_unpack_u16b(checksum_offset);
    for (size_t i = 0; i < 6; ++i)
      checksum += (0xff - rdm_buf->data[RDM_OFFSET_DEST_MANUFACTURER + i]);

    etcpal_pack_u16b(&rdm_buf->data[RDM_OFFSET_DEST_MANUFACTURER], kRdmBroadcastUid.manu);
    etcpal_pack_u32b(&rdm_buf->data[RDM_OFFSET_DEST_DEVICE], kRdmBroadcastUid.id);
    etcpal_pack_u16b(checksum_offset, (uint16_t)checksum);
  }
}

bool get_rdm_response_data_buf(const RptRdmBufList* buf_list, uint8_t** buf_ptr)
{
  RDMNET_ASSERT(buf_list);
  RDMNET_ASSERT(buf_ptr);

  size_t size_needed = 0;

  for (const RdmBuffer* buf = buf_list->rdm_buffers; buf < buf_list->rdm_buffers + buf_list->num_rdm_buffers; ++buf)
  {
    if (RDM_CC_IS_NON_DISC_RESPONSE(buf->data[RDM_OFFSET_COMMAND_CLASS]))
      size_needed += buf->data[RDM_OFFSET_PARAM_DATA_LEN];
  }

  if (size_needed == 0)
  {
    *buf_ptr = NULL;
    return true;
  }

#if RDMNET_DYNAMIC_MEM
  *buf_ptr = (uint8_t*)malloc(size_needed);
  return (*buf_ptr != NULL);
#else
  if (size_needed <= RDM_RESP_BUF_STATIC_SIZE)
  {
    *buf_ptr = received_rdm_response_buf;
    return true;
  }
  else
  {
    return false;
  }
#endif
}

void free_rdm_response_data_buf(uint8_t* buf)
{
#if RDMNET_DYNAMIC_MEM
  free(buf);
#else
  ETCPAL_UNUSED_ARG(buf);
#endif
}
