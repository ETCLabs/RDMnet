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

#include "rdmnet/device.h"

#include "etcpal/common.h"
#include "rdmnet/private/device.h"
#include "rdmnet/private/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if RDMNET_DYNAMIC_MEM
#define ALLOC_RDMNET_DEVICE() malloc(sizeof(RdmnetDevice))
#define FREE_RDMNET_DEVICE(ptr) free(ptr)
#else
#define ALLOC_RDMNET_DEVICE() etcpal_mempool_alloc(rdmnet_devices)
#define FREE_RDMNET_DEVICE(ptr) etcpal_mempool_free(rdmnet_devices, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(rdmnet_devices, RdmnetDevice, RDMNET_MAX_DEVICES);
#endif

/*********************** Private function prototypes *************************/

// static void client_connected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
//                             const RdmnetClientConnectedInfo* info, void* context);
// static void client_connect_failed(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
//                                  const RdmnetClientConnectFailedInfo* info, void* context);
// static void client_disconnected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
//                                const RdmnetClientDisconnectedInfo* info, void* context);
// static void client_broker_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
//                                       const BrokerMessage* msg, void* context);
// static llrp_response_action_t client_llrp_msg_received(rdmnet_client_t handle, const LlrpRemoteRdmCommand* cmd,
//                                                       LlrpSyncRdmResponse* response, void* context);
// static rdmnet_response_action_t client_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
//                                                    const RptClientMessage* msg, RdmnetSyncRdmResponse* response,
//                                                    void* context);

// clang-format off
//static const RptClientCallbacks client_callbacks =
//{
//  client_connected,
//  client_connect_failed,
//  client_disconnected,
//  client_broker_msg_received,
//  client_llrp_msg_received,
//  client_msg_received
//};
// clang-format on

/*************************** Function definitions ****************************/

etcpal_error_t rdmnet_device_init(void)
{
#if RDMNET_DYNAMIC_MEM
  return kEtcPalErrOk;
#else
  return etcpal_mempool_init(rdmnet_devices);
#endif
}

void rdmnet_device_deinit(void)
{
}

/*!
 * \brief Initialize an RDMnet Device Config with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meanizngfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * \code
 * RdmnetDeviceConfig config;
 * RDMNET_DEVICE_CONFIG_INIT(&config, 0x6574);
 * \endcode
 *
 * \param[out] config Pointer to RdmnetDeviceConfig to init.
 * \param[in] manufacturer_id ESTA manufacturer ID. All RDMnet Devices must have one.
 */
void rdmnet_device_config_init(RdmnetDeviceConfig* config, uint16_t manufacturer_id)
{
  if (config)
  {
    memset(config, 0, sizeof(RdmnetDeviceConfig));
    RDMNET_CLIENT_SET_DEFAULT_SCOPE(&config->scope_config);
    RDMNET_INIT_DYNAMIC_UID_REQUEST(&config->uid, manufacturer_id);
  }
}

/*!
 * \brief Set the main callbacks in an RDMnet device configuration structure.
 *
 * Items marked "optional" can be NULL.
 *
 * \param[out] config Config struct in which to set the callbacks.
 * \param[in] connected Callback called when the device has connected to a broker.
 * \param[in] connect_failed Callback called when a connection to a broker has failed.
 * \param[in] disconnected Callback called when a connection to a broker has disconnected.
 * \param[in] rdm_command_received Callback called when a device receives an RDM command.
 * \param[in] llrp_rdm_command_received Callback called when a device receives an RDM command over
 *                                      LLRP.
 * \param[in] dynamic_uid_status_received (optional) Callback called when a device receives dynamic
 *                                        UID assignments for one or more virtual responders.
 * \param[in] context (optional) Pointer to opaque data passed back with each callback.
 */
void rdmnet_device_set_callbacks(RdmnetDeviceConfig* config, RdmnetDeviceConnectedCallback connected,
                                 RdmnetDeviceConnectFailedCallback connect_failed,
                                 RdmnetDeviceDisconnectedCallback disconnected,
                                 RdmnetDeviceRdmCommandReceivedCallback rdm_command_received,
                                 RdmnetDeviceLlrpRdmCommandReceivedCallback llrp_rdm_command_received,
                                 RdmnetDeviceDynamicUidStatusCallback dynamic_uid_status_received, void* context)
{
  if (config)
  {
    config->callbacks.connected = connected;
    config->callbacks.connect_failed = connect_failed;
    config->callbacks.disconnected = disconnected;
    config->callbacks.rdm_command_received = rdm_command_received;
    config->callbacks.llrp_rdm_command_received = llrp_rdm_command_received;
    config->callbacks.dynamic_uid_status_received = dynamic_uid_status_received;
    config->callbacks.context = context;
  }
}

/*!
 * \brief Create a new instance of RDMnet device functionality.
 *
 * Each device is identified by a single component ID (CID). Typical device applications will only
 * need one device instance. The library will attempt to discover and connect to a broker for the
 * scope given in config->scope_config (or just connect if a static broker is given); the status of
 * these attempts will be communicated via the callbacks associated with the device instance.
 *
 * \param[in] config Configuration parameters to use for this device instance.
 * \param[out] handle Filled in on success with a handle to the new device instance.
 * \return #kEtcPalErrOk: Device created successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No memory to allocate new device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_create(const RdmnetDeviceConfig* config, rdmnet_device_t* handle)
{
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
  //  if (!config || !handle)
  //    return kEtcPalErrInvalid;
  //
  //  RdmnetDevice* new_device = ALLOC_RDMNET_DEVICE();
  //  if (!new_device)
  //    return kEtcPalErrNoMem;
  //
  //  RdmnetRptClientConfig client_config;
  //  client_config.type = kRPTClientTypeDevice;
  //  client_config.cid = config->cid;
  //  client_config.callbacks = client_callbacks;
  //  client_config.callback_context = new_device;
  //  // client_config.optional = config->optional;
  //
  //  etcpal_error_t res = rdmnet_rpt_client_create(&client_config, &new_device->client_handle);
  //  if (res == kEtcPalErrOk)
  //  {
  //    res = rdmnet_client_add_scope(new_device->client_handle, &config->scope_config, &new_device->scope_handle);
  //    if (res == kEtcPalErrOk)
  //    {
  //      // Do the rest of the initialization
  //      new_device->callbacks = config->callbacks;
  //      new_device->callback_context = config->callback_context;
  //
  //      *handle = new_device;
  //    }
  //    else
  //    {
  //      rdmnet_client_destroy(new_device->client_handle, kRdmnetDisconnectSoftwareFault);
  //      FREE_RDMNET_DEVICE(new_device);
  //    }
  //  }
  //  else
  //  {
  //    FREE_RDMNET_DEVICE(new_device);
  //  }
  //  return res;
}

/*!
 * \brief Destroy a device instance.
 *
 * Will disconnect from the broker to which this device is currently connected (if applicable),
 * sending the disconnect reason provided in the disconnect_reason parameter.
 *
 * \param[in] handle Handle to device to destroy, no longer valid after this function returns.
 * \param[in] disconnect_reason Disconnect reason code to send to the connected broker.
 * \return #kEtcPalErrOk: Device destroyed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_destroy(rdmnet_device_t handle, rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
  //  if (!handle)
  //    return kEtcPalErrInvalid;
  //
  //  etcpal_error_t res = rdmnet_client_destroy(handle->client_handle, disconnect_reason);
  //  if (res == kEtcPalErrOk)
  //    FREE_RDMNET_DEVICE(handle);
  //
  //  return res;
}

/*!
 * \brief Send an RDM ACK response from a device.
 *
 * \param[in] handle Handle to the device from which to send the RDM ACK response.
 * \param[in] received_cmd Previously-received command that the ACK is a response to.
 * \param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * \param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * \return #kEtcPalErrOk: ACK response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_rdm_ack(rdmnet_device_t handle, const RdmnetSavedRdmCommand* received_cmd,
                                          const uint8_t* response_data, size_t response_data_len)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(response_data);
  ETCPAL_UNUSED_ARG(response_data_len);
  return kEtcPalErrNotImpl;
  //  if (!handle)
  //    return kEtcPalErrInvalid;
  //
  //  return rdmnet_rpt_client_send_rdm_ack(handle->client_handle, handle->scope_handle, received_cmd, response_data,
  //                                        response_data_len);
}

/*!
 * \brief Send an RDM NACK response from a device.
 *
 * \param[in] handle Handle to the device from which to send the RDM NACK response.
 * \param[in] received_cmd Previously-received command that the NACK is a response to.
 * \param[in] nack_reason RDM NACK reason code to send with the NACK.
 * \return #kEtcPalErrOk: NACK response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_rdm_nack(rdmnet_device_t handle, const RdmnetSavedRdmCommand* received_cmd,
                                           rdm_nack_reason_t nack_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(nack_reason);
  return kEtcPalErrNotImpl;
  //  if (!handle)
  //    return kEtcPalErrInvalid;
  //
  //  return rdmnet_rpt_client_send_rdm_nack(handle->client_handle, handle->scope_handle, received_cmd, nack_reason);
}

/*!
 * \brief Send an asynchronous RDM GET response to update the value of a local parameter.
 *
 * This version is for updating a parameter on the device's default responder. For updates from
 * sub-responders, use rdmnet_device_send_rdm_udpate_from_responder(). See
 * \ref devices_and_gateways for more information.
 *
 * \param[in] handle Handle to the device from which to send the updated RDM data.
 * \param[in] param_id The RDM parameter ID that has been updated.
 * \param[in] data The updated parameter data, or NULL for messages with no data.
 * \param[in] data_len The length of the updated parameter data, or 0 for messages with no data.
 * \return #kEtcPalErrOk: RDM update sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_rdm_update(rdmnet_device_t handle, uint16_t param_id, const uint8_t* data,
                                             size_t data_len)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send an asynchronous RDM GET response to update the value of a parameter on a sub-responder.
 *
 * This version is for updating a parameter on a physical or vitual responder associated with one
 * of a device's endpoints. In particular, this is the one for a gateway to use when it collects a
 * new queued message from a responder. See \ref devices_and_gateways for more information.
 *
 * \param[in] handle Handle to the device from which to send the updated RDM data.
 * \param[in] endpoint The endpoint of the responder that has an updated parameter.
 * \param[in] source_uid The UID of the responder that has an updated parameter.
 * \param[in] param_id The RDM parameter ID that has been updated.
 * \param[in] data The updated parameter data, or NULL for messages with no data.
 * \param[in] data_len The length of the updated parameter data, or 0 for messages with no data.
 * \return #kEtcPalErrOk: RDM update sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_rdm_update_from_responder(rdmnet_device_t handle, uint16_t endpoint,
                                                            const RdmUid* source_uid, uint16_t param_id,
                                                            const uint8_t* data, size_t data_len)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint);
  ETCPAL_UNUSED_ARG(source_uid);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send an RPT status message from a device.
 *
 * Status messages should only be sent in response to RDM commands received over RDMnet, if
 * something has gone wrong while attempting to resolve the command.
 *
 * \param[in] handle Handle to the device from which to send the RPT status.
 * \param[in] received_cmd Previously-received command that the status message is a response to.
 * \param[in] status_code RPT status code to send.
 * \param[in] status_string (optional) status string to send. NULL for no string.
 * \return #kEtcPalErrOk: Status sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_status(rdmnet_device_t handle, const RdmnetSavedRdmCommand* received_cmd,
                                         rpt_status_code_t status_code, const char* status_string)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(status_code);
  ETCPAL_UNUSED_ARG(status_string);
  return kEtcPalErrNotImpl;
  //  if (!handle)
  //    return kEtcPalErrInvalid;
  //
  //  return rdmnet_rpt_client_send_status(handle->client_handle, handle->scope_handle, received_cmd, status_code,
  //                                       status_string);
}

/*!
 * \brief Send an ACK response to an RDM command received over LLRP.
 *
 * \param[in] handle Handle to the device from which to send the LLRP RDM ACK response.
 * \param[in] received_cmd Previously-received command that the ACK is a response to.
 * \param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * \param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * \return #kEtcPalErrOk: LLRP ACK response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_llrp_ack(rdmnet_device_t handle, const LlrpSavedRdmCommand* received_cmd,
                                           const uint8_t* response_data, uint8_t response_data_len)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(response_data);
  ETCPAL_UNUSED_ARG(response_data_len);
  return kEtcPalErrNotImpl;
  //  if (!handle)
  //    return kEtcPalErrInvalid;
  //
  //  return rdmnet_rpt_client_send_llrp_ack(handle->client_handle, received_cmd, response_data, response_data_len);
}

/*!
 * \brief Send an ACK response to an RDM command received over LLRP.
 *
 * \param[in] handle Handle to the device from which to send the LLRP RDM NACK response.
 * \param[in] received_cmd Previously-received command that the ACK is a response to.
 * \param[in] nack_reason RDM NACK reason code to send with the NACK.
 * \return #kEtcPalErrOk: LLRP NACK response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_llrp_nack(rdmnet_device_t handle, const LlrpSavedRdmCommand* received_cmd,
                                            rdm_nack_reason_t nack_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(nack_reason);
  return kEtcPalErrNotImpl;
  //  if (!handle)
  //    return kEtcPalErrInvalid;
  //
  //  return rdmnet_rpt_client_send_llrp_nack(handle->client_handle, received_cmd, nack_reason);
}

/*!
 * \brief Add a physical endpoint to a device.
 * \details See \ref devices_and_gateways for more information on endpoints.
 * \param handle Handle to the device to which to add a physical endpoint.
 * \param endpoint_config Configuration information for the new physical endpoint.
 * \return #kEtcPalErrOk: Endpoint added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_physical_endpoint(rdmnet_device_t handle,
                                                   const RdmnetPhysicalEndpointConfig* endpoint_config)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_config);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Add multiple physical endpoints to a device.
 * \details See \ref devices_and_gateways for more information on endpoints.
 * \param handle Handle to the device to which to add physical endpoints.
 * \param endpoint_configs Array of configuration structures for the new physical endpoints.
 * \param num_endpoints Size of endpoint_configs array.
 * \return #kEtcPalErrOk: Endpoints added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_physical_endpoints(rdmnet_device_t handle,
                                                    const RdmnetPhysicalEndpointConfig* endpoint_configs,
                                                    size_t num_endpoints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_configs);
  ETCPAL_UNUSED_ARG(num_endpoints);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Add a virtual endpoint to a device.
 * \details See \ref devices_and_gateways for more information on endpoints.
 * \param handle Handle to the device to which to add a virtual endpoint.
 * \param endpoint_config Configuration information for the new virtual endpoint.
 * \return #kEtcPalErrOk: Endpoint added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_virtual_endpoint(rdmnet_device_t handle,
                                                  const RdmnetVirtualEndpointConfig* endpoint_config)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_config);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Add multiple virtual endpoints to a device.
 * \details See \ref devices_and_gateways for more information on endpoints.
 * \param handle Handle to the device to which to add virtual endpoints.
 * \param endpoint_configs Array of configuration structures for the new virtual endpoints.
 * \param num_endpoints Size of endpoint_configs array.
 * \return #kEtcPalErrOk: Endpoints added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_virtual_endpoints(rdmnet_device_t handle,
                                                   const RdmnetVirtualEndpointConfig* endpoint_configs,
                                                   size_t num_endpoints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_configs);
  ETCPAL_UNUSED_ARG(num_endpoints);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Remove an endpoint from a device.
 * \details See \ref devices_and_gateways for more information on endpoints.
 * \param handle Handle to the device from which to remove an endpoint.
 * \param endpoint_id ID of the endpoint to remove.
 * \return #kEtcPalErrOk: Endpoint removed sucessfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or
 *         endpoint_id is not an endpoint that was previously added.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_endpoint(rdmnet_device_t handle, uint16_t endpoint_id)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_id);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Remove multiple endpoints from a device.
 * \details See \ref devices_and_gateways for more information on endpoints.
 * \param handle Handle to the device from which to remove endpoints.
 * \param endpoint_ids Array of IDs representing the endpoints to remove.
 * \param num_endpoints Size of the endpoint_ids array.
 * \return #kEtcPalErrOk: Endpoints removed sucessfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or one or
 *         more endpoint_ids are not endpoints that were previously added.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_endpoints(rdmnet_device_t handle, const uint16_t* endpoint_ids,
                                              size_t num_endpoints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_ids);
  ETCPAL_UNUSED_ARG(num_endpoints);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Add one or more responders with static UIDs to an endpoint.
 *
 * This can be used to add responders discovered on a physical endpoint, or to add virtual
 * responders with static UIDs to a virtual endpoint. Add the endpoint first with
 * rdmnet_device_add_physical_endpoint() or rdmnet_device_add_virtual_endpoint(). See
 * \ref devices_and_gateways for more information on endpoints.
 *
 * \param handle Handle to the device to which to add responders.
 * \param endpoint_id ID for the endpoint on which to add the responders.
 * \param responder_uids Array of static responder UIDs.
 * \param num_responders Size of the responder_uids array.
 * \return #kEtcPalErrOk: Responders added sucessfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or
 *         endpoint_id is not an endpoint that was previously added.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_static_responders(rdmnet_device_t handle, uint16_t endpoint_id,
                                                   const RdmUid* responder_uids, size_t num_responders)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_uids);
  ETCPAL_UNUSED_ARG(num_responders);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Add one or more responders with dynamic UIDs to a virtual endpoint.
 *
 * This function can only be used on virtual endpoints. Dynamic UIDs for the responders will be
 * requested from the broker and the assigned UIDs (or error codes) will be delivered to the
 * device's RdmnetDeviceDynamicUidStatusCallback. Save these UIDs for comparison when handling RDM
 * commands addressed to the dynamic responders. Add the endpoint first with
 * rdmnet_device_add_virtual_endpoint(). See \ref devices_and_gateways for more information on
 * endpoints.
 *
 * \param handle Handle to the device to which to add responders.
 * \param endpoint_id ID for the endpoint on which to add the responders.
 * \param responder_ids Array of responder IDs (permanent UUIDs representing the responder).
 * \param num_responders Size of the responder_ids array.
 * \return #kEtcPalErrOk: Responders added sucessfully (pending dynamic UID assignment).
 * \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or
 *         endpoint_id is not an endpoint that was previously added.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_dynamic_responders(rdmnet_device_t handle, uint16_t endpoint_id,
                                                    const EtcPalUuid* responder_ids, size_t num_responders)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_ids);
  ETCPAL_UNUSED_ARG(num_responders);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Remove one or more responders with static UIDs from an endpoint.
 *
 * This can be used to remove responders previously discovered on a physical endpoint, or to remove
 * virtual responders with static UIDs from a virtual endpoint. See \ref devices_and_gateways for
 * more information on endpoints.
 *
 * \param handle Handle to the device from which to remove responders.
 * \param endpoint_id ID for the endpoint on which to remove the responders.
 * \param responder_uids Array of static responder UIDs to remove.
 * \param num_responders Size of the responder_uids array.
 * \return #kEtcPalErrOk: Responders removed sucessfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, endpoint_id
 *         is not an endpoint that was previously added, or one or more responder_uids were not
 *         previously added to the endpoint.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_static_responders(rdmnet_device_t handle, uint16_t endpoint_id,
                                                      const RdmUid* responder_uids, size_t num_responders)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_uids);
  ETCPAL_UNUSED_ARG(num_responders);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Remove one or more responder with dynamic UIDs from a virtual endpoint.
 *
 * This function can only be used on virtual endpoints. See \ref devices_and_gateways for more
 * information on endpoints.
 *
 * \param handle Handle to the device from which to remove responders.
 * \param endpoint_id ID for the endpoint on which to remove the responders.
 * \param responder_ids Array of responder IDs to remove.
 * \param num_responders Size of responder_ids array.
 * \return #kEtcPalErrOk: Responders removed sucessfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, endpoint_id
 *         is not an endpoint that was previously added, or one or more responder_uids were not
 *         previously added to the endpoint.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_dynamic_responders(rdmnet_device_t handle, uint16_t endpoint_id,
                                                       const EtcPalUuid* responder_ids, size_t num_responders)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_ids);
  ETCPAL_UNUSED_ARG(num_responders);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Change the device's scope.
 *
 * Will disconnect from the current scope, sending the disconnect reason provided in the
 * disconnect_reason parameter, and then attempt to discover and connect to a broker for the new
 * scope. The status of the connection attempt will be communicated via the callbacks associated
 * with the device instance.
 *
 * \param handle Handle to the device for which to change the scope.
 * \param new_scope_config Configuration information for the new scope.
 * \param disconnect_reason Disconnect reason to send on the previous scope, if connected.
 * \return #kEtcPalErrOk: Scope changed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_change_scope(rdmnet_device_t handle, const RdmnetScopeConfig* new_scope_config,
                                          rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_scope_config);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
  //  if (!handle)
  //    return kEtcPalErrInvalid;
  //
  //  return rdmnet_client_change_scope(handle->client_handle, handle->scope_handle, new_scope_config,
  //  disconnect_reason);
}

/*!
 * \brief Change the device's DNS search domain.
 *
 * Non-default search domains are considered advanced usage. If the device's scope does not have a
 * static broker configuration, the scope will be disconnected, sending the disconnect reason
 * provided in the disconnect_reason parameter. Then discovery will be re-attempted on the new
 * search domain.
 *
 * \param handle Handle to the device for which to change the search domain.
 * \param new_search_domain New search domain to use for discovery.
 * \param disconnect_reason Disconnect reason to send to the broker, if connected.
 * \return #kEtcPalErrOk: Search domain changed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_change_search_domain(rdmnet_device_t handle, const char* new_search_domain,
                                                  rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_search_domain);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
  //  if (!handle)
  //    return kEtcPalErrInvalid;
  //
  //  return rdmnet_client_change_search_domain(handle->client_handle, new_search_domain, disconnect_reason);
}

// void client_connected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo*
// info,
//                      void* context)
//{
//  ETCPAL_UNUSED_ARG(handle);
//
//  RdmnetDevice* device = (RdmnetDevice*)context;
//  if (device && scope_handle == device->scope_handle)
//  {
//    device->callbacks.connected(device, info, device->callback_context);
//  }
//}
//
// void client_connect_failed(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
//                           const RdmnetClientConnectFailedInfo* info, void* context)
//{
//  ETCPAL_UNUSED_ARG(handle);
//
//  RdmnetDevice* device = (RdmnetDevice*)context;
//  if (device && scope_handle == device->scope_handle)
//  {
//    device->callbacks.connect_failed(device, info, device->callback_context);
//  }
//}
//
// void client_disconnected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
//                         const RdmnetClientDisconnectedInfo* info, void* context)
//{
//  ETCPAL_UNUSED_ARG(handle);
//
//  RdmnetDevice* device = (RdmnetDevice*)context;
//  if (device && scope_handle == device->scope_handle)
//  {
//    device->callbacks.disconnected(device, info, device->callback_context);
//  }
//}
//
// void client_broker_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const BrokerMessage* msg,
//                                void* context)
//{
//  ETCPAL_UNUSED_ARG(handle);
//  ETCPAL_UNUSED_ARG(scope_handle);
//  ETCPAL_UNUSED_ARG(context);
//
//  etcpal_log(rdmnet_log_params, ETCPAL_LOG_INFO, "Got Broker message with vector %d", msg->vector);
//}
//
// llrp_response_action_t client_llrp_msg_received(rdmnet_client_t handle, const LlrpRemoteRdmCommand* cmd,
//                                                LlrpSyncRdmResponse* response, void* context)
//{
//  ETCPAL_UNUSED_ARG(handle);
//
//  RdmnetDevice* device = (RdmnetDevice*)context;
//  if (device)
//  {
//    return device->callbacks.llrp_rdm_command_received(device, cmd, response, device->callback_context);
//  }
//  return kLlrpResponseActionDefer;
//}
//
// rdmnet_response_action_t client_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
//                                             const RptClientMessage* msg, RdmnetSyncRdmResponse* response,
//                                             void* context)
//{
//  ETCPAL_UNUSED_ARG(handle);
//
//  RdmnetDevice* device = (RdmnetDevice*)context;
//  if (device && scope_handle == device->scope_handle)
//  {
//    if (msg->type == kRptClientMsgRdmCmd)
//    {
//      return device->callbacks.rdm_command_received(device, &msg->payload.cmd, response, device->callback_context);
//    }
//    else
//    {
//      RDMNET_LOG_INFO("Device incorrectly got non-RDM-command message.");
//    }
//  }
//  return kRdmnetRdmResponseActionDefer;
//}
