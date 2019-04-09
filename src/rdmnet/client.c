/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "rdmnet/client.h"

#include <string.h>
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "lwpa/mempool.h"
#endif
#include "lwpa/lock.h"
#include "lwpa/rbtree.h"
#include "rdm/controller.h"
#include "rdm/responder.h"
#include "rdmnet/core.h"
#include "rdmnet/core/client_entry.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/discovery.h"
#include "rdmnet/core/util.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/client.h"
#include "rdmnet/private/opts.h"

/*************************** Private constants *******************************/

#define RDMNET_MAX_CLIENT_SCOPES ((RDMNET_MAX_CONTROLLERS * RDMNET_MAX_SCOPES_PER_CONTROLLER) + RDMNET_MAX_DEVICES)
#define MAX_CLIENT_RB_NODES (RDMNET_MAX_CLIENTS + (RDMNET_MAX_CLIENT_SCOPES * 2))

#if RDMNET_POLL_CONNECTIONS_INTERNALLY
#define CB_STORAGE_CLASS static
#else
#define CB_STORAGE_CLASS
#endif

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using lwpa_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_client() malloc(sizeof(RdmnetClient))
#define alloc_client_scope() malloc(sizeof(ClientScopeListEntry))
#define alloc_client_rdm_response() malloc(sizeof(RemoteRdmRespListEntry))
#define free_rdmnet_client(ptr) free(ptr)
#define free_client_scope(ptr) free(ptr)
#define free_client_rdm_response(ptr) free(ptr)
#else
#define alloc_rdmnet_client() lwpa_mempool_alloc(rdmnet_clients)
#define alloc_client_scope() lwpa_mempool_alloc(client_scopes)
#define alloc_client_rdm_response() lwpa_mempool_alloc(client_rdm_responses)
#define free_rdmnet_client(ptr) lwpa_mempool_free(rdmnet_clients, ptr)
#define free_client_scope(ptr) lwpa_mempool_free(client_scopes, ptr)
#define free_client_rdm_response(ptr) lwpa_mempool_free(client_rdm_responses, ptr)
#endif

#define rdmnet_client_lock() lwpa_mutex_take(&client_lock, LWPA_WAIT_FOREVER)
#define rdmnet_client_unlock() lwpa_mutex_give(&client_lock)

#define init_callback_info(cbptr) ((cbptr)->which = kClientCallbackNone)

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(rdmnet_clients, RdmnetClient, RDMNET_MAX_CLIENTS);
LWPA_MEMPOOL_DEFINE(client_scopes, ClientScopeListEntry, RDMNET_MAX_CLIENT_SCOPES);
LWPA_MEMPOOL_DEFINE(client_rb_nodes, LwpaRbNode, CLIENT_MAX_RB_NODES);
LWPA_MEMPOOL_DEFINE(client_rdm_responses, RemoteRdmRespListEntry, RDMNET_MAX_RECEIVED_ACK_OVERFLOW_RESPONSES);
#endif

static bool client_lock_initted = false;
static lwpa_mutex_t client_lock;

static struct RdmnetClientState
{
  LwpaRbTree clients;

  LwpaRbTree scopes_by_handle;
  LwpaRbTree scopes_by_disc_handle;

  rdmnet_client_t next_handle;
  bool handle_has_wrapped_around;
} state;

static void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo *broker_info,
                                   void *context);
static void monitorcb_broker_lost(rdmnet_scope_monitor_t handle, const char *scope, const char *service_name,
                                  void *context);
static void monitorcb_scope_monitor_error(rdmnet_scope_monitor_t handle, const char *scope, int platform_error,
                                          void *context);

// clang-format off
static const RdmnetScopeMonitorCallbacks disc_callbacks =
{
  monitorcb_broker_found,
  monitorcb_broker_lost,
  monitorcb_scope_monitor_error
};
// clang-format on

static void conncb_connected(rdmnet_conn_t handle, const RdmnetConnectedInfo *connect_info, void *context);
static void conncb_connect_failed(rdmnet_conn_t handle, const RdmnetConnectFailedInfo *failed_info, void *context);
static void conncb_disconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo *disconn_info, void *context);
static void conncb_msg_received(rdmnet_conn_t handle, const RdmnetMessage *message, void *context);

// clang-format off
static const RdmnetConnCallbacks conn_callbacks =
{
  conncb_connected,
  conncb_connect_failed,
  conncb_disconnected,
  conncb_msg_received
};
// clang-format on

/*********************** Private function prototypes *************************/

// Create and destroy clients and scopes
static lwpa_error_t validate_rpt_client_config(const RdmnetRptClientConfig *config);
static RdmnetClient *rpt_client_new(const RdmnetRptClientConfig *config);
static rdmnet_client_t get_new_client_handle();
static lwpa_error_t create_and_append_scope_entry(const RdmnetScopeConfig *config, RdmnetClient *client,
                                                  ClientScopeListEntry **new_entry);
static ClientScopeListEntry *find_scope_in_list(ClientScopeListEntry *list, const char *scope);
static void remove_scope_from_list(ClientScopeListEntry **list, ClientScopeListEntry *entry);

static void start_scope_discovery(ClientScopeListEntry *scope_entry);
static void start_connection_for_scope(ClientScopeListEntry *scope_entry, const LwpaSockaddr *broker_addr);

// Find clients and scopes
static lwpa_error_t get_client(rdmnet_client_t handle, RdmnetClient **client);
static void release_client(const RdmnetClient *client);
static ClientScopeListEntry *get_scope(rdmnet_client_scope_t handle);
static ClientScopeListEntry *get_scope_by_disc_handle(rdmnet_scope_monitor_t handle);
static void release_scope(const ClientScopeListEntry *scope_entry);
static lwpa_error_t get_client_and_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                         RdmnetClient **client, ClientScopeListEntry **scope_entry);
static void release_client_and_scope(const RdmnetClient *client, const ClientScopeListEntry *scope_entry);

// Manage callbacks
static void fill_callback_info(const RdmnetClient *client, ClientCallbackDispatchInfo *cb);
static void deliver_callback(ClientCallbackDispatchInfo *info);
static bool connect_failed_will_retry(rdmnet_connect_fail_event_t event);
static bool disconnected_will_retry(rdmnet_connect_fail_event_t event);

// Message handling
static void free_rpt_client_message(RptClientMessage *msg);
static void free_ept_client_message(EptClientMessage *msg);
static bool handle_rpt_request(const RptMessage *rmsg, RptClientMessage *msg_out);
static bool handle_rpt_notification(const RptMessage *rmsg, RptClientMessage *msg_out);
static bool handle_rpt_status(const RptMessage *rmsg, RptClientMessage *msg_out);
static bool handle_rpt_message(const RdmnetClient *cli, const ClientScopeListEntry *scope_entry, const RptMessage *rmsg,
                               RptMsgReceivedArgs *cb_args);

// Red-black tree management
static int client_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static int scope_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static int scope_disc_handle_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static LwpaRbNode *client_node_alloc();
static void client_node_free(LwpaRbNode *node);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_client_init(const LwpaLogParams *lparams)
{
  // The lock is created only on the first call to this function.
  if (!client_lock_initted)
  {
    if (lwpa_mutex_create(&client_lock))
      client_lock_initted = true;
    else
      return kLwpaErrSys;
  }

  if (rdmnet_core_initialized())
  {
    // Already initted
    return kLwpaErrOk;
  }

  lwpa_error_t res = kLwpaErrSys;
  if (rdmnet_client_lock())
  {
    res = rdmnet_core_init(lparams);

#if !RDMNET_DYNAMIC_MEM
    if (res == kLwpaErrOk)
    {
      res |= lwpa_mempool_init(rdmnet_clients);
      res |= lwpa_mempool_init(client_scopes);
      res |= lwpa_mempool_init(client_rb_nodes);
      res |= lwpa_mempool_init(client_rdm_responses);
    }

    if (res != kLwpaErrOk)
    {
      rdmnet_core_deinit();
    }
#endif

    if (res == kLwpaErrOk)
    {
      lwpa_rbtree_init(&state.clients, client_cmp, client_node_alloc, client_node_free);
      lwpa_rbtree_init(&state.scopes_by_handle, scope_cmp, client_node_alloc, client_node_free);
      lwpa_rbtree_init(&state.scopes_by_disc_handle, scope_disc_handle_cmp, client_node_alloc, client_node_free);

      state.next_handle = 0;
      state.handle_has_wrapped_around = false;
    }
    rdmnet_client_unlock();
  }
  return res;
}

void rdmnet_client_deinit()
{
  if (!rdmnet_core_initialized())
    return;

  if (rdmnet_client_lock())
  {
    // TODO destroy all clients

    rdmnet_core_deinit();
    rdmnet_client_unlock();
  }
}

lwpa_error_t rdmnet_rpt_client_create(const RdmnetRptClientConfig *config, rdmnet_client_t *handle)
{
  if (!config || !handle)
    return kLwpaErrInvalid;
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;

  lwpa_error_t res = validate_rpt_client_config(config);
  if (res != kLwpaErrOk)
    return res;

  if (rdmnet_client_lock())
  {
    RdmnetClient *new_cli = rpt_client_new(config);
    if (new_cli)
    {
      *handle = new_cli->handle;
    }
    else
    {
      res = kLwpaErrNoMem;
    }
    rdmnet_client_unlock();
  }
  else
  {
    res = kLwpaErrSys;
  }

  return res;
}

lwpa_error_t rdmnet_client_destroy(rdmnet_client_t handle)
{
  (void)handle;
  // TODO
  return kLwpaErrNotImpl;
}

lwpa_error_t rdmnet_client_add_scope(rdmnet_client_t handle, const RdmnetScopeConfig *scope_config,
                                     rdmnet_client_scope_t *scope_handle)
{
  if (handle < 0 || !scope_config || !scope_handle)
    return kLwpaErrInvalid;

  RdmnetClient *cli;
  lwpa_error_t res = get_client(handle, &cli);
  if (res != kLwpaErrOk)
    return res;

  ClientScopeListEntry *new_entry;
  res = create_and_append_scope_entry(scope_config, cli, &new_entry);
  if (res == kLwpaErrOk)
  {
    *scope_handle = new_entry->handle;

    // Start discovery or connection on the new scope (depending on whether a static broker was
    // configured)
    if (new_entry->state == kScopeStateDiscovery)
      start_scope_discovery(new_entry);
    else if (new_entry->state == kScopeStateConnecting)
      start_connection_for_scope(new_entry, &new_entry->config.static_broker_addr);
  }

  release_client(cli);
  return res;
}

lwpa_error_t rdmnet_client_remove_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                        rdmnet_disconnect_reason_t reason)
{
  if (handle < 0 || scope_handle < 0)
    return kLwpaErrInvalid;

  RdmnetClient *cli;
  ClientScopeListEntry *scope_entry;
  lwpa_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  if (res != kLwpaErrOk)
    return res;

  if (scope_entry->monitor_handle)
  {
    rdmnetdisc_stop_monitoring(scope_entry->monitor_handle);
    lwpa_rbtree_remove(&state.scopes_by_disc_handle, scope_entry);
  }
  rdmnet_destroy_connection(scope_entry->handle, &reason);
  remove_scope_from_list(&cli->scope_list, scope_entry);
  lwpa_rbtree_remove(&state.scopes_by_handle, scope_entry);
  free_client_scope(scope_entry);

  release_client(cli);
  return res;
}

lwpa_error_t rdmnet_client_change_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                        const RdmnetScopeConfig *new_config, rdmnet_disconnect_reason_t reason)
{
  (void)handle;
  (void)scope_handle;
  (void)new_config;
  (void)reason;
  // TODO
  return kLwpaErrNotImpl;
}

lwpa_error_t rdmnet_client_request_client_list(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle)
{
  if (handle < 0 || scope_handle < 0)
    return kLwpaErrInvalid;

  RdmnetClient *cli;
  ClientScopeListEntry *scope_entry;
  lwpa_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  if (res != kLwpaErrOk)
    return res;

  res = send_fetch_client_list(scope_handle, &cli->cid);

  release_client(cli);
  return res;
}

lwpa_error_t rdmnet_rpt_client_send_rdm_command(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                const LocalRdmCommand *cmd, uint32_t *seq_num)
{
  if (handle < 0 || scope_handle < 0 || !cmd)
    return kLwpaErrInvalid;

  RdmnetClient *cli;
  ClientScopeListEntry *scope_entry;
  lwpa_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  if (res != kLwpaErrOk)
    return res;

  RptHeader header;
  header.source_uid = scope_entry->uid;
  header.source_endpoint_id = E133_NULL_ENDPOINT;
  header.dest_uid = cmd->dest_uid;
  header.dest_endpoint_id = cmd->dest_endpoint;
  header.seqnum = scope_entry->send_seq_num++;

  RdmCommand rdm_to_send = cmd->rdm;
  rdm_to_send.source_uid = scope_entry->uid;
  rdm_to_send.port_id = 1;
  rdm_to_send.transaction_num = (uint8_t)(header.seqnum & 0xffu);

  RdmBuffer buf_to_send;
  res = rdmctl_create_command(&rdm_to_send, &buf_to_send);
  if (res == kLwpaErrOk)
  {
    res = send_rpt_request(scope_handle, &cli->cid, &header, &buf_to_send);
    if (res == kLwpaErrOk && seq_num)
      *seq_num = header.seqnum;
  }

  release_client(cli);
  return res;
}

lwpa_error_t rdmnet_rpt_client_send_rdm_response(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                 const LocalRdmResponse *resp)
{
  if (handle < 0 || scope_handle < 0 || !resp)
    return kLwpaErrInvalid;

  RdmnetClient *cli;
  ClientScopeListEntry *scope_entry;
  lwpa_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  if (res != kLwpaErrOk)
    return res;

  size_t resp_buf_size = (resp->command_included ? resp->num_responses + 1 : resp->num_responses);
#if RDMNET_DYNAMIC_MEM
  RdmBuffer *resp_buf = calloc(resp_buf_size, sizeof(RdmBuffer));
  if (!resp_buf)
  {
    release_client(cli);
    return kLwpaErrNoMem;
  }
#else
  static RdmBuffer resp_buf[RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES + 1];
  if (resp->num_responses > RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES)
  {
    release_client(cli);
    return kLwpaErrMsgSize;
  }
#endif

  RptHeader header;
  header.source_uid = scope_entry->uid;
  header.source_endpoint_id = resp->source_endpoint;
  header.dest_uid = resp->dest_uid;
  header.dest_endpoint_id = E133_NULL_ENDPOINT;
  header.seqnum = resp->seq_num;

  if (resp->command_included)
  {
    res = rdmctl_create_command(&resp->cmd, &resp_buf[0]);
  }
  if (res == kLwpaErrOk)
  {
    for (size_t i = 0; i < resp->num_responses; ++i)
    {
      size_t out_buf_offset = resp->command_included ? i + 1 : i;
      RdmResponse resp_data = resp->rdm_arr[i];
      resp_data.source_uid = scope_entry->uid;
      res = rdmresp_create_response(&resp_data, &resp_buf[out_buf_offset]);
      if (res != kLwpaErrOk)
        break;
    }
  }
  if (res == kLwpaErrOk)
  {
    res = send_rpt_notification(scope_handle, &cli->cid, &header, resp_buf, resp_buf_size);
  }

#if RDMNET_DYNAMIC_MEM
  free(resp_buf);
#endif
  release_client(cli);
  return res;
}

lwpa_error_t rdmnet_rpt_client_send_status(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                           const RptStatusMsg *status)
{
  if (handle < 0 || scope_handle < 0 || !status)
    return kLwpaErrInvalid;

  RdmnetClient *cli;
  ClientScopeListEntry *scope_entry;
  lwpa_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  if (res != kLwpaErrOk)
    return res;

  // TODO
  (void)status;
  res = kLwpaErrNotImpl;

  release_client(cli);
  return res;
}

// Callback functions from the discovery interface

void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo *broker_info, void *context)
{
  (void)context;

  ClientScopeListEntry *scope_entry = get_scope_by_disc_handle(handle);
  if (scope_entry)
  {
    for (size_t i = 0; i < broker_info->listen_addrs_count; ++i)
    {
      // TODO temporary until we enable IPv6
      if (lwpaip_is_v4(&broker_info->listen_addrs[i].ip))
      {
        start_connection_for_scope(scope_entry, &broker_info->listen_addrs[i]);
        break;
      }
    }
    release_scope(scope_entry);
  }
}

void monitorcb_broker_lost(rdmnet_scope_monitor_t handle, const char *scope, const char *service_name, void *context)
{
  (void)handle;
  (void)context;

  lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, RDMNET_LOG_MSG("Broker '%s' no longer discovered on scope '%s'"),
           service_name, scope);
}

void monitorcb_scope_monitor_error(rdmnet_scope_monitor_t handle, const char *scope, int platform_error, void *context)
{
  (void)handle;
  (void)scope;
  (void)platform_error;
  (void)context;

  // TODO
}

void conncb_connected(rdmnet_conn_t handle, const RdmnetConnectedInfo *connect_info, void *context)
{
  (void)context;

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  init_callback_info(&cb);

  ClientScopeListEntry *scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient *cli = scope_entry->client;

    scope_entry->state = kScopeStateConnected;
    if (cli->type == kClientProtocolRPT && !cli->data.rpt.has_static_uid)
      scope_entry->uid = connect_info->client_uid;

    fill_callback_info(cli, &cb);
    cb.which = kClientCallbackConnected;
    cb.common_args.connected.scope_handle = scope_entry->handle;
    cb.common_args.connected.info.broker_addr = connect_info->connected_addr;

    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

void conncb_connect_failed(rdmnet_conn_t handle, const RdmnetConnectFailedInfo *failed_info, void *context)
{
  (void)context;

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  init_callback_info(&cb);

  ClientScopeListEntry *scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient *cli = scope_entry->client;

    scope_entry->state = kScopeStateDiscovery;

    RdmnetClientConnectFailedInfo info;
    info.event = failed_info->event;
    info.socket_err = failed_info->socket_err;
    info.rdmnet_reason = failed_info->rdmnet_reason;
    info.will_retry = connect_failed_will_retry(info.event);

    fill_callback_info(cli, &cb);
    cb.which = kClientCallbackConnectFailed;
    cb.common_args.connect_failed.scope_handle = handle;
    cb.common_args.connect_failed.info = info;

    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

void conncb_disconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo *disconn_info, void *context)
{
  (void)context;

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  init_callback_info(&cb);

  ClientScopeListEntry *scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient *cli = scope_entry->client;

    scope_entry->state = kScopeStateDiscovery;

    fill_callback_info(cli, &cb);
    cb.which = kClientCallbackDisconnected;

    RdmnetClientDisconnectedInfo info;
    info.event = disconn_info->event;
    info.socket_err = disconn_info->socket_err;
    info.rdmnet_reason = disconn_info->rdmnet_reason;
    info.will_retry = disconnected_will_retry(info.event);

    fill_callback_info(cli, &cb);
    cb.which = kClientCallbackDisconnected;
    cb.common_args.disconnected.scope_handle = handle;
    cb.common_args.disconnected.info = info;

    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

void conncb_msg_received(rdmnet_conn_t handle, const RdmnetMessage *message, void *context)
{
  (void)context;

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  init_callback_info(&cb);

  ClientScopeListEntry *scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient *cli = scope_entry->client;

    fill_callback_info(cli, &cb);

    switch (message->vector)
    {
      case ACN_VECTOR_ROOT_BROKER:
        cb.which = kClientCallbackBrokerMsgReceived;
        cb.common_args.broker_msg_received.scope_handle = handle;
        cb.common_args.broker_msg_received.msg = get_broker_msg(message);
        break;
      case ACN_VECTOR_ROOT_RPT:
        if (cli->type == kClientProtocolRPT)
        {
          if (handle_rpt_message(cli, scope_entry, get_rpt_msg(message), &cb.prot_info.rpt.msg_received))
          {
            cb.which = kClientCallbackMsgReceived;
          }
        }
        else
        {
          lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
                   RDMNET_LOG_MSG("Incorrectly got RPT message for non-RPT client %d on scope %d"), cli->handle,
                   handle);
        }
        break;
      case ACN_VECTOR_ROOT_EPT:
        // TODO, for now fall through
      default:
        lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
                 RDMNET_LOG_MSG("Got message with unhandled vector type %u on scope %d"), message->vector, handle);
        break;
    }
    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

bool handle_rpt_message(const RdmnetClient *cli, const ClientScopeListEntry *scope_entry, const RptMessage *rmsg,
                        RptMsgReceivedArgs *cb_args)
{
  (void)cli;
  bool res = false;

  switch (rmsg->vector)
  {
    case VECTOR_RPT_REQUEST:
      res = handle_rpt_request(rmsg, &cb_args->msg);
      break;
    case VECTOR_RPT_NOTIFICATION:
      res = handle_rpt_notification(rmsg, &cb_args->msg);
      break;
    case VECTOR_RPT_STATUS:
      res = handle_rpt_status(rmsg, &cb_args->msg);
      break;
  }

  if (res)
    cb_args->scope_handle = scope_entry->handle;
  return res;
}

bool handle_rpt_request(const RptMessage *rmsg, RptClientMessage *msg_out)
{
  RemoteRdmCommand *cmd = &msg_out->payload.cmd;
  const RdmBufListEntry *list = get_rdm_buf_list(rmsg)->list;

  if (!list->next)  // Only one RDM command allowed in an RPT request
  {
    lwpa_error_t unpack_res = rdmresp_unpack_command(&list->msg, &cmd->rdm);
    if (unpack_res == kLwpaErrOk)
    {
      msg_out->type = kRptClientMsgRdmCmd;
      cmd->source_uid = rmsg->header.source_uid;
      cmd->dest_endpoint = rmsg->header.dest_endpoint_id;
      cmd->seq_num = rmsg->header.seqnum;
      return true;
    }
  }
  return false;
}

bool handle_rpt_notification(const RptMessage *rmsg, RptClientMessage *msg_out)
{
  RemoteRdmResponse *resp = &msg_out->payload.resp;

  // Do some initialization
  msg_out->type = kRptClientMsgRdmResp;
  resp->command_included = false;
  resp->more_coming = get_rdm_buf_list(rmsg)->more_coming;
  resp->resp_list = NULL;
  RemoteRdmRespListEntry **next_entry = &resp->resp_list;

  bool good_parse = true;
  bool first_msg = true;
  for (const RdmBufListEntry *buf_entry = get_rdm_buf_list(rmsg)->list; buf_entry && good_parse;
       buf_entry = buf_entry->next)
  {
    if (first_msg)
    {
      if (rdmresp_is_non_disc_command(&buf_entry->msg))
      {
        // The command is included.
        lwpa_error_t unpack_res = rdmresp_unpack_command(&buf_entry->msg, &resp->cmd);
        if (unpack_res == kLwpaErrOk)
        {
          resp->command_included = true;
        }
        else
        {
          good_parse = false;
        }
        continue;
      }
      first_msg = false;
    }
    *next_entry = alloc_client_rdm_response();
    if (*next_entry)
    {
      lwpa_error_t unpack_res = rdmctl_unpack_response(&buf_entry->msg, &(*next_entry)->msg);
      if (unpack_res == kLwpaErrOk)
      {
        (*next_entry)->next = NULL;
        next_entry = &(*next_entry)->next;
      }
      else
      {
        good_parse = false;
      }
    }
    else
    {
      good_parse = false;
    }
  }

  if (good_parse)
  {
    // Fill in the rest of the info
    resp->source_uid = rmsg->header.source_uid;
    resp->source_endpoint = rmsg->header.source_endpoint_id;
    resp->seq_num = rmsg->header.seqnum;
    return true;
  }
  else
  {
    // Clean up
    free_rpt_client_message(msg_out);
    return false;
  }
}

bool handle_rpt_status(const RptMessage *rmsg, RptClientMessage *msg_out)
{
  RemoteRptStatus *status_out = &msg_out->payload.status;
  const RptStatusMsg *status = get_rpt_status_msg(rmsg);

  // This one is quick and simple with no failure condition
  msg_out->type = kRptClientMsgStatus;
  status_out->source_uid = rmsg->header.source_uid;
  status_out->source_endpoint = rmsg->header.source_endpoint_id;
  status_out->seq_num = rmsg->header.seqnum;
  status_out->msg = *status;
  return true;
}

void free_rpt_client_message(RptClientMessage *msg)
{
  if (msg->type == kRptClientMsgRdmResp)
  {
    RemoteRdmRespListEntry *next;
    for (RemoteRdmRespListEntry *to_free = get_remote_rdm_response(msg)->resp_list; to_free; to_free = next)
    {
      next = to_free->next;
      free_client_rdm_response(to_free);
    }
  }
}

void free_ept_client_message(EptClientMessage *msg)
{
  (void)msg;
  // TODO
}

void fill_callback_info(const RdmnetClient *client, ClientCallbackDispatchInfo *cb)
{
  cb->handle = client->handle;
  cb->type = client->type;
  cb->context = client->callback_context;
  if (client->type == kClientProtocolRPT)
  {
    cb->prot_info.rpt.cbs = client->data.rpt.callbacks;
  }
  else if (client->type == kClientProtocolEPT)
  {
    cb->prot_info.ept.cbs = client->data.ept.callbacks;
  }
}

void deliver_callback(ClientCallbackDispatchInfo *info)
{
  if (info->type == kClientProtocolRPT)
  {
    RptCallbackDispatchInfo *rpt_info = &info->prot_info.rpt;
    switch (info->which)
    {
      case kClientCallbackConnected:
        rpt_info->cbs.connected(info->handle, info->common_args.connected.scope_handle,
                                &info->common_args.connected.info, info->context);
        break;
      case kClientCallbackConnectFailed:
        rpt_info->cbs.connect_failed(info->handle, info->common_args.connect_failed.scope_handle,
                                     &info->common_args.connect_failed.info, info->context);
        break;
      case kClientCallbackDisconnected:
        rpt_info->cbs.disconnected(info->handle, info->common_args.disconnected.scope_handle,
                                   &info->common_args.disconnected.info, info->context);
        break;
      case kClientCallbackBrokerMsgReceived:
        rpt_info->cbs.broker_msg_received(info->handle, info->common_args.broker_msg_received.scope_handle,
                                          info->common_args.broker_msg_received.msg, info->context);
        break;
      case kClientCallbackMsgReceived:
        rpt_info->cbs.msg_received(info->handle, rpt_info->msg_received.scope_handle, &rpt_info->msg_received.msg,
                                   info->context);
        free_rpt_client_message(&rpt_info->msg_received.msg);
        break;
      case kClientCallbackNone:
      default:
        break;
    }
  }
  else if (info->type == kClientProtocolEPT)
  {
    EptCallbackDispatchInfo *ept_info = &info->prot_info.ept;
    switch (info->which)
    {
      case kClientCallbackConnected:
        ept_info->cbs.connected(info->handle, info->common_args.connected.scope_handle,
                                &info->common_args.connected.info, info->context);
        break;
      case kClientCallbackConnectFailed:
        ept_info->cbs.connect_failed(info->handle, info->common_args.connect_failed.scope_handle,
                                     &info->common_args.connect_failed.info, info->context);
        break;
      case kClientCallbackDisconnected:
        ept_info->cbs.disconnected(info->handle, info->common_args.disconnected.scope_handle,
                                   &info->common_args.disconnected.info, info->context);
        break;
      case kClientCallbackBrokerMsgReceived:
        ept_info->cbs.broker_msg_received(info->handle, info->common_args.broker_msg_received.scope_handle,
                                          info->common_args.broker_msg_received.msg, info->context);
        break;
      case kClientCallbackMsgReceived:
        ept_info->cbs.msg_received(info->handle, ept_info->msg_received.scope_handle, &ept_info->msg_received.msg,
                                   info->context);
        free_ept_client_message(&ept_info->msg_received.msg);
        break;
      case kClientCallbackNone:
      default:
        break;
    }
  }
}

bool connect_failed_will_retry(rdmnet_connect_fail_event_t event)
{
  (void)event;
  // TODO
  return true;
}

bool disconnected_will_retry(rdmnet_connect_fail_event_t event)
{
  (void)event;
  // TODO
  return true;
}

/* Validate the data in an RdmnetRptClientConfig structure. */
lwpa_error_t validate_rpt_client_config(const RdmnetRptClientConfig *config)
{
  if ((config->type != kRPTClientTypeDevice && config->type != kRPTClientTypeController) ||
      (!rpt_client_uid_is_dynamic(&config->uid) && (config->uid.manu & 0x8000)) ||
      (lwpa_uuid_is_null(&config->cid)))
  {
    return kLwpaErrInvalid;
  }
  return kLwpaErrOk;
}

/* Create and initialize a new RdmnetClient structure from a given config. */
RdmnetClient *rpt_client_new(const RdmnetRptClientConfig *config)
{
  rdmnet_client_t new_handle = get_new_client_handle();
  if (new_handle == RDMNET_CLIENT_INVALID)
    return NULL;

  RdmnetClient *new_cli = alloc_rdmnet_client();
  if (new_cli)
  {
    new_cli->handle = new_handle;
    if (0 != lwpa_rbtree_insert(&state.clients, new_cli))
    {
      // Init the client data
      new_cli->type = kClientProtocolRPT;
      new_cli->cid = config->cid;
      new_cli->data.rpt.callbacks = config->callbacks;
      new_cli->callback_context = config->callback_context;
      new_cli->data.rpt.type = config->type;
      if (rpt_client_uid_is_dynamic(&config->uid))
      {
        new_cli->data.rpt.has_static_uid = false;
        new_cli->data.rpt.uid.manu = config->uid.manu;
      }
      else
      {
        new_cli->data.rpt.has_static_uid = true;
        new_cli->data.rpt.uid = config->uid;
      }
      new_cli->scope_list = NULL;
    }
    else
    {
      free_rdmnet_client(new_cli);
      new_cli = NULL;
    }
  }

  return new_cli;
}

/* Get a new client handle to assign to a new client.
 * Must have lock.
 */
rdmnet_client_t get_new_client_handle()
{
  rdmnet_client_t new_handle = state.next_handle;
  if (++state.next_handle < 0)
  {
    state.next_handle = 0;
    state.handle_has_wrapped_around = true;
  }
  // Optimization - keep track of whether the handle counter has wrapped around.
  // If not, we don't need to check if the new handle is in use.
  if (state.handle_has_wrapped_around)
  {
    // We have wrapped around at least once, we need to check for handles in use
    rdmnet_client_t original = new_handle;
    while (lwpa_rbtree_find(&state.clients, &new_handle))
    {
      if (state.next_handle == original)
      {
        // Incredibly unlikely case of all handles used
        new_handle = RDMNET_CLIENT_INVALID;
        break;
      }
      new_handle = state.next_handle;
      if (++state.next_handle < 0)
        state.next_handle = 0;
    }
  }
  return new_handle;
}

/* Allocate a new scope list entry and append it to a client's scope list. If a scope string is
 * already in the list, fails with kLwpaErrExists. Attempts to create a new connection handle to
 * accompany the scope. Returns kLwpaErrOk on success, other error code otherwise. Fills in
 * new_entry with the newly-created entry on success.
 */
lwpa_error_t create_and_append_scope_entry(const RdmnetScopeConfig *config, RdmnetClient *client,
                                           ClientScopeListEntry **new_entry)
{
  if (find_scope_in_list(client->scope_list, config->scope))
    return kLwpaErrExists;

  ClientScopeListEntry **entry_ptr = &client->scope_list;
  for (; *entry_ptr; entry_ptr = &(*entry_ptr)->next)
    ;

  // The scope string was not in the list, try to allocate it
  lwpa_error_t res = kLwpaErrNoMem;
  ClientScopeListEntry *new_scope = alloc_client_scope();
  if (new_scope)
  {
    RdmnetConnectionConfig conn_config;
    conn_config.local_cid = client->cid;
    conn_config.callbacks = conn_callbacks;
    conn_config.callback_context = NULL;

    res = rdmnet_new_connection(&conn_config, &new_scope->handle);
    if (res == kLwpaErrOk)
    {
      if (0 != lwpa_rbtree_insert(&state.scopes_by_handle, new_scope))
      {
        res = kLwpaErrOk;
        new_scope->next = NULL;
        *entry_ptr = new_scope;
      }
      else
      {
        res = kLwpaErrNoMem;
        rdmnet_destroy_connection(new_scope->handle, NULL);
        free_client_scope(new_scope);
        new_scope = NULL;
      }
    }
    else
    {
      free_client_scope(new_scope);
      new_scope = NULL;
    }
  }

  if (res == kLwpaErrOk)
  {
    // Do the rest of the initialization
    new_scope->config = *config;
    if (config->has_static_broker_addr)
      new_scope->state = kScopeStateConnecting;
    else
      new_scope->state = kScopeStateDiscovery;
    // uid init is done at connection time
    new_scope->send_seq_num = 1;
    new_scope->monitor_handle = NULL;
    new_scope->client = client;

    *new_entry = new_scope;
  }
  return res;
}

ClientScopeListEntry *find_scope_in_list(ClientScopeListEntry *list, const char *scope)
{
  ClientScopeListEntry *entry;
  for (entry = list; entry; entry = entry->next)
  {
    if (strcmp(entry->config.scope, scope) == 0)
    {
      // Found
      return entry;
    }
  }
  return NULL;
}

void remove_scope_from_list(ClientScopeListEntry **list, ClientScopeListEntry *entry)
{
  ClientScopeListEntry *last_entry = NULL;

  for (ClientScopeListEntry *cur_entry = *list; cur_entry; last_entry = cur_entry, cur_entry = cur_entry->next)
  {
    if (cur_entry == entry)
    {
      if (last_entry)
        last_entry->next = cur_entry->next;
      else
        *list = cur_entry->next;
      break;
    }
  }
}

void start_scope_discovery(ClientScopeListEntry *scope_entry)
{
  RdmnetScopeMonitorConfig config;

  rdmnet_safe_strncpy(config.scope, scope_entry->config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
  // TODO expose domain to API
  rdmnet_safe_strncpy(config.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
  config.callbacks = disc_callbacks;
  config.callback_context = NULL;

  // TODO capture errors
  int platform_error;
  if (kLwpaErrOk == rdmnetdisc_start_monitoring(&config, &scope_entry->monitor_handle, &platform_error))
  {
    lwpa_rbtree_insert(&state.scopes_by_disc_handle, scope_entry);
  }
}

void start_connection_for_scope(ClientScopeListEntry *scope_entry, const LwpaSockaddr *broker_addr)
{
  ClientConnectMsg connect_msg;
  RdmnetClient *cli = scope_entry->client;

  if (cli->type == kClientProtocolRPT)
  {
    RptClientData *rpt_data = &cli->data.rpt;
    RdmUid my_uid;
    if (rpt_data->has_static_uid)
    {
      my_uid = rpt_data->uid;
    }
    else
    {
      rdmnet_init_dynamic_uid_request(&my_uid, rpt_data->uid.manu);
    }

    rdmnet_safe_strncpy(connect_msg.scope, scope_entry->config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
    connect_msg.e133_version = E133_VERSION;
    // TODO expose to API
    rdmnet_safe_strncpy(connect_msg.search_domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
    if (rpt_data->type == kRPTClientTypeController)
      connect_msg.connect_flags = CONNECTFLAG_INCREMENTAL_UPDATES;
    else
      connect_msg.connect_flags = 0;
    create_rpt_client_entry(&cli->cid, &my_uid, rpt_data->type, NULL, &connect_msg.client_entry);
  }
  else
  {
    // TODO EPT
    return;
  }

  rdmnet_connect(scope_entry->handle, broker_addr, &connect_msg);
}

lwpa_error_t get_client(rdmnet_client_t handle, RdmnetClient **client)
{
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;
  if (!rdmnet_client_lock())
    return kLwpaErrSys;

  RdmnetClient *found_cli = (RdmnetClient *)lwpa_rbtree_find(&state.clients, &handle);
  if (!found_cli)
  {
    rdmnet_client_unlock();
    return kLwpaErrNotFound;
  }
  *client = found_cli;
  // Return keeping the lock
  return kLwpaErrOk;
}

void release_client(const RdmnetClient *client)
{
  (void)client;
  rdmnet_client_unlock();
}

ClientScopeListEntry *get_scope(rdmnet_client_scope_t handle)
{
  if (!rdmnet_client_lock())
    return NULL;

  ClientScopeListEntry *found_scope = (ClientScopeListEntry *)lwpa_rbtree_find(&state.scopes_by_handle, &handle);
  if (!found_scope)
  {
    rdmnet_client_unlock();
    return NULL;
  }
  // Return keeping the lock
  return found_scope;
}

ClientScopeListEntry *get_scope_by_disc_handle(rdmnet_scope_monitor_t handle)
{
  if (!rdmnet_client_lock())
    return NULL;

  ClientScopeListEntry scope_cmp;
  scope_cmp.monitor_handle = handle;
  ClientScopeListEntry *found_scope =
      (ClientScopeListEntry *)lwpa_rbtree_find(&state.scopes_by_disc_handle, &scope_cmp);
  if (!found_scope)
  {
    rdmnet_client_unlock();
    return NULL;
  }
  // Return keeping the lock
  return found_scope;
}

void release_scope(const ClientScopeListEntry *scope_entry)
{
  (void)scope_entry;
  rdmnet_client_unlock();
}

lwpa_error_t get_client_and_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, RdmnetClient **client,
                                  ClientScopeListEntry **scope_entry)
{
  RdmnetClient *found_cli;
  lwpa_error_t res = get_client(handle, &found_cli);
  if (res != kLwpaErrOk)
    return res;

  ClientScopeListEntry *found_scope = (ClientScopeListEntry *)lwpa_rbtree_find(&state.scopes_by_handle, &scope_handle);
  if (!found_scope)
  {
    release_client(found_cli);
    return kLwpaErrNotFound;
  }
  if (found_scope->client != found_cli)
  {
    release_client(found_cli);
    return kLwpaErrInvalid;
  }
  *client = found_cli;
  *scope_entry = found_scope;
  // Return keeping the lock
  return kLwpaErrOk;
}

void release_client_and_scope(const RdmnetClient *client, const ClientScopeListEntry *scope)
{
  (void)client;
  (void)scope;
  rdmnet_client_unlock();
}

int client_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;

  RdmnetClient *a = (RdmnetClient *)node_a->value;
  RdmnetClient *b = (RdmnetClient *)node_b->value;

  return a->handle - b->handle;
}

int scope_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;

  ClientScopeListEntry *a = (ClientScopeListEntry *)node_a->value;
  ClientScopeListEntry *b = (ClientScopeListEntry *)node_b->value;

  return a->handle - b->handle;
}

int scope_disc_handle_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;

  ClientScopeListEntry *a = (ClientScopeListEntry *)node_a->value;
  ClientScopeListEntry *b = (ClientScopeListEntry *)node_b->value;

  if (a->monitor_handle > b->monitor_handle)
    return 1;
  else if (a->monitor_handle < b->monitor_handle)
    return -1;
  else
    return 0;
}

LwpaRbNode *client_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode *)malloc(sizeof(LwpaRbNode));
#else
  return lwpa_mempool_alloc(client_rb_nodes);
#endif
}

void client_node_free(LwpaRbNode *node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  lwpa_mempool_free(client_rb_nodes, node);
#endif
}
