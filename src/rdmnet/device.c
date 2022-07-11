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

#include "rdmnet/device.h"

#include <stddef.h>
#include <string.h>
#include "etcpal/common.h"
#include "etcpal/mutex.h"
#include "etcpal/pack.h"
#include "etcpal/rbtree.h"
#include "rdmnet/common_priv.h"
#include "rdmnet/core/client.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/**************************** Private constants ******************************/

#define INITIAL_ENDPOINT_RESPONDER_CAPACITY 8

/***************************** Private macros ********************************/

#define GET_DEVICE_FROM_CLIENT(clientptr) (RdmnetDevice*)((char*)(clientptr)-offsetof(RdmnetDevice, client))
#define ENDPOINT_ID_VALID(id) (id != 0 && id < 64000)

#define DEVICE_LOCK(device_ptr) etcpal_mutex_lock(&(device_ptr)->lock)
#define DEVICE_UNLOCK(device_ptr) etcpal_mutex_unlock(&(device_ptr)->lock)

/*********************** Private function prototypes *************************/

static etcpal_error_t validate_device_config(const RdmnetDeviceConfig* config);
static bool           validate_device_callbacks(const RdmnetDeviceCallbacks* callbacks);
static etcpal_error_t validate_physical_endpoints(const RdmnetPhysicalEndpointConfig* endpoints, size_t num_endpoints);
static etcpal_error_t validate_virtual_endpoints(const RdmnetVirtualEndpointConfig* endpoints, size_t num_endpoints);
static etcpal_error_t create_new_device(const RdmnetDeviceConfig* config, rdmnet_device_t* handle);
static etcpal_error_t get_device(rdmnet_device_t handle, RdmnetDevice** device);
static void           release_device(RdmnetDevice* device);

static bool add_virtual_endpoints(RdmnetDevice*                      device,
                                  const RdmnetVirtualEndpointConfig* endpoints,
                                  size_t                             num_endpoints);
static bool add_physical_endpoints(RdmnetDevice*                       device,
                                   const RdmnetPhysicalEndpointConfig* endpoints,
                                   size_t                              num_endpoints);

static bool remove_endpoints(RdmnetDevice* device, const uint16_t* endpoint_ids, size_t num_endpoints);

static void notify_endpoint_list_change(RdmnetDevice* device);
static void notify_endpoint_responder_list_change(RdmnetDevice* device, DeviceEndpoint* endpoint);

static DeviceEndpoint* find_endpoint(RdmnetDevice* device, uint16_t endpoint_id);

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

static bool handle_assigned_dynamic_uids(RdmnetDevice* device, const RdmnetDynamicUidAssignmentList* assignment_list);
static bool handle_rdm_command_internally(RdmnetDevice*           device,
                                          const RdmCommandHeader* rdm_header,
                                          const uint8_t*          data,
                                          uint8_t                 data_len,
                                          RdmnetSyncRdmResponse*  response);

static void handle_endpoint_list(RdmnetDevice*           device,
                                 const RdmCommandHeader* rdm_header,
                                 RdmnetSyncRdmResponse*  response);
static void handle_endpoint_list_change(RdmnetDevice*           device,
                                        const RdmCommandHeader* rdm_header,
                                        RdmnetSyncRdmResponse*  response);
static void handle_endpoint_responders(RdmnetDevice*           device,
                                       const RdmCommandHeader* rdm_header,
                                       const uint8_t*          data,
                                       uint8_t                 data_len,
                                       RdmnetSyncRdmResponse*  response);
static void handle_endpoint_responder_list_change(RdmnetDevice*           device,
                                                  const RdmCommandHeader* rdm_header,
                                                  const uint8_t*          data,
                                                  uint8_t                 data_len,
                                                  RdmnetSyncRdmResponse*  response);
static void handle_binding_control_fields(RdmnetDevice*           device,
                                          const RdmCommandHeader* rdm_header,
                                          const uint8_t*          data,
                                          uint8_t                 data_len,
                                          RdmnetSyncRdmResponse*  response);

// clang-format off
 static const RCClientCommonCallbacks client_callbacks =
{
  client_connected,
  client_connect_failed,
  client_disconnected,
  client_broker_msg_received,
  client_destroyed
};

 static const RCRptClientCallbacks rpt_client_callbacks =
{
  client_llrp_msg_received,
  client_rpt_msg_received
};
// clang-format on

/*************************** Function definitions ****************************/

/**
 * @brief Initialize an RDMnet Device Config with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meanizngfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * @code
 * RdmnetDeviceConfig config;
 * RDMNET_DEVICE_CONFIG_INIT(&config, 0x6574);
 * @endcode
 *
 * @param[out] config Pointer to RdmnetDeviceConfig to init.
 * @param[in] manufacturer_id ESTA manufacturer ID. All RDMnet Devices must have one.
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

/**
 * @brief Set the main callbacks in an RDMnet device configuration structure.
 *
 * Items marked "optional" can be NULL.
 *
 * @param[out] config Config struct in which to set the callbacks.
 * @param[in] connected Callback called when the device has connected to a broker.
 * @param[in] connect_failed Callback called when a connection to a broker has failed.
 * @param[in] disconnected Callback called when a connection to a broker has disconnected.
 * @param[in] rdm_command_received Callback called when a device receives an RDM command.
 * @param[in] llrp_rdm_command_received Callback called when a device receives an RDM command over
 *                                      LLRP.
 * @param[in] dynamic_uid_status_received (optional) Callback called when a device receives dynamic
 *                                        UID assignments for one or more virtual responders.
 * @param[in] context (optional) Pointer to opaque data passed back with each callback.
 */
void rdmnet_device_set_callbacks(RdmnetDeviceConfig*                        config,
                                 RdmnetDeviceConnectedCallback              connected,
                                 RdmnetDeviceConnectFailedCallback          connect_failed,
                                 RdmnetDeviceDisconnectedCallback           disconnected,
                                 RdmnetDeviceRdmCommandReceivedCallback     rdm_command_received,
                                 RdmnetDeviceLlrpRdmCommandReceivedCallback llrp_rdm_command_received,
                                 RdmnetDeviceDynamicUidStatusCallback       dynamic_uid_status_received,
                                 void*                                      context)
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

/**
 * @brief Create a new instance of RDMnet device functionality.
 *
 * Each device is identified by a single component ID (CID). Typical device applications will only
 * need one device instance. The library will attempt to discover and connect to a broker for the
 * scope given in config->scope_config (or just connect if a static broker is given); the status of
 * these attempts will be communicated via the callbacks associated with the device instance.
 *
 * @param[in] config Configuration parameters to use for this device instance.
 * @param[out] handle Filled in on success with a handle to the new device instance.
 * @return #kEtcPalErrOk: Device created successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No memory to allocate new device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_create(const RdmnetDeviceConfig* config, rdmnet_device_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = validate_device_config(config);
  if (res != kEtcPalErrOk)
    return res;

  if (rdmnet_writelock())
  {
    res = create_new_device(config, handle);
    rdmnet_writeunlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/**
 * @brief Destroy a device instance.
 *
 * Will disconnect from the broker to which this device is currently connected (if applicable),
 * sending the disconnect reason provided in the disconnect_reason parameter.
 *
 * @param[in] handle Handle to device to destroy, no longer valid after this function returns.
 * @param[in] disconnect_reason Disconnect reason code to send to the connected broker.
 * @return #kEtcPalErrOk: Device destroyed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_destroy(rdmnet_device_t handle, rdmnet_disconnect_reason_t disconnect_reason)
{
  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  bool destroy_immediately = rc_client_unregister(&device->client, disconnect_reason);
  rdmnet_unregister_struct_instance(device);
  release_device(device);

  if (destroy_immediately)
    rdmnet_free_struct_instance(device);
  return res;
}

/**
 * @brief Send an RDM ACK response from a device.
 *
 * @param[in] handle Handle to the device from which to send the RDM ACK response.
 * @param[in] received_cmd Previously-received command that the ACK is a response to.
 * @param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * @param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * @return #kEtcPalErrOk: ACK response sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_rdm_ack(rdmnet_device_t              handle,
                                          const RdmnetSavedRdmCommand* received_cmd,
                                          const uint8_t*               response_data,
                                          size_t                       response_data_len)
{
  if (!received_cmd)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_ack(&device->client, device->scope_handle, received_cmd, response_data, response_data_len);
  release_device(device);
  return res;
}

/**
 * @brief Send an RDM NACK response from a device.
 *
 * @param[in] handle Handle to the device from which to send the RDM NACK response.
 * @param[in] received_cmd Previously-received command that the NACK is a response to.
 * @param[in] nack_reason RDM NACK reason code to send with the NACK.
 * @return #kEtcPalErrOk: NACK response sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_rdm_nack(rdmnet_device_t              handle,
                                           const RdmnetSavedRdmCommand* received_cmd,
                                           rdm_nack_reason_t            nack_reason)
{
  if (!received_cmd)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_nack(&device->client, device->scope_handle, received_cmd, nack_reason);
  release_device(device);
  return res;
}

/**
 * @brief Send an asynchronous RDM GET response to update the value of a local parameter.
 *
 * This version is for updating a parameter on the device's default responder. For updates from
 * sub-responders, use rdmnet_device_send_rdm_udpate_from_responder(). See
 * @ref devices_and_gateways for more information.
 *
 * @param[in] handle Handle to the device from which to send the updated RDM data.
 * @param[in] subdevice The subdevice of the default responder from which the update is being sent
 *                      (0 for the root device).
 * @param[in] param_id The RDM parameter ID that has been updated.
 * @param[in] data The updated parameter data, or NULL for messages with no data.
 * @param[in] data_len The length of the updated parameter data, or 0 for messages with no data.
 * @return #kEtcPalErrOk: RDM update sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_rdm_update(rdmnet_device_t handle,
                                             uint16_t        subdevice,
                                             uint16_t        param_id,
                                             const uint8_t*  data,
                                             size_t          data_len)
{
  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_update(&device->client, device->scope_handle, subdevice, param_id, data, data_len);
  release_device(device);
  return res;
}

/**
 * @brief Send an asynchronous RDM GET response to update the value of a parameter on a sub-responder.
 *
 * This version is for updating a parameter on a physical or vitual responder associated with one
 * of a device's endpoints. In particular, this is the one for a gateway to use when it collects a
 * new queued message from a responder. See @ref devices_and_gateways for more information.
 *
 * @param[in] handle Handle to the device from which to send the updated RDM data.
 * @param[in] source_addr The addressing information of the responder that has an updated parameter.
 * @param[in] param_id The RDM parameter ID that has been updated.
 * @param[in] data The updated parameter data, or NULL for messages with no data.
 * @param[in] data_len The length of the updated parameter data, or 0 for messages with no data.
 * @return #kEtcPalErrOk: RDM update sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_rdm_update_from_responder(rdmnet_device_t         handle,
                                                            const RdmnetSourceAddr* source_addr,
                                                            uint16_t                param_id,
                                                            const uint8_t*          data,
                                                            size_t                  data_len)
{
  if (!source_addr)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rdm_update_from_responder(&device->client, device->scope_handle, source_addr, param_id, data,
                                                 data_len);
  release_device(device);
  return res;
}

/**
 * @brief Send an RPT status message from a device.
 *
 * Status messages should only be sent in response to RDM commands received over RDMnet, if
 * something has gone wrong while attempting to resolve the command.
 *
 * @param[in] handle Handle to the device from which to send the RPT status.
 * @param[in] received_cmd Previously-received command that the status message is a response to.
 * @param[in] status_code RPT status code to send.
 * @param[in] status_string (optional) status string to send. NULL for no string.
 * @return #kEtcPalErrOk: Status sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_status(rdmnet_device_t              handle,
                                         const RdmnetSavedRdmCommand* received_cmd,
                                         rpt_status_code_t            status_code,
                                         const char*                  status_string)
{
  if (!received_cmd)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_rpt_status(&device->client, device->scope_handle, received_cmd, status_code, status_string);
  release_device(device);
  return res;
}

/**
 * @brief Send an ACK response to an RDM command received over LLRP.
 *
 * @param[in] handle Handle to the device from which to send the LLRP RDM ACK response.
 * @param[in] received_cmd Previously-received command that the ACK is a response to.
 * @param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * @param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * @return #kEtcPalErrOk: LLRP ACK response sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_llrp_ack(rdmnet_device_t            handle,
                                           const LlrpSavedRdmCommand* received_cmd,
                                           const uint8_t*             response_data,
                                           uint8_t                    response_data_len)
{
  if (!received_cmd)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_llrp_ack(&device->client, received_cmd, response_data, response_data_len);
  release_device(device);
  return res;
}

/**
 * @brief Send an ACK response to an RDM command received over LLRP.
 *
 * @param[in] handle Handle to the device from which to send the LLRP RDM NACK response.
 * @param[in] received_cmd Previously-received command that the ACK is a response to.
 * @param[in] nack_reason RDM NACK reason code to send with the NACK.
 * @return #kEtcPalErrOk: LLRP NACK response sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_send_llrp_nack(rdmnet_device_t            handle,
                                            const LlrpSavedRdmCommand* received_cmd,
                                            rdm_nack_reason_t          nack_reason)
{
  if (!received_cmd)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_send_llrp_nack(&device->client, received_cmd, nack_reason);
  release_device(device);
  return res;
}

/**
 * @brief Add a physical endpoint to a device.
 * @details See @ref devices_and_gateways for more information on endpoints.
 * @param handle Handle to the device to which to add a physical endpoint.
 * @param endpoint_config Configuration information for the new physical endpoint.
 * @return #kEtcPalErrOk: Endpoint added successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Could not allocate memory for additional endpoint.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_physical_endpoint(rdmnet_device_t                     handle,
                                                   const RdmnetPhysicalEndpointConfig* endpoint_config)
{
  if (!endpoint_config)
    return kEtcPalErrInvalid;
  etcpal_error_t res = validate_physical_endpoints(endpoint_config, 1);
  if (res != kEtcPalErrOk)
    return res;

  RdmnetDevice* device;
  res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  if (add_physical_endpoints(device, endpoint_config, 1))
    notify_endpoint_list_change(device);
  else
    res = kEtcPalErrNoMem;

  release_device(device);
  return res;
}

/**
 * @brief Add multiple physical endpoints to a device.
 * @details See @ref devices_and_gateways for more information on endpoints.
 * @param handle Handle to the device to which to add physical endpoints.
 * @param endpoint_configs Array of configuration structures for the new physical endpoints.
 * @param num_endpoints Size of endpoint_configs array.
 * @return #kEtcPalErrOk: Endpoints added successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Could not allocate memory for additional endpoints.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_physical_endpoints(rdmnet_device_t                     handle,
                                                    const RdmnetPhysicalEndpointConfig* endpoint_configs,
                                                    size_t                              num_endpoints)
{
  if (!endpoint_configs || num_endpoints == 0)
    return kEtcPalErrInvalid;
  etcpal_error_t res = validate_physical_endpoints(endpoint_configs, num_endpoints);
  if (res != kEtcPalErrOk)
    return res;

  RdmnetDevice* device;
  res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  if (add_physical_endpoints(device, endpoint_configs, num_endpoints))
    notify_endpoint_list_change(device);
  else
    res = kEtcPalErrNoMem;

  release_device(device);
  return res;
}

/**
 * @brief Add a virtual endpoint to a device.
 * @details See @ref devices_and_gateways for more information on endpoints.
 * @param handle Handle to the device to which to add a virtual endpoint.
 * @param endpoint_config Configuration information for the new virtual endpoint.
 * @return #kEtcPalErrOk: Endpoint added successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Could not allocate memory for additional endpoint.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_virtual_endpoint(rdmnet_device_t                    handle,
                                                  const RdmnetVirtualEndpointConfig* endpoint_config)
{
  if (!endpoint_config)
    return kEtcPalErrInvalid;
  etcpal_error_t res = validate_virtual_endpoints(endpoint_config, 1);
  if (res != kEtcPalErrOk)
    return res;

  RdmnetDevice* device;
  res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  if (add_virtual_endpoints(device, endpoint_config, 1))
    notify_endpoint_list_change(device);
  else
    res = kEtcPalErrNoMem;

  release_device(device);
  return res;
}

/**
 * @brief Add multiple virtual endpoints to a device.
 * @details See @ref devices_and_gateways for more information on endpoints.
 * @param handle Handle to the device to which to add virtual endpoints.
 * @param endpoint_configs Array of configuration structures for the new virtual endpoints.
 * @param num_endpoints Size of endpoint_configs array.
 * @return #kEtcPalErrOk: Endpoints added successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Could not allocate memory for additional endpoints.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_virtual_endpoints(rdmnet_device_t                    handle,
                                                   const RdmnetVirtualEndpointConfig* endpoint_configs,
                                                   size_t                             num_endpoints)
{
  if (!endpoint_configs || num_endpoints == 0)
    return kEtcPalErrInvalid;
  etcpal_error_t res = validate_virtual_endpoints(endpoint_configs, num_endpoints);
  if (res != kEtcPalErrOk)
    return res;

  RdmnetDevice* device;
  res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  if (add_virtual_endpoints(device, endpoint_configs, num_endpoints))
    notify_endpoint_list_change(device);
  else
    res = kEtcPalErrNoMem;

  release_device(device);
  return res;
}

/**
 * @brief Remove an endpoint from a device.
 * @details See @ref devices_and_gateways for more information on endpoints.
 * @param handle Handle to the device from which to remove an endpoint.
 * @param endpoint_id ID of the endpoint to remove.
 * @return #kEtcPalErrOk: Endpoint removed sucessfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or
 *         endpoint_id is not an endpoint that was previously added.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_endpoint(rdmnet_device_t handle, uint16_t endpoint_id)
{
  if (!ENDPOINT_ID_VALID(endpoint_id))
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  if (remove_endpoints(device, &endpoint_id, 1))
    notify_endpoint_list_change(device);
  else
    res = kEtcPalErrNotFound;

  release_device(device);
  return res;
}

/**
 * @brief Remove multiple endpoints from a device.
 * @details See @ref devices_and_gateways for more information on endpoints.
 * @param handle Handle to the device from which to remove endpoints.
 * @param endpoint_ids Array of IDs representing the endpoints to remove.
 * @param num_endpoints Size of the endpoint_ids array.
 * @return #kEtcPalErrOk: Endpoints removed sucessfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or one or
 *         more endpoint_ids are not endpoints that were previously added.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_endpoints(rdmnet_device_t handle,
                                              const uint16_t* endpoint_ids,
                                              size_t          num_endpoints)
{
  if (!endpoint_ids || num_endpoints == 0)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  if (remove_endpoints(device, endpoint_ids, num_endpoints))
    notify_endpoint_list_change(device);
  else
    res = kEtcPalErrNotFound;

  release_device(device);
  return res;
}

/**
 * @brief Add one or more responders with static UIDs to a virtual endpoint.
 *
 * This function can only be used on virtual endpoints. Add the endpoint first with
 * rdmnet_device_add_virtual_endpoint(). See @ref devices_and_gateways for more information on
 * endpoints.
 *
 * @param handle Handle to the device to which to add responders.
 * @param endpoint_id ID for the endpoint on which to add the responders.
 * @param responder_uids Array of static responder UIDs.
 * @param num_responders Size of the responder_uids array.
 * @return #kEtcPalErrOk: Responders added sucessfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Could not allocate memory for additional responders.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or
 *         endpoint_id is not an endpoint that was previously added.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_static_responders(rdmnet_device_t handle,
                                                   uint16_t        endpoint_id,
                                                   const RdmUid*   responder_uids,
                                                   size_t          num_responders)
{
  if (!responder_uids || num_responders == 0)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
    res = kEtcPalErrNotFound;
  else if (endpoint->type != kDeviceEndpointTypeVirtual)
    res = kEtcPalErrInvalid;

  if (res == kEtcPalErrOk)
    res = rdmnet_add_static_responders(device, endpoint, responder_uids, num_responders);

  if (res == kEtcPalErrOk)
    notify_endpoint_responder_list_change(device, endpoint);

  release_device(device);
  return res;
}

/**
 * @brief Add one or more responders with dynamic UIDs to a virtual endpoint.
 *
 * This function can only be used on virtual endpoints. Dynamic UIDs for the responders will be
 * requested from the broker and the assigned UIDs (or error codes) will be delivered to the
 * device's RdmnetDeviceDynamicUidStatusCallback. Save these UIDs for comparison when handling RDM
 * commands addressed to the dynamic responders. Add the endpoint first with
 * rdmnet_device_add_virtual_endpoint(). See @ref devices_and_gateways for more information on
 * endpoints.
 *
 * @param handle Handle to the device to which to add responders.
 * @param endpoint_id ID for the endpoint on which to add the responders.
 * @param responder_ids Array of responder IDs (permanent UUIDs representing the responder).
 * @param num_responders Size of the responder_ids array.
 * @return #kEtcPalErrOk: Responders added sucessfully (pending dynamic UID assignment).
 * @return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Could not allocate memory for additional responders.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or
 *         endpoint_id is not an endpoint that was previously added.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_dynamic_responders(rdmnet_device_t   handle,
                                                    uint16_t          endpoint_id,
                                                    const EtcPalUuid* responder_ids,
                                                    size_t            num_responders)
{
  if (!responder_ids || num_responders == 0)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
    res = kEtcPalErrNotFound;
  else if (endpoint->type != kDeviceEndpointTypeVirtual)
    res = kEtcPalErrInvalid;

  if (res == kEtcPalErrOk)
    res = rdmnet_add_dynamic_responders(device, endpoint, device->manufacturer_id, responder_ids, num_responders);

  if (res == kEtcPalErrOk)
  {
    if (device->connected_to_broker)
      res = rc_client_request_dynamic_uids(&device->client, device->scope_handle, responder_ids, num_responders);
  }

  release_device(device);
  return res;
}

/**
 * @brief Add one or more responders to a physical endpoint.
 *
 * This function can only be used on physical endpoints. Add the endpoint first with
 * rdmnet_device_add_physical_endpoint(). See @ref devices_and_gateways for more information on
 * endpoints.
 *
 * @param handle Handle to the device to which to add responders.
 * @param endpoint_id ID for the endpoint on which to add the responders.
 * @param responders Array of physical responder structures.
 * @param num_responders Size of the responders array.
 * @return #kEtcPalErrOk: Responders added sucessfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Could not allocate memory for additional responders.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, or
 *         endpoint_id is not an endpoint that was previously added.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_add_physical_responders(rdmnet_device_t                        handle,
                                                     uint16_t                               endpoint_id,
                                                     const RdmnetPhysicalEndpointResponder* responders,
                                                     size_t                                 num_responders)
{
  if (!responders || num_responders == 0)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
    res = kEtcPalErrNotFound;
  else if (endpoint->type != kDeviceEndpointTypePhysical)
    res = kEtcPalErrInvalid;

  if (res == kEtcPalErrOk)
    res = rdmnet_add_physical_responders(device, endpoint, responders, num_responders);

  if (res == kEtcPalErrOk)
    notify_endpoint_responder_list_change(device, endpoint);

  release_device(device);
  return res;
}

/**
 * @brief Remove one or more responders with static UIDs from a virtual endpoint.
 *
 * This function can only be used on virtual endpoints. See @ref devices_and_gateways for more
 * information on endpoints.
 *
 * @param handle Handle to the device from which to remove responders.
 * @param endpoint_id ID for the endpoint on which to remove the responders.
 * @param responder_uids Array of static responder UIDs to remove.
 * @param num_responders Size of the responder_uids array.
 * @return #kEtcPalErrOk: Responders removed sucessfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, endpoint_id
 *         is not an endpoint that was previously added, or one or more responder_uids were not
 *         previously added to the endpoint.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_static_responders(rdmnet_device_t handle,
                                                      uint16_t        endpoint_id,
                                                      const RdmUid*   responder_uids,
                                                      size_t          num_responders)
{
  if (!responder_uids || num_responders == 0)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
    res = kEtcPalErrNotFound;
  else if (endpoint->type != kDeviceEndpointTypeVirtual)
    res = kEtcPalErrInvalid;

  if (res == kEtcPalErrOk)
  {
    // Make sure all of the responders exist
    for (const RdmUid* uid = responder_uids; uid < responder_uids + num_responders; ++uid)
    {
      if (!rdmnet_find_responder_by_uid(endpoint, uid))
      {
        res = kEtcPalErrNotFound;
        break;
      }
    }
  }

  if (res == kEtcPalErrOk)
  {
    rdmnet_remove_responders_by_uid(endpoint, responder_uids, num_responders);

    if (device->connected_to_broker)
    {
      // Send an RDM update
      // TODO change from hardcoded when we have RDM message packing code
      uint8_t update_buf[6];
      etcpal_pack_u16b(update_buf, endpoint_id);
      etcpal_pack_u32b(&update_buf[2], ++endpoint->responder_list_change_number);
      rc_client_send_rdm_update(&device->client, device->scope_handle, 0, E137_7_ENDPOINT_RESPONDER_LIST_CHANGE,
                                update_buf, 6);
    }

    notify_endpoint_responder_list_change(device, endpoint);
  }

  release_device(device);
  return res;
}

/**
 * @brief Remove one or more responders with dynamic UIDs from a virtual endpoint.
 *
 * This function can only be used on virtual endpoints. See @ref devices_and_gateways for more
 * information on endpoints.
 *
 * @param handle Handle to the device from which to remove responders.
 * @param endpoint_id ID for the endpoint on which to remove the responders.
 * @param responder_ids Array of responder IDs to remove.
 * @param num_responders Size of responder_ids array.
 * @return #kEtcPalErrOk: Responders removed sucessfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, endpoint_id
 *         is not an endpoint that was previously added, or one or more responder_ids were not
 *         previously added to the endpoint.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_dynamic_responders(rdmnet_device_t   handle,
                                                       uint16_t          endpoint_id,
                                                       const EtcPalUuid* responder_ids,
                                                       size_t            num_responders)
{
  if (!responder_ids || num_responders == 0)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
    res = kEtcPalErrNotFound;
  else if (endpoint->type != kDeviceEndpointTypeVirtual)
    res = kEtcPalErrInvalid;

  if (res == kEtcPalErrOk)
  {
    // Make sure all of the responders exist
    for (const EtcPalUuid* rid = responder_ids; rid < responder_ids + num_responders; ++rid)
    {
      if (!rdmnet_find_responder_by_rid(endpoint, rid))
      {
        res = kEtcPalErrNotFound;
        break;
      }
    }
  }

  if (res == kEtcPalErrOk)
  {
    rdmnet_remove_responders_by_rid(endpoint, responder_ids, num_responders);
    notify_endpoint_responder_list_change(device, endpoint);
  }

  release_device(device);
  return res;
}

/**
 * @brief Remove one or more responders from a physical endpoint.
 *
 * This function can only be used on physical endpoints. See @ref devices_and_gateways for more
 * information on endpoints.
 *
 * @param handle Handle to the device from which to remove responders.
 * @param endpoint_id ID for the endpoint on which to remove the responders.
 * @param responder_uids Array of responder UIDs to remove.
 * @param num_responders Size of responder_uids array.
 * @return #kEtcPalErrOk: Responders removed sucessfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance, endpoint_id
 *         is not an endpoint that was previously added, or one or more responder_uids were not
 *         previously added to the endpoint.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_remove_physical_responders(rdmnet_device_t handle,
                                                        uint16_t        endpoint_id,
                                                        const RdmUid*   responder_uids,
                                                        size_t          num_responders)
{
  if (!responder_uids || num_responders == 0)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
    res = kEtcPalErrNotFound;
  else if (endpoint->type != kDeviceEndpointTypePhysical)
    res = kEtcPalErrInvalid;

  if (res == kEtcPalErrOk)
  {
    // Make sure all of the responders exist
    for (const RdmUid* uid = responder_uids; uid < responder_uids + num_responders; ++uid)
    {
      if (!rdmnet_find_responder_by_uid(endpoint, uid))
      {
        res = kEtcPalErrNotFound;
        break;
      }
    }
  }

  if (res == kEtcPalErrOk)
  {
    rdmnet_remove_responders_by_uid(endpoint, responder_uids, num_responders);

    if (device->connected_to_broker)
    {
      // Send an RDM update
      // TODO change from hardcoded when we have RDM message packing code
      uint8_t update_buf[6];
      etcpal_pack_u16b(update_buf, endpoint_id);
      etcpal_pack_u32b(&update_buf[2], ++endpoint->responder_list_change_number);
      rc_client_send_rdm_update(&device->client, device->scope_handle, 0, E137_7_ENDPOINT_RESPONDER_LIST_CHANGE,
                                update_buf, 6);
    }

    notify_endpoint_responder_list_change(device, endpoint);
  }

  release_device(device);
  return res;
}

/**
 * @brief Change the device's scope.
 *
 * Will disconnect from the current scope, sending the disconnect reason provided in the
 * disconnect_reason parameter, and then attempt to discover and connect to a broker for the new
 * scope. The status of the connection attempt will be communicated via the callbacks associated
 * with the device instance.
 *
 * @param handle Handle to the device for which to change the scope.
 * @param new_scope_config Configuration information for the new scope.
 * @param disconnect_reason Disconnect reason to send on the previous scope, if connected.
 * @return #kEtcPalErrOk: Scope changed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_change_scope(rdmnet_device_t            handle,
                                          const RdmnetScopeConfig*   new_scope_config,
                                          rdmnet_disconnect_reason_t disconnect_reason)
{
  if (!new_scope_config)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_change_scope(&device->client, device->scope_handle, new_scope_config, disconnect_reason);

  release_device(device);
  return res;
}

/**
 * @brief Change the device's DNS search domain.
 *
 * Non-default search domains are considered advanced usage. If the device's scope does not have a
 * static broker configuration, the scope will be disconnected, sending the disconnect reason
 * provided in the disconnect_reason parameter. Then discovery will be re-attempted on the new
 * search domain.
 *
 * @param handle Handle to the device for which to change the search domain.
 * @param new_search_domain New search domain to use for discovery.
 * @param disconnect_reason Disconnect reason to send to the broker, if connected.
 * @return #kEtcPalErrOk: Search domain changed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_change_search_domain(rdmnet_device_t            handle,
                                                  const char*                new_search_domain,
                                                  rdmnet_disconnect_reason_t disconnect_reason)
{
  if (!new_search_domain || strlen(new_search_domain) == 0)
    return kEtcPalErrInvalid;

  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_change_search_domain(&device->client, new_search_domain, disconnect_reason);

  release_device(device);
  return res;
}

/**
 * @brief Retrieve the device's current scope configuration.
 *
 * @param[in] handle Handle to the device from which to retrieve the scope configuration.
 * @param[out] scope_str_buf Filled in on success with the scope string. Must be at least of length
 *                           E133_SCOPE_STRING_PADDED_LENGTH.
 * @param[out] static_broker_addr (optional) Filled in on success with the static broker address,
 *                                if present. Leave NULL if you don't care about the static broker
 *                                address.
 * @return #kEtcPalErrOk: Scope information retrieved successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid device instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_device_get_scope(rdmnet_device_t handle, char* scope_str_buf, EtcPalSockAddr* static_broker_addr)
{
  RdmnetDevice*  device;
  etcpal_error_t res = get_device(handle, &device);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_client_get_scope(&device->client, device->scope_handle, scope_str_buf, static_broker_addr);
  release_device(device);
  return res;
}

etcpal_error_t validate_device_config(const RdmnetDeviceConfig* config)
{
  if (ETCPAL_UUID_IS_NULL(&config->cid) || !validate_device_callbacks(&config->callbacks) ||
      !config->scope_config.scope || (!RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&config->uid) && (config->uid.manu & 0x8000)))
  {
    return kEtcPalErrInvalid;
  }

  etcpal_error_t res = validate_physical_endpoints(config->physical_endpoints, config->num_physical_endpoints);
  if (res != kEtcPalErrOk)
    return res;

  return validate_virtual_endpoints(config->virtual_endpoints, config->num_virtual_endpoints);
}

bool validate_device_callbacks(const RdmnetDeviceCallbacks* callbacks)
{
  return (callbacks->connected && callbacks->connect_failed && callbacks->disconnected &&
          callbacks->rdm_command_received && callbacks->llrp_rdm_command_received);
}

etcpal_error_t validate_physical_endpoints(const RdmnetPhysicalEndpointConfig* endpoints, size_t num_endpoints)
{
  if (endpoints && num_endpoints)
  {
#if !RDMNET_DYNAMIC_MEM
    if (num_endpoints > RDMNET_MAX_ENDPOINTS_PER_DEVICE)
      return kEtcPalErrNoMem;
#endif

    for (const RdmnetPhysicalEndpointConfig* endpoint = endpoints; endpoint < endpoints + num_endpoints; ++endpoint)
    {
      if (!ENDPOINT_ID_VALID(endpoint->endpoint_id))
        return kEtcPalErrInvalid;
#if !RDMNET_DYNAMIC_MEM
      if (endpoint->num_responders > RDMNET_MAX_RESPONDERS_PER_DEVICE_ENDPOINT)
        return kEtcPalErrNoMem;
#endif
    }
  }
  return kEtcPalErrOk;
}

etcpal_error_t validate_virtual_endpoints(const RdmnetVirtualEndpointConfig* endpoints, size_t num_endpoints)
{
  if (endpoints && num_endpoints)
  {
#if !RDMNET_DYNAMIC_MEM
    if (num_endpoints > RDMNET_MAX_ENDPOINTS_PER_DEVICE)
      return kEtcPalErrNoMem;
#endif

    for (const RdmnetVirtualEndpointConfig* endpoint = endpoints; endpoint < endpoints + num_endpoints; ++endpoint)
    {
      if (!ENDPOINT_ID_VALID(endpoint->endpoint_id))
        return kEtcPalErrInvalid;
#if !RDMNET_DYNAMIC_MEM
      if (endpoint->num_static_responders + endpoint->num_dynamic_responders >
          RDMNET_MAX_RESPONDERS_PER_DEVICE_ENDPOINT)
      {
        return kEtcPalErrNoMem;
      }
#endif
    }
  }
  return kEtcPalErrOk;
}

etcpal_error_t create_new_device(const RdmnetDeviceConfig* config, rdmnet_device_t* handle)
{
  etcpal_error_t res = kEtcPalErrNoMem;

  RdmnetDevice* new_device = rdmnet_alloc_device_instance();
  if (!new_device)
    return res;

  new_device->connected_to_broker = false;
  new_device->endpoint_list_change_number = 0;

  if (!add_physical_endpoints(new_device, config->physical_endpoints, config->num_physical_endpoints))
  {
    rdmnet_unregister_struct_instance(new_device);
    rdmnet_free_struct_instance(new_device);
    return res;
  }

  if (!add_virtual_endpoints(new_device, config->virtual_endpoints, config->num_virtual_endpoints))
  {
    rdmnet_unregister_struct_instance(new_device);
    rdmnet_free_struct_instance(new_device);
    return res;
  }

  RCClient* client = &new_device->client;
  client->lock = &new_device->lock;
  client->type = kClientProtocolRPT;
  client->cid = config->cid;
  client->callbacks = client_callbacks;
  RC_RPT_CLIENT_DATA(client)->type = kRPTClientTypeDevice;
  RC_RPT_CLIENT_DATA(client)->uid = config->uid;
  RC_RPT_CLIENT_DATA(client)->callbacks = rpt_client_callbacks;
  if (config->search_domain)
    rdmnet_safe_strncpy(client->search_domain, config->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  else
    client->search_domain[0] = '\0';
  client->sync_resp_buf = config->response_buf;

  res = rc_rpt_client_register(client, true, config->llrp_netints, config->num_llrp_netints);
  if (res != kEtcPalErrOk)
  {
    rdmnet_unregister_struct_instance(new_device);
    rdmnet_free_struct_instance(new_device);
    return res;
  }

  res = rc_client_add_scope(client, &config->scope_config, &new_device->scope_handle);
  if (res != kEtcPalErrOk)
  {
    rdmnet_unregister_struct_instance(new_device);
    bool destroy_immediately = rc_client_unregister(client, kRdmnetDisconnectShutdown);
    if (destroy_immediately)
      rdmnet_free_struct_instance(new_device);
    return res;
  }

  // Do the rest of the initialization
  new_device->callbacks = config->callbacks;
  *handle = new_device->id.handle;
  return res;
}

etcpal_error_t get_device(rdmnet_device_t handle, RdmnetDevice** device)
{
  if (handle == RDMNET_DEVICE_INVALID)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;
  if (!rdmnet_readlock())
    return kEtcPalErrSys;

  RdmnetDevice* found_device = (RdmnetDevice*)rdmnet_find_struct_instance(handle, kRdmnetStructTypeDevice);
  if (!found_device)
  {
    rdmnet_readunlock();
    return kEtcPalErrNotFound;
  }

  if (!DEVICE_LOCK(found_device))
  {
    rdmnet_readunlock();
    return kEtcPalErrSys;
  }

  *device = found_device;
  // Return keeping the locks
  return kEtcPalErrOk;
}

void release_device(RdmnetDevice* device)
{
  DEVICE_UNLOCK(device);
  rdmnet_readunlock();
}

bool add_virtual_endpoints(RdmnetDevice* device, const RdmnetVirtualEndpointConfig* endpoints, size_t num_endpoints)
{
  if (!DEVICE_CHECK_ENDPOINTS_CAPACITY(device, num_endpoints))
    return false;

  rdmnet_init_endpoints(&device->endpoints[device->num_endpoints], num_endpoints);

  bool res = true;
  for (size_t i = 0; i < num_endpoints; ++i)
  {
    const RdmnetVirtualEndpointConfig* endpoint_config = &endpoints[i];
    DeviceEndpoint*                    new_endpoint = &device->endpoints[device->num_endpoints + i];

    if (res)
    {
      res = rdmnet_add_dynamic_responders(device, new_endpoint, device->manufacturer_id,
                                          endpoint_config->dynamic_responders,
                                          endpoint_config->num_dynamic_responders) == kEtcPalErrOk;
    }

    if (res)
    {
      res = rdmnet_add_static_responders(device, new_endpoint, endpoint_config->static_responders,
                                         endpoint_config->num_static_responders) == kEtcPalErrOk;
      // If this part failed, the dynamic responders will be cleaned up in the call to rdmnet_deinit_endpoints
    }

    if (!res)
      break;

    new_endpoint->id = endpoint_config->endpoint_id;
    new_endpoint->type = kDeviceEndpointTypeVirtual;
    new_endpoint->responder_list_change_number = 0;
  }

  if (res)
    device->num_endpoints += num_endpoints;
  else  // Cleanup on failure
    rdmnet_deinit_endpoints(&device->endpoints[device->num_endpoints], num_endpoints);

  return res;
}

bool add_physical_endpoints(RdmnetDevice* device, const RdmnetPhysicalEndpointConfig* endpoints, size_t num_endpoints)
{
  if (!DEVICE_CHECK_ENDPOINTS_CAPACITY(device, num_endpoints))
    return false;

  rdmnet_init_endpoints(&device->endpoints[device->num_endpoints], num_endpoints);

  bool res = true;
  for (size_t i = 0; i < num_endpoints; ++i)
  {
    const RdmnetPhysicalEndpointConfig* endpoint_config = &endpoints[i];
    DeviceEndpoint*                     new_endpoint = &device->endpoints[device->num_endpoints + i];

    res = rdmnet_add_physical_responders(device, new_endpoint, endpoint_config->responders,
                                         endpoint_config->num_responders) == kEtcPalErrOk;

    if (!res)
      break;

    new_endpoint->id = endpoint_config->endpoint_id;
    new_endpoint->type = kDeviceEndpointTypePhysical;
    new_endpoint->responder_list_change_number = 0;
  }

  if (res)
    device->num_endpoints += num_endpoints;
  else  // Cleanup on failure
    rdmnet_deinit_endpoints(&device->endpoints[device->num_endpoints], num_endpoints);

  return res;
}

bool remove_endpoints(RdmnetDevice* device, const uint16_t* endpoint_ids, size_t num_endpoints)
{
  // Make sure all of the endpoints exist
  for (const uint16_t* endpoint_id = endpoint_ids; endpoint_id < endpoint_ids + num_endpoints; ++endpoint_id)
  {
    if (!find_endpoint(device, *endpoint_id))
      return false;
  }

  for (const uint16_t* endpoint_id = endpoint_ids; endpoint_id < endpoint_ids + num_endpoints; ++endpoint_id)
  {
    DeviceEndpoint* endpoint = find_endpoint(device, *endpoint_id);

    rdmnet_deinit_endpoints(endpoint, 1);

    size_t endpoint_index = endpoint - device->endpoints;
    if (endpoint_index != device->num_endpoints - 1)
    {
      memmove(endpoint, endpoint + 1, (device->num_endpoints - (endpoint_index + 1)) * sizeof(DeviceEndpoint));
    }
  }
  device->num_endpoints -= num_endpoints;

  return true;
}

void notify_endpoint_list_change(RdmnetDevice* device)
{
  ++device->endpoint_list_change_number;
  if (device->connected_to_broker)
  {
    // Send an RDM update
    // TODO change from hardcoded when we have RDM message packing code
    uint8_t update_buf[4];
    etcpal_pack_u32b(update_buf, device->endpoint_list_change_number);
    rc_client_send_rdm_update(&device->client, device->scope_handle, 0, E137_7_ENDPOINT_LIST_CHANGE, update_buf, 4);
  }
}

void notify_endpoint_responder_list_change(RdmnetDevice* device, DeviceEndpoint* endpoint)
{
  ++endpoint->responder_list_change_number;
  if (device->connected_to_broker)
  {
    // Send an RDM update
    // TODO change from hardcoded when we have RDM message packing code
    uint8_t update_buf[6];
    etcpal_pack_u16b(update_buf, endpoint->id);
    etcpal_pack_u32b(&update_buf[2], endpoint->responder_list_change_number);
    rc_client_send_rdm_update(&device->client, device->scope_handle, 0, E137_7_ENDPOINT_RESPONDER_LIST_CHANGE,
                              update_buf, 6);
  }
}

DeviceEndpoint* find_endpoint(RdmnetDevice* device, uint16_t endpoint_id)
{
  for (DeviceEndpoint* endpoint = device->endpoints; endpoint < device->endpoints + device->num_endpoints; ++endpoint)
  {
    if (endpoint->id == endpoint_id)
      return endpoint;
  }
  return NULL;
}

void client_connected(RCClient* client, rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo* info)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  RDMNET_ASSERT(client);
  RdmnetDevice* device = GET_DEVICE_FROM_CLIENT(client);
  RDMNET_ASSERT(scope_handle == device->scope_handle);

  if (DEVICE_LOCK(device))
  {
    device->connected_to_broker = true;
    DEVICE_UNLOCK(device);
  }

  device->callbacks.connected(device->id.handle, info, device->callbacks.context);
}

void client_connect_failed(RCClient*                            client,
                           rdmnet_client_scope_t                scope_handle,
                           const RdmnetClientConnectFailedInfo* info)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  RDMNET_ASSERT(client);
  RdmnetDevice* device = GET_DEVICE_FROM_CLIENT(client);
  RDMNET_ASSERT(scope_handle == device->scope_handle);
  device->callbacks.connect_failed(device->id.handle, info, device->callbacks.context);
}

void client_disconnected(RCClient* client, rdmnet_client_scope_t scope_handle, const RdmnetClientDisconnectedInfo* info)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  RDMNET_ASSERT(client);
  RdmnetDevice* device = GET_DEVICE_FROM_CLIENT(client);
  RDMNET_ASSERT(scope_handle == device->scope_handle);

  if (DEVICE_LOCK(device))
  {
    device->connected_to_broker = false;

    // Reset all dynamic UIDs on dynamic responders.
    for (DeviceEndpoint* endpoint = device->endpoints; endpoint < device->endpoints + device->num_endpoints; ++endpoint)
    {
      EtcPalRbIter iter;
      etcpal_rbiter_init(&iter);
      for (EndpointResponder* responder = etcpal_rbiter_first(&iter, &endpoint->responders); responder;
           responder = etcpal_rbiter_next(&iter))
      {
        if (!ETCPAL_UUID_IS_NULL(&responder->rid))
          RDMNET_INIT_DYNAMIC_UID_REQUEST(&responder->uid, device->manufacturer_id);
      }
    }
    DEVICE_UNLOCK(device);
  }

  device->callbacks.disconnected(device->id.handle, info, device->callbacks.context);
}

void client_broker_msg_received(RCClient* client, rdmnet_client_scope_t scope_handle, const BrokerMessage* msg)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  RDMNET_ASSERT(client);
  RDMNET_ASSERT(msg);

  RdmnetDevice* device = GET_DEVICE_FROM_CLIENT(client);
  RDMNET_ASSERT(scope_handle == device->scope_handle);

  switch (msg->vector)
  {
    case VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS:
      if (handle_assigned_dynamic_uids(device, BROKER_GET_DYNAMIC_UID_ASSIGNMENT_LIST(msg)) &&
          device->callbacks.dynamic_uid_status_received)
      {
        device->callbacks.dynamic_uid_status_received(device->id.handle, BROKER_GET_DYNAMIC_UID_ASSIGNMENT_LIST(msg),
                                                      device->callbacks.context);
      }
      break;

    default:
      break;
  }
}

void client_destroyed(RCClient* client)
{
  RDMNET_ASSERT(client);
  RdmnetDevice* device = GET_DEVICE_FROM_CLIENT(client);
  rdmnet_free_struct_instance(device);
}

void client_llrp_msg_received(RCClient*              client,
                              const LlrpRdmCommand*  cmd,
                              RdmnetSyncRdmResponse* response,
                              bool*                  use_internal_buf_for_response)
{
  RDMNET_ASSERT(client);
  RdmnetDevice* device = GET_DEVICE_FROM_CLIENT(client);

  if (handle_rdm_command_internally(device, &cmd->rdm_header, cmd->data, cmd->data_len, response))
  {
    *use_internal_buf_for_response = true;
  }
  else
  {
    device->callbacks.llrp_rdm_command_received(device->id.handle, cmd, response, device->callbacks.context);
  }
}

void client_rpt_msg_received(RCClient*               client,
                             rdmnet_client_scope_t   scope_handle,
                             const RptClientMessage* msg,
                             RdmnetSyncRdmResponse*  response,
                             bool*                   use_internal_buf_for_response)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  RDMNET_ASSERT(client);
  RdmnetDevice* device = GET_DEVICE_FROM_CLIENT(client);
  RDMNET_ASSERT(scope_handle == device->scope_handle);
  if (msg->type == kRptClientMsgRdmCmd)
  {
    const RdmnetRdmCommand* cmd = RDMNET_GET_RDM_COMMAND(msg);

    if (cmd->dest_endpoint == E133_NULL_ENDPOINT &&
        handle_rdm_command_internally(device, &cmd->rdm_header, cmd->data, cmd->data_len, response))
    {
      *use_internal_buf_for_response = true;
    }
    else
    {
      device->callbacks.rdm_command_received(device->id.handle, RDMNET_GET_RDM_COMMAND(msg), response,
                                             device->callbacks.context);
    }
  }
  else
  {
    RDMNET_LOG_INFO("Device incorrectly got non-RDM-command message.");
  }
}

bool handle_assigned_dynamic_uids(RdmnetDevice* device, const RdmnetDynamicUidAssignmentList* assignment_list)
{
  size_t num_responders_found = 0;

  if (DEVICE_LOCK(device))
  {
    for (DeviceEndpoint* endpoint = device->endpoints; endpoint < device->endpoints + device->num_endpoints; ++endpoint)
    {
      // No dynamic UIDs on physical endpoints
      if (endpoint->type == kDeviceEndpointTypePhysical)
        continue;

      bool endpoint_responders_changed = false;

      for (const RdmnetDynamicUidMapping* mapping = assignment_list->mappings;
           mapping < assignment_list->mappings + assignment_list->num_mappings; ++mapping)
      {
        if (mapping->status_code == kRdmnetDynamicUidStatusOk)
        {
          EndpointResponder* responder = rdmnet_find_responder_by_rid(endpoint, &mapping->rid);
          if (responder)
          {
            responder->uid = mapping->uid;
            endpoint_responders_changed = true;
            ++num_responders_found;
          }
        }
      }

      if (endpoint_responders_changed)
        notify_endpoint_responder_list_change(device, endpoint);

      if (num_responders_found >= assignment_list->num_mappings)
        break;
    }
    DEVICE_UNLOCK(device);
  }

  return (num_responders_found > 0 ? true : false);
}

bool handle_rdm_command_internally(RdmnetDevice*           device,
                                   const RdmCommandHeader* rdm_header,
                                   const uint8_t*          data,
                                   uint8_t                 data_len,
                                   RdmnetSyncRdmResponse*  resp)
{
  bool res = false;
  if (DEVICE_LOCK(device))
  {
    res = true;
    switch (rdm_header->param_id)
    {
      case E137_7_ENDPOINT_LIST:
        handle_endpoint_list(device, rdm_header, resp);
        break;
      case E137_7_ENDPOINT_LIST_CHANGE:
        handle_endpoint_list_change(device, rdm_header, resp);
        break;
      case E137_7_ENDPOINT_RESPONDERS:
        handle_endpoint_responders(device, rdm_header, data, data_len, resp);
        break;
      case E137_7_ENDPOINT_RESPONDER_LIST_CHANGE:
        handle_endpoint_responder_list_change(device, rdm_header, data, data_len, resp);
        break;
      case E137_7_BINDING_CONTROL_FIELDS:
        handle_binding_control_fields(device, rdm_header, data, data_len, resp);
        break;
      default:
        res = false;
        break;
    }
    DEVICE_UNLOCK(device);
  }
  return res;
}

void handle_endpoint_list(RdmnetDevice* device, const RdmCommandHeader* rdm_header, RdmnetSyncRdmResponse* response)
{
  if (rdm_header->command_class != kRdmCCGetCommand)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnsupportedCommandClass);
    return;
  }

  size_t   pd_len = (device->num_endpoints * 3) + 4;
  uint8_t* buf = rc_client_get_internal_response_buf(pd_len);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  uint8_t* cur_ptr = buf;
  etcpal_pack_u32b(cur_ptr, device->endpoint_list_change_number);
  cur_ptr += 4;

  for (const DeviceEndpoint* endpoint = device->endpoints; endpoint < device->endpoints + device->num_endpoints;
       ++endpoint)
  {
    etcpal_pack_u16b(cur_ptr, endpoint->id);
    cur_ptr += 2;
    *cur_ptr++ = endpoint->type;
  }

  RDMNET_SYNC_SEND_RDM_ACK(response, pd_len);
}

void handle_endpoint_list_change(RdmnetDevice*           device,
                                 const RdmCommandHeader* rdm_header,
                                 RdmnetSyncRdmResponse*  response)
{
  if (rdm_header->command_class != kRdmCCGetCommand)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnsupportedCommandClass);
    return;
  }

  size_t   pd_len = 4;
  uint8_t* buf = rc_client_get_internal_response_buf(pd_len);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  etcpal_pack_u32b(buf, device->endpoint_list_change_number);

  RDMNET_SYNC_SEND_RDM_ACK(response, pd_len);
}

void handle_endpoint_responders(RdmnetDevice*           device,
                                const RdmCommandHeader* rdm_header,
                                const uint8_t*          data,
                                uint8_t                 data_len,
                                RdmnetSyncRdmResponse*  response)
{
  if (rdm_header->command_class != kRdmCCGetCommand)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnsupportedCommandClass);
    return;
  }

  if (!data || data_len < 2)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
    return;
  }

  uint16_t        endpoint_id = etcpal_unpack_u16b(data);
  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNREndpointNumberInvalid);
    return;
  }

  size_t   pd_len = (etcpal_rbtree_size(&endpoint->responders) * 6) + 6;
  uint8_t* buf = rc_client_get_internal_response_buf(pd_len);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  uint8_t* cur_ptr = buf;
  etcpal_pack_u16b(cur_ptr, endpoint_id);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, endpoint->responder_list_change_number);
  cur_ptr += 4;

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  for (EndpointResponder* responder = etcpal_rbiter_first(&iter, &endpoint->responders); responder;
       responder = etcpal_rbiter_next(&iter))
  {
    if (RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&responder->uid))
    {
      // Don't include responders that do not have dynamic UIDs yet
      pd_len -= 6;
    }
    else
    {
      etcpal_pack_u16b(cur_ptr, responder->uid.manu);
      cur_ptr += 2;
      etcpal_pack_u32b(cur_ptr, responder->uid.id);
      cur_ptr += 4;
    }
  }

  RDMNET_SYNC_SEND_RDM_ACK(response, pd_len);
}

void handle_endpoint_responder_list_change(RdmnetDevice*           device,
                                           const RdmCommandHeader* rdm_header,
                                           const uint8_t*          data,
                                           uint8_t                 data_len,
                                           RdmnetSyncRdmResponse*  response)
{
  if (rdm_header->command_class != kRdmCCGetCommand)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnsupportedCommandClass);
    return;
  }

  if (!data || data_len < 2)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
    return;
  }

  uint16_t        endpoint_id = etcpal_unpack_u16b(data);
  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNREndpointNumberInvalid);
    return;
  }

  size_t   pd_len = 6;
  uint8_t* buf = rc_client_get_internal_response_buf(pd_len);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  uint8_t* cur_ptr = buf;
  etcpal_pack_u16b(cur_ptr, endpoint_id);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, endpoint->responder_list_change_number);
  cur_ptr += 4;

  RDMNET_SYNC_SEND_RDM_ACK(response, pd_len);
}

void handle_binding_control_fields(RdmnetDevice*           device,
                                   const RdmCommandHeader* rdm_header,
                                   const uint8_t*          data,
                                   uint8_t                 data_len,
                                   RdmnetSyncRdmResponse*  response)
{
  if (rdm_header->command_class != kRdmCCGetCommand)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnsupportedCommandClass);
    return;
  }

  if (!data || data_len < 8)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
    return;
  }

  uint16_t        endpoint_id = etcpal_unpack_u16b(data);
  DeviceEndpoint* endpoint = find_endpoint(device, endpoint_id);
  if (!endpoint)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNREndpointNumberInvalid);
    return;
  }

  RdmUid responder_uid;
  responder_uid.manu = etcpal_unpack_u16b(&data[2]);
  responder_uid.id = etcpal_unpack_u32b(&data[4]);

  EndpointResponder* responder = rdmnet_find_responder_by_uid(endpoint, &responder_uid);
  if (!responder)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnknownUid);
    return;
  }

  size_t   pd_len = 16;
  uint8_t* buf = rc_client_get_internal_response_buf(pd_len);
  if (!buf)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRHardwareFault);
    return;
  }

  uint8_t* cur_ptr = buf;
  etcpal_pack_u16b(cur_ptr, endpoint_id);
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, responder_uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, responder_uid.id);
  cur_ptr += 4;
  etcpal_pack_u16b(cur_ptr, responder->control_field);
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, responder->binding_uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, responder->binding_uid.id);
  cur_ptr += 4;

  RDMNET_SYNC_SEND_RDM_ACK(response, pd_len);
}
