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

#include "rdmnet/ept_client.h"

/*************************** Function definitions ****************************/

/**
 * @brief Initialize an RdmnetEptClientConfig with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meaningfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * @code
 * RdmnetEptClientConfig config;
 * rdmnet_ept_client_config_init(&config);
 * @endcode
 *
 * @param[out] config Pointer to RdmnetEptClientConfig to init.
 */
void rdmnet_ept_client_config_init(RdmnetEptClientConfig* config)
{
  if (config)
  {
    memset(config, 0, sizeof(RdmnetEptClientConfig));
  }
}

/**
 * @brief Set the callbacks in an RDMnet EPT client configuration structure.
 *
 * @param[out] config Config struct in which to set the callbacks.
 * @param[in] connected Callback called when the EPT client has connected to a broker.
 * @param[in] connect_failed Callback called when a connection to a broker has failed.
 * @param[in] disconnected Callback called when a connection to a broker has disconnected.
 * @param[in] client_list_update_received Callback called when an EPT client receives an updated
 *                                        RDMnet client list.
 * @param[in] data_received Callback called when an EPT client receives an EPT data message.
 * @param[in] status_received Callback called when an EPT client receives an EPT status message in
 *                            response to an EPT data message.
 * @param[in] context (optional) Pointer to opaque data passed back with each callback.
 */
void rdmnet_ept_client_set_callbacks(RdmnetEptClientConfig*                          config,
                                     RdmnetEptClientConnectedCallback                connected,
                                     RdmnetEptClientConnectFailedCallback            connect_failed,
                                     RdmnetEptClientDisconnectedCallback             disconnected,
                                     RdmnetEptClientClientListUpdateReceivedCallback client_list_update_received,
                                     RdmnetEptClientDataReceivedCallback             data_received,
                                     RdmnetEptClientStatusReceivedCallback           status_received,
                                     void*                                           context)
{
  if (config)
  {
    config->callbacks.connected = connected;
    config->callbacks.connect_failed = connect_failed;
    config->callbacks.disconnected = disconnected;
    config->callbacks.client_list_update_received = client_list_update_received;
    config->callbacks.data_received = data_received;
    config->callbacks.status_received = status_received;
    config->callbacks.context = context;
  }
}

/**
 * @brief Create a new instance of RDMnet EPT client functionality.
 *
 * Each EPT client is identified by a single component ID (CID). RDMnet connection will not be
 * attempted until at least one scope is added using rdmnet_ept_client_add_scope().
 *
 * @param[in] config Configuration parameters to use for this EPT client instance.
 * @param[out] handle Filled in on success with a handle to the new EPT client instance.
 * @return #kEtcPalErrOk: EPT client created successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No memory to allocate new EPT client instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_create(const RdmnetEptClientConfig* config, rdmnet_ept_client_t* handle)
{
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Destroy an EPT client instance.
 *
 * Will disconnect from all brokers to which this EPT client is currently connected, sending the
 * disconnect reason provided in the disconnect_reason parameter.
 *
 * @param[in] client_handle Handle to EPT client to destroy, no longer valid after this function returns.
 * @param[in] disconnect_reason Disconnect reason code to send on all connected scopes.
 * @return #kEtcPalErrOk: EPT client destroyed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_destroy(rdmnet_ept_client_t        client_handle,
                                         rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Add a new scope to an EPT client instance.
 *
 * The library will attempt to discover and connect to a broker for the scope (or just connect if a
 * static broker address is given); the status of these attempts will be communicated via the
 * callbacks associated with the EPT client instance.
 *
 * @param[in] client_handle Handle to EPT client to which to add a new scope.
 * @param[in] scope_config Configuration parameters for the new scope.
 * @param[out] scope_handle Filled in on success with a handle to the new scope.
 * @return #kEtcPalErrOk: New scope added successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance.
 * @return #kEtcPalErrNoMem: No memory to allocate new scope.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_add_scope(rdmnet_ept_client_t      client_handle,
                                           const RdmnetScopeConfig* scope_config,
                                           rdmnet_client_scope_t*   scope_handle)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(scope_config);
  ETCPAL_UNUSED_ARG(scope_handle);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Add a new scope representing the default RDMnet scope to an EPT client instance.
 *
 * This is a shortcut to easily add the default RDMnet scope to an EPT client. The default behavior
 * is to not use a statically-configured broker. If a static broker is needed on the default scope,
 * rdmnet_ept_client_add_scope() must be used.
 *
 * @param[in] client_handle Handle to EPT client to which to add the default scope.
 * @param[out] scope_handle Filled in on success with a handle to the new scope.
 * @return #kEtcPalErrOk: Default scope added successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance.
 * @return #kEtcPalErrNoMem: No memory to allocate new scope.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_add_default_scope(rdmnet_ept_client_t    client_handle,
                                                   rdmnet_client_scope_t* scope_handle)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Remove a previously-added scope from an EPT client instance.
 *
 * After this call completes, scope_handle will no longer be valid.
 *
 * @param[in] client_handle Handle to the EPT client from which to remove a scope.
 * @param[in] scope_handle Handle to scope to remove.
 * @param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * @return #kEtcPalErrOk: Scope removed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_remove_scope(rdmnet_ept_client_t        client_handle,
                                              rdmnet_client_scope_t      scope_handle,
                                              rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Change the configuration of a scope on an EPT client.
 *
 * Will disconnect from any connected brokers and attempt connection again using the new
 * configuration given.
 *
 * @param[in] client_handle Handle to the EPT client on which to change a scope.
 * @param[in] scope_handle Handle to the scope for which to change the configuration.
 * @param[in] new_scope_config New configuration parameters for the scope.
 * @param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * @return #kEtcPalErrOk: Scope changed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_change_scope(rdmnet_ept_client_t        client_handle,
                                              rdmnet_client_scope_t      scope_handle,
                                              const RdmnetScopeConfig*   new_scope_config,
                                              rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(new_scope_config);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Retrieve the scope string of a previously-added scope.
 *
 * @param[in] client_handle Handle to the EPT client from which to retrieve the scope string.
 * @param[in] scope_handle Handle to the scope for which to retrieve the scope string.
 * @param[out] scope_str_buf Filled in on success with the scope string. Must be at least of length
 *                           E133_SCOPE_STRING_PADDED_LENGTH.
 * @param[out] static_broker_addr (optional) Filled in on success with the static broker address,
 *                                if present. Leave NULL if you don't care about the static broker
 *                                address.
 * @return #kEtcPalErrOk: Scope information retrieved successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_get_scope(rdmnet_ept_client_t   client_handle,
                                           rdmnet_client_scope_t scope_handle,
                                           char*                 scope_str_buf,
                                           EtcPalSockAddr*       static_broker_addr)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(scope_str_buf);
  ETCPAL_UNUSED_ARG(static_broker_addr);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Request a client list from a broker.
 *
 * The response will be delivered via the RdmnetEptClientClientListUpdateReceivedCallback.
 *
 * @param[in] client_handle Handle to the EPT client from which to request the client list.
 * @param[in] scope_handle Handle to the scope on which to request the client list.
 * @return #kEtcPalErrOk: Request sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_request_client_list(rdmnet_ept_client_t   client_handle,
                                                     rdmnet_client_scope_t scope_handle)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Send data from an EPT client on a scope.
 *
 * @param[in] client_handle Handle to the EPT client from which to send data.
 * @param[in] scope_handle Handle to the scope on which to send data.
 * @param[in] dest_cid CID of the EPT client to which to send the data.
 * @param[in] manufacturer_id Manufacturer ID portion of the EPT sub-protocol identifier.
 * @param[in] protocol_id Manufacturer ID portion of the EPT sub-protocol identifier.
 * @param[in] data The data to send.
 * @param[in] data_len Size in bytes of data.
 * @return #kEtcPalErrOk: Data sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_send_data(rdmnet_ept_client_t   client_handle,
                                           rdmnet_client_scope_t scope_handle,
                                           const EtcPalUuid*     dest_cid,
                                           uint16_t              manufacturer_id,
                                           uint16_t              protocol_id,
                                           const uint8_t*        data,
                                           size_t                data_len)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(dest_cid);
  ETCPAL_UNUSED_ARG(manufacturer_id);
  ETCPAL_UNUSED_ARG(protocol_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Send a status message from an EPT client on a scope.
 *
 * @param[in] client_handle Handle to the EPT client from which to send the status message.
 * @param[in] scope_handle Handle to the scope on which to send the status message.
 * @param[in] dest_cid CID of the EPT client to which to send the status message.
 * @param[in] status_code EPT status code to send.
 * @param[in] status_string Optional status string accompanying the code.
 * @return #kEtcPalErrOk: Status sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: client_handle is not associated with a valid EPT client instance, or
 *                              scope_handle is not associated with a valid scope instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_ept_client_send_status(rdmnet_ept_client_t   client_handle,
                                             rdmnet_client_scope_t scope_handle,
                                             const EtcPalUuid*     dest_cid,
                                             ept_status_code_t     status_code,
                                             const char*           status_string)
{
  ETCPAL_UNUSED_ARG(client_handle);
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(dest_cid);
  ETCPAL_UNUSED_ARG(status_code);
  ETCPAL_UNUSED_ARG(status_string);
  return kEtcPalErrNotImpl;
}
