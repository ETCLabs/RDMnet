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
#include "rdmnet/core.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/discovery.h"
#include "rdmnet/core/util.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/client.h"
#include "rdmnet/private/opts.h"

/*************************** Private constants *******************************/

#define RDMNET_MAX_CLIENT_SCOPES ((RDMNET_MAX_CONTROLLERS * RDMNET_MAX_SCOPES_PER_CONTROLLER) + RDMNET_MAX_DEVICES)

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using lwpa_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_client() malloc(sizeof(RdmnetClientInternal))
#define alloc_client_scope() malloc(sizeof(ClientScopeListEntry))
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

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(rdmnet_clients, RdmnetClientInternal, RDMNET_MAX_CLIENTS);
LWPA_MEMPOOL_DEFINE(client_scopes, ClientScopeListEntry, RDMNET_MAX_CLIENT_SCOPES);
#endif

static bool client_lock_initted = false;
static lwpa_mutex_t client_lock;

static struct RdmnetClientState
{
  RdmnetClientInternal *clients;
} state;

static void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo *broker_info,
                                   void *context);
static void monitorcb_broker_lost(rdmnet_scope_monitor_t handle, const char *scope, const char *service_name,
                                  void *context);
static void monitorcb_scope_monitor_error(rdmnet_scope_monitor_t handle, const char *scope, int platform_error,
                                          void *context);

static const RdmnetScopeMonitorCallbacks disc_callbacks = {monitorcb_broker_found, monitorcb_broker_lost,
                                                           monitorcb_scope_monitor_error};

static void conncb_connected(rdmnet_conn_t handle, const ConnectReplyMsg *connect_reply, void *context);
static void conncb_disconnected(rdmnet_conn_t handle, const RdmnetDisconnectInfo *disconn_info, void *context);
static void conncb_msg_received(rdmnet_conn_t handle, const RdmnetMessage *message, void *context);

static const RdmnetConnCallbacks conn_callbacks = {conncb_connected, conncb_disconnected, conncb_msg_received};

/*********************** Private function prototypes *************************/

static RdmnetClientInternal *rpt_client_new(const RdmnetRptClientConfig *config);
static lwpa_error_t validate_rpt_client_config(const RdmnetRptClientConfig *config);
static void append_to_client_list(RdmnetClientInternal *new_client);
static void start_scope_discovery(ClientScopeListEntry *scope_entry);
static void start_connection_for_scope(ClientScopeListEntry *scope_entry, const LwpaSockaddr *broker_addr);
static bool client_in_list(const RdmnetClientInternal *client, const RdmnetClientInternal *list);
static bool create_and_append_scope_entry(const RdmnetScopeConfig *config, RdmnetClientInternal *client);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_rpt_client_create(const RdmnetRptClientConfig *config, rdmnet_client_t *handle)
{
  if (!config || !handle)
    return LWPA_INVALID;
  if (!rdmnet_core_initialized())
    return LWPA_NOTINIT;

  // The lock is created only on the first call to this function.
  if (!client_lock_initted)
  {
    if (lwpa_mutex_create(&client_lock))
      client_lock_initted = true;
    else
      return LWPA_SYSERR;
  }

  lwpa_error_t res = validate_rpt_client_config(config);
  if (res != LWPA_OK)
    return res;

  if (lwpa_mutex_take(&client_lock, LWPA_WAIT_FOREVER))
  {
    RdmnetClientInternal *new_cli = rpt_client_new(config);
    if (new_cli)
    {
      append_to_client_list(new_cli);
      for (ClientScopeListEntry *scope = new_cli->scope_list; scope; scope = scope->next)
      {
        if (scope->state == kScopeStateDiscovery)
          start_scope_discovery(scope);
        else if (scope->state == kScopeStateConnecting)
          start_connection_for_scope(scope, &scope->scope_config.static_broker_addr);
      }
    }
    else
    {
      res = LWPA_NOMEM;
    }
    lwpa_mutex_give(&client_lock);
  }
  else
  {
    res = LWPA_SYSERR;
  }

  return res;
}

void rdmnet_rpt_client_destroy(rdmnet_client_t handle)
{
  (void)handle;
  // TODO
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

/* Validate the data in an RdmnetRptClientConfig structure. */
lwpa_error_t validate_rpt_client_config(const RdmnetRptClientConfig *config)
{
  if ((config->type != kRPTClientTypeDevice && config->type != kRPTClientTypeController) ||
      (config->has_static_uid && (config->static_uid.manu & 0x8000)) ||
      (lwpa_uuid_cmp(&config->cid, &LWPA_NULL_UUID) == 0) || (config->num_scopes == 0) || (!config->scope_list) ||
      (config->type == kRPTClientTypeDevice && config->num_scopes != 1))
  {
    return LWPA_INVALID;
  }
#if !RDMNET_DYNAMIC_MEM
  if (config->type == kRPTClientTypeController && config->num_scopes > RDMNET_MAX_SCOPES_PER_CONTROLLER)
  {
    return LWPA_NOMEM;
  }
#endif
  return LWPA_OK;
}

/* Create and initialize a new RdmnetClientInternal structure from a given config. */
RdmnetClientInternal *rpt_client_new(const RdmnetRptClientConfig *config)
{
  RdmnetClientInternal *new_cli = alloc_rdmnet_client();
  if (new_cli)
  {
    // Init the client data
    new_cli->type = kClientProtocolRPT;
    new_cli->cid = config->cid;
    new_cli->callbacks = config->callbacks;
    new_cli->callback_context = config->callback_context;
    new_cli->data.rpt.type = config->type;
    new_cli->data.rpt.has_static_uid = config->has_static_uid;
    new_cli->data.rpt.static_uid = config->static_uid;
    new_cli->scope_list = NULL;
    new_cli->next = NULL;

    bool scope_alloc_successful = true;  // Set false if any scope initialization in the list fails.
    // Create an entry for each configured scope
    for (size_t i = 0; i < config->num_scopes; ++i)
    {
      if (!create_and_append_scope_entry(&config->scope_list[i], new_cli))
      {
        scope_alloc_successful = false;
        break;
      }
    }

    if (!scope_alloc_successful)
    {
      // Clean up
      ClientScopeListEntry *to_free = new_cli->scope_list;
      ClientScopeListEntry *next;
      while (to_free)
      {
        if (to_free->conn_handle >= 0)
        {
          rdmnet_destroy_connection(to_free->conn_handle, NULL);
        }
        next = to_free->next;
        free_client_scope(to_free);
        to_free = next;
      }
      free_rdmnet_client(new_cli);
      new_cli = NULL;
    }
  }
  return new_cli;
}

/* Allocate a new scope list entry and append it to a client's scope list. If a scope string is
 * already in the list, replaces the configuration for that scope. Creates a new connection handle
 * to accompany the scope. Returns true if scope allocation was successful, false otherwise.
 */
bool create_and_append_scope_entry(const RdmnetScopeConfig *config, RdmnetClientInternal *client)
{
  // For each scope, we need to create a list entry and a connection handle.
  ClientScopeListEntry **entry_ptr = &client->scope_list;
  ClientScopeListEntry *new_scope = NULL;

  for (; *entry_ptr; entry_ptr = &(*entry_ptr)->next)
  {
    if (strcmp((*entry_ptr)->scope_config.scope, config->scope) == 0)
    {
      break;
    }
  }

  if (*entry_ptr)
  {
    // We will replace the configuration and reset the state below
    new_scope = *entry_ptr;
  }
  else
  {
    // The scope string was not in the list, try to allocate it
    new_scope = alloc_client_scope();
    if (new_scope)
    {
      RdmnetConnectionConfig conn_config;
      conn_config.local_cid = client->cid;
      conn_config.callbacks = conn_callbacks;
      conn_config.callback_context = new_scope;

      if (rdmnet_new_connection(&conn_config, &new_scope->conn_handle) == LWPA_OK)
      {
        new_scope->next = NULL;
        *entry_ptr = new_scope;
      }
      else
      {
        free_client_scope(new_scope);
        new_scope = NULL;
      }
    }
  }

  if (new_scope)
  {
    // Do the rest of the initialization
    new_scope->scope_config = *config;
    new_scope->cli = client;
    if (config->has_static_broker_addr)
      new_scope->state = kScopeStateConnecting;
    else
      new_scope->state = kScopeStateDiscovery;
    return true;
  }
  return false;
}

void append_to_client_list(RdmnetClientInternal *new_client)
{
  RdmnetClientInternal **new_client_ptr = &state.clients;
  while (*new_client_ptr)
    new_client_ptr = &(*new_client_ptr)->next;
  *new_client_ptr = new_client;
}

void start_scope_discovery(ClientScopeListEntry *scope_entry)
{
  RdmnetScopeMonitorConfig config;

  rdmnet_safe_strncpy(config.scope, scope_entry->scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
  // TODO expose to API
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
  RdmnetClientInternal *cli = scope_entry->cli;

  if (cli->type == kClientProtocolRPT)
  {
    RptClientData *rpt_data = &cli->data.rpt;
    RdmUid my_uid;
    if (rpt_data->has_static_uid)
    {
      my_uid = rpt_data->static_uid;
    }
    else
    {
      // TODO better interface for manufacturer ID
      rdmnet_init_dynamic_uid_request(&my_uid, 0x6574);
    }

    rdmnet_safe_strncpy(connect_msg.scope, scope_entry->scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
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

  rdmnet_connect(scope_entry->conn_handle, broker_addr, &connect_msg);
}

// ClientScopeListEntry *find_scope_in_list(const char *scope, ClientScopeListEntry *list)
//{
//  ClientScopeListEntry *entry;
//  for (entry = list; entry; entry = entry->next)
//  {
//    if (strcmp(entry->scope_config.scope, scope) == 0)
//    {
//      // Found
//      return entry;
//    }
//  }
//  return NULL;
//}

bool client_in_list(const RdmnetClientInternal *client, const RdmnetClientInternal *list)
{
  for (const RdmnetClientInternal *entry = list; entry; entry = entry->next)
  {
    if (client == entry)
      return true;
  }
  return false;
}

// Callback functions from the discovery interface

void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo *broker_info, void *context)
{
  (void)handle;
  (void)broker_info;

  if (rdmnet_client_lock())
  {
    ClientScopeListEntry *scope_entry = (ClientScopeListEntry *)context;
    if (scope_entry)
    {
      RdmnetClientInternal *cli = scope_entry->cli;
      if (client_in_list(cli, state.clients))
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
      }
    }
    rdmnet_client_unlock();
  }
}

void monitorcb_broker_lost(rdmnet_scope_monitor_t handle, const char *scope, const char *service_name, void *context)
{
  (void)handle;
  (void)scope;
  (void)service_name;
  (void)context;

  /*
  if (rdmnet_client_lock())
  {
    RdmnetClientInternal *cli = (RdmnetClientInternal *)context;
    if (cli && client_in_list(cli, state.clients))
    {
    }
    rdmnet_client_unlock();
  }
  */
}

void monitorcb_scope_monitor_error(rdmnet_scope_monitor_t handle, const char *scope, int platform_error, void *context)
{
  (void)handle;
  (void)scope;
  (void)platform_error;

  if (rdmnet_client_lock())
  {
    RdmnetClientInternal *cli = (RdmnetClientInternal *)context;
    if (cli && client_in_list(cli, state.clients))
    {
    }
    rdmnet_client_unlock();
  }
}

void conncb_connected(rdmnet_conn_t handle, const ConnectReplyMsg *connect_reply, void *context)
{
  bool deliver_cb = false;
  RdmnetClientConnectedCb cb = NULL;
  rdmnet_client_t cb_handle = NULL;
  char cb_scope[E133_SCOPE_STRING_PADDED_LENGTH];
  void *cb_context = NULL;

  if (rdmnet_client_lock())
  {
    ClientScopeListEntry *scope_entry = (ClientScopeListEntry *)context;
    if (scope_entry)
    {
      RdmnetClientInternal *cli = scope_entry->cli;
      if (client_in_list(cli, state.clients))
      {
        scope_entry->state = kScopeStateConnected;
        if (cli->type == kClientProtocolRPT && !cli->data.rpt.has_static_uid)
          scope_entry->uid = connect_reply->client_uid;

        deliver_cb = true;
        cb = cli->callbacks.connected;
        cb_handle = cli;
        rdmnet_safe_strncpy(cb_scope, scope_entry->scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
        cb_context = cli->callback_context;
      }
    }
    rdmnet_client_unlock();
  }

  if (deliver_cb)
    cb(cb_handle, cb_scope, cb_context);
}

void conncb_disconnected(rdmnet_conn_t handle, const RdmnetDisconnectInfo *disconn_info, void *context)
{
  bool deliver_cb = false;
  RdmnetClientConnectedCb cb = NULL;
  rdmnet_client_t cb_handle = NULL;
  char cb_scope[E133_SCOPE_STRING_PADDED_LENGTH];
  void *cb_context = NULL;

  if (rdmnet_client_lock())
  {
    ClientScopeListEntry *scope_entry = (ClientScopeListEntry *)context;
    if (scope_entry)
    {
      RdmnetClientInternal *cli = scope_entry->cli;
      if (client_in_list(cli, state.clients))
      {
        deliver_cb = true;
        cb = cli->callbacks.disconnected;
        cb_handle = cli;
        rdmnet_safe_strncpy(cb_scope, scope_entry->scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
        cb_context = cli->callback_context;
      }
    }
    rdmnet_client_unlock();
  }

  if (deliver_cb)
    cb(cb_handle, cb_scope, cb_context);
}

void conncb_msg_received(rdmnet_conn_t handle, const RdmnetMessage *message, void *context)
{
  lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, "Got message on connection %p", handle);
  /* TODO
  if (rdmnet_client_lock())
  {
    RdmnetClientInternal *cli = (RdmnetClientInternal *)context;
    if (cli && client_in_list(cli, state.clients))
    {
    }
    rdmnet_client_unlock();
  }
  */
}
