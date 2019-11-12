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

/* disc_platform_api.h
 * Platform-specific functions called by discovery API functions.
 */

#ifndef DISC_PLATFORM_API_H_
#define DISC_PLATFORM_API_H_

#include "rdmnet/core/discovery.h"

etcpal_error_t rdmnet_disc_platform_init(void);
void rdmnet_disc_platform_deinit(void);
void rdmnet_disc_platform_tick(void);
etcpal_error_t rdmnet_disc_platform_start_monitoring(const RdmnetScopeMonitorConfig* config,
                                                     RdmnetScopeMonitorRef* handle, int* platform_specific_error);
void rdmnet_disc_platform_stop_monitoring(RdmnetScopeMonitorRef* handle);
etcpal_error_t rdmnet_disc_platform_register_broker(const RdmnetBrokerDiscInfo* info,
                                                    RdmnetBrokerRegisterRef* broker_ref, int* platform_specific_error);
void rdmnet_disc_platform_unregister_broker(rdmnet_registered_broker_t handle);

#endif /* DISC_PLATFORM_API_H_ */
