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

/*!
 * \file rdmnet/device.h
 * \brief Definitions for the RDMnet Device API
 */

#ifndef RDMNET_DEVICE_H_
#define RDMNET_DEVICE_H_

#include <stdbool.h>
#include "etcpal/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/client.h"
#include "rdmnet/message.h"

/*!
 * \defgroup rdmnet_device Device API
 * \ingroup rdmnet_api
 * \brief Implementation of RDMnet device functionality; see \ref using_device.
 *
 * RDMnet devices are clients which exclusively receive and respond to RDM commands. Devices
 * operate on only one scope at a time. This API wraps the RDMnet Client API and provides functions
 * tailored specifically to the usage concerns of an RDMnet device.
 *
 * See \ref using_device for a detailed description of how to use this API.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! A handle to an RDMnet device. */
typedef int rdmnet_device_t;
/*! An invalid RDMnet device handle value. */
#define RDMNET_DEVICE_INVALID -1

/*!
 * \brief A device has successfully connected to a broker.
 * \param[in] handle Handle to the device which has connected.
 * \param[in] info More information about the successful connection.
 * \param[in] context Context pointer that was given at the creation of the device instance.
 */
typedef void (*RdmnetDeviceConnectedCallback)(rdmnet_device_t handle, const RdmnetClientConnectedInfo* info,
                                              void* context);

/*!
 * \brief A connection attempt failed between a device and a broker.
 * \param[in] handle Handle to the device which has failed to connect.
 * \param[in] info More information about the failed connection.
 * \param[in] context Context pointer that was given at the creation of the device instance.
 */
typedef void (*RdmnetDeviceConnectFailedCallback)(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo* info,
                                                  void* context);

/*!
 * \brief A device which was previously connected to a broker has disconnected.
 * \param[in] handle Handle to the device which has disconnected.
 * \param[in] info More information about the disconnect event.
 * \param[in] context Context pointer that was given at the creation of the device instance.
 */
typedef void (*RdmnetDeviceDisconnectedCallback)(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo* info,
                                                 void* context);

/*!
 * \brief An RDM command has been received addressed to a device.
 * \param[in] handle Handle to the device which has received the RDM command.
 * \param[in] cmd The RDM command data.
 * \param[out] response Fill in with response data if responding synchronously (see \ref handling_rdm_commands).
 * \param[in] context Context pointer that was given at the creation of the device instance.
 */
typedef void (*RdmnetDeviceRdmCommandReceivedCallback)(rdmnet_device_t handle, const RdmnetRdmCommand* cmd,
                                                       RdmnetSyncRdmResponse* response, void* context);

/*!
 * \brief An RDM command has been received over LLRP, addressed to a device.
 * \param[in] handle Handle to the device which has received the RDM command.
 * \param[in] cmd The RDM command data.
 * \param[out] response Fill in with response data if responding synchronously (see \ref handling_rdm_commands).
 * \param[in] context Context pointer that was given at the creation of the device instance.
 */
typedef void (*RdmnetDeviceLlrpRdmCommandReceivedCallback)(rdmnet_device_t handle, const LlrpRdmCommand* cmd,
                                                           RdmnetSyncRdmResponse* response, void* context);

/*!
 * \brief The dynamic UID assignment status for a set of virtual responders has been received.
 *
 * This callback need only be implemented if adding virtual responders with dynamic UIDs. See
 * \ref devices_and_gateways and \ref using_device for more information.
 *
 * Note that the list may indicate failed assignments for some or all responders, with a status
 * code.
 *
 * \param[in] handle Handle to the device which has received the dynamic UID assignments.
 * \param[in] list The list of dynamic UID assignments.
 * \param[in] context Context pointer that was given at the creation of the device instance.
 */
typedef void (*RdmnetDeviceDynamicUidStatusCallback)(rdmnet_device_t handle, const RdmnetDynamicUidAssignmentList* list,
                                                     void* context);

/*! A set of notification callbacks received about a device. */
typedef struct RdmnetDeviceCallbacks
{
  RdmnetDeviceConnectedCallback connected;                              /*!< Required. */
  RdmnetDeviceConnectFailedCallback connect_failed;                     /*!< Required. */
  RdmnetDeviceDisconnectedCallback disconnected;                        /*!< Required. */
  RdmnetDeviceRdmCommandReceivedCallback rdm_command_received;          /*!< Required. */
  RdmnetDeviceLlrpRdmCommandReceivedCallback llrp_rdm_command_received; /*!< Required. */
  RdmnetDeviceDynamicUidStatusCallback dynamic_uid_status_received;     /*!< Optional. */
  void* context; /*!< (optional) Pointer to opaque data passed back with each callback. */
} RdmnetDeviceCallbacks;

/*!
 * \brief Configuration information for a virtual endpoint on a device.
 * \details See \ref devices_and_gateways for more information on endpoints.
 */
typedef struct RdmnetVirtualEndpointConfig
{
  /*! The endpoint identifier for this endpoint. Valid values are between 1 and 63,999 inclusive. */
  uint16_t endpoint_id;
  /*!
   * \brief An array of initial virtual RDM responders on this endpoint, identified by RID.
   * \details See \ref devices_and_gateways for more information on virtual responders and RIDs.
   */
  const EtcPalUuid* dynamic_responders;
  /*! Size of the dynamic_responders array. */
  size_t num_dynamic_responders;
  /*! An array of initial virtual RDM responders on this endpoint, identified by static UID. */
  const RdmUid* static_responders;
  /*! Size of the static_responders array. */
  size_t num_static_responders;
} RdmnetVirtualEndpointConfig;

/*!
 * \brief An initializer for an RdmnetVirtualEndpointConfig instance.
 *
 * Usage:
 * \code
 * // Create a virtual endpoint with an endpoint ID of 20.
 * RdmnetVirtualEndpointConfig endpoint_config = RDMNET_VIRTUAL_ENDPOINT_INIT(20);
 * // Assign the other members of the struct to associate initial responders with this endpoint.
 * \endcode
 *
 * \param endpoint_num The endpoint identifier for this endpoint. Valid values are between 1 and
 *                     63,999 inclusive.
 */
#define RDMNET_VIRTUAL_ENDPOINT_INIT(endpoint_num) \
  {                                                \
    (endpoint_num), NULL, 0, NULL, 0               \
  }

/*!
 * \brief Configuration information for a physical endpoint on a device.
 * \details See \ref devices_and_gateways for more information on endpoints.
 */
typedef struct RdmnetPhysicalEndpointConfig
{
  /*! The endpoint identifier for this endpoint. Valid values are between 1 and 63,999 inclusive. */
  uint16_t endpoint_id;
  /*! An array of initial physical RDM responders on this endpoint, identified by static UID. */
  const RdmUid* responders;
  /*! Size of the responders array. */
  size_t num_responders;
} RdmnetPhysicalEndpointConfig;

/*!
 * \brief An initializer for an RdmnetPhysicalEndpointConfig instance.
 *
 * Usage:
 * \code
 * // Create a physical endpoint with an endpoint ID of 4.
 * RdmnetPhysicalEndpointConfig endpoint_config = RDMNET_PHYSICAL_ENDPOINT_INIT(4);
 * // Assign the other members of the struct to associate initial responders with this endpoint.
 * \endcode
 *
 * \param endpoint_num The endpoint identifier for this endpoint. Valid values are between 1 and
 *                     63,999 inclusive.
 */
#define RDMNET_PHYSICAL_ENDPOINT_INIT(endpoint_num) \
  {                                                 \
    (endpoint_num), NULL, 0                         \
  }

/*! A set of information that defines the startup parameters of an RDMnet Device. */
typedef struct RdmnetDeviceConfig
{
  /************************************************************************************************
   * Required Values
   ***********************************************************************************************/

  /*! The device's CID. */
  EtcPalUuid cid;
  /*! A set of callbacks for the device to receive RDMnet notifications. */
  RdmnetDeviceCallbacks callbacks;

  /************************************************************************************************
   * Optional Values
   ***********************************************************************************************/

  /*!
   * (optional) A data buffer to be used to respond synchronously to RDM commands. See
   * \ref handling_rdm_commands for more information.
   */
  uint8_t* response_buf;

  /*!
   * (optional) The device's configured RDMnet scope. Will be initialized to the RDMnet default
   * scope using the initialization functions/macros for this structure.
   */
  RdmnetScopeConfig scope_config;

  /*!
   * (optional) The device's UID. This will be intialized with a Dynamic UID request value using
   * the initialization functions/macros for this structure. If you want a static UID instead, just
   * fill this in with the static UID after initializing.
   */
  RdmUid uid;

  /*!
   * (optional) The device's configured search domain for discovery. NULL to use the default
   * search domain(s).
   */
  const char* search_domain;

  /*! An array of initial physical endpoints that the device uses. */
  const RdmnetPhysicalEndpointConfig* physical_endpoints;
  /*! Size of the physical_endpoints array. */
  size_t num_physical_endpoints;

  /*! An array of initial virtual endpoints that the device uses. */
  const RdmnetVirtualEndpointConfig* virtual_endpoints;
  /*! Size of the virtual_endpoints array. */
  size_t num_virtual_endpoints;

  /*!
   * (optional) A set of network interfaces to use for the LLRP target associated with this device.
   * If NULL, the set passed to rdmnet_init() will be used, or all network interfaces on the system
   * if that was not provided.
   */
  RdmnetMcastNetintId* llrp_netints;
  /*! (optional) The size of the llrp_netints array. */
  size_t num_llrp_netints;
} RdmnetDeviceConfig;

/*!
 * \brief A default-value initializer for an RdmnetDeviceConfig struct.
 *
 * Usage:
 * \code
 * RdmnetDeviceConfig config = RDMNET_DEVICE_CONFIG_DEFAULT_INIT(MY_ESTA_MANUFACTURER_ID);
 * // Now fill in the required portions as necessary with your data...
 * \endcode
 *
 * \param manu_id Your ESTA manufacturer ID.
 */
#define RDMNET_DEVICE_CONFIG_DEFAULT_INIT(manu_id)                                             \
  {                                                                                            \
    {{0}}, {NULL, NULL, NULL, NULL, NULL, NULL, NULL}, NULL, RDMNET_SCOPE_CONFIG_DEFAULT_INIT, \
        {(0x8000 | manu_id), 0}, NULL, NULL, 0, NULL, 0, NULL, 0                               \
  }

void rdmnet_device_config_init(RdmnetDeviceConfig* config, uint16_t manufacturer_id);
void rdmnet_device_set_callbacks(RdmnetDeviceConfig* config, RdmnetDeviceConnectedCallback connected,
                                 RdmnetDeviceConnectFailedCallback connect_failed,
                                 RdmnetDeviceDisconnectedCallback disconnected,
                                 RdmnetDeviceRdmCommandReceivedCallback rdm_command_received,
                                 RdmnetDeviceLlrpRdmCommandReceivedCallback llrp_rdm_command_received,
                                 RdmnetDeviceDynamicUidStatusCallback dynamic_uid_status_received, void* context);

etcpal_error_t rdmnet_device_create(const RdmnetDeviceConfig* config, rdmnet_device_t* handle);
etcpal_error_t rdmnet_device_destroy(rdmnet_device_t handle, rdmnet_disconnect_reason_t disconnect_reason);

etcpal_error_t rdmnet_device_send_rdm_ack(rdmnet_device_t handle, const RdmnetSavedRdmCommand* received_cmd,
                                          const uint8_t* response_data, size_t response_data_len);
etcpal_error_t rdmnet_device_send_rdm_nack(rdmnet_device_t handle, const RdmnetSavedRdmCommand* received_cmd,
                                           rdm_nack_reason_t nack_reason);
etcpal_error_t rdmnet_device_send_rdm_update(rdmnet_device_t handle, uint16_t param_id, const uint8_t* data,
                                             size_t data_len);
etcpal_error_t rdmnet_device_send_rdm_update_from_responder(rdmnet_device_t handle, uint16_t endpoint,
                                                            const RdmUid* source_uid, uint16_t param_id,
                                                            const uint8_t* data, size_t data_len);
etcpal_error_t rdmnet_device_send_status(rdmnet_device_t handle, const RdmnetSavedRdmCommand* received_cmd,
                                         rpt_status_code_t status_code, const char* status_string);

etcpal_error_t rdmnet_device_send_llrp_ack(rdmnet_device_t handle, const LlrpSavedRdmCommand* received_cmd,
                                           const uint8_t* response_data, uint8_t response_data_len);
etcpal_error_t rdmnet_device_send_llrp_nack(rdmnet_device_t handle, const LlrpSavedRdmCommand* received_cmd,
                                            rdm_nack_reason_t nack_reason);

etcpal_error_t rdmnet_device_add_physical_endpoint(rdmnet_device_t handle,
                                                   const RdmnetPhysicalEndpointConfig* endpoint_config);
etcpal_error_t rdmnet_device_add_physical_endpoints(rdmnet_device_t handle,
                                                    const RdmnetPhysicalEndpointConfig* endpoint_configs,
                                                    size_t num_endpoints);
etcpal_error_t rdmnet_device_add_virtual_endpoint(rdmnet_device_t handle,
                                                  const RdmnetVirtualEndpointConfig* endpoint_config);
etcpal_error_t rdmnet_device_add_virtual_endpoints(rdmnet_device_t handle,
                                                   const RdmnetVirtualEndpointConfig* endpoint_configs,
                                                   size_t num_endpoints);
etcpal_error_t rdmnet_device_remove_endpoint(rdmnet_device_t handle, uint16_t endpoint_id);
etcpal_error_t rdmnet_device_remove_endpoints(rdmnet_device_t handle, const uint16_t* endpoint_ids,
                                              size_t num_endpoints);

etcpal_error_t rdmnet_device_add_static_responders(rdmnet_device_t handle, uint16_t endpoint_id,
                                                   const RdmUid* responder_uids, size_t num_responders);
etcpal_error_t rdmnet_device_add_dynamic_responders(rdmnet_device_t handle, uint16_t endpoint_id,
                                                    const EtcPalUuid* responder_ids, size_t num_responders);

etcpal_error_t rdmnet_device_remove_static_responders(rdmnet_device_t handle, uint16_t endpoint_id,
                                                      const RdmUid* responder_uids, size_t num_responders);
etcpal_error_t rdmnet_device_remove_dynamic_responders(rdmnet_device_t handle, uint16_t endpoint_id,
                                                       const EtcPalUuid* responder_ids, size_t num_responders);

etcpal_error_t rdmnet_device_change_scope(rdmnet_device_t handle, const RdmnetScopeConfig* new_scope_config,
                                          rdmnet_disconnect_reason_t disconnect_reason);
etcpal_error_t rdmnet_device_change_search_domain(rdmnet_device_t handle, const char* new_search_domain,
                                                  rdmnet_disconnect_reason_t disconnect_reason);

#ifdef __cplusplus
};
#endif

/*!
 * @}
 */

#endif /* RDMNET_DEVICE_H_ */
