/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

/*! \file rdmnet/device.h
 *  \brief Definitions for the RDMnet Device API
 *  \author Sam Kearney
 */
#ifndef _RDMNET_DEVICE_H_
#define _RDMNET_DEVICE_H_

#include "lwpa/bool.h"
#include "lwpa/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/client.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup rdmnet_device Device API
 *  \ingroup rdmnet_client
 *  \brief Implementation of RDMnet device functionality.
 *
 *  RDMnet devices are clients which exclusively receive and respond to RDM commands. Devices
 *  operate on only one scope at a time. This API wraps the RDMnet Client API and provides functions
 *  tailored specifically to the usage concerns of an RDMnet device.
 *
 *  @{
 */

/*! A handle to an RDMnet device. */
typedef struct RdmnetDevice* rdmnet_device_t;

typedef struct RdmnetDeviceCallbacks
{
  void (*connected)(rdmnet_device_t handle, const RdmnetClientConnectedInfo* info, void* context);
  void (*connect_failed)(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo* info, void* context);
  void (*disconnected)(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo* info, void* context);
  void (*rdm_command_received)(rdmnet_device_t handle, const RemoteRdmCommand* cmd, void* context);
  void (*llrp_rdm_command_received)(rdmnet_device_t handle, const LlrpRemoteRdmCommand* cmd, void* context);
} RdmnetDeviceCallbacks;

/*! A set of information that defines the startup parameters of an RDMnet Device. */
typedef struct RdmnetDeviceConfig
{
  /*! The device's CID. */
  LwpaUuid cid;
  /*! The device's configured RDMnet scope. */
  RdmnetScopeConfig scope_config;
  /*! A set of callbacks for the device to receive RDMnet notifications. */
  RdmnetDeviceCallbacks callbacks;
  /*! Pointer to opaque data passed back with each callback. Can be NULL. */
  void* callback_context;
  /*! Optional configuration data for the device's RPT Client functionality. */
  RptClientOptionalConfig optional;
  /*! Optional configuration data for the device's LLRP Target functionality. */
  LlrpTargetOptionalConfig llrp_optional;
} RdmnetDeviceConfig;

/*! \brief Initialize an RDMnet Device Config with default values for the optional config options.
 *
 *  The config struct members not marked 'optional' are not initialized by this macro. Those members
 *  do not have default values and must be initialized manually before passing the config struct to
 *  an API function.
 *
 *  Usage example:
 *  \code
 *  RdmnetDeviceConfig config;
 *  RDMNET_DEVICE_CONFIG_INIT(&config, 0x6574);
 *  \endcode
 *
 *  \param devicecfgptr Pointer to RdmnetDeviceConfig.
 *  \param manu_id ESTA manufacturer ID. All RDMnet Devices must have one.
 */
#define RDMNET_DEVICE_CONFIG_INIT(devicecfgptr, manu_id) RPT_CLIENT_CONFIG_INIT(devicecfgptr, manu_id)

lwpa_error_t rdmnet_device_init(const LwpaLogParams* lparams);
void rdmnet_device_deinit();

lwpa_error_t rdmnet_device_create(const RdmnetDeviceConfig* config, rdmnet_device_t* handle);
lwpa_error_t rdmnet_device_destroy(rdmnet_device_t handle);

lwpa_error_t rdmnet_device_send_rdm_response(rdmnet_device_t handle, const LocalRdmResponse* resp);
lwpa_error_t rdmnet_device_send_status(rdmnet_device_t handle, const LocalRptStatus* status);
lwpa_error_t rdmnet_device_send_llrp_response(rdmnet_device_t handle, const LlrpLocalRdmResponse* resp);

lwpa_error_t rdmnet_device_change_scope(rdmnet_device_t handle, const RdmnetScopeConfig* new_scope_config,
                                        rdmnet_disconnect_reason_t reason);
lwpa_error_t rdmnet_device_change_search_domain(rdmnet_device_t handle, const char* new_search_domain,
                                                rdmnet_disconnect_reason_t reason);

/*! @} */

#ifdef __cplusplus
};
#endif

#endif /* _RDMNET_DEVICE_H_ */
