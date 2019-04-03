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
#include "rdmnet/core.h"
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
#define alloc_client_rb_node()
#define free_rdmnet_client(ptr) free(ptr)
#define free_client_scope(ptr) free(ptr)
#else
#define alloc_rdmnet_client() lwpa_mempool_alloc(rdmnet_clients)
#define alloc_client_scope() lwpa_mempool_alloc(client_scopes)
#define free_rdmnet_client(ptr) lwpa_mempool_free(rdmnet_clients, ptr)
#define free_client_scope(ptr) lwpa_mempool_free(client_scopes, ptr)
#endif

#define rdmnet_client_lock() lwpa_mutex_take(&client_lock, LWPA_WAIT_FOREVER)
#define rdmnet_client_unlock() lwpa_mutex_give(&client_lock)

#define init_callback_info(cbptr) ((cbptr)->which = kClientCallbackNone)

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(rdmnet_clients, RdmnetClient, RDMNET_MAX_CLIENTS);
LWPA_MEMPOOL_DEFINE(client_scopes, ClientScopeListEntry, RDMNET_MAX_CLIENT_SCOPES);
LWPA_MEMPOOL_DEFINE(client_rb_nodes, LwpaRbNode, CLIENT_MAX_RB_NODES);
#endif

static bool client_lock_initted = false;
static lwpa_mutex_t client_lock;

static struct RdmnetClientState
{
  LwpaRbTree clients;

  LwpaRbTree scopes_by_handle;
  LwpaRbTree scopes_by_disc_handle;
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
static lwpa_error_t create_and_append_scope_entry(const RdmnetScopeConfig *config, RdmnetClient *client,
                                                  ClientScopeListEntry **new_entry);
static ClientScopeListEntry *find_scope_in_list(ClientScopeListEntry *list, const char *scope);
static void remove_scope_from_list(ClientScopeListEntry *list, ClientScopeListEntry *entry);

static bool create_rpt_client_entry(const LwpaUuid *cid, const RdmUid *uid, rpt_client_type_t client_type,
                                    const LwpaUuid *binding_cid, ClientEntryData *entry);
static void start_scope_discovery(ClientScopeListEntry *scope_entry);
static void start_connection_for_scope(ClientScopeListEntry *scope_entry, const LwpaSockaddr *broker_addr);

// Find clients and scopes
static bool client_exists(rdmnet_client_t handle);
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
static void deliver_callback(const ClientCallbackDispatchInfo *info);

// Red-black tree management
static int client_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static int scope_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
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

void rdmnet_rpt_client_destroy(rdmnet_client_t handle)
{
  (void)handle;
  // TODO
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
                                        const RdmnetScopeConfig *new_config)
{
  return kLwpaErrNotImpl;
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

  RptHeader header;
  header.source_uid = scope_entry->uid;
  header.source_endpoint_id = resp->source_endpoint;
  header.dest_uid = resp->dest_uid;
  header.dest_endpoint_id = E133_NULL_ENDPOINT;
  header.seqnum = resp->seq_num;

  // TODO
  res = kLwpaErrNotImpl;

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
  (void)handle;
  (void)broker_info;

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
  (void)scope;
  (void)service_name;
  (void)context;

  lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, RDMNET_LOG_MSG("Broker '%s' no longer discovered on scope '%s'"),
           service_name, scope);
}

void monitorcb_scope_monitor_error(rdmnet_scope_monitor_t handle, const char *scope, int platform_error, void *context)
{
  (void)handle;
  (void)scope;
  (void)platform_error;

  // TODO
}

void conncb_connected(rdmnet_conn_t handle, const RdmnetConnectedInfo *connect_info, void *context)
{
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

    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

void conncb_connect_failed(rdmnet_conn_t handle, const RdmnetConnectFailedInfo *failed_info, void *context)
{
}

void conncb_disconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo *disconn_info, void *context)
{
  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  init_callback_info(&cb);

  ClientScopeListEntry *scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient *cli = scope_entry->client;

    scope_entry->state = kScopeStateDiscovery;
    if (cli->type == kClientProtocolRPT && !cli->data.rpt.has_static_uid)
      rdmnet_init_dynamic_uid_request(&scope_entry->uid, 0x6574);  // TODO un-hardcode

    fill_callback_info(cli, &cb);
    cb.which = kClientCallbackDisconnected;

    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

void conncb_msg_received(rdmnet_conn_t handle, const RdmnetMessage *message, void *context)
{
  lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, RDMNET_LOG_MSG("Got message on connection %p"), handle);
  /* TODO
  if (rdmnet_client_lock())
  {
    RdmnetClient *cli = (RdmnetClient *)context;
    if (cli && client_in_list(cli, state.clients))
    {
    }
    rdmnet_client_unlock();
  }
  */
}

void fill_callback_info(const RdmnetClient *client, ClientCallbackDispatchInfo *cb)
{
  cb->handle = client;
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

void deliver_callback(const ClientCallbackDispatchInfo *info)
{
  if (info->type == kClientProtocolRPT)
  {
    const RptCallbackDispatchInfo *rpt_info = &info->prot_info.rpt;
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
        break;
      case kClientCallbackNone:
      default:
        break;
    }
  }
  else if (info->type == kClientProtocolEPT)
  {
    const EptCallbackDispatchInfo *ept_info = &info->prot_info.ept;
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
        break;
      case kClientCallbackNone:
      default:
        break;
    }
  }
}

/* Validate the data in an RdmnetRptClientConfig structure. */
lwpa_error_t validate_rpt_client_config(const RdmnetRptClientConfig *config)
{
  if ((config->type != kRPTClientTypeDevice && config->type != kRPTClientTypeController) ||
      (!rpt_client_uid_is_dynamic(&config->uid) && (config->uid.manu & 0x8000)) ||
      (lwpa_uuid_cmp(&config->cid, &LWPA_NULL_UUID) == 0))
  {
    return kLwpaErrInvalid;
  }
  return kLwpaErrOk;
}

/* Create and initialize a new RdmnetClient structure from a given config. */
RdmnetClient *rpt_client_new(const RdmnetRptClientConfig *config)
{
  RdmnetClient *new_cli = alloc_rdmnet_client();
  if (new_cli)
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

  return new_cli;
}

/* Allocate a new scope list entry and append it to a client's scope list. If a scope string is
 * already in the list, fails with kLwpaErrExists. Attempts to create a new connection handle to
 * accompany the scope. Returns kLwpaErrOk on success, other error code otherwise.
 */
lwpa_error_t create_and_append_scope_entry(const RdmnetScopeConfig *config, RdmnetClient *client)
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
    conn_config.callback_context = new_scope;

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

bool create_rpt_client_entry(const LwpaUuid *cid, const RdmUid *uid, rpt_client_type_t client_type,
                             const LwpaUuid *binding_cid, ClientEntryData *entry)
{
  if (!cid || !uid || !entry)
    return false;

  entry->client_protocol = kClientProtocolRPT;
  entry->client_cid = *cid;
  entry->data.rpt_data.client_uid = *uid;
  entry->data.rpt_data.client_type = client_type;
  if (binding_cid)
    entry->data.rpt_data.binding_cid = *binding_cid;
  else
    memset(entry->data.rpt_data.binding_cid.data, 0, LWPA_UUID_BYTES);
  entry->next = NULL;
  return true;
}

void start_scope_discovery(ClientScopeListEntry *scope_entry)
{
  RdmnetScopeMonitorConfig config;

  rdmnet_safe_strncpy(config.scope, scope_entry->config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
  // TODO expose domain to API
  rdmnet_safe_strncpy(config.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
  config.callbacks = disc_callbacks;
  config.callback_context = scope_entry;

  // TODO capture errors
  int platform_error;
  rdmnetdisc_start_monitoring(&config, &scope_entry->monitor_handle, &platform_error);
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
  RdmnetClient *a = (RdmnetClient *)node_a->value;
  RdmnetClient *b = (RdmnetClient *)node_b->value;

  return a->handle - b->handle;
}

int scope_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  ClientScopeListEntry *a = (ClientScopeListEntry *)node_a->value;
  ClientScopeListEntry *b = (ClientScopeListEntry *)node_b->value;

  return a->handle - b->handle;
}

int scope_disc_handle_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
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
