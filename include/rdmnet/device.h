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

typedef struct RdmnetDeviceInternal *rdmnet_device_t;

typedef void (*RdmnetDeviceConnectedCb)(rdmnet_device_t handle, const char *scope, void *context);
typedef void (*RdmnetDeviceDisconnectedCb)(rdmnet_device_t handle, const char *scope, void *context);
typedef void (*RdmnetDeviceRdmCmdReceivedCb)(rdmnet_device_t handle, const char *scope, const DeviceRdmCommand *cmd,
                                             void *context);

typedef struct RdmnetDeviceCallbacks
{
  RdmnetDeviceConnectedCb connected;
  RdmnetDeviceDisconnectedCb disconnected;
  RdmnetDeviceRdmCmdReceivedCb rdm_cmd_received;
} RdmnetDeviceCallbacks;

typedef struct RdmnetDeviceConfig
{
  bool has_static_uid;
  RdmUid static_uid;
  LwpaUuid cid;
  RdmnetScopeConfig scope_config;
  RdmnetDeviceCallbacks callbacks;
  void *callback_context;
} RdmnetDeviceConfig;

lwpa_error_t rdmnet_device_create(const RdmnetDeviceConfig *config, rdmnet_device_t *handle);
void rdmnet_device_destroy(rdmnet_device_t handle);

lwpa_error_t rdmnet_device_send_rdm_response(rdmnet_device_t handle, const DeviceRdmResponse *resp);
lwpa_error_t rdmnet_device_send_status(rdmnet_device_t handle, const RptStatusMsg *status);

#endif /* _RDMNET_DEVICE_H_ */
