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

#include "rdmnet/controller.h"

#include <string.h>
#include "etcpal/common.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/controller.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if RDMNET_DYNAMIC_MEM
#define ALLOC_RDMNET_CONTROLLER() malloc(sizeof(RdmnetController))
#define FREE_RDMNET_CONTROLLER(ptr) free(ptr)
#else
#define ALLOC_RDMNET_CONTROLLER() etcpal_mempool_alloc(rdmnet_controllers)
#define FREE_RDMNET_CONTROLLER(ptr) etcpal_mempool_free(rdmnet_controllers, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(rdmnet_controllers, RdmnetController, RDMNET_MAX_CONTROLLERS);
#endif

/*********************** Private function prototypes *************************/

static void client_connected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                             const RdmnetClientConnectedInfo* info, void* context);
static void client_connect_failed(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                  const RdmnetClientConnectFailedInfo* info, void* context);
static void client_disconnected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                const RdmnetClientDisconnectedInfo* info, void* context);
static void client_broker_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                       const BrokerMessage* msg, void* context);
static llrp_response_action_t client_llrp_msg_received(rdmnet_client_t handle, const LlrpRemoteRdmCommand* cmd,
                                                       LlrpSyncRdmResponse* response, void* context);
static rdmnet_response_action_t client_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                    const RptClientMessage* msg, RdmnetSyncRdmResponse* response,
                                                    void* context);

// clang-format off
static const RptClientCallbacks client_callbacks =
{
  client_connected,
  client_connect_failed,
  client_disconnected,
  client_broker_msg_received,
  client_llrp_msg_received,
  client_msg_received
};
// clang-format on

/*************************** Function definitions ****************************/

etcpal_error_t rdmnet_controller_init(void)
{
#if RDMNET_DYNAMIC_MEM
  return kEtcPalErrOk;
#else
  return etcpal_mempool_init(rdmnet_controllers);
#endif
}

void rdmnet_controller_deinit(void)
{
}

/*!
 * \brief Initialize an RdmnetControllerConfig with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meaningfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * \code
 * RdmnetControllerConfig config;
 * rdmnet_controller_config_init(&config, 0x6574);
 * \endcode
 *
 * \param[out] config Pointer to RdmnetControllerConfig to init.
 * \param[in] manufacturer_id ESTA manufacturer ID. All RDMnet Controllers must have one.
 */
void rdmnet_controller_config_init(RdmnetControllerConfig* config, uint16_t manufacturer_id)
{
  if (config)
  {
    memset(config, 0, sizeof(RdmnetControllerConfig));
    RDMNET_INIT_DYNAMIC_UID_REQUEST(&config->uid, manufacturer_id);
  }
}

/*!
 * \brief Set the main callbacks in an RDMnet controller configuration structure.
 *
 * Items marked "optional" can be NULL.
 *
 * \param[out] config Config struct in which to set the callbacks.
 * \param[in] connected Callback called when the controller has connected to a broker.
 * \param[in] connect_failed Callback called when a connection to a broker has failed.
 * \param[in] disconnected Callback called when a connection to a broker has disconnected.
 * \param[in] client_list_update_received Callback called when a controller receives an updated
 *                                        RDMnet client list.
 * \param[in] rdm_response_received Callback called when a controller receives a response to an RDM
 *                                  command.
 * \param[in] status_received Callback called when a controller receives a status message in
 *                            response to an RDM command.
 * \param[in] responder_ids_received (optional) Callback called when a controller receives a set of
 *                                   dynamic UID mappings.
 * \param[in] callback_context (optional) Pointer to opaque data passed back with each callback.
 */
void rdmnet_controller_set_callbacks(RdmnetControllerConfig* config, RdmnetControllerConnectedCallback connected,
                                     RdmnetControllerConnectFailedCallback connect_failed,
                                     RdmnetControllerDisconnectedCallback disconnected,
                                     RdmnetControllerClientListUpdateReceivedCallback client_list_update_received,
                                     RdmnetControllerRdmResponseReceivedCallback rdm_response_received,
                                     RdmnetControllerStatusReceivedCallback status_received,
                                     RdmnetControllerResponderIdsReceivedCallback responder_ids_received,
                                     void* callback_context)
{
  if (config)
  {
    config->callbacks.connected = connected;
    config->callbacks.connect_failed = connect_failed;
    config->callbacks.disconnected = disconnected;
    config->callbacks.client_list_update_received = client_list_update_received;
    config->callbacks.rdm_response_received = rdm_response_received;
    config->callbacks.status_received = status_received;
    config->callbacks.responder_ids_received = responder_ids_received;
    config->callback_context = callback_context;
  }
}

/*!
 * \brief Provide a set of basic information that the library will use for responding to RDM commands.
 *
 * RDMnet controllers are required to respond to a basic set of RDM commands. This library provides
 * two possible approaches to this. With this approach, the library stores some basic data about
 * a controller instance and handles and responds to all RDM commands internally. Use this function
 * to set that data in the configuration structure. See \ref using_controller for more information.
 *
 * \param[out] config Config struct in which to set the data. This doesn't copy the strings
 *                    themselves yet; they must remain valid in their original location until the
 *                    config struct is passed to rdmnet_controller_create().
 * \param[in] manufacturer_label A string representing the manufacturer of the controller.
 * \param[in] device_model_description A string representing the name of the product model
 *                                     implementing the controller.
 * \param[in] software_version_label A string representing the software version of the controller.
 * \param[in] device_label A string representing a user-settable name for this controller instance.
 * \param[in] device_label_settable Whether the library should allow the device label to be changed
 *                                  remotely.
 */
void rdmnet_controller_set_rdm_data(RdmnetControllerConfig* config, const char* manufacturer_label,
                                    const char* device_model_description, const char* software_version_label,
                                    const char* device_label, bool device_label_settable)
{
  if (config)
  {
    config->rdm_data.manufacturer_label = manufacturer_label;
    config->rdm_data.device_model_description = device_model_description;
    config->rdm_data.software_version_label = software_version_label;
    config->rdm_data.device_label = device_label;
    config->rdm_data.device_label_settable = device_label_settable;
  }
}

/*!
 * \brief Set callbacks to handle RDM commands in an RDMnet controller configuration structure.
 *
 * RDMnet controllers are required to respond to a basic set of RDM commands. This library provides
 * two possible approaches to this. With this approach, the library forwards RDM commands received
 * via callbacks to the application to handle. GET requests for COMPONENT_SCOPE, SEARCH_DOMAIN, and
 * TCP_COMMS_STATUS will still be consumed internally. See \ref using_controller for more
 * information.
 *
 * \param[out] config Config struct in which to set the callbacks.
 * \param[in] rdm_command_received Callback called when a controller receives an RDM command.
 * \param[in] llrp_rdm_command_received Callback called when a controller receives an RDM command
 *                                      over LLRP. Only required if `create_llrp_target == true` in
 *                                      the config struct.
 */
void rdmnet_controller_set_rdm_cmd_callbacks(RdmnetControllerConfig* config,
                                             RdmnetControllerRdmCommandReceivedCallback rdm_command_received,
                                             RdmnetControllerLlrpRdmCommandReceivedCallback llrp_rdm_command_received)
{
  if (config)
  {
    config->rdm_callbacks.rdm_command_received = rdm_command_received;
    config->rdm_callbacks.llrp_rdm_command_received = llrp_rdm_command_received;
  }
}

/*!
 * \brief Create a new instance of RDMnet controller functionality.
 *
 * Each controller is identified by a single component ID (CID). Typical controller applications
 * will only need one controller instance. RDMnet connection will not be attempted until at least
 * one scope is added using rdmnet_controller_add_scope().
 *
 * \param[in] config Configuration parameters to use for this controller instance.
 * \param[out] handle Filled in on success with a handle to the new controller instance.
 * \return #kEtcPalErrOk: Controller created successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No memory to allocate new controller instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_create(const RdmnetControllerConfig* config, rdmnet_controller_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;

  RdmnetController* new_controller = ALLOC_RDMNET_CONTROLLER();
  if (!new_controller)
    return kEtcPalErrNoMem;

  RdmnetRptClientConfig client_config;
  client_config.type = kRPTClientTypeController;
  client_config.cid = config->cid;
  client_config.callbacks = client_callbacks;
  client_config.callback_context = new_controller;
  // client_config.optional = config->optional;

  etcpal_error_t res = rdmnet_rpt_client_create(&client_config, &new_controller->client_handle);
  if (res == kEtcPalErrOk)
  {
    // Do the rest of the initialization
    new_controller->callbacks = config->callbacks;
    new_controller->callback_context = config->callback_context;

    *handle = new_controller;
  }
  else
  {
    FREE_RDMNET_CONTROLLER(new_controller);
  }
  return res;
}

/*!
 * \brief Destroy a controller instance.
 *
 * Will disconnect from all brokers to which this controller is currently connected, sending the
 * disconnect reason provided in the disconnect_reason parameter.
 *
 * \param[in] handle Handle to controller to destroy, no longer valid after this function returns.
 * \param[in] disconnect_reason Disconnect reason code to send on all connected scopes.
 * \return #kEtcPalErrOk: Controller destroyed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_destroy(rdmnet_controller_t handle, rdmnet_disconnect_reason_t disconnect_reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  etcpal_error_t res = rdmnet_client_destroy(handle->client_handle, disconnect_reason);
  if (res == kEtcPalErrOk)
    FREE_RDMNET_CONTROLLER(handle);

  return res;
}

/*!
 * \brief Add a new scope to a controller instance.
 *
 * The library will attempt to discover and connect to a broker for the scope (or just connect if a
 * static broker address is given); the status of these attempts will be communicated via the
 * callbacks associated with the controller instance.
 *
 * \param[in] handle Handle to controller to which to add a new scope.
 * \param[in] scope_config Configuration parameters for the new scope.
 * \param[out] scope_handle Filled in on success with a handle to the new scope.
 * \return #kEtcPalErrOk: New scope added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance.
 * \return #kEtcPalErrNoMem: No memory to allocate new scope.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_add_scope(rdmnet_controller_t handle, const RdmnetScopeConfig* scope_config,
                                           rdmnet_client_scope_t* scope_handle)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_add_scope(handle->client_handle, scope_config, scope_handle);
}

/*!
 * \brief Add a new scope representing the default RDMnet scope to a controller instance.
 *
 * This is a shortcut to easily add the default RDMnet scope to a controller. The default behavior
 * is to not use a statically-configured broker. If a static broker is needed on the default scope,
 * rdmnet_controller_add_scope() must be used.
 *
 * \param[in] handle Handle to controller to which to add the default scope.
 * \param[out] scope_handle Filled in on success with a handle to the new scope.
 * \return #kEtcPalErrOk: Default scope added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance.
 * \return #kEtcPalErrNoMem: No memory to allocate new scope.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_add_default_scope(rdmnet_controller_t handle, rdmnet_client_scope_t* scope_handle)
{
  if (!handle)
    return kEtcPalErrInvalid;

  RdmnetScopeConfig default_scope;
  RDMNET_CLIENT_SET_DEFAULT_SCOPE(&default_scope);
  return rdmnet_client_add_scope(handle->client_handle, &default_scope, scope_handle);
}

/*!
 * \brief Remove a previously-added scope from a controller instance.
 *
 * After this call completes, scope_handle will no longer be valid.
 *
 * \param[in] handle Handle to the controller from which to remove a scope.
 * \param[in] scope_handle Handle to scope to remove.
 * \param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * \return #kEtcPalErrOk: Scope removed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_remove_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                              rdmnet_disconnect_reason_t disconnect_reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_remove_scope(handle->client_handle, scope_handle, disconnect_reason);
}

/*!
 * \brief Change the configuration of a scope on a controller.
 *
 * Will disconnect from any connected brokers and attempt connection again using the new
 * configuration given.
 *
 * \param[in] handle Handle to the controller on which to change a scope.
 * \param[in] scope_handle Handle to the scope for which to change the configuration.
 * \param[in] new_scope_config New configuration parameters for the scope.
 * \param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * \return #kEtcPalErrOk: Scope changed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_change_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
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
 * \brief Retrieve the scope string of a previously-added scope.
 *
 * \param[in] handle Handle to the controller from which to retrieve the scope string.
 * \param[in] scope_handle Handle to the scope for which to retrieve the scope string.
 * \param[out] scope_str_buf Filled in on success with the scope string. Must be at least of length
 *                           E133_SCOPE_STRING_PADDED_LENGTH.
 * \param[out] static_broker_addr (optional) Filled in on success with the static broker address,
 *                                if present. Leave NULL if you don't care about the static broker
 *                                address.
 * \return #kEtcPalErrOk: Scope information retrieved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_get_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                           char* scope_str_buf, EtcPalSockAddr* static_broker_addr)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(scope_str_buf);
  ETCPAL_UNUSED_ARG(static_broker_addr);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Request a client list from a broker.
 *
 * The response will be delivered via the RdmnetControllerClientListUpdateReceivedCallback.
 *
 * \param[in] handle Handle to the controller from which to request the client list.
 * \param[in] scope_handle Handle to the scope on which to request the client list.
 * \return #kEtcPalErrOk: Request sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_request_client_list(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_request_client_list(handle->client_handle, scope_handle);
}

/*!
 * \brief Request a set of responder IDs corresponding with dynamic responder UIDs from a broker.
 *
 * The response will be delivered via the RdmnetControllerResponderIdsReceivedCallback.
 *
 * \param[in] handle Handle to the controller from which to request the responder IDs.
 * \param[in] scope_handle Handle to the scope on which to request the responder IDs.
 * \param[in] uids Array of dynamic RDM UIDs for which to request the corresponding responder IDs.
 * \param[in] num_uids Size of the uids array.
 * \return #kEtcPalErrOk: Request sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_request_responder_ids(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                       const RdmUid* uids, size_t num_uids)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope);
  ETCPAL_UNUSED_ARG(uids);
  ETCPAL_UNUSED_ARG(num_uids);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Requesting the mapping of one or more dynamic UIDs to RIDs from a broker.
 *
 * The response will be delivered via the RdmnetControllerDynamicUidMappingsReceivedCallback.
 *
 * \param[in] handle Handle to the controller from which to request dynamic UID mappings.
 * \param[in] scope_handle Handle to the scope on which to request dynamic UID mappings.
 * \param[in] uids Array of UIDs for which to request the mapped RIDs.
 * \param[in] num_uids Size of uids array.
 * \return #kEtcPalErrOk: Dynamic UID mappings requested successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_request_dynamic_uid_mappings(rdmnet_controller_t handle,
                                                              rdmnet_client_scope_t scope_handle, const RdmUid* uids,
                                                              size_t num_uids)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(uids);
  ETCPAL_UNUSED_ARG(num_uids);
}

/*!
 * \brief Send an RDM command from a controller on a scope.
 *
 * The response will be delivered via either the RdmnetControllerRdmResponseReceived callback or
 * the RdmnetControllerStatusReceivedCallback, depending on the outcome of the command.
 *
 * \param[in] handle Handle to the controller from which to send the RDM command.
 * \param[in] scope_handle Handle to the scope on which to send the RDM command.
 * \param[in] destination Addressing information for the RDMnet client and responder to which to
 *                        send the command.
 * \param[in] command_class Whether this is a GET or a SET command.
 * \param[in] param_id The command's RDM parameter ID.
 * \param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * \param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * \param[out] seq_num Filled in on success with a sequence number which can be used to match the
 *                     command with a response.
 * \return #kEtcPalErrOk: Command sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_rdm_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetDestinationAddr* destination,
                                                  rdmnet_command_class_t command_class, uint16_t param_id,
                                                  const uint8_t* data, uint8_t data_len, uint32_t* seq_num)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_rdm_command(handle->client_handle, scope_handle, cmd, seq_num);
}

/*!
 * \brief Send an RDM GET command from a controller on a scope.
 *
 * The response will be delivered via either the RdmnetControllerRdmResponseReceived callback or
 * the RdmnetControllerStatusReceivedCallback, depending on the outcome of the command.
 *
 * \param[in] handle Handle to the controller from which to send the GET command.
 * \param[in] scope_handle Handle to the scope on which to send the GET command.
 * \param[in] destination Addressing information for the RDMnet client and responder to which to
 *                        send the command.
 * \param[in] param_id The command's RDM parameter ID.
 * \param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * \param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * \param[out] seq_num Filled in on success with a sequence number which can be used to match the
 *                     command with a response.
 * \return #kEtcPalErrOk: Command sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_get_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetDestinationAddr* destination, uint16_t param_id,
                                                  const uint8_t* data, uint8_t data_len, uint32_t* seq_num)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(destination);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  ETCPAL_UNUSED_ARG(seq_num);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send an RDM SET command from a controller on a scope.
 *
 * The response will be delivered via either the RdmnetControllerRdmResponseReceived callback or
 * the RdmnetControllerStatusReceivedCallback, depending on the outcome of the command.
 *
 * \param[in] handle Handle to the controller from which to send the SET command.
 * \param[in] scope_handle Handle to the scope on which to send the SET command.
 * \param[in] destination Addressing information for the RDMnet client and responder to which to
 *                        send the command.
 * \param[in] param_id The command's RDM parameter ID.
 * \param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * \param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * \param[out] seq_num Filled in on success with a sequence number which can be used to match the
 *                     command with a response.
 * \return #kEtcPalErrOk: Command sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_set_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetDestinationAddr* destination, uint16_t param_id,
                                                  const uint8_t* data, uint8_t data_len, uint32_t* seq_num)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(destination);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  ETCPAL_UNUSED_ARG(seq_num);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send an RDM ACK response from a controller on a scope.
 *
 * This function should only be used if a valid RdmnetControllerRdmCmdHandler was provided with the
 * associated config struct.
 *
 * \param[in] handle Handle to the controller from which to send the RDM ACK response.
 * \param[in] scope_handle Handle to the scope on which to send the RDM ACK response.
 * \param[in] received_cmd Previously-received command that the ACK is a response to.
 * \param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * \param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * \return #kEtcPalErrOk: ACK response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_rdm_ack(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetSavedRdmCommand* received_cmd, const uint8_t* response_data,
                                              size_t response_data_len)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_rdm_ack(handle->client_handle, scope_handle, received_cmd, response_data,
                                        response_data_len);
}

/*!
 * \brief Send an RDM NACK response from a controller on a scope.
 *
 * This function should only be used if a valid RdmnetControllerRdmCmdHandler was provided with the
 * associated config struct.
 *
 * \param[in] handle Handle to the controller from which to send the RDM NACK response.
 * \param[in] scope_handle Handle to the scope on which to send the RDM NACK response.
 * \param[in] received_cmd Previously-received command that the NACK is a response to.
 * \param[in] nack_reason RDM NACK reason code to send with the NACK.
 * \return #kEtcPalErrOk: NACK response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_rdm_nack(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                               const RdmnetSavedRdmCommand* received_cmd, rdm_nack_reason_t nack_reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_rdm_nack(handle->client_handle, scope_handle, received_cmd, nack_reason);
}

/*!
 * \brief Send an asynchronous RDM GET response to update the value of a local parameter.
 *
 * This function should only be used if a valid RdmnetControllerRdmCmdHandler was provided with the
 * associated config struct.
 *
 * \param[in] handle Handle to the controller from which to send the RDM update.
 * \param[in] scope_handle Handle to the scope on which to send the RDM update.
 * \param[in] param_id The RDM parameter ID that has been updated.
 * \param[in] data The updated parameter data, or NULL if no data.
 * \param[in] data_len The length of the updated parameter data, or NULL if no data.
 * \return #kEtcPalErrOk: RDM update sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_rdm_update(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                 uint16_t param_id, const uint8_t* data, size_t data_len)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send an LLRP RDM ACK response from a controller.
 *
 * \param[in] handle Handle to the controller from which to send the LLRP RDM ACK response.
 * \param[in] received_cmd Previously-received LLRP RDM command that the ACK is a response to.
 * \param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * \param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * \return #kEtcPalErrOk: LLRP ACK response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_llrp_ack(rdmnet_controller_t handle, const LlrpSavedRdmCommand* received_cmd,
                                               const uint8_t* response_data, uint8_t response_data_len)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_llrp_ack(handle->client_handle, received_cmd, response_data, response_data_len);
}

/*!
 * \brief Send an LLRP RDM NACK response from a controller.
 *
 * \param[in] handle Handle to the controller from which to send the LLRP RDM NACK response.
 * \param[in] received_cmd Previously-received LLRP RDM command that the NACK is a response to.
 * \param[in] nack_reason RDM NACK reason code to send with the NACK.
 * \return #kEtcPalErrOk: LLRP NACK response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid controller instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_llrp_nack(rdmnet_controller_t handle, const LlrpSavedRdmCommand* received_cmd,
                                                rdm_nack_reason_t nack_reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_llrp_nack(handle->client_handle, received_cmd, nack_reason);
}

void client_connected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo* info,
                      void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    controller->callbacks.connected(controller, scope_handle, info, controller->callback_context);
  }
}

void client_connect_failed(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                           const RdmnetClientConnectFailedInfo* info, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    controller->callbacks.connect_failed(controller, scope_handle, info, controller->callback_context);
  }
}

void client_disconnected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetClientDisconnectedInfo* info, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    controller->callbacks.disconnected(controller, scope_handle, info, controller->callback_context);
  }
}

void client_broker_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const BrokerMessage* msg,
                                void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    switch (msg->vector)
    {
      case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
      case VECTOR_BROKER_CLIENT_ADD:
      case VECTOR_BROKER_CLIENT_REMOVE:
      case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
        RDMNET_ASSERT(BROKER_GET_CLIENT_LIST(msg)->client_protocol == kClientProtocolRPT);
        controller->callbacks.client_list_update_received(controller, scope_handle, (client_list_action_t)msg->vector,
                                                          BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(msg)),
                                                          controller->callback_context);
        break;
      default:
        break;
    }
  }
}

llrp_response_action_t client_llrp_msg_received(rdmnet_client_t handle, const LlrpRemoteRdmCommand* cmd,
                                                LlrpSyncRdmResponse* response, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(response);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    if (controller->rdm_handle_method == kRdmHandleMethodUseCallbacks)
    {
      return controller->rdm_handler.callbacks.llrp_rdm_command_received(controller, cmd, response,
                                                                         controller->callback_context);
    }
    else
    {
      // TODO
    }
  }
}

rdmnet_response_action_t client_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                             const RptClientMessage* msg, RdmnetSyncRdmResponse* response,
                                             void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(response);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    switch (msg->type)
    {
      case kRptClientMsgRdmCmd:
        if (controller->rdm_handle_method == kRdmHandleMethodUseCallbacks)
        {
          return controller->rdm_handler.callbacks.rdm_command_received(
              controller, scope_handle, RDMNET_GET_REMOTE_RDM_COMMAND(msg), response, controller->callback_context);
        }
        else
        {
          // TODO
        }
        break;
      case kRptClientMsgRdmResp:
        controller->callbacks.rdm_response_received(controller, scope_handle, RDMNET_GET_REMOTE_RDM_RESPONSE(msg),
                                                    controller->callback_context);
        return kRdmnetRdmResponseActionDefer;
      case kRptClientMsgStatus:
        controller->callbacks.status_received(controller, scope_handle, RDMNET_GET_REMOTE_RPT_STATUS(msg),
                                              controller->callback_context);
        return kRdmnetRdmResponseActionDefer;
      default:
        break;
    }
  }
  return kRdmnetRdmResponseActionDefer;
}
