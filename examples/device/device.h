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

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "lwpa/int.h"
#include "lwpa/log.h"
#include "rdm/message.h"
#include "rdmnet/defs.h"
#include "rdmnet/common/message.h"
#include "default_responder.h"

typedef struct DeviceSettings
{
  LwpaUuid cid;
  LwpaSockaddr static_broker_addr;
  const char *scope;
} DeviceSettings;

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t device_init(const DeviceSettings *settings, const LwpaLogParams *lparams);
void device_deinit();
void device_run();

bool device_llrp_set(const RdmCommand *cmd_data, uint16_t *nack_reason);

#ifdef __cplusplus
}
#endif

#endif /* _DEVICE_H_ */
