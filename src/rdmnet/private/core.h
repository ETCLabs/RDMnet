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

#ifndef _RDMNET_PRIVATE_CORE_H_
#define _RDMNET_PRIVATE_CORE_H_

#include "lwpa/lock.h"
#include "lwpa/log.h"
#include "lwpa/socket.h"
#include "rdmnet/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

bool rdmnet_core_initialized();

extern lwpa_rwlock_t rdmnet_lock;
extern const LwpaLogParams *rdmnet_log_params;

#define RDMNET_LOG_MSG(msg) RDMNET_LOG_MSG_PREFIX msg

#define rdmnet_readlock() lwpa_rwlock_readlock(&rdmnet_lock, LWPA_WAIT_FOREVER)
#define rdmnet_readunlock() lwpa_rwlock_readunlock(&rdmnet_lock)
#define rdmnet_writelock() lwpa_rwlock_writelock(&rdmnet_lock, LWPA_WAIT_FOREVER)
#define rdmnet_writeunlock() lwpa_rwlock_writeunlock(&rdmnet_lock)

typedef enum
{
  kRdmnetPolledSocketConnection = 0,
  kRdmnetPolledSocketLlrp = 1
} rdmnet_polled_socket_t;

lwpa_error_t rdmnet_core_add_polled_socket(lwpa_socket_t socket, lwpa_poll_events_t events, rdmnet_polled_socket_t type);
lwpa_error_t rdmnet_core_remove_polled_socket(lwpa_socket_t socket);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_PRIVATE_CORE_H_ */
