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

#include "rdmnet/core/client.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "rdm/controller.h"
#include "rdm/responder.h"
#include "rdmnet/core.h"
#include "rdmnet/core/client_entry.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/util.h"
#include "rdmnet/discovery.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/client.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/util.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/*************************** Private constants *******************************/

#define RDMNET_MAX_CLIENT_SCOPES ((RDMNET_MAX_CONTROLLERS * RDMNET_MAX_SCOPES_PER_CONTROLLER) + RDMNET_MAX_DEVICES)
#define MAX_CLIENT_RB_NODES ((RDMNET_MAX_CLIENTS * 2) + (RDMNET_MAX_CLIENT_SCOPES * 2))

#if RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS
#define CB_STORAGE_CLASS
#else
#define CB_STORAGE_CLASS static
#endif

/***************************** Private macros ********************************/

// Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool for
// client tracking information, and via a static buffer for RDM responses.
#if RDMNET_DYNAMIC_MEM

#define ALLOC_RDMNET_CLIENT() malloc(sizeof(RdmnetClient))
#define ALLOC_CLIENT_SCOPE() malloc(sizeof(ClientScopeListEntry))
#define FREE_RDMNET_CLIENT(ptr) free(ptr)
#define FREE_CLIENT_SCOPE(ptr) free(ptr)

#define ALLOC_CLIENT_RDM_RESPONSE_ARRAY(num_responses) calloc((num_responses), sizeof(RdmResponse))
#define FREE_CLIENT_RDM_RESPONSE_ARRAY(ptr) free(ptr)

#else

#define ALLOC_RDMNET_CLIENT() etcpal_mempool_alloc(rdmnet_clients)
#define ALLOC_CLIENT_SCOPE() etcpal_mempool_alloc(client_scopes)
#define FREE_RDMNET_CLIENT(ptr) etcpal_mempool_free(rdmnet_clients, ptr)
#define FREE_CLIENT_SCOPE(ptr) etcpal_mempool_free(client_scopes, ptr)

#define ALLOC_CLIENT_RDM_RESPONSE_ARRAY(num_responses) \
  (assert(num_responses <= RDMNET_MAX_RECEIVED_ACK_OVERFLOW_RESPONSES), client_rdm_response_buf)
#define FREE_CLIENT_RDM_RESPONSE_ARRAY(ptr)

#endif

#define INIT_CALLBACK_INFO(cbptr) ((cbptr)->which = kClientCallbackNone)

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(rdmnet_clients, RdmnetClient, RDMNET_MAX_CLIENTS);
ETCPAL_MEMPOOL_DEFINE(client_scopes, ClientScopeListEntry, RDMNET_MAX_CLIENT_SCOPES);
ETCPAL_MEMPOOL_DEFINE(client_rb_nodes, EtcPalRbNode, MAX_CLIENT_RB_NODES);

static RdmResponse client_rdm_response_buf[RDMNET_MAX_RECEIVED_ACK_OVERFLOW_RESPONSES];
#endif

static struct RdmnetClientState
{
  EtcPalRbTree clients;
  EtcPalRbTree clients_by_llrp_handle;

  EtcPalRbTree scopes_by_handle;
  EtcPalRbTree scopes_by_disc_handle;

  IntHandleManager handle_mgr;
} state;

static void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info,
                                   void* context);
static void monitorcb_broker_lost(rdmnet_scope_monitor_t handle, const char* scope, const char* service_name,
                                  void* context);
static void monitorcb_scope_monitor_error(rdmnet_scope_monitor_t handle, const char* scope, int platform_error,
                                          void* context);

// clang-format off
static const RdmnetScopeMonitorCallbacks disc_callbacks =
{
  monitorcb_broker_found,
  monitorcb_broker_lost,
  monitorcb_scope_monitor_error
};
// clang-format on

static void conncb_connected(rdmnet_conn_t handle, const RdmnetConnectedInfo* connect_info, void* context);
static void conncb_connect_failed(rdmnet_conn_t handle, const RdmnetConnectFailedInfo* failed_info, void* context);
static void conncb_disconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo* disconn_info, void* context);
static void conncb_msg_received(rdmnet_conn_t handle, const RdmnetMessage* message, void* context);

// clang-format off
static const RdmnetConnCallbacks conn_callbacks =
{
  conncb_connected,
  conncb_connect_failed,
  conncb_disconnected,
  conncb_msg_received
};
// clang-format on

static void llrpcb_rdm_cmd_received(llrp_target_t handle, const LlrpRemoteRdmCommand* cmd, void* context);

// clang-format off
static const LlrpTargetCallbacks llrp_callbacks =
{
  llrpcb_rdm_cmd_received
};
// clang-format on

/*********************** Private function prototypes *************************/

// Create and destroy clients and scopes
static etcpal_error_t validate_rpt_client_config(const RdmnetRptClientConfig* config);
static etcpal_error_t new_rpt_client(const RdmnetRptClientConfig* config, rdmnet_client_t* handle);
static void destroy_client(RdmnetClient* cli, rdmnet_disconnect_reason_t reason);
static etcpal_error_t create_llrp_handle_for_client(const RdmnetRptClientConfig* config, RdmnetClient* cli);
static bool client_handle_in_use(int handle_val);
static etcpal_error_t create_and_append_scope_entry(const RdmnetScopeConfig* config, RdmnetClient* client,
                                                    ClientScopeListEntry** new_entry);
static ClientScopeListEntry* find_scope_in_list(ClientScopeListEntry* list, const char* scope);
static void remove_scope_from_list(ClientScopeListEntry** list, ClientScopeListEntry* entry);

static etcpal_error_t start_scope_discovery(ClientScopeListEntry* scope_entry, const char* search_domain);
static void attempt_connection_on_listen_addrs(ClientScopeListEntry* scope_entry);
static etcpal_error_t start_connection_for_scope(ClientScopeListEntry* scope_entry, const EtcPalSockAddr* broker_addr);

// Find clients and scopes
static etcpal_error_t get_client(rdmnet_client_t handle, RdmnetClient** client);
static RdmnetClient* get_client_by_llrp_handle(llrp_target_t handle);
static void release_client(const RdmnetClient* client);
static ClientScopeListEntry* get_scope(rdmnet_client_scope_t handle);
static ClientScopeListEntry* get_scope_by_disc_handle(rdmnet_scope_monitor_t handle);
static void release_scope(const ClientScopeListEntry* scope_entry);
static etcpal_error_t get_client_and_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                           RdmnetClient** client, ClientScopeListEntry** scope_entry);
static void release_client_and_scope(const RdmnetClient* client, const ClientScopeListEntry* scope_entry);

// Manage callbacks
static void fill_callback_info(const RdmnetClient* client, ClientCallbackDispatchInfo* cb);
static void deliver_callback(ClientCallbackDispatchInfo* info);
static bool connect_failed_will_retry(rdmnet_connect_fail_event_t event, rdmnet_connect_status_t status);
static bool disconnected_will_retry(rdmnet_disconnect_event_t event, rdmnet_disconnect_reason_t reason);

// Message handling
static void free_rpt_client_message(RptClientMessage* msg);
static void free_ept_client_message(EptClientMessage* msg);
static bool handle_rpt_request(const RptMessage* rmsg, RptClientMessage* msg_out);
static bool handle_rpt_notification(const RptMessage* rmsg, RptClientMessage* msg_out);
static bool handle_rpt_status(const RptMessage* rmsg, RptClientMessage* msg_out);
static bool handle_rpt_message(const RdmnetClient* cli, const ClientScopeListEntry* scope_entry, const RptMessage* rmsg,
                               RptMsgReceivedArgs* cb_args);

// Red-black tree management
static int client_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int client_llrp_handle_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int scope_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int scope_disc_handle_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static EtcPalRbNode* client_node_alloc(void);
static void client_node_free(EtcPalRbNode* node);

/*************************** Function definitions ****************************/

etcpal_error_t rdmnet_client_init(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !RDMNET_DYNAMIC_MEM
  res |= etcpal_mempool_init(rdmnet_clients);
  res |= etcpal_mempool_init(client_scopes);
  res |= etcpal_mempool_init(client_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&state.clients, client_compare, client_node_alloc, client_node_free);
    etcpal_rbtree_init(&state.clients_by_llrp_handle, client_llrp_handle_compare, client_node_alloc, client_node_free);

    etcpal_rbtree_init(&state.scopes_by_handle, scope_compare, client_node_alloc, client_node_free);
    etcpal_rbtree_init(&state.scopes_by_disc_handle, scope_disc_handle_compare, client_node_alloc, client_node_free);

    init_int_handle_manager(&state.handle_mgr, client_handle_in_use);
  }
  return res;
}

static void client_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  RdmnetClient* cli = (RdmnetClient*)node->value;
  if (cli)
    destroy_client(cli, kRdmnetDisconnectShutdown);
  client_node_free(node);
}

void rdmnet_client_deinit(void)
{
  etcpal_rbtree_clear_with_cb(&state.clients, client_dealloc);
}

/*!
 * \brief Initialize an RPT Client Config struct to default values.
 *
 * The config struct members not marked 'optional' are initialized to invalid values by this
 * function. Those members must be set manually with meaningful data before passing the config
 * struct to an API function.
 *
 * Usage example:
 * \code
 * RdmnetRptClientConfig config;
 * rdmnet_rpt_client_config_init(&config, MY_ESTA_MANUFACTURER_ID_VAL);
 * // Now fill in the required values...
 * \endcode
 *
 * \param[out] config RdmnetRptClientConfig to initialize.
 * \param[in] manu_id ESTA manufacturer ID. All RDMnet RPT components must have one.
 */
void rdmnet_rpt_client_config_init(RdmnetRptClientConfig* config, uint16_t manufacturer_id)
{
  if (config)
  {
    memset(config, 0, sizeof(RdmnetRptClientConfig));
    RDMNET_INIT_DYNAMIC_UID_REQUEST(&config->uid, manufacturer_id);
    config->search_domain = E133_DEFAULT_DOMAIN;
  }
}

/*!
 * \brief Initialize an EPT Client Config struct to default values.
 *
 * The config struct members not marked 'optional' are initialized to invalid values by this
 * function. Those members must be set manually with meaningful data before passing the config
 * struct to an API function.
 *
 * Usage example:
 * \code
 * RdmnetEptClientConfig config;
 * rdmnet_ept_client_config_init(&config);
 * // Now fill in the required values...
 * \endcode
 *
 * \param[out] config RdmnetEptClientConfig to initialize.
 */
void rdmnet_ept_client_config_init(RdmnetEptClientConfig* config)
{
  if (config)
  {
    memset(config, 0, sizeof(RdmnetEptClientConfig));
  }
}

/*!
 * \brief Create a new RPT client from the given configuration.
 *
 * The RPT client will be created with no scopes; nothing will happen until you add a scope using
 * rdmnet_client_add_scope().
 *
 * \param[in] config Configuration parameters for the RPT client to be created.
 * \param[out] handle Filled in on success with a handle to the RPT client.
 * \return #kEtcPalErrOk: RPT Client created successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No memory to allocate new client instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_rpt_client_create(const RdmnetRptClientConfig* config, rdmnet_client_t* handle)
{
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
  //  if (!config || !handle)
  //    return kEtcPalErrInvalid;
  //  if (!rdmnet_core_initialized())
  //    return kEtcPalErrNotInit;
  //
  //  etcpal_error_t res = validate_rpt_client_config(config);
  //  if (res != kEtcPalErrOk)
  //    return res;
  //
  //  if (RDMNET_CLIENT_LOCK())
  //  {
  //    res = new_rpt_client(config, handle);
  //    RDMNET_CLIENT_UNLOCK();
  //  }
  //  else
  //  {
  //    res = kEtcPalErrSys;
  //  }
  //
  //  return res;
}

/*!
 * \brief Destroy an RDMnet client instance.
 *
 * Will disconnect from all brokers to which this client is currently connected, sending the
 * disconnect reason provided in the disconnect_reason parameter.
 *
 * \param[in] handle Handle to client to destroy, no longer valid after this function returns.
 * \param[in] disconnect_reason Disconnect reason code to send on all connected scopes.
 * \return #kEtcPalErrOk: Client destroyed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_destroy(rdmnet_client_t handle, rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
  //  if (!rdmnet_core_initialized())
  //    return kEtcPalErrNotInit;
  //
  //  RdmnetClient* cli;
  //  etcpal_error_t res = get_client(handle, &cli);
  //  if (res != kEtcPalErrOk)
  //    return res;
  //
  //  etcpal_rbtree_remove(&state.clients, cli);
  //  destroy_client(cli, reason);
  //  RDMNET_CLIENT_UNLOCK();
  //  return res;
}

/*!
 * \brief Add a new scope to a client instance.
 *
 * The library will attempt to discover and connect to a broker for the scope (or just connect if a
 * static broker address is given); the status of these attempts will be communicated via the
 * callbacks associated with the client instance.
 *
 * \param[in] handle Handle to client to which to add a new scope.
 * \param[in] scope_config Configuration parameters for the new scope.
 * \param[out] scope_handle Filled in on success with a handle to the new scope.
 * \return #kEtcPalErrOk: New scope added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance.
 * \return #kEtcPalErrNoMem: No memory to allocate new scope.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_add_scope(rdmnet_client_t handle, const RdmnetScopeConfig* scope_config,
                                       rdmnet_client_scope_t* scope_handle)
{
  if (handle < 0 || !scope_config || !scope_config->scope || !scope_handle)
    return kEtcPalErrInvalid;

  RdmnetClient* cli;
  etcpal_error_t res = get_client(handle, &cli);
  if (res != kEtcPalErrOk)
    return res;

  ClientScopeListEntry* new_entry;
  res = create_and_append_scope_entry(scope_config, cli, &new_entry);
  if (res == kEtcPalErrOk)
  {
    *scope_handle = new_entry->handle;

    // Start discovery or connection on the new scope (depending on whether a static broker was
    // configured)
    if (new_entry->state == kScopeStateDiscovery)
      res = start_scope_discovery(new_entry, cli->search_domain);
    else if (new_entry->state == kScopeStateConnecting)
      res = start_connection_for_scope(new_entry, &new_entry->static_broker_addr);

    if (res != kEtcPalErrOk)
    {
      rdmnet_connection_destroy(new_entry->handle, NULL);
      remove_scope_from_list(&cli->scope_list, new_entry);
      etcpal_rbtree_remove(&state.scopes_by_handle, new_entry);
      FREE_CLIENT_SCOPE(new_entry);
    }
  }

  release_client(cli);
  return res;
}

/*!
 * \brief Remove a previously-added scope from a client instance.
 *
 * After this call completes, scope_handle will no longer be valid.
 *
 * \param[in] handle Handle to the client from which to remove a scope.
 * \param[in] scope_handle Handle to scope to remove.
 * \param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * \return #kEtcPalErrOk: Scope removed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_remove_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                          rdmnet_disconnect_reason_t reason)
{
  if (handle < 0 || scope_handle < 0)
    return kEtcPalErrInvalid;

  RdmnetClient* cli;
  ClientScopeListEntry* scope_entry;
  etcpal_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  if (res != kEtcPalErrOk)
    return res;

  if (scope_entry->monitor_handle)
  {
    rdmnet_disc_stop_monitoring(scope_entry->monitor_handle);
    etcpal_rbtree_remove(&state.scopes_by_disc_handle, scope_entry);
  }
  rdmnet_connection_destroy(scope_entry->handle, &reason);
  remove_scope_from_list(&cli->scope_list, scope_entry);
  etcpal_rbtree_remove(&state.scopes_by_handle, scope_entry);
  FREE_CLIENT_SCOPE(scope_entry);

  release_client(cli);
  return res;
}

/*!
 * \brief Retrieve the scope string of a previously-added scope.
 *
 * \param[in] handle Handle to the client from which to retrieve the scope string.
 * \param[in] scope_handle Handle to scope for which to retrieve the scope string.
 * \param[out] scope_str_buf Filled in on success with the scope string. Must be at least of length
 *                           #E133_SCOPE_STRING_PADDED_LENGTH.
 * \return #kEtcPalErrOk: Scope string retrieved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_get_scope_string(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                              char* scope_str_buf)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(scope_str_buf);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Retrieve the static broker configuration of a previously-added scope.
 *
 * \param[in] handle Handle to the client from which to retrieve the static broker configuration.
 * \param[in] scope_handle Handle to scope for which to retrieve the static broker configuration.
 * \param[out] has_static_broker_addr Filled in on success with whether the scope has a static
 *                                    broker address.
 * \param[out] has_static_broker_addr Filled in on success with the static broker address, if
 *                                    present.
 * \return #kEtcPalErrOk: Scope string retrieved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_get_static_broker_config(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                      bool* has_static_broker_addr, EtcPalSockAddr* static_broker_addr)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(has_static_broker_addr);
  ETCPAL_UNUSED_ARG(static_broker_addr);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Change the settings of a previously-added scope.
 *
 * Changed settings will cause the client to disconnect from any connected broker for the old
 * scope.
 *
 * \param[in] handle Handle to the client for which to change a scope.
 * \param[in] scope_handle Handle to scope to change.
 * \param[in] new_scope_config New configuration settings to use for the scope.
 * \param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * \return #kEtcPalErrOk: Scope changed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_change_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                          const RdmnetScopeConfig* new_scope_config,
                                          rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(new_scope_config);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Change the search domain setting of a client.
 *
 * A changed domain will cause the client to disconnect from any connected broker for which dynamic
 * discovery is configured and restart the discovery process.
 *
 * \param[in] handle Handle to the client for which to change the search domain.
 * \param[in] new_search_domain New search domain to use for DNS discovery.
 * \param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * \return #kEtcPalErrOk: Scope changed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_change_search_domain(rdmnet_client_t handle, const char* new_search_domain,
                                                  rdmnet_disconnect_reason_t reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_search_domain);
  ETCPAL_UNUSED_ARG(reason);
  // TODO
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send a message requesting an RDMnet client list from a broker on a given scope.
 *
 * The response will be delivered via an RdmnetClientBrokerMsgReceivedCb containing a ClientList
 * broker message.
 *
 * \param[in] handle Handle to the client from which to request a client list.
 * \param[in] scope_handle Scope handle on which to request a client list.
 * \return #kEtcPalErrOk: Client list requested successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_request_client_list(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle)
{
  if (handle < 0 || scope_handle < 0)
    return kEtcPalErrInvalid;

  RdmnetClient* cli;
  ClientScopeListEntry* scope_entry;
  etcpal_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  if (res != kEtcPalErrOk)
    return res;

  res = broker_send_fetch_client_list(scope_handle, &cli->cid);

  release_client_and_scope(cli, scope_entry);
  return res;
}

/*!
 * \brief Send a message requesting one or more dynamic UIDs from a broker on a given scope.
 *
 * The response will be delivered via an RdmnetClientBrokerMsgReceivedCb containing a
 * BrokerDynamicUidAssignmentList broker message.
 *
 * \param[in] handle Handle to the client from which to request dynamic UIDs.
 * \param[in] scope_handle Scope handle on which to request dynamic UIDs.
 * \param[in] requests Array of Dynamic UID Request structures representing requested dynamic UIDs.
 * \param[in] num_requests Size of requests array.
 * \return #kEtcPalErrOk: Dynamic UIDs requested successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_request_dynamic_uids(rdmnet_conn_t handle, rdmnet_client_scope_t scope_handle,
                                                  const BrokerDynamicUidRequest* requests, size_t num_requests)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(requests);
  ETCPAL_UNUSED_ARG(num_requests);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send a message requesting the mapping of one or more dynamic UIDs to RIDs from a broker
 *        on a given scope.
 *
 * The response will be delivered via an RdmnetClientBrokerMsgReceivedCb containing a
 * BrokerDynamicUidAssignmentList broker message.
 *
 * \param[in] handle Handle to the client from which to request dynamic UID mappings.
 * \param[in] scope_handle Scope handle on which to request dynamic UID mappings.
 * \param[in] uids Array of UIDs for which to request the mapped RIDs.
 * \param[in] num_uids Size of uids array.
 * \return #kEtcPalErrOk: Dynamic UID mappings requested successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_client_request_dynamic_uid_mappings(rdmnet_conn_t handle, rdmnet_client_scope_t scope_handle,
                                                          const RdmUid* uids, size_t num_uids)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(uids);
  ETCPAL_UNUSED_ARG(num_uids);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send an RDM command from an RPT client on a scope.
 *
 * The response will be delivered via an RptClientMsgReceivedCb containing an
 * RdmnetRemoteRdmResponse.
 *
 * \param[in] handle Handle to the client from which to send the RDM command.
 * \param[in] scope_handle Handle to the scope on which to send the RDM command.
 * \param[in] cmd The RDM command data to send, including its addressing information.
 * \param[out] seq_num Filled in on success with a sequence number which can be used to match the
 *                     command with a response.
 * \return #kEtcPalErrOk: Command sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_rpt_client_send_rdm_command(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetLocalRdmCommand* cmd, uint32_t* seq_num)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(cmd);
  ETCPAL_UNUSED_ARG(seq_num);
  return kEtcPalErrNotImpl;
  //  if (handle < 0 || scope_handle < 0 || !cmd)
  //    return kEtcPalErrInvalid;
  //
  //  RdmnetClient* cli;
  //  ClientScopeListEntry* scope_entry;
  //  etcpal_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  //  if (res != kEtcPalErrOk)
  //    return res;
  //
  //  RptHeader header;
  //  header.source_uid = scope_entry->uid;
  //  header.source_endpoint_id = E133_NULL_ENDPOINT;
  //  header.dest_uid = cmd->rdmnet_dest_uid;
  //  header.dest_endpoint_id = cmd->dest_endpoint;
  //  header.seqnum = scope_entry->send_seq_num++;
  //
  //  RdmCommand rdm_to_send;
  //  rdm_to_send.source_uid = scope_entry->uid;
  //  rdm_to_send.dest_uid = cmd->rdm_dest_uid;
  //  rdm_to_send.port_id = 1;
  //  rdm_to_send.transaction_num = (uint8_t)(header.seqnum & 0xffu);
  //  rdm_to_send.subdevice = cmd->subdevice;
  //  rdm_to_send.command_class = cmd->command_class;
  //  rdm_to_send.param_id = cmd->param_id;
  //  rdm_to_send.data_len = cmd->data_len;
  //  memcpy(rdm_to_send.data, cmd->data, rdm_to_send.data_len);
  //
  //  RdmBuffer buf_to_send;
  //  res = rdmctl_pack_command(&rdm_to_send, &buf_to_send);
  //  if (res == kEtcPalErrOk)
  //  {
  //    res = rpt_send_request(scope_handle, &cli->cid, &header, &buf_to_send);
  //    if (res == kEtcPalErrOk && seq_num)
  //      *seq_num = header.seqnum;
  //  }
  //
  //  release_client_and_scope(cli, scope_entry);
  //  return res;
}

/*!
 * \brief Send an RDM ACK response from an RPT client.
 *
 * \param[in] handle Handle to the device from which to send the RDM ACK response.
 * \param[in] scope_handle Handle to the scope from which to send the RDM ACK response.
 * \param[in] received_cmd Previously-received command that the ACK is a response to.
 * \param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * \param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * \return #kEtcPalErrOk: ACK sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_rpt_client_send_rdm_ack(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetRemoteRdmCommand* received_cmd, const uint8_t* response_data,
                                              size_t response_data_len)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(response_data);
  ETCPAL_UNUSED_ARG(response_data_len);
  return kEtcPalErrNotImpl;
  //  if (handle < 0 || scope_handle < 0 || !resp)
  //    return kEtcPalErrInvalid;
  //
  //  RdmnetClient* cli;
  //  ClientScopeListEntry* scope_entry;
  //  etcpal_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  //  if (res != kEtcPalErrOk)
  //    return res;
  //
  //  size_t resp_buf_size = (resp->original_command_included ? resp->num_responses + 1 : resp->num_responses);
  //#if RDMNET_DYNAMIC_MEM
  //  RdmBuffer* resp_buf = (RdmBuffer*)calloc(resp_buf_size, sizeof(RdmBuffer));
  //  if (!resp_buf)
  //  {
  //    release_client_and_scope(cli, scope_entry);
  //    return kEtcPalErrNoMem;
  //  }
  //#else
  //  static RdmBuffer resp_buf[RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES + 1];
  //  if (resp->num_responses > RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES)
  //  {
  //    release_client_and_scope(cli, scope_entry);
  //    return kEtcPalErrMsgSize;
  //  }
  //#endif
  //
  //  RptHeader header;
  //  header.source_uid = scope_entry->uid;
  //  header.source_endpoint_id = resp->source_endpoint;
  //  header.dest_uid = resp->rdmnet_dest_uid;
  //  header.dest_endpoint_id = E133_NULL_ENDPOINT;
  //  header.seqnum = resp->seq_num;
  //
  //  if (resp->original_command_included)
  //  {
  //    res = rdmctl_pack_command(&resp->original_command, &resp_buf[0]);
  //  }
  //  if (res == kEtcPalErrOk)
  //  {
  //    for (size_t i = 0; i < resp->num_responses; ++i)
  //    {
  //      size_t out_buf_offset = resp->original_command_included ? i + 1 : i;
  //      RdmResponse resp_data = resp->responses[i];
  //      if (resp->source_endpoint == E133_NULL_ENDPOINT)
  //        resp_data.source_uid = scope_entry->uid;
  //      res = rdmresp_pack_response(&resp_data, &resp_buf[out_buf_offset]);
  //      if (res != kEtcPalErrOk)
  //        break;
  //    }
  //  }
  //  if (res == kEtcPalErrOk)
  //  {
  //    res = rpt_send_notification(scope_handle, &cli->cid, &header, resp_buf, resp_buf_size);
  //  }
  //
  //#if RDMNET_DYNAMIC_MEM
  //  free(resp_buf);
  //#endif
  //  release_client_and_scope(cli, scope_entry);
  //  return res;
}

etcpal_error_t rdmnet_rpt_client_send_rdm_nack(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                               const RdmnetRemoteRdmCommand* received_cmd,
                                               rdm_nack_reason_t nack_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(nack_reason);
  return kEtcPalErrNotImpl;
}

etcpal_error_t rdmnet_rpt_client_send_unsolicited_response(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                           const RdmnetUnsolicitedRdmResponse* response)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(response);
  return kEtcPalErrNotImpl;
}

etcpal_error_t rdmnet_rpt_client_send_status(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                             const RdmnetRemoteRdmCommand* received_cmd, rpt_status_code_t status_code,
                                             const char* status_string)
{
  //  if (handle < 0 || scope_handle < 0 || !status)
  //    return kEtcPalErrInvalid;
  //
  //  RdmnetClient* cli;
  //  ClientScopeListEntry* scope_entry;
  //  etcpal_error_t res = get_client_and_scope(handle, scope_handle, &cli, &scope_entry);
  //  if (res != kEtcPalErrOk)
  //    return res;
  //
  //  RptHeader header;
  //  header.source_uid = scope_entry->uid;
  //  header.source_endpoint_id = status->source_endpoint;
  //  header.dest_uid = status->rdmnet_dest_uid;
  //  header.dest_endpoint_id = E133_NULL_ENDPOINT;
  //  header.seqnum = status->seq_num;
  //
  //  res = rpt_send_status(scope_handle, &cli->cid, &header, &status->msg);
  //
  //  release_client_and_scope(cli, scope_entry);
  //  return res;
}

etcpal_error_t rdmnet_rpt_client_send_llrp_ack(rdmnet_client_t handle, const LlrpRemoteRdmCommand* received_cmd,
                                               const uint8_t* response_data, uint8_t response_data_len)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(response_data);
  ETCPAL_UNUSED_ARG(response_data_len);
  return kEtcPalErrNotImpl;
  //  if (handle < 0 || !resp)
  //    return kEtcPalErrInvalid;
  //
  //  RdmnetClient* cli;
  //  etcpal_error_t res = get_client(handle, &cli);
  //  if (res != kEtcPalErrOk)
  //    return res;
  //
  //  res = llrp_target_send_rdm_response(cli->llrp_handle, resp);
  //
  //  release_client(cli);
  //  return res;
}

etcpal_error_t rdmnet_rpt_client_send_llrp_nack(rdmnet_client_t handle, const LlrpLocalRdmCommand* received_cmd,
                                                rdm_nack_reason_t nack_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(nack_reason);
  return kEtcPalErrNotImpl;
}

etcpal_error_t rdmnet_ept_client_send_data(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                           const EtcPalUuid* dest_cid, const EptDataMsg* data)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(dest_cid);
  ETCPAL_UNUSED_ARG(data);
  return kEtcPalErrNotImpl;
}

etcpal_error_t rdmnet_ept_client_send_status(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                             const EtcPalUuid* dest_cid, ept_status_code_t status_code,
                                             const char* status_string)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(dest_cid);
  ETCPAL_UNUSED_ARG(status_code);
  ETCPAL_UNUSED_ARG(status_string);
  return kEtcPalErrNotImpl;
}

// Callback functions from the discovery interface

void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  RDMNET_LOG_INFO("Broker '%s' for scope '%s' discovered.", broker_info->service_name, broker_info->scope);

  ClientScopeListEntry* scope_entry = get_scope_by_disc_handle(handle);
  if (scope_entry && !scope_entry->broker_found)
  {
    scope_entry->broker_found = true;
    scope_entry->listen_addrs = broker_info->listen_addrs;
    scope_entry->num_listen_addrs = broker_info->num_listen_addrs;
    scope_entry->current_listen_addr = 0;
    scope_entry->port = broker_info->port;

    attempt_connection_on_listen_addrs(scope_entry);

    release_scope(scope_entry);
  }
}

void monitorcb_broker_lost(rdmnet_scope_monitor_t handle, const char* scope, const char* service_name, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  ClientScopeListEntry* scope_entry = get_scope_by_disc_handle(handle);
  if (scope_entry)
  {
    scope_entry->broker_found = false;
    scope_entry->listen_addrs = NULL;
    scope_entry->num_listen_addrs = 0;
    scope_entry->current_listen_addr = 0;
    scope_entry->port = 0;

    release_scope(scope_entry);
  }
  RDMNET_LOG_INFO("Broker '%s' no longer discovered on scope '%s'", service_name, scope);
}

void monitorcb_scope_monitor_error(rdmnet_scope_monitor_t handle, const char* scope, int platform_error, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope);
  ETCPAL_UNUSED_ARG(platform_error);
  ETCPAL_UNUSED_ARG(context);

  // TODO
}

void conncb_connected(rdmnet_conn_t handle, const RdmnetConnectedInfo* connect_info, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  ClientScopeListEntry* scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient* cli = scope_entry->client;

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

void conncb_connect_failed(rdmnet_conn_t handle, const RdmnetConnectFailedInfo* failed_info, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  ClientScopeListEntry* scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient* cli = scope_entry->client;

    scope_entry->state = kScopeStateDiscovery;

    RdmnetClientConnectFailedInfo info;
    info.event = failed_info->event;
    info.socket_err = failed_info->socket_err;
    info.rdmnet_reason = failed_info->rdmnet_reason;
    info.will_retry = connect_failed_will_retry(info.event, info.rdmnet_reason);

    if (info.will_retry)
    {
      if (scope_entry->monitor_handle)
      {
        if (scope_entry->broker_found)
        {
          // Attempt to connect on the next listen address.
          if (++scope_entry->current_listen_addr == scope_entry->num_listen_addrs)
            scope_entry->current_listen_addr = 0;
          attempt_connection_on_listen_addrs(scope_entry);
        }
      }
      else
      {
        if (kEtcPalErrOk != start_connection_for_scope(scope_entry, &scope_entry->static_broker_addr))
        {
          // Some fatal error while attempting to connect to the statically-configured address.
          info.will_retry = false;
        }
      }
    }

    fill_callback_info(cli, &cb);
    cb.which = kClientCallbackConnectFailed;
    cb.common_args.connect_failed.scope_handle = handle;
    cb.common_args.connect_failed.info = info;

    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

void conncb_disconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo* disconn_info, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  ClientScopeListEntry* scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient* cli = scope_entry->client;

    RdmnetClientDisconnectedInfo info;
    info.event = disconn_info->event;
    info.socket_err = disconn_info->socket_err;
    info.rdmnet_reason = disconn_info->rdmnet_reason;
    info.will_retry = disconnected_will_retry(info.event, info.rdmnet_reason);

    if (info.will_retry)
    {
      // Retry connection on the scope.
      scope_entry->state = kScopeStateConnecting;
      if (scope_entry->monitor_handle)
      {
        if (scope_entry->broker_found)
        {
          // Attempt to connect to the Broker on its reported listen addresses.
          attempt_connection_on_listen_addrs(scope_entry);
        }
      }
      else
      {
        if (kEtcPalErrOk != start_connection_for_scope(scope_entry, &scope_entry->static_broker_addr))
        {
          // Some fatal error while attempting to connect to the statically-configured address.
          info.will_retry = false;
        }
      }
    }

    fill_callback_info(cli, &cb);
    cb.which = kClientCallbackDisconnected;
    cb.common_args.disconnected.scope_handle = handle;
    cb.common_args.disconnected.info = info;

    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

void conncb_msg_received(rdmnet_conn_t handle, const RdmnetMessage* message, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  ClientScopeListEntry* scope_entry = get_scope(handle);
  if (scope_entry)
  {
    RdmnetClient* cli = scope_entry->client;

    fill_callback_info(cli, &cb);

    switch (message->vector)
    {
      case ACN_VECTOR_ROOT_BROKER:
        cb.which = kClientCallbackBrokerMsgReceived;
        cb.common_args.broker_msg_received.scope_handle = handle;
        cb.common_args.broker_msg_received.msg = RDMNET_GET_BROKER_MSG(message);
        break;
      case ACN_VECTOR_ROOT_RPT:
        if (cli->type == kClientProtocolRPT)
        {
          if (handle_rpt_message(cli, scope_entry, RDMNET_GET_RPT_MSG(message), &cb.prot_info.rpt.args.msg_received))
          {
            cb.which = kClientCallbackMsgReceived;
          }
        }
        else
        {
          RDMNET_LOG_WARNING("Incorrectly got RPT message for non-RPT client %d on scope %d", cli->handle, handle);
        }
        break;
      case ACN_VECTOR_ROOT_EPT:
        // TODO, for now fall through
      default:
        RDMNET_LOG_WARNING("Got message with unhandled vector type %" PRIu32 " on scope %d", message->vector, handle);
        break;
    }
    release_scope(scope_entry);
  }

  deliver_callback(&cb);
}

bool handle_rpt_message(const RdmnetClient* cli, const ClientScopeListEntry* scope_entry, const RptMessage* rmsg,
                        RptMsgReceivedArgs* cb_args)
{
  ETCPAL_UNUSED_ARG(cli);
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

bool handle_rpt_request(const RptMessage* rmsg, RptClientMessage* msg_out)
{
  ETCPAL_UNUSED_ARG(rmsg);
  ETCPAL_UNUSED_ARG(msg_out);
  //  RdmnetRemoteRdmCommand* cmd = RDMNET_GET_REMOTE_RDM_COMMAND(msg_out);
  //  const RptRdmBufList* list = RPT_GET_RDM_BUF_LIST(rmsg);
  //
  //  if (list->num_rdm_buffers == 1)  // Only one RDM command allowed in an RPT request
  //  {
  //    etcpal_error_t unpack_res = rdmresp_unpack_command(list->rdm_buffers, &cmd->rdm_command);
  //    if (unpack_res == kEtcPalErrOk)
  //    {
  //      msg_out->type = kRptClientMsgRdmCmd;
  //      cmd->source_uid = rmsg->header.source_uid;
  //      cmd->dest_endpoint = rmsg->header.dest_endpoint_id;
  //      cmd->seq_num = rmsg->header.seqnum;
  //      return true;
  //    }
  //  }
  return false;
}

bool handle_rpt_notification(const RptMessage* rmsg, RptClientMessage* msg_out)
{
  ETCPAL_UNUSED_ARG(rmsg);
  ETCPAL_UNUSED_ARG(msg_out);
  return false;
  //  RdmnetRemoteRdmResponse* resp = RDMNET_GET_REMOTE_RDM_RESPONSE(msg_out);
  //
  //  // Do some initialization
  //  msg_out->type = kRptClientMsgRdmResp;
  //  resp->command_included = false;
  //  resp->more_coming = RPT_GET_RDM_BUF_LIST(rmsg)->more_coming;
  //
  //  const RptRdmBufList* list = RPT_GET_RDM_BUF_LIST(rmsg);
  //  resp->responses = ALLOC_CLIENT_RDM_RESPONSE_ARRAY(list->num_rdm_buffers);
  //  if (!resp->responses)
  //    return false;
  //
  //  bool good_parse = true;
  //  bool first_msg = true;
  //  for (size_t i = 0; i < list->num_rdm_buffers; ++i)
  //  {
  //    const RdmBuffer* buffer = &list->rdm_buffers[i];
  //    if (first_msg)
  //    {
  //      if (rdmresp_is_non_disc_command(buffer))
  //      {
  //        // The command is included.
  //        etcpal_error_t unpack_res = rdmresp_unpack_command(buffer, &resp->cmd);
  //        if (unpack_res == kEtcPalErrOk)
  //        {
  //          resp->command_included = true;
  //        }
  //        else
  //        {
  //          good_parse = false;
  //        }
  //        continue;
  //      }
  //      first_msg = false;
  //    }
  //
  //    etcpal_error_t unpack_res = rdmctl_unpack_response(buffer, &resp->responses[i]);
  //    if (unpack_res != kEtcPalErrOk)
  //      good_parse = false;
  //  }
  //
  //  if (good_parse)
  //  {
  //    // Fill in the rest of the info
  //    resp->rdmnet_source_uid = rmsg->header.source_uid;
  //    resp->source_endpoint = rmsg->header.source_endpoint_id;
  //    resp->seq_num = rmsg->header.seqnum;
  //    return true;
  //  }
  //  else
  //  {
  //    // Clean up
  //    free_rpt_client_message(msg_out);
  //    return false;
  //  }
}

bool handle_rpt_status(const RptMessage* rmsg, RptClientMessage* msg_out)
{
  ETCPAL_UNUSED_ARG(rmsg);
  ETCPAL_UNUSED_ARG(msg_out);
  return false;
  //  RdmnetRemoteRptStatus* status_out = RDMNET_GET_REMOTE_RPT_STATUS(msg_out);
  //  const RptStatusMsg* status = RPT_GET_STATUS_MSG(rmsg);
  //
  //  // This one is quick and simple with no failure condition
  //  msg_out->type = kRptClientMsgStatus;
  //  status_out->rdmnet_source_uid = rmsg->header.source_uid;
  //  status_out->source_endpoint = rmsg->header.source_endpoint_id;
  //  status_out->seq_num = rmsg->header.seqnum;
  //  status_out->msg = *status;
  //  return true;
}

void free_rpt_client_message(RptClientMessage* msg)
{
  ETCPAL_UNUSED_ARG(msg);
  //  if (msg->type == kRptClientMsgRdmResp)
  //  {
  //    FREE_CLIENT_RDM_RESPONSE_ARRAY(RDMNET_GET_REMOTE_RDM_RESPONSE(msg)->responses);
  //  }
}

void free_ept_client_message(EptClientMessage* msg)
{
  ETCPAL_UNUSED_ARG(msg);
  // TODO
}

void llrpcb_rdm_cmd_received(llrp_target_t handle, const LlrpRemoteRdmCommand* cmd, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  CB_STORAGE_CLASS ClientCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  RdmnetClient* cli = get_client_by_llrp_handle(handle);
  if (cli)
  {
    // Not much to do here but pass the message through to the client callback.
    fill_callback_info(cli, &cb);
    cb.which = kClientCallbackLlrpMsgReceived;
    cb.prot_info.rpt.args.llrp_msg_received.cmd = cmd;
    release_client(cli);
  }

  deliver_callback(&cb);
}

void fill_callback_info(const RdmnetClient* client, ClientCallbackDispatchInfo* cb)
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

void deliver_callback(ClientCallbackDispatchInfo* info)
{
  ETCPAL_UNUSED_ARG(info);
  //  if (info->type == kClientProtocolRPT)
  //  {
  //    RptCallbackDispatchInfo* rpt_info = &info->prot_info.rpt;
  //    switch (info->which)
  //    {
  //      case kClientCallbackConnected:
  //        if (rpt_info->cbs.connected)
  //        {
  //          rpt_info->cbs.connected(info->handle, info->common_args.connected.scope_handle,
  //                                  &info->common_args.connected.info, info->context);
  //        }
  //        break;
  //      case kClientCallbackConnectFailed:
  //        if (rpt_info->cbs.connect_failed)
  //        {
  //          rpt_info->cbs.connect_failed(info->handle, info->common_args.connect_failed.scope_handle,
  //                                       &info->common_args.connect_failed.info, info->context);
  //        }
  //        break;
  //      case kClientCallbackDisconnected:
  //        if (rpt_info->cbs.disconnected)
  //        {
  //          rpt_info->cbs.disconnected(info->handle, info->common_args.disconnected.scope_handle,
  //                                     &info->common_args.disconnected.info, info->context);
  //        }
  //        break;
  //      case kClientCallbackBrokerMsgReceived:
  //        if (rpt_info->cbs.broker_msg_received)
  //        {
  //          rpt_info->cbs.broker_msg_received(info->handle, info->common_args.broker_msg_received.scope_handle,
  //                                            info->common_args.broker_msg_received.msg, info->context);
  //        }
  //        break;
  //      case kClientCallbackLlrpMsgReceived:
  //        if (rpt_info->cbs.llrp_msg_received)
  //        {
  //          rpt_info->cbs.llrp_msg_received(info->handle, rpt_info->args.llrp_msg_received.cmd, info->context);
  //        }
  //      case kClientCallbackMsgReceived:
  //        if (rpt_info->cbs.msg_received)
  //        {
  //          rpt_info->cbs.msg_received(info->handle, rpt_info->args.msg_received.scope_handle,
  //                                     &rpt_info->args.msg_received.msg, info->context);
  //        }
  //        free_rpt_client_message(&rpt_info->args.msg_received.msg);
  //        break;
  //      case kClientCallbackNone:
  //      default:
  //        break;
  //    }
  //  }
  //  else if (info->type == kClientProtocolEPT)
  //  {
  //    EptCallbackDispatchInfo* ept_info = &info->prot_info.ept;
  //    switch (info->which)
  //    {
  //      case kClientCallbackConnected:
  //        if (ept_info->cbs.connected)
  //        {
  //          ept_info->cbs.connected(info->handle, info->common_args.connected.scope_handle,
  //                                  &info->common_args.connected.info, info->context);
  //        }
  //        break;
  //      case kClientCallbackConnectFailed:
  //        if (ept_info->cbs.connect_failed)
  //        {
  //          ept_info->cbs.connect_failed(info->handle, info->common_args.connect_failed.scope_handle,
  //                                       &info->common_args.connect_failed.info, info->context);
  //        }
  //        break;
  //      case kClientCallbackDisconnected:
  //        if (ept_info->cbs.disconnected)
  //        {
  //          ept_info->cbs.disconnected(info->handle, info->common_args.disconnected.scope_handle,
  //                                     &info->common_args.disconnected.info, info->context);
  //        }
  //        break;
  //      case kClientCallbackBrokerMsgReceived:
  //        if (ept_info->cbs.broker_msg_received)
  //        {
  //          ept_info->cbs.broker_msg_received(info->handle, info->common_args.broker_msg_received.scope_handle,
  //                                            info->common_args.broker_msg_received.msg, info->context);
  //        }
  //        break;
  //      case kClientCallbackMsgReceived:
  //        if (ept_info->cbs.msg_received)
  //        {
  //          ept_info->cbs.msg_received(info->handle, ept_info->msg_received.scope_handle, &ept_info->msg_received.msg,
  //                                     info->context);
  //        }
  //        free_ept_client_message(&ept_info->msg_received.msg);
  //        break;
  //      case kClientCallbackNone:
  //      default:
  //        break;
  //    }
  //  }
}

bool connect_failed_will_retry(rdmnet_connect_fail_event_t event, rdmnet_connect_status_t status)
{
  switch (event)
  {
    case kRdmnetConnectFailSocketFailure:
      return false;
    case kRdmnetConnectFailRejected:
      return (status == kRdmnetConnectCapacityExceeded);
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

/* Validate the data in an RdmnetRptClientConfig structure. */
etcpal_error_t validate_rpt_client_config(const RdmnetRptClientConfig* config)
{
  if ((config->type != kRPTClientTypeDevice && config->type != kRPTClientTypeController) ||
      (ETCPAL_UUID_IS_NULL(&config->cid)) ||
      (!RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&config->uid) && (config->uid.manu & 0x8000)) || !config->search_domain)
  {
    return kEtcPalErrInvalid;
  }
  return kEtcPalErrOk;
}

/* Create and initialize a new RdmnetClient structure from a given RPT config. */
etcpal_error_t new_rpt_client(const RdmnetRptClientConfig* config, rdmnet_client_t* handle)
{
  etcpal_error_t res = kEtcPalErrNoMem;

  rdmnet_client_t new_handle = get_next_int_handle(&state.handle_mgr);
  if (new_handle == RDMNET_CLIENT_INVALID)
    return res;

  RdmnetClient* new_cli = (RdmnetClient*)ALLOC_RDMNET_CLIENT();
  if (new_cli)
  {
    res = create_llrp_handle_for_client(config, new_cli);
    if (res == kEtcPalErrOk)
    {
      new_cli->handle = new_handle;
      if (kEtcPalErrOk == etcpal_rbtree_insert(&state.clients, new_cli))
      {
        // Init the client data
        new_cli->type = kClientProtocolRPT;
        new_cli->cid = config->cid;
        new_cli->data.rpt.callbacks = config->callbacks;
        new_cli->callback_context = config->callback_context;
        rdmnet_safe_strncpy(new_cli->search_domain, config->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
        new_cli->data.rpt.type = config->type;
        if (RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&config->uid))
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
        *handle = new_handle;
      }
      else
      {
        etcpal_rbtree_remove(&state.clients_by_llrp_handle, new_cli);
        FREE_RDMNET_CLIENT(new_cli);
        res = kEtcPalErrNoMem;
      }
    }
    else
    {
      FREE_RDMNET_CLIENT(new_cli);
    }
  }

  return res;
}

void destroy_client(RdmnetClient* cli, rdmnet_disconnect_reason_t reason)
{
  ClientScopeListEntry* scope = cli->scope_list;

  while (scope)
  {
    if (scope->monitor_handle)
    {
      rdmnet_disc_stop_monitoring(scope->monitor_handle);
      etcpal_rbtree_remove(&state.scopes_by_disc_handle, scope);
    }
    rdmnet_connection_destroy(scope->handle, &reason);
    etcpal_rbtree_remove(&state.scopes_by_handle, scope);

    ClientScopeListEntry* next_scope = scope->next;
    FREE_CLIENT_SCOPE(scope);
    scope = next_scope;
  }

  llrp_target_destroy(cli->llrp_handle);

  FREE_RDMNET_CLIENT(cli);
}

etcpal_error_t create_llrp_handle_for_client(const RdmnetRptClientConfig* config, RdmnetClient* cli)
{
  LlrpTargetConfig target_config;
  target_config.optional.netint_arr = config->llrp_netint_arr;
  target_config.optional.num_netints = config->num_llrp_netints;
  target_config.optional.uid = config->uid;

  target_config.cid = config->cid;
  target_config.component_type =
      (config->type == kRPTClientTypeController ? kLlrpCompRptController : kLlrpCompRptDevice);
  target_config.callbacks = llrp_callbacks;
  target_config.callback_context = NULL;
  etcpal_error_t res = llrp_target_create(&target_config, &cli->llrp_handle);

  if (res == kEtcPalErrOk)
  {
    if (kEtcPalErrOk != etcpal_rbtree_insert(&state.clients_by_llrp_handle, cli))
    {
      llrp_target_destroy(cli->llrp_handle);
      res = kEtcPalErrNoMem;
    }
  }
  return res;
}

/* Callback for IntHandleManager to determine whether a handle is in use. */
bool client_handle_in_use(int handle_val)
{
  return etcpal_rbtree_find(&state.clients, &handle_val);
}

/* Allocate a new scope list entry and append it to a client's scope list. If a scope string is
 * already in the list, fails with kEtcPalErrExists. Attempts to create a new connection handle to
 * accompany the scope. Returns kEtcPalErrOk on success, other error code otherwise. Fills in
 * new_entry with the newly-created entry on success.
 */
etcpal_error_t create_and_append_scope_entry(const RdmnetScopeConfig* config, RdmnetClient* client,
                                             ClientScopeListEntry** new_entry)
{
  if (find_scope_in_list(client->scope_list, config->scope))
    return kEtcPalErrExists;

  ClientScopeListEntry** entry_ptr = &client->scope_list;
  for (; *entry_ptr; entry_ptr = &(*entry_ptr)->next)
    ;

  // The scope string was not in the list, try to allocate it
  etcpal_error_t res = kEtcPalErrNoMem;
  ClientScopeListEntry* new_scope = (ClientScopeListEntry*)ALLOC_CLIENT_SCOPE();
  if (new_scope)
  {
    RdmnetConnectionConfig conn_config;
    conn_config.local_cid = client->cid;
    conn_config.callbacks = conn_callbacks;
    conn_config.callback_context = NULL;

    res = rdmnet_connection_create(&conn_config, &new_scope->handle);
    if (res == kEtcPalErrOk)
    {
      if (kEtcPalErrOk == etcpal_rbtree_insert(&state.scopes_by_handle, new_scope))
      {
        res = kEtcPalErrOk;
        new_scope->next = NULL;
        *entry_ptr = new_scope;
      }
      else
      {
        res = kEtcPalErrNoMem;
        rdmnet_connection_destroy(new_scope->handle, NULL);
        FREE_CLIENT_SCOPE(new_scope);
        new_scope = NULL;
      }
    }
    else
    {
      FREE_CLIENT_SCOPE(new_scope);
      new_scope = NULL;
    }
  }

  if (res == kEtcPalErrOk)
  {
    // Do the rest of the initialization
    rdmnet_safe_strncpy(new_scope->id, config->scope, E133_SCOPE_STRING_PADDED_LENGTH);
    new_scope->has_static_broker_addr = config->has_static_broker_addr;
    new_scope->static_broker_addr = config->static_broker_addr;
    if (config->has_static_broker_addr)
      new_scope->state = kScopeStateConnecting;
    else
      new_scope->state = kScopeStateDiscovery;
    // uid init is done at connection time
    new_scope->send_seq_num = 1;
    new_scope->monitor_handle = NULL;
    new_scope->broker_found = false;
    new_scope->listen_addrs = NULL;
    new_scope->num_listen_addrs = 0;
    new_scope->current_listen_addr = 0;
    new_scope->port = 0;
    new_scope->client = client;

    *new_entry = new_scope;
  }
  return res;
}

ClientScopeListEntry* find_scope_in_list(ClientScopeListEntry* list, const char* scope)
{
  ClientScopeListEntry* entry;
  for (entry = list; entry; entry = entry->next)
  {
    if (strcmp(entry->id, scope) == 0)
    {
      // Found
      return entry;
    }
  }
  return NULL;
}

void remove_scope_from_list(ClientScopeListEntry** list, ClientScopeListEntry* entry)
{
  ClientScopeListEntry* last_entry = NULL;

  for (ClientScopeListEntry* cur_entry = *list; cur_entry; last_entry = cur_entry, cur_entry = cur_entry->next)
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

etcpal_error_t start_scope_discovery(ClientScopeListEntry* scope_entry, const char* search_domain)
{
  RdmnetScopeMonitorConfig config;

  rdmnet_safe_strncpy(config.scope, scope_entry->id, E133_SCOPE_STRING_PADDED_LENGTH);
  rdmnet_safe_strncpy(config.domain, search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  config.callbacks = disc_callbacks;
  config.callback_context = NULL;

  // TODO capture errors
  int platform_error;
  etcpal_error_t res = rdmnet_disc_start_monitoring(&config, &scope_entry->monitor_handle, &platform_error);
  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_insert(&state.scopes_by_disc_handle, scope_entry);
  }
  else
  {
    RDMNET_LOG_WARNING("Starting discovery failed on scope '%s' with error '%s' (platform-specific error code %d)",
                       scope_entry->id, etcpal_strerror(res), platform_error);
  }
  return res;
}

void attempt_connection_on_listen_addrs(ClientScopeListEntry* scope_entry)
{
  size_t listen_addr_index = scope_entry->current_listen_addr;

  while (true)
  {
    char addr_str[ETCPAL_INET6_ADDRSTRLEN] = {'\0'};

    if (RDMNET_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      etcpal_inet_ntop(&scope_entry->listen_addrs[listen_addr_index], addr_str, ETCPAL_INET6_ADDRSTRLEN);
    }

    RDMNET_LOG_INFO("Attempting broker connection on scope '%s' at address %s:%d...", scope_entry->id, addr_str,
                    scope_entry->port);

    EtcPalSockAddr connect_addr;
    connect_addr.ip = scope_entry->listen_addrs[listen_addr_index];
    connect_addr.port = scope_entry->port;

    etcpal_error_t connect_res = start_connection_for_scope(scope_entry, &connect_addr);
    if (connect_res == kEtcPalErrOk)
    {
      scope_entry->current_listen_addr = listen_addr_index;
      break;
    }
    else
    {
      if (++listen_addr_index == scope_entry->num_listen_addrs)
        listen_addr_index = 0;
      if (listen_addr_index == scope_entry->current_listen_addr)
      {
        // We've looped through all the addresses. This broker is no longer valid.
        scope_entry->broker_found = false;
        scope_entry->listen_addrs = NULL;
        scope_entry->num_listen_addrs = 0;
        scope_entry->current_listen_addr = 0;
        scope_entry->port = 0;
      }

      RDMNET_LOG_WARNING("Connection to broker for scope '%s' at address %s:%d failed with error: '%s'. %s",
                         scope_entry->id, addr_str, connect_addr.port, etcpal_strerror(connect_res),
                         scope_entry->broker_found ? "Trying next address..." : "All addresses exhausted. Giving up.");

      if (!scope_entry->broker_found)
        break;
    }
  }
}

etcpal_error_t start_connection_for_scope(ClientScopeListEntry* scope_entry, const EtcPalSockAddr* broker_addr)
{
  BrokerClientConnectMsg connect_msg;
  RdmnetClient* cli = scope_entry->client;

  if (cli->type == kClientProtocolRPT)
  {
    RptClientData* rpt_data = &cli->data.rpt;
    RdmUid my_uid;
    if (rpt_data->has_static_uid)
    {
      my_uid = rpt_data->uid;
    }
    else
    {
      RDMNET_INIT_DYNAMIC_UID_REQUEST(&my_uid, rpt_data->uid.manu);
    }

    rdmnet_safe_strncpy(connect_msg.scope, scope_entry->id, E133_SCOPE_STRING_PADDED_LENGTH);
    connect_msg.e133_version = E133_VERSION;
    rdmnet_safe_strncpy(connect_msg.search_domain, cli->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
    if (rpt_data->type == kRPTClientTypeController)
      connect_msg.connect_flags = BROKER_CONNECT_FLAG_INCREMENTAL_UPDATES;
    else
      connect_msg.connect_flags = 0;
    connect_msg.client_entry.client_protocol = kClientProtocolRPT;
    create_rpt_client_entry(&cli->cid, &my_uid, rpt_data->type, NULL, GET_RPT_CLIENT_ENTRY(&connect_msg.client_entry));
  }
  else
  {
    // TODO EPT
    return kEtcPalErrNotImpl;
  }

  return rdmnet_connect(scope_entry->handle, broker_addr, &connect_msg);
}

etcpal_error_t get_client(rdmnet_client_t handle, RdmnetClient** client)
{
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;
  if (!RDMNET_CLIENT_LOCK())
    return kEtcPalErrSys;

  RdmnetClient* found_cli = (RdmnetClient*)etcpal_rbtree_find(&state.clients, &handle);
  if (!found_cli)
  {
    RDMNET_CLIENT_UNLOCK();
    return kEtcPalErrNotFound;
  }
  *client = found_cli;
  // Return keeping the lock
  return kEtcPalErrOk;
}

RdmnetClient* get_client_by_llrp_handle(llrp_target_t handle)
{
  if (!RDMNET_CLIENT_LOCK())
    return NULL;

  RdmnetClient llrp_cmp;
  llrp_cmp.llrp_handle = handle;
  RdmnetClient* found_cli = (RdmnetClient*)etcpal_rbtree_find(&state.clients_by_llrp_handle, &llrp_cmp);
  if (!found_cli)
  {
    RDMNET_CLIENT_UNLOCK();
    return NULL;
  }
  // Return keeping the lock
  return found_cli;
}

void release_client(const RdmnetClient* client)
{
  ETCPAL_UNUSED_ARG(client);
  RDMNET_CLIENT_UNLOCK();
}

ClientScopeListEntry* get_scope(rdmnet_client_scope_t handle)
{
  if (!RDMNET_CLIENT_LOCK())
    return NULL;

  ClientScopeListEntry* found_scope = (ClientScopeListEntry*)etcpal_rbtree_find(&state.scopes_by_handle, &handle);
  if (!found_scope)
  {
    RDMNET_CLIENT_UNLOCK();
    return NULL;
  }
  // Return keeping the lock
  return found_scope;
}

ClientScopeListEntry* get_scope_by_disc_handle(rdmnet_scope_monitor_t handle)
{
  if (!RDMNET_CLIENT_LOCK())
    return NULL;

  ClientScopeListEntry scope_cmp;
  scope_cmp.monitor_handle = handle;
  ClientScopeListEntry* found_scope =
      (ClientScopeListEntry*)etcpal_rbtree_find(&state.scopes_by_disc_handle, &scope_cmp);
  if (!found_scope)
  {
    RDMNET_CLIENT_UNLOCK();
    return NULL;
  }
  // Return keeping the lock
  return found_scope;
}

void release_scope(const ClientScopeListEntry* scope_entry)
{
  ETCPAL_UNUSED_ARG(scope_entry);
  RDMNET_CLIENT_UNLOCK();
}

etcpal_error_t get_client_and_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, RdmnetClient** client,
                                    ClientScopeListEntry** scope_entry)
{
  RdmnetClient* found_cli;
  etcpal_error_t res = get_client(handle, &found_cli);
  if (res != kEtcPalErrOk)
    return res;

  ClientScopeListEntry* found_scope = (ClientScopeListEntry*)etcpal_rbtree_find(&state.scopes_by_handle, &scope_handle);
  if (!found_scope)
  {
    release_client(found_cli);
    return kEtcPalErrNotFound;
  }
  if (found_scope->client != found_cli)
  {
    release_client(found_cli);
    return kEtcPalErrInvalid;
  }
  *client = found_cli;
  *scope_entry = found_scope;
  // Return keeping the lock
  return kEtcPalErrOk;
}

void release_client_and_scope(const RdmnetClient* client, const ClientScopeListEntry* scope)
{
  ETCPAL_UNUSED_ARG(client);
  ETCPAL_UNUSED_ARG(scope);
  RDMNET_CLIENT_UNLOCK();
}

int client_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const RdmnetClient* a = (const RdmnetClient*)value_a;
  const RdmnetClient* b = (const RdmnetClient*)value_b;
  return (a->handle > b->handle) - (a->handle < b->handle);
}

int client_llrp_handle_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const RdmnetClient* a = (const RdmnetClient*)value_a;
  const RdmnetClient* b = (const RdmnetClient*)value_b;
  return (a->llrp_handle > b->llrp_handle) - (a->llrp_handle < b->llrp_handle);
}

int scope_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const ClientScopeListEntry* a = (const ClientScopeListEntry*)value_a;
  const ClientScopeListEntry* b = (const ClientScopeListEntry*)value_b;
  return (a->handle > b->handle) - (a->handle < b->handle);
}

int scope_disc_handle_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const ClientScopeListEntry* a = (const ClientScopeListEntry*)value_a;
  const ClientScopeListEntry* b = (const ClientScopeListEntry*)value_b;
  return (a->monitor_handle > b->monitor_handle) - (a->monitor_handle < b->monitor_handle);
}

EtcPalRbNode* client_node_alloc(void)
{
#if RDMNET_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(client_rb_nodes);
#endif
}

void client_node_free(EtcPalRbNode* node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(client_rb_nodes, node);
#endif
}
