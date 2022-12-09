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

/*
 * rdmnet/disc/common.h
 * Common functions and definitions used by all mDNS/DNS-SD providers across platforms.
 */

#ifndef RDMNET_DISC_COMMON_H_
#define RDMNET_DISC_COMMON_H_

#include "etcpal/mutex.h"
#include "etcpal/timer.h"
#include "rdmnet/common.h"
#include "rdmnet/discovery.h"

#ifdef __cplusplus
extern "C" {
#endif

// The interval at which this broker checks for conflicting brokers. At least one of these must elapse before DNS
// registration.
#define BROKER_REG_QUERY_TIMEOUT 3000

#define E133_TXT_VERS_KEY "TxtVers"
#define E133_TXT_SCOPE_KEY "E133Scope"
#define E133_TXT_E133VERS_KEY "E133Vers"
#define E133_TXT_CID_KEY "CID"
#define E133_TXT_UID_KEY "UID"
#define E133_TXT_MODEL_KEY "Model"
#define E133_TXT_MANUFACTURER_KEY "Manuf"

/**************************************************************************************************
 * Access to the global discovery lock
 *************************************************************************************************/

extern etcpal_mutex_t rdmnet_disc_lock;
#define RDMNET_DISC_LOCK() etcpal_mutex_lock(&rdmnet_disc_lock)
#define RDMNET_DISC_UNLOCK() etcpal_mutex_unlock(&rdmnet_disc_lock)

/**************************************************************************************************
 * Platform-neutral functions callable from both common.c and the platform-specific sources
 *************************************************************************************************/

etcpal_error_t rdmnet_disc_module_init(const RdmnetNetintConfig* netint_config);
void           rdmnet_disc_module_deinit(void);
void           rdmnet_disc_module_tick(void);

bool rdmnet_disc_broker_should_deregister(const EtcPalUuid* this_broker_cid, const EtcPalUuid* other_broker_cid);

// Callbacks called from platform-specific code, must be called in a locked context
void notify_scope_monitor_error(rdmnet_scope_monitor_t handle, int platform_specific_error);
void notify_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info);
void notify_broker_updated(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info);
void notify_broker_lost(rdmnet_scope_monitor_t handle, const char* service_name, const EtcPalUuid* broker_cid);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_DISC_COMMON_H_ */
