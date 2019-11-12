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

/* disc_common.h
 * Common functions and definitions used by all mDNS/DNS-SD providers across platforms.
 */
#ifndef DISC_COMMON_H_
#define DISC_COMMON_H_

#include "etcpal/timer.h"
#include "rdmnet/private/discovery.h"
#include "disc_platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * Access to the global discovery lock
 *************************************************************************************************/

extern etcpal_mutex_t rdmnet_disc_lock;
#define RDMNET_DISC_LOCK() etcpal_mutex_take(&rdmnet_disc_lock)
#define RDMNET_DISC_UNLOCK() etcpal_mutex_give(&rdmnet_disc_lock)

/**************************************************************************************************
 * Platform-neutral functions callable from both common.c and the platform-specific sources
 *************************************************************************************************/

// Callbacks called from platform-specific code, must be called in a locked context
void notify_scope_monitor_error(RdmnetScopeMonitorRef* ref, int platform_specific_error);
void notify_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info);
void notify_broker_lost(rdmnet_scope_monitor_t handle, const char* service_name);

#ifdef __cplusplus
}
#endif

#endif /* DISC_COMMON_H_ */
