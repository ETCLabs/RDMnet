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

#ifndef _RDMNET_PRIVATE_CORE_H_
#define _RDMNET_PRIVATE_CORE_H_

#include "lwpa/lock.h"
#include "lwpa/log.h"
#include "lwpa/socket.h"
#include "rdmnet/core.h"
#include "rdmnet/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RDMNET_LOG_MSG(msg) RDMNET_LOG_MSG_PREFIX msg

typedef union PolledSocketOpaqueData
{
  rdmnet_conn_t conn_handle;
  void* ptr;
} PolledSocketOpaqueData;

typedef void (*PolledSocketActivityCallback)(const LwpaPollEvent* event, PolledSocketOpaqueData data);

typedef struct PolledSocketInfo
{
  PolledSocketActivityCallback callback;
  PolledSocketOpaqueData data;
} PolledSocketInfo;

extern const LwpaLogParams* rdmnet_log_params;

bool rdmnet_core_initialized();

bool rdmnet_readlock();
void rdmnet_readunlock();
bool rdmnet_writelock();
void rdmnet_writeunlock();

lwpa_error_t rdmnet_core_add_polled_socket(lwpa_socket_t socket, lwpa_poll_events_t events, PolledSocketInfo* info);
lwpa_error_t rdmnet_core_modify_polled_socket(lwpa_socket_t socket, lwpa_poll_events_t events, PolledSocketInfo* info);
void rdmnet_core_remove_polled_socket(lwpa_socket_t socket);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_PRIVATE_CORE_H_ */
