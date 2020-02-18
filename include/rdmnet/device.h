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
typedef struct RdmnetDevice* rdmnet_device_t;
/*! An invalid RDMnet controller handle value. */
#define RDMNET_DEVICE_INVALID NULL

/*! A set of notification callbacks received about a device. */
typedef struct RdmnetDeviceCallbacks
{
  /*!
   * \brief A device has successfully connected to a broker.
   * \param[in] handle Handle to the device which has connected.
   * \param[in] info More information about the successful connection.
   * \param[in] context Context pointer that was given at the creation of the device instance.
   */
  void (*connected)(rdmnet_device_t handle, const RdmnetClientConnectedInfo* info, void* context);

  /*!
   * \brief A connection attempt failed between a device and a broker.
   * \param[in] handle Handle to the device which has failed to connect.
   * \param[in] info More information about the failed connection.
   * \param[in] context Context pointer that was given at the creation of the device instance.
   */
  void (*connect_failed)(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo* info, void* context);

  /*!
   * \brief A device which was previously connected to a broker has disconnected.
   * \param[in] handle Handle to the device which has disconnected.
   * \param[in] info More information about the disconnect event.
   * \param[in] context Context pointer that was given at the creation of the device instance.
   */
  void (*disconnected)(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo* info, void* context);

  /*!
   * \brief An RDM command has been received addressed to a device.
   * \param[in] handle Handle to the device which has received the RDM command.
   * \param[in] cmd The RDM command data.
   * \param[in] context Context pointer that was given at the creation of the device instance.
   */
  rdmnet_response_action_t (*rdm_command_received)(rdmnet_device_t handle, const RdmnetRemoteRdmCommand* cmd,
                                                   RdmnetSyncRdmResponse* response, void* context);

  /*!
   * \brief An RDM command has been received over LLRP, addressed to a device.
   * \param[in] handle Handle to the device which has received the RDM command.
   * \param[in] cmd The RDM command data.
   * \param[in] context Context pointer that was given at the creation of the device instance.
   */
  rdmnet_response_action_t (*llrp_rdm_command_received)(rdmnet_device_t handle, const LlrpRemoteRdmCommand* cmd,
                                                        RdmnetSyncRdmResponse* response, void* context);

  /*!
   * \brief A set of previously-requested dynamic UID assignments has been received.
   * \param[in] handle Handle to the device which has received the dynamic UID assignments.
   * \param[in] list The list of dynamic UID assignments.
   * \param[in] context Context pointer that was given at the creation of the device instance.
   */
  void (*dynamic_uid_assignments_received)(rdmnet_device_t handle, const BrokerDynamicUidAssignmentList* list,
                                           void* context);
} RdmnetDeviceCallbacks;

/*! A set of information that defines the startup parameters of an RDMnet Device. */
typedef struct RdmnetDeviceConfig
{
  /**** Required Values ****/

  /*! The device's CID. */
  EtcPalUuid cid;
  /*! The device's configured RDMnet scope. */
  RdmnetScopeConfig scope_config;
  /*! A set of callbacks for the device to receive RDMnet notifications. */
  RdmnetDeviceCallbacks callbacks;
  /*! Pointer to opaque data passed back with each callback. Can be NULL. */
  void* callback_context;

  /**** Optional Values ****/

  uint8_t* response_buf;
  size_t response_buf_size;

  /*!
   * \brief OPTIONAL: The device's UID.
   *
   * This should only be filled in manually if the device is using a static UID. Otherwise,
   * rdmnet_device_config_init() will initialize this field with a dynamic UID request generated
   * from your ESTA manufacturer ID. All RDMnet components are required to have a valid ESTA
   * manufacturer ID.
   */
  RdmUid uid;
  /*! OPTIONAL: The client's configured search domain for discovery. */
  const char* search_domain;
  /*!
   * \brief OPTIONAL: A set of network interfaces to use for the LLRP target associated with this
   *        device.
   *
   * If NULL, the set passed to rdmnet_core_init() will be used, or all network interfaces on the
   * system if that was not provided.
   */
  RdmnetMcastNetintId* llrp_netint_arr;
  /*! OPTIONAL: The size of llrp_netint_arr. */
  size_t num_llrp_netints;
} RdmnetDeviceConfig;

/*!
 * \brief Set the main callbacks in an RDMnet device configuration structure.
 *
 * All callbacks are required.
 *
 * \param configptr Pointer to the RdmnetDeviceConfig in which to set the callbacks.
 * \param connected_cb Function pointer to use for the \ref RdmnetDeviceCallbacks::connected
 *                     "connected" callback.
 * \param connect_failed_cb Function pointer to use for the \ref RdmnetDeviceCallbacks::connect_failed
 *                          "connect_failed" callback.
 * \param disconnected_cb Function pointer to use for the \ref RdmnetDeviceCallbacks::disconnected
 *                       "disconnected" callback.
 * \param rdm_command_received_cb Function pointer to use for the \ref RdmnetDeviceCallbacks::rdm_command_received
 *                                "rdm_command_received" callback.
 * \param llrp_rdm_command_received_cb Function pointer to use for the \ref
 *                                     RdmnetDeviceCallbacks::llrp_rdm_command_received "llrp_rdm_command_received"
 *                                     callback.
 * \param cb_context Pointer to opaque data passed back with each callback. Can be NULL.
 */
#define RDMNET_DEVICE_SET_CALLBACKS(configptr, connected_cb, connect_failed_cb, disconnected_cb,       \
                                    rdm_command_received_cb, llrp_rdm_command_received_cb, cb_context) \
  do                                                                                                   \
  {                                                                                                    \
    (configptr)->callbacks.connected = (connected_cb);                                                 \
    (configptr)->callbacks.connect_failed = (connect_failed_cb);                                       \
    (configptr)->callbacks.disconnected = (disconnected_cb);                                           \
    (configptr)->callbacks.rdm_command_received = (rdm_command_received_cb);                           \
    (configptr)->callbacks.llrp_rdm_command_received = (llrp_rdm_command_received_cb);                 \
    (configptr)->callback_context = (cb_context);                                                      \
  } while (0)

etcpal_error_t rdmnet_device_init(const EtcPalLogParams* lparams, const RdmnetNetintConfig* netint_config);
void rdmnet_device_deinit();

void rdmnet_device_config_init(RdmnetDeviceConfig* config, uint16_t manufacturer_id);

etcpal_error_t rdmnet_device_create(const RdmnetDeviceConfig* config, rdmnet_device_t* handle);
etcpal_error_t rdmnet_device_destroy(rdmnet_device_t handle, rdmnet_disconnect_reason_t disconnect_reason);

etcpal_error_t rdmnet_device_send_rdm_response(rdmnet_device_t handle, const RdmnetLocalRdmResponse* resp);
etcpal_error_t rdmnet_device_send_status(rdmnet_device_t handle, const RdmnetLocalRptStatus* status);
etcpal_error_t rdmnet_device_send_llrp_response(rdmnet_device_t handle, const LlrpLocalRdmResponse* resp);

etcpal_error_t rdmnet_device_request_dynamic_uids(rdmnet_device_t handle, const BrokerDynamicUidRequest* requests,
                                                  size_t num_requests);

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
