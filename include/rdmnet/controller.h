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

/*! \file rdmnet/controller.h
 *  \brief Definitions for the RDMnet Controller API
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CONTROLLER_H_
#define _RDMNET_CONTROLLER_H_

#include "lwpa/bool.h"
#include "lwpa/uuid.h"
#include "lwpa/inet.h"
#include "rdm/uid.h"
#include "rdmnet/common/broker_prot.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RdmnetControllerInternal *rdmnet_controller_t;

typedef void (*RdmnetControllerConnectedCb)(rdmnet_controller_t handle, const char *scope);
typedef void (*RdmnetControllerDisconnectedCb)(rdmnet_controller_t handle, const char *scope);

typedef struct RdmnetControllerCallbacks
{
  RdmnetControllerConnectedCb connected;
  RdmnetControllerDisconnectedCb disconnected;
} RdmnetControllerCallbacks;

typedef struct RdmnetScopeListEntry RdmnetScopeListEntry;
struct RdmnetScopeListEntry
{
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  LwpaSockaddr broker_addr;
  RdmnetScopeListEntry *next;
};

typedef struct RdmnetControllerData
{
  bool has_static_uid;
  RdmUid static_uid;
  LwpaUuid cid;
  RdmnetScopeListEntry *scope_list;
  RdmnetControllerCallbacks cb;
} RdmnetControllerData;

lwpa_error_t rdmnet_controller_create(const RdmnetControllerData *data, rdmnet_controller_t *handle);
lwpa_error_t rdmnet_controller_destroy(rdmnet_controller_t handle);

#ifdef __cplusplus
};
#endif

#endif /* _RDMNET_CONTROLLER_H_ */