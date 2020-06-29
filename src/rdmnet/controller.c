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

#include "rdmnet/controller.h"

#include <stddef.h>
#include <string.h>
#include "etcpal/common.h"
#include "etcpal/lock.h"
#include "etcpal/pack.h"
#include "etcpal/rbtree.h"
#include "rdmnet/common_priv.h"
#include "rdmnet/core/client.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/core/util.h"

/***************************** Private macros ********************************/

#define GET_CONTROLLER_FROM_CLIENT(clientptr) (RdmnetController*)((char*)(clientptr)-offsetof(RdmnetController, client))

#define CONTROLLER_LOCK(controller_ptr) etcpal_mutex_lock(&(controller_ptr)->lock)
#define CONTROLLER_UNLOCK(controller_ptr) etcpal_mutex_unlock(&(controller_ptr)->lock)

/*********************** Private function prototypes *************************/

static etcpal_error_t validate_controller_config(const RdmnetControllerConfig* config);
static etcpal_error_t create_new_controller(const RdmnetControllerConfig* config, rdmnet_controller_t* handle);
static etcpal_error_t get_controller(rdmnet_controller_t handle, RdmnetController** controller);
static void           release_controller(RdmnetController* controller);

void copy_rdm_data(const RdmnetControllerRdmData* config_data, ControllerRdmDataInternal* data);

// Client callbacks
static void client_connected(RCClient*                        client,
                             rdmnet_client_scope_t            scope_handle,
                             const RdmnetClientConnectedInfo* info);
static void client_connect_failed(RCClient*                            client,
                                  rdmnet_client_scope_t                scope_handle,
                                  const RdmnetClientConnectFailedInfo* info);
static void client_disconnected(RCClient*                           client,
                                rdmnet_client_scope_t               scope_handle,
                                const RdmnetClientDisconnectedInfo* info);
static void client_broker_msg_received(RCClient* client, rdmnet_client_scope_t scope_handle, const BrokerMessage* msg);
static void client_destroyed(RCClient* client);
static void client_llrp_msg_received(RCClient*              client,
                                     const LlrpRdmCommand*  cmd,
                                     RdmnetSyncRdmResponse* response,
                                     bool*                  use_internal_buf_for_response);
static void client_rpt_msg_received(RCClient*               client,
                                    rdmnet_client_scope_t   scope_handle,
                                    const RptClientMessage* msg,
                                    RdmnetSyncRdmResponse*  response,
                                    bool*                   use_internal_buf_for_response);

static void handle_rdm_command_internally(RdmnetController*       controller,
                                          const RdmCommandHeader* rdm_header,
                                          const uint8_t*          data,
                                          uint8_t                 data_len,
                                          RdmnetSyncRdmResponse*  response);

static void handle_supported_parameters(RdmnetController*       controller,
                                        const RdmCommandHeader* rdm_header,
                                        RdmnetSyncRdmResponse*  response);
static void handle_device_info(RdmnetController*       controller,
                               const RdmCommandHeader* rdm_header,
                               RdmnetSyncRdmResponse*  response);
static void handle_generic_label_query(char*                   label,
                                       const RdmCommandHeader* rdm_header,
                                       const uint8_t*          data,
                                       uint8_t                 data_len,
                                       RdmnetSyncRdmResponse*  response);
static void handle_component_scope(RdmnetController*       controller,
                                   const RdmCommandHeader* rdm_header,
                                   const uint8_t*          data,
                                   uint8_t                 data_len,
                                   RdmnetSyncRdmResponse*  response);
static void handle_search_domain(RdmnetController*       controller,
                                 const RdmCommandHeader* rdm_header,
                                 RdmnetSyncRdmResponse*  response);
static void handle_identify_device(RdmnetController*       controller,
                                   const RdmCommandHeader* rdm_header,
                                   RdmnetSyncRdmResponse*  response);

// clang-format off
static const RCClientCommonCallbacks client_callbacks = {
  client_connected,
  client_connect_failed,
  client_disconnected,
  client_broker_msg_received,
  client_destroyed
};

static const RCRptClientCallbacks rpt_client_callbacks = {
  client_llrp_msg_received,
  client_rpt_msg_received
};

static uint16_t kControllerInternalSupportedParameters[] = {
  E120_SUPPORTED_PARAMETERS,
  E120_DEVICE_MODEL_DESCRIPTION,
  E120_MANUFACTURER_LABEL,
  E120_DEVICE_LABEL,
  E120_SOFTWARE_VERSION_LABEL,
  E133_COMPONENT_SCOPE,
  E133_SEARCH_DOMAIN,
  E133_TCP_COMMS_STATUS,
  E120_IDENTIFY_DEVICE
};
#define NUM_INTERNAL_SUPPORTED_PARAMETERS \
  (sizeof(kControllerInternalSupportedParameters) / sizeof(kControllerInternalSupportedParameters[0]))
// clang-format on

/*************************** Function definitions ****************************/

/**
 * @brief Initialize an RdmnetControllerConfig with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meaningfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * @code
 * RdmnetControllerConfig config;
 * rdmnet_controller_config_init(&config, 0x6574);
 * @endcode
 *
 * @param[out] config Pointer to RdmnetControllerConfig to init.
 * @param[in] manufacturer_id ESTA manufacturer ID. All RDMnet Controllers must have one.
 */
void rdmnet_controller_config_init(RdmnetControllerConfig* config, uint16_t manufacturer_id)
{
  if (config)
  {
    memset(config, 0, sizeof(RdmnetControllerConfig));
    RDMNET_INIT_DYNAMIC_UID_REQUEST(&config->uid, manufacturer_id);
  }
}

/**
 * @brief Set the main callbacks in an RDMnet controller configuration structure.
 *
 * Items marked "optional" can be NULL.
 *
 * @param[out] config Config struct in which to set the callbacks.
 * @param[in] connected Callback called when the controller has connected to a broker.
 * @param[in] connect_failed Callback called when a connection to a broker has failed.
 * @param[in] disconnected Callback called when a connection to a broker has disconnected.
 * @param[in] client_list_update_received Callback called when a controller receives an updated
 *                                        RDMnet client list.
 * @param[in] rdm_response_received Callback called when a controller receives a response to an RDM
 *                                  command.
 * @param[in] status_received Callback called when a controller receives a status message in
 *                            response to an RDM command.
 * @param[in] responder_ids_received (optional) Callback called when a controller receives a set of
 *                                   dynamic UID mappings.
 * @param[in] context (optional) Pointer to opaque data passed back with each callback.
 */
void rdmnet_controller_set_callbacks(RdmnetControllerConfig*                          config,
                                     RdmnetControllerConnectedCallback                connected,
                                     RdmnetControllerConnectFailedCallback            connect_failed,
                                     RdmnetControllerDisconnectedCallback             disconnected,
                                     RdmnetControllerClientListUpdateReceivedCallback client_list_update_received,
                                     RdmnetControllerRdmResponseReceivedCallback      rdm_response_received,
                                     RdmnetControllerStatusReceivedCallback           status_received,
                                     RdmnetControllerResponderIdsReceivedCallback     responder_ids_received,
                                     void*                                            context)
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
    config->callbacks.context = context;
  }
}

/**
 * @brief Provide a set of basic information that the library will use for responding to RDM commands.
 *
 * RDMnet controllers are required to respond to a basic set of RDM commands. This library provides
 * two possible approaches to this. With this approach, the library stores some basic data about
 * a controller instance and handles and responds to all RDM commands internally. Use this function
 * to set that data in the configuration structure. See @ref using_controller for more information.
 *
 * @param[out] config Config struct in which to set the data. This doesn't copy the strings
 *                    themselves yet; they must remain valid in their original location until the
 *                    config struct is passed to rdmnet_controller_create().
 * @param[in] manufacturer_label A string representing the manufacturer of the controller.
 * @param[in] device_model_description A string representing the name of the product model
 *                                     implementing the controller.
 * @param[in] software_version_label A string representing the software version of the controller.
 * @param[in] device_label A string representing a user-settable name for this controller instance.
 * @param[in] device_label_settable Whether the library should allow the device label to be changed
 *                                  remotely.
 */
void rdmnet_controller_set_rdm_data(RdmnetControllerConfig* config,
                                    const char*             manufacturer_label,
                                    const char*             device_model_description,
                                    const char*             software_version_label,
                                    const char*             device_label,
                                    bool                    device_label_settable)
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

/**
 * @brief Set callbacks to handle RDM commands in an RDMnet controller configuration structure.
 *
 * RDMnet controllers are required to respond to a basic set of RDM commands. This library provides
 * two possible approaches to this. With this approach, the library forwards RDM commands received
 * via callbacks to the application to handle. GET requests for COMPONENT_SCOPE, SEARCH_DOMAIN, and
 * TCP_COMMS_STATUS will still be consumed internally. See @ref using_controller for more
 * information.
 *
 * Items marked "optional" can be NULL.
 *
 * @param[out] config Config struct in which to set the callbacks.
 * @param[in] rdm_command_received Callback called when a controller receives an RDM command.
 * @param[in] llrp_rdm_command_received Callback called when a controller receives an RDM command
 *                                      over LLRP. Only required if `create_llrp_target == true` in
 *                                      the config struct.
 * @param[in] response_buf (optional) A data buffer used to respond synchronously to RDM commands.
 *                         See @ref handling_rdm_commands for more information.
 * @param[in] context (optional) Pointer to opaque data passed back with each callback.
 */
void rdmnet_controller_set_rdm_cmd_callbacks(RdmnetControllerConfig*                        config,
                                             RdmnetControllerRdmCommandReceivedCallback     rdm_command_received,
                                             RdmnetControllerLlrpRdmCommandReceivedCallback llrp_rdm_command_received,
                                             uint8_t*                                       response_buf,
                                             void*                                          context)
{
  if (config)
  {
    config->rdm_handler.rdm_command_received = rdm_command_received;
    config->rdm_handler.llrp_rdm_command_received = llrp_rdm_command_received;
    config->rdm_handler.response_buf = response_buf;
    config->rdm_handler.context = context;
  }
}

/**
 * @brief Create a new instance of RDMnet controller functionality.
 *
 * Each controller is identified by a single component ID (CID). Typical controller applications
 * will only need one controller instance. RDMnet connection will not be attempted until at least
 * one scope is added using rdmnet_controller_add_scope().
 *
 * @param[in] config Configuration parameters to use for this controller instance.
 * @param[out] handle Filled in on success with a handle to the new controller instance.
 * @return #kEtcPalErrOk: Controller created successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No memory to allocate new controller instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_create(const RdmnetControllerConfig* config, rdmnet_controller_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = validate_controller_config(config);
  if (res != kEtcPalErrOk)
    return res;

  if (rdmnet_writelock())
  {
    res = create_new_controller(config, handle);
    rdmnet_writeunlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/**
 * @brief Destroy a controller instance.
 *
 * Will disconnect from all brokers to which this controller is currently connected, sending the
 * disconnect reason provided in the disconnect_reason parameter.
 *
 * @param[in] controller_handle Handle to controller to destroy, no longer valid after this function returns.
 * @param[in] disconnect_reason Disconnect reason code to send on all connected scopes.
 * @return #kEtcPalErrOk: Controller destroyed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_destroy(rdmnet_controller_t        controller_handle,
                                         rdmnet_disconnect_reason_t disconnect_reason)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_unregister(&controller->client, disconnect_reason);
  release_controller(controller);
  return res;
}

/**
 * @brief Add a new scope to a controller instance.
 *
 * The library will attempt to discover and connect to a broker for the scope (or just connect if a
 * static broker address is given); the status of these attempts will be communicated via the
 * callbacks associated with the controller instance.
 *
 * @param[in] controller_handle Handle to controller to which to add a new scope.
 * @param[in] scope_config Configuration parameters for the new scope.
 * @param[out] scope_handle Filled in on success with a handle to the new scope.
 * @return #kEtcPalErrOk: New scope added successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance.
 * @return #kEtcPalErrNoMem: No memory to allocate new scope.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_add_scope(rdmnet_controller_t      controller_handle,
                                           const RdmnetScopeConfig* scope_config,
                                           rdmnet_client_scope_t*   scope_handle)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_add_scope(&controller->client, scope_config, scope_handle);
  release_controller(controller);
  return res;
}

/**
 * @brief Add a new scope representing the default RDMnet scope to a controller instance.
 *
 * This is a shortcut to easily add the default RDMnet scope to a controller. The default behavior
 * is to not use a statically-configured broker. If a static broker is needed on the default scope,
 * rdmnet_controller_add_scope() must be used.
 *
 * @param[in] controller_handle Handle to controller to which to add the default scope.
 * @param[out] scope_handle Filled in on success with a handle to the new scope.
 * @return #kEtcPalErrOk: Default scope added successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance.
 * @return #kEtcPalErrNoMem: No memory to allocate new scope.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_add_default_scope(rdmnet_controller_t    controller_handle,
                                                   rdmnet_client_scope_t* scope_handle)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  RdmnetScopeConfig default_scope;
  RDMNET_CLIENT_SET_DEFAULT_SCOPE(&default_scope);
  res = rc_client_add_scope(&controller->client, &default_scope, scope_handle);
  release_controller(controller);
  return res;
}

/**
 * @brief Remove a previously-added scope from a controller instance.
 *
 * After this call completes, scope_handle will no longer be valid.
 *
 * @param[in] controller_handle Handle to the controller from which to remove a scope.
 * @param[in] scope_handle Handle to scope to remove.
 * @param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * @return #kEtcPalErrOk: Scope removed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_remove_scope(rdmnet_controller_t        controller_handle,
                                              rdmnet_client_scope_t      scope_handle,
                                              rdmnet_disconnect_reason_t disconnect_reason)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_remove_scope(&controller->client, scope_handle, disconnect_reason);
  release_controller(controller);
  return res;
}

/**
 * @brief Change the configuration of a scope on a controller.
 *
 * Will disconnect from any connected brokers and attempt connection again using the new
 * configuration given.
 *
 * @param[in] controller_handle Handle to the controller on which to change a scope.
 * @param[in] scope_handle Handle to the scope for which to change the configuration.
 * @param[in] new_scope_config New configuration parameters for the scope.
 * @param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * @return #kEtcPalErrOk: Scope changed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_change_scope(rdmnet_controller_t        controller_handle,
                                              rdmnet_client_scope_t      scope_handle,
                                              const RdmnetScopeConfig*   new_scope_config,
                                              rdmnet_disconnect_reason_t disconnect_reason)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_change_scope(&controller->client, scope_handle, new_scope_config, disconnect_reason);
  release_controller(controller);
  return res;
}

/**
 * @brief Retrieve the scope configuration of a previously-added scope.
 *
 * @param[in] controller_handle Handle to the controller from which to retrieve the scope configuration.
 * @param[in] scope_handle Handle to the scope for which to retrieve the scope configuration.
 * @param[out] scope_str_buf Filled in on success with the scope string. Must be at least of length
 *                           E133_SCOPE_STRING_PADDED_LENGTH.
 * @param[out] static_broker_addr (optional) Filled in on success with the static broker address,
 *                                if present. Leave NULL if you don't care about the static broker
 *                                address.
 * @return #kEtcPalErrOk: Scope information retrieved successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_get_scope(rdmnet_controller_t   controller_handle,
                                           rdmnet_client_scope_t scope_handle,
                                           char*                 scope_str_buf,
                                           EtcPalSockAddr*       static_broker_addr)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_get_scope(&controller->client, scope_handle, scope_str_buf, static_broker_addr);
  release_controller(controller);
  return res;
}

/**
 * @brief Change the controller's DNS search domain.
 *
 * Non-default search domains are considered advanced usage. Any added scopes which do not have a
 * static broker configuration will be disconnected, sending the disconnect reason provided in the
 * disconnect_reason parameter. Then discovery will be re-attempted on the new search domain.
 *
 * @param controller_handle Handle to the controller for which to change the search domain.
 * @param new_search_domain New search domain to use for discovery.
 * @param disconnect_reason Disconnect reason to send to any connected brokers.
 * @return #kEtcPalErrOk: Search domain changed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_change_search_domain(rdmnet_controller_t        controller_handle,
                                                      const char*                new_search_domain,
                                                      rdmnet_disconnect_reason_t disconnect_reason)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_change_search_domain(&controller->client, new_search_domain, disconnect_reason);
  release_controller(controller);
  return res;
}

/**
 * @brief Request a client list from a broker.
 *
 * The response will be delivered via the RdmnetControllerClientListUpdateReceivedCallback.
 *
 * @param[in] controller_handle Handle to the controller from which to request the client list.
 * @param[in] scope_handle Handle to the scope on which to request the client list.
 * @return #kEtcPalErrOk: Request sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_request_client_list(rdmnet_controller_t   controller_handle,
                                                     rdmnet_client_scope_t scope_handle)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_request_client_list(&controller->client, scope_handle);
  release_controller(controller);
  return res;
}

/**
 * @brief Request a set of responder IDs corresponding with dynamic responder UIDs from a broker.
 *
 * The response will be delivered via the RdmnetControllerResponderIdsReceivedCallback.
 *
 * @param[in] controller_handle Handle to the controller from which to request the responder IDs.
 * @param[in] scope_handle Handle to the scope on which to request the responder IDs.
 * @param[in] uids Array of dynamic RDM UIDs for which to request the corresponding responder IDs.
 * @param[in] num_uids Size of the uids array.
 * @return #kEtcPalErrOk: Request sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_request_responder_ids(rdmnet_controller_t   controller_handle,
                                                       rdmnet_client_scope_t scope_handle,
                                                       const RdmUid*         uids,
                                                       size_t                num_uids)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_request_responder_ids(&controller->client, scope_handle, uids, num_uids);
  release_controller(controller);
  return res;
}

/**
 * @brief Send an RDM command from a controller on a scope.
 *
 * The response will be delivered via either the RdmnetControllerRdmResponseReceived callback or
 * the RdmnetControllerStatusReceivedCallback, depending on the outcome of the command.
 *
 * @param[in] controller_handle Handle to the controller from which to send the RDM command.
 * @param[in] scope_handle Handle to the scope on which to send the RDM command.
 * @param[in] destination Addressing information for the RDMnet client and responder to which to
 *                        send the command.
 * @param[in] command_class Whether this is a GET or a SET command.
 * @param[in] param_id The command's RDM parameter ID.
 * @param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * @param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * @param[out] seq_num Filled in on success with a sequence number which can be used to match the
 *                     command with a response.
 * @return #kEtcPalErrOk: Command sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_rdm_command(rdmnet_controller_t          controller_handle,
                                                  rdmnet_client_scope_t        scope_handle,
                                                  const RdmnetDestinationAddr* destination,
                                                  rdmnet_command_class_t       command_class,
                                                  uint16_t                     param_id,
                                                  const uint8_t*               data,
                                                  uint8_t                      data_len,
                                                  uint32_t*                    seq_num)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_command(&controller->client, scope_handle, destination, command_class, param_id, data,
                                   data_len, seq_num);
  release_controller(controller);
  return res;
}

/**
 * @brief Send an RDM GET command from a controller on a scope.
 *
 * The response will be delivered via either the RdmnetControllerRdmResponseReceived callback or
 * the RdmnetControllerStatusReceivedCallback, depending on the outcome of the command.
 *
 * @param[in] controller_handle Handle to the controller from which to send the GET command.
 * @param[in] scope_handle Handle to the scope on which to send the GET command.
 * @param[in] destination Addressing information for the RDMnet client and responder to which to
 *                        send the command.
 * @param[in] param_id The command's RDM parameter ID.
 * @param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * @param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * @param[out] seq_num Filled in on success with a sequence number which can be used to match the
 *                     command with a response.
 * @return #kEtcPalErrOk: Command sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_get_command(rdmnet_controller_t          controller_handle,
                                                  rdmnet_client_scope_t        scope_handle,
                                                  const RdmnetDestinationAddr* destination,
                                                  uint16_t                     param_id,
                                                  const uint8_t*               data,
                                                  uint8_t                      data_len,
                                                  uint32_t*                    seq_num)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_command(&controller->client, scope_handle, destination, kRdmnetCCGetCommand, param_id, data,
                                   data_len, seq_num);
  release_controller(controller);
  return res;
}

/**
 * @brief Send an RDM SET command from a controller on a scope.
 *
 * The response will be delivered via either the RdmnetControllerRdmResponseReceived callback or
 * the RdmnetControllerStatusReceivedCallback, depending on the outcome of the command.
 *
 * @param[in] controller_handle Handle to the controller from which to send the SET command.
 * @param[in] scope_handle Handle to the scope on which to send the SET command.
 * @param[in] destination Addressing information for the RDMnet client and responder to which to
 *                        send the command.
 * @param[in] param_id The command's RDM parameter ID.
 * @param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * @param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * @param[out] seq_num Filled in on success with a sequence number which can be used to match the
 *                     command with a response.
 * @return #kEtcPalErrOk: Command sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_set_command(rdmnet_controller_t          controller_handle,
                                                  rdmnet_client_scope_t        scope_handle,
                                                  const RdmnetDestinationAddr* destination,
                                                  uint16_t                     param_id,
                                                  const uint8_t*               data,
                                                  uint8_t                      data_len,
                                                  uint32_t*                    seq_num)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_command(&controller->client, scope_handle, destination, kRdmnetCCSetCommand, param_id, data,
                                   data_len, seq_num);
  release_controller(controller);
  return res;
}

/**
 * @brief Send an RDM ACK response from a controller on a scope.
 *
 * This function should only be used if a valid RdmnetControllerRdmCmdHandler was provided with the
 * associated config struct.
 *
 * @param[in] controller_handle Handle to the controller from which to send the RDM ACK response.
 * @param[in] scope_handle Handle to the scope on which to send the RDM ACK response.
 * @param[in] received_cmd Previously-received command that the ACK is a response to.
 * @param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * @param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * @return #kEtcPalErrOk: ACK response sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_rdm_ack(rdmnet_controller_t          controller_handle,
                                              rdmnet_client_scope_t        scope_handle,
                                              const RdmnetSavedRdmCommand* received_cmd,
                                              const uint8_t*               response_data,
                                              size_t                       response_data_len)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_ack(&controller->client, scope_handle, received_cmd, response_data, response_data_len);
  release_controller(controller);
  return res;
}

/**
 * @brief Send an RDM NACK response from a controller on a scope.
 *
 * This function should only be used if a valid RdmnetControllerRdmCmdHandler was provided with the
 * associated config struct.
 *
 * @param[in] controller_handle Handle to the controller from which to send the RDM NACK response.
 * @param[in] scope_handle Handle to the scope on which to send the RDM NACK response.
 * @param[in] received_cmd Previously-received command that the NACK is a response to.
 * @param[in] nack_reason RDM NACK reason code to send with the NACK.
 * @return #kEtcPalErrOk: NACK response sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_rdm_nack(rdmnet_controller_t          controller_handle,
                                               rdmnet_client_scope_t        scope_handle,
                                               const RdmnetSavedRdmCommand* received_cmd,
                                               rdm_nack_reason_t            nack_reason)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_nack(&controller->client, scope_handle, received_cmd, nack_reason);
  release_controller(controller);
  return res;
}

/**
 * @brief Send an asynchronous RDM GET response to update the value of a local parameter.
 *
 * This function should only be used if a valid RdmnetControllerRdmCmdHandler was provided with the
 * associated config struct.
 *
 * @param[in] controller_handle Handle to the controller from which to send the RDM update.
 * @param[in] scope_handle Handle to the scope on which to send the RDM update.
 * @param[in] subdevice The subdevice of the default responder from which the update is being sent
 *                      (0 for the root device).
 * @param[in] param_id The RDM parameter ID that has been updated.
 * @param[in] data The updated parameter data, or NULL if no data.
 * @param[in] data_len The length of the updated parameter data, or NULL if no data.
 * @return #kEtcPalErrOk: RDM update sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance,
 *                              or scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_rdm_update(rdmnet_controller_t   controller_handle,
                                                 rdmnet_client_scope_t scope_handle,
                                                 uint16_t              subdevice,
                                                 uint16_t              param_id,
                                                 const uint8_t*        data,
                                                 size_t                data_len)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_update(&controller->client, scope_handle, subdevice, param_id, data, data_len);
  release_controller(controller);
  return res;
}

/**
 * @brief Send an LLRP RDM ACK response from a controller.
 *
 * @param[in] controller_handle Handle to the controller from which to send the LLRP RDM ACK response.
 * @param[in] received_cmd Previously-received LLRP RDM command that the ACK is a response to.
 * @param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * @param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * @return #kEtcPalErrOk: LLRP ACK response sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_llrp_ack(rdmnet_controller_t        controller_handle,
                                               const LlrpSavedRdmCommand* received_cmd,
                                               const uint8_t*             response_data,
                                               uint8_t                    response_data_len)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_llrp_ack(&controller->client, received_cmd, response_data, response_data_len);
  release_controller(controller);
  return res;
}

/**
 * @brief Send an LLRP RDM NACK response from a controller.
 *
 * @param[in] controller_handle Handle to the controller from which to send the LLRP RDM NACK response.
 * @param[in] received_cmd Previously-received LLRP RDM command that the NACK is a response to.
 * @param[in] nack_reason RDM NACK reason code to send with the NACK.
 * @return #kEtcPalErrOk: LLRP NACK response sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: controller_handle is not associated with a valid controller instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_controller_send_llrp_nack(rdmnet_controller_t        controller_handle,
                                                const LlrpSavedRdmCommand* received_cmd,
                                                rdm_nack_reason_t          nack_reason)
{
  RdmnetController* controller;
  etcpal_error_t    res = get_controller(controller_handle, &controller);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_llrp_nack(&controller->client, received_cmd, nack_reason);
  release_controller(controller);
  return res;
}

static bool validate_controller_callbacks(const RdmnetControllerCallbacks* callbacks)
{
  return (callbacks->connected && callbacks->connect_failed && callbacks->disconnected &&
          callbacks->client_list_update_received && callbacks->rdm_response_received && callbacks->status_received);
}

static bool validate_rdm_handler(const RdmnetControllerRdmCmdHandler* handler)
{
  return (handler->rdm_command_received && handler->llrp_rdm_command_received);
}

static bool validate_rdm_data(const RdmnetControllerRdmData* data)
{
  return (data->manufacturer_label && (strlen(data->manufacturer_label) > 0) && data->device_model_description &&
          (strlen(data->device_model_description) > 0) && data->software_version_label &&
          (strlen(data->software_version_label) > 0) && data->device_label && (strlen(data->device_label) > 0));
}

static etcpal_error_t validate_controller_config(const RdmnetControllerConfig* config)
{
  if (ETCPAL_UUID_IS_NULL(&config->cid) || !validate_controller_callbacks(&config->callbacks) ||
      (!validate_rdm_handler(&config->rdm_handler) && !validate_rdm_data(&config->rdm_data)) ||
      (!RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&config->uid) && (config->uid.manu & 0x8000)))
  {
    return kEtcPalErrInvalid;
  }
  return kEtcPalErrOk;
}

etcpal_error_t create_new_controller(const RdmnetControllerConfig* config, rdmnet_controller_t* handle)
{
  etcpal_error_t res = kEtcPalErrNoMem;

  RdmnetController* new_controller = alloc_controller_instance();
  if (!new_controller)
    return res;

  RCClient* client = &new_controller->client;
  client->lock = &new_controller->lock;
  client->type = kClientProtocolRPT;
  client->cid = config->cid;
  client->callbacks = client_callbacks;
  RC_RPT_CLIENT_DATA(client)->type = kRPTClientTypeController;
  RC_RPT_CLIENT_DATA(client)->uid = config->uid;
  RC_RPT_CLIENT_DATA(client)->callbacks = rpt_client_callbacks;
  if (config->search_domain)
    rdmnet_safe_strncpy(client->search_domain, config->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  else
    client->search_domain[0] = '\0';

  res = rc_rpt_client_register(client, config->create_llrp_target, config->llrp_netints, config->num_llrp_netints);
  if (res != kEtcPalErrOk)
  {
    free_controller_instance(new_controller);
    return res;
  }

  // Initialize the rest of the controller data
  if (config->rdm_handler.rdm_command_received && config->rdm_handler.llrp_rdm_command_received)
  {
    new_controller->rdm_handle_method = kRdmHandleMethodUseCallbacks;
    new_controller->rdm_handler.handler = config->rdm_handler;
  }
  else
  {
    new_controller->rdm_handle_method = kRdmHandleMethodUseData;
    copy_rdm_data(&config->rdm_data, &new_controller->rdm_handler.data);
  }
  new_controller->callbacks = config->callbacks;
  *handle = new_controller->id.handle;
  return kEtcPalErrOk;
}

etcpal_error_t get_controller(rdmnet_controller_t handle, RdmnetController** controller)
{
  if (handle == RDMNET_CONTROLLER_INVALID)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;
  if (!rdmnet_readlock())
    return kEtcPalErrSys;

  RdmnetController* found_controller = (RdmnetController*)find_struct_instance(handle, kRdmnetStructTypeController);
  if (!found_controller)
  {
    rdmnet_readunlock();
    return kEtcPalErrNotFound;
  }

  if (!etcpal_mutex_lock(&found_controller->lock))
  {
    rdmnet_readunlock();
    return kEtcPalErrSys;
  }

  *controller = found_controller;
  // Return keeping the locks
  return kEtcPalErrOk;
}

void release_controller(RdmnetController* controller)
{
  etcpal_mutex_unlock(&controller->lock);
  rdmnet_readunlock();
}

void copy_rdm_data(const RdmnetControllerRdmData* config_data, ControllerRdmDataInternal* data)
{
  data->model_id = config_data->model_id;
  data->product_category = config_data->product_category;
  data->software_version_id = config_data->software_version_id;
  rdmnet_safe_strncpy(data->manufacturer_label, config_data->manufacturer_label, CONTROLLER_RDM_LABEL_BUF_LENGTH);
  rdmnet_safe_strncpy(data->device_model_description, config_data->device_model_description,
                      CONTROLLER_RDM_LABEL_BUF_LENGTH);
  rdmnet_safe_strncpy(data->software_version_label, config_data->software_version_label,
                      CONTROLLER_RDM_LABEL_BUF_LENGTH);
  rdmnet_safe_strncpy(data->device_label, config_data->device_label, CONTROLLER_RDM_LABEL_BUF_LENGTH);
  data->device_label_settable = config_data->device_label_settable;
}

void client_connected(RCClient* client, rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo* info)
{
  RDMNET_ASSERT(client);
  RdmnetController* controller = GET_CONTROLLER_FROM_CLIENT(client);
  controller->callbacks.connected(controller->id.handle, scope_handle, info, controller->callbacks.context);
}

void client_connect_failed(RCClient*                            client,
                           rdmnet_client_scope_t                scope_handle,
                           const RdmnetClientConnectFailedInfo* info)
{
  RDMNET_ASSERT(client);
  RdmnetController* controller = GET_CONTROLLER_FROM_CLIENT(client);
  controller->callbacks.connect_failed(controller->id.handle, scope_handle, info, controller->callbacks.context);
}

void client_disconnected(RCClient* client, rdmnet_client_scope_t scope_handle, const RdmnetClientDisconnectedInfo* info)
{
  RDMNET_ASSERT(client);
  RdmnetController* controller = GET_CONTROLLER_FROM_CLIENT(client);
  controller->callbacks.disconnected(controller->id.handle, scope_handle, info, controller->callbacks.context);
}

void client_broker_msg_received(RCClient* client, rdmnet_client_scope_t scope_handle, const BrokerMessage* msg)
{
  RDMNET_ASSERT(client);
  RDMNET_ASSERT(msg);

  RdmnetController* controller = GET_CONTROLLER_FROM_CLIENT(client);

  switch (msg->vector)
  {
    case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
    case VECTOR_BROKER_CLIENT_ADD:
    case VECTOR_BROKER_CLIENT_REMOVE:
    case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
      RDMNET_ASSERT(BROKER_GET_CLIENT_LIST(msg)->client_protocol == kClientProtocolRPT);
      controller->callbacks.client_list_update_received(
          controller->id.handle, scope_handle, (client_list_action_t)msg->vector,
          BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(msg)), controller->callbacks.context);
      break;
    default:
      break;
  }
}

void client_destroyed(RCClient* client)
{
  RDMNET_ASSERT(client);
  RdmnetController* controller = GET_CONTROLLER_FROM_CLIENT(client);
  free_controller_instance(controller);
}

void client_llrp_msg_received(RCClient*              client,
                              const LlrpRdmCommand*  cmd,
                              RdmnetSyncRdmResponse* response,
                              bool*                  use_internal_buf_for_response)
{
  RDMNET_ASSERT(client);

  RdmnetController* controller = GET_CONTROLLER_FROM_CLIENT(client);
  if (controller)
  {
    if (controller->rdm_handle_method == kRdmHandleMethodUseCallbacks)
    {
      RdmnetControllerRdmCmdHandler* handler = &controller->rdm_handler.handler;
      handler->llrp_rdm_command_received(controller->id.handle, cmd, response, handler->context);
    }
    else
    {
      handle_rdm_command_internally(controller, &cmd->rdm_header, cmd->data, cmd->data_len, response);
      *use_internal_buf_for_response = true;
    }
  }
}

void client_rpt_msg_received(RCClient*               client,
                             rdmnet_client_scope_t   scope_handle,
                             const RptClientMessage* msg,
                             RdmnetSyncRdmResponse*  response,
                             bool*                   use_internal_buf_for_response)
{
  RDMNET_ASSERT(client);

  RdmnetController* controller = GET_CONTROLLER_FROM_CLIENT(client);
  if (controller)
  {
    switch (msg->type)
    {
      case kRptClientMsgRdmCmd:
        if (controller->rdm_handle_method == kRdmHandleMethodUseCallbacks)
        {
          RdmnetControllerRdmCmdHandler* handler = &controller->rdm_handler.handler;
          handler->rdm_command_received(controller->id.handle, scope_handle, RDMNET_GET_RDM_COMMAND(msg), response,
                                        handler->context);
        }
        else
        {
          const RdmnetRdmCommand* cmd = RDMNET_GET_RDM_COMMAND(msg);
          handle_rdm_command_internally(controller, &cmd->rdm_header, cmd->data, cmd->data_len, response);
          *use_internal_buf_for_response = true;
        }
        break;
      case kRptClientMsgRdmResp:
        controller->callbacks.rdm_response_received(controller->id.handle, scope_handle, RDMNET_GET_RDM_RESPONSE(msg),
                                                    controller->callbacks.context);
        break;
      case kRptClientMsgStatus:
        controller->callbacks.status_received(controller->id.handle, scope_handle, RDMNET_GET_RPT_STATUS(msg),
                                              controller->callbacks.context);
        break;
      default:
        break;
    }
  }
}

void handle_rdm_command_internally(RdmnetController*       controller,
                                   const RdmCommandHeader* rdm_header,
                                   const uint8_t*          data,
                                   uint8_t                 data_len,
                                   RdmnetSyncRdmResponse*  response)
{
  if (CONTROLLER_LOCK(controller))
  {
    if (rdm_header->command_class != kRdmCCGetCommand &&
        !(rdm_header->param_id == E120_DEVICE_LABEL && CONTROLLER_RDM_DATA(controller)->device_label_settable))
    {
      RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnsupportedCommandClass);
      CONTROLLER_UNLOCK(controller);
      return;
    }

    switch (rdm_header->param_id)
    {
      case E120_SUPPORTED_PARAMETERS:
        handle_supported_parameters(controller, rdm_header, response);
        break;
      case E120_DEVICE_INFO:
        handle_device_info(controller, rdm_header, response);
        break;
      case E120_DEVICE_MODEL_DESCRIPTION:
        handle_generic_label_query(CONTROLLER_RDM_DATA(controller)->device_model_description, rdm_header, data,
                                   data_len, response);
        break;
      case E120_MANUFACTURER_LABEL:
        handle_generic_label_query(CONTROLLER_RDM_DATA(controller)->manufacturer_label, rdm_header, data, data_len,
                                   response);
        break;
      case E120_DEVICE_LABEL:
        handle_generic_label_query(CONTROLLER_RDM_DATA(controller)->device_label, rdm_header, data, data_len, response);
        break;
      case E120_SOFTWARE_VERSION_LABEL:
        handle_generic_label_query(CONTROLLER_RDM_DATA(controller)->software_version_label, rdm_header, data, data_len,
                                   response);
        break;
      case E133_COMPONENT_SCOPE:
        handle_component_scope(controller, rdm_header, data, data_len, response);
        break;
      case E133_SEARCH_DOMAIN:
        handle_search_domain(controller, rdm_header, response);
        break;
      case E120_IDENTIFY_DEVICE:
        handle_identify_device(controller, rdm_header, response);
        break;
      default:
        RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnknownPid);
        break;
    }
    CONTROLLER_UNLOCK(controller);
  }
}

void handle_supported_parameters(RdmnetController*       controller,
                                 const RdmCommandHeader* rdm_header,
                                 RdmnetSyncRdmResponse*  response)
{
  ETCPAL_UNUSED_ARG(controller);
  ETCPAL_UNUSED_ARG(rdm_header);

  size_t   pd_len = NUM_INTERNAL_SUPPORTED_PARAMETERS * 2;
  uint8_t* buf = rc_client_get_internal_response_buf(pd_len);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  uint8_t* cur_ptr = buf;
  for (const uint16_t* param = kControllerInternalSupportedParameters;
       param < kControllerInternalSupportedParameters + NUM_INTERNAL_SUPPORTED_PARAMETERS; ++param)
  {
    etcpal_pack_u16b(cur_ptr, *param);
    cur_ptr += 2;
  }

  RDMNET_SYNC_SEND_RDM_ACK(response, pd_len);
}

void handle_device_info(RdmnetController*       controller,
                        const RdmCommandHeader* rdm_header,
                        RdmnetSyncRdmResponse*  response)
{
  ETCPAL_UNUSED_ARG(controller);
  ETCPAL_UNUSED_ARG(rdm_header);

  uint8_t* buf = rc_client_get_internal_response_buf(19);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  ControllerRdmDataInternal* rdm_data = CONTROLLER_RDM_DATA(controller);
  uint8_t*                   cur_ptr = buf;
  etcpal_pack_u16b(cur_ptr, E120_PROTOCOL_VERSION);
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, rdm_data->model_id);
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, rdm_data->product_category);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, rdm_data->software_version_id);
  cur_ptr += 4;
  etcpal_pack_u32b(cur_ptr, 0);
  cur_ptr += 4;
  etcpal_pack_u16b(cur_ptr, 0xffffu);
  cur_ptr += 2;
  memset(cur_ptr, 0, 3);
  cur_ptr += 3;

  RDMNET_SYNC_SEND_RDM_ACK(response, 19);
}

void handle_generic_label_query(char*                   label,
                                const RdmCommandHeader* rdm_header,
                                const uint8_t*          data,
                                uint8_t                 data_len,
                                RdmnetSyncRdmResponse*  response)
{
  if (rdm_header->command_class == kRdmCCGetCommand)
  {
    size_t   pd_len = strlen(label);
    uint8_t* buf = rc_client_get_internal_response_buf(pd_len);
    if (!buf)
    {
      RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
      return;
    }

    memcpy(buf, label, pd_len);
    RDMNET_SYNC_SEND_RDM_ACK(response, pd_len);
  }
  else if (rdm_header->command_class == kRdmCCSetCommand)
  {
    // Whether the label is settable is handled before this function is called.
    if (data_len < CONTROLLER_RDM_LABEL_BUF_LENGTH)
    {
      memcpy(label, data, data_len);
      label[data_len] = '\0';
    }
    else
    {
      rdmnet_safe_strncpy(label, (const char*)data, CONTROLLER_RDM_LABEL_BUF_LENGTH);
    }
    RDMNET_SYNC_SEND_RDM_ACK(response, 0);
  }
  else
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnsupportedCommandClass);
  }
}

#define COMPONENT_SCOPE_PD_SIZE 88

void handle_component_scope(RdmnetController*       controller,
                            const RdmCommandHeader* rdm_header,
                            const uint8_t*          data,
                            uint8_t                 data_len,
                            RdmnetSyncRdmResponse*  response)
{
  ETCPAL_UNUSED_ARG(rdm_header);

  if (data_len < 2)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
    return;
  }

  uint8_t* buf = rc_client_get_internal_response_buf(COMPONENT_SCOPE_PD_SIZE);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  uint16_t scope_slot = etcpal_unpack_u16b(data);

  // This is a bit of a hack and relies on knowledge of how the client struct works.
  // We use a convention that the scope slot corresponds to the scope handle plus 1.
  RCClient* client = &controller->client;
  if (scope_slot == 0 || scope_slot > client->num_scopes)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRDataOutOfRange);
    return;
  }

  RCClientScope* scope = &client->scopes[scope_slot - 1];
  while ((scope->state == kRCScopeStateInactive || scope->state == kRCScopeStateMarkedForDestruction))
  {
    if (++scope_slot <= client->num_scopes)
      scope = &client->scopes[scope_slot - 1];
    else
      break;
  }

  if (scope_slot > client->num_scopes)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRDataOutOfRange);
    return;
  }

  // Pack the scope information
  uint8_t* cur_ptr = buf;
  etcpal_pack_u16b(cur_ptr, scope_slot);
  cur_ptr += 2;
  memcpy(cur_ptr, scope->id, E133_SCOPE_STRING_PADDED_LENGTH);
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  if (ETCPAL_IP_IS_V4(&scope->static_broker_addr.ip))
  {
    *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
    etcpal_pack_u32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&scope->static_broker_addr.ip));
    cur_ptr += 4;
    memset(cur_ptr, 0, 16);
    cur_ptr += 16;
    etcpal_pack_u16b(cur_ptr, scope->static_broker_addr.port);
    cur_ptr += 2;
  }
  else if (ETCPAL_IP_IS_V6(&scope->static_broker_addr.ip))
  {
    *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
    memset(cur_ptr, 0, 4);
    cur_ptr += 4;
    memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&scope->static_broker_addr.ip), 16);
    cur_ptr += 16;
    etcpal_pack_u16b(cur_ptr, scope->static_broker_addr.port);
    cur_ptr += 2;
  }
  else
  {
    *cur_ptr++ = E133_NO_STATIC_CONFIG;
    memset(cur_ptr, 0, 22);
    cur_ptr += 22;
  }

  RDMNET_SYNC_SEND_RDM_ACK(response, COMPONENT_SCOPE_PD_SIZE);
}

void handle_search_domain(RdmnetController*       controller,
                          const RdmCommandHeader* rdm_header,
                          RdmnetSyncRdmResponse*  response)
{
  ETCPAL_UNUSED_ARG(rdm_header);

  // This is a bit of a hack and relies on knowledge of how the client struct works.
  RCClient* client = &controller->client;
  size_t    pd_len = strlen(client->search_domain);
  uint8_t*  buf = rc_client_get_internal_response_buf(pd_len);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  if (pd_len == 0)
  {
    pd_len = sizeof(E133_DEFAULT_DOMAIN) - 1;
    memcpy(buf, E133_DEFAULT_DOMAIN, pd_len);
  }
  else
  {
    memcpy(buf, client->search_domain, pd_len);
  }
  RDMNET_SYNC_SEND_RDM_ACK(response, pd_len);
}

void handle_identify_device(RdmnetController*       controller,
                            const RdmCommandHeader* rdm_header,
                            RdmnetSyncRdmResponse*  response)
{
  ETCPAL_UNUSED_ARG(controller);
  ETCPAL_UNUSED_ARG(rdm_header);

  uint8_t* buf = rc_client_get_internal_response_buf(1);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  *buf = 0;
  RDMNET_SYNC_SEND_RDM_ACK(response, 1);
}
