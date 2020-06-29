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

#ifndef TEST_DISC_COMMON_FAKES_H_
#define TEST_DISC_COMMON_FAKES_H_

#include "fff.h"
#include "rdmnet/discovery.h"
#include "rdmnet/disc/platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific rdmnet_disc sources
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_init, const RdmnetNetintConfig*);
DECLARE_FAKE_VOID_FUNC(rdmnet_disc_platform_deinit);
DECLARE_FAKE_VOID_FUNC(rdmnet_disc_platform_tick);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_start_monitoring, RdmnetScopeMonitorRef*, int*);
DECLARE_FAKE_VOID_FUNC(rdmnet_disc_platform_stop_monitoring, RdmnetScopeMonitorRef*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_register_broker, RdmnetBrokerRegisterRef*, int*);
DECLARE_FAKE_VOID_FUNC(rdmnet_disc_platform_unregister_broker, rdmnet_registered_broker_t);
DECLARE_FAKE_VOID_FUNC(discovered_broker_free_platform_resources, DiscoveredBroker*);

// rdmnet_disc callbacks
DECLARE_FAKE_VOID_FUNC(monitorcb_broker_found, rdmnet_scope_monitor_t, const RdmnetBrokerDiscInfo*, void*);
DECLARE_FAKE_VOID_FUNC(monitorcb_broker_updated, rdmnet_scope_monitor_t, const RdmnetBrokerDiscInfo*, void*);
DECLARE_FAKE_VOID_FUNC(monitorcb_broker_lost, rdmnet_scope_monitor_t, const char*, const char*, void*);

DECLARE_FAKE_VOID_FUNC(regcb_broker_registered, rdmnet_registered_broker_t, const char*, void*);
DECLARE_FAKE_VOID_FUNC(regcb_broker_register_error, rdmnet_registered_broker_t, int, void*);
DECLARE_FAKE_VOID_FUNC(regcb_other_broker_found, rdmnet_registered_broker_t, const RdmnetBrokerDiscInfo*, void*);
DECLARE_FAKE_VOID_FUNC(regcb_other_broker_lost, rdmnet_registered_broker_t, const char*, const char*, void*);

#ifdef __cplusplus
}
#endif

inline void TestDiscoveryCommonResetAllFakes()
{
  RESET_FAKE(rdmnet_disc_platform_init);
  RESET_FAKE(rdmnet_disc_platform_deinit);
  RESET_FAKE(rdmnet_disc_platform_tick);
  RESET_FAKE(rdmnet_disc_platform_start_monitoring);
  RESET_FAKE(rdmnet_disc_platform_stop_monitoring);
  RESET_FAKE(rdmnet_disc_platform_register_broker);
  RESET_FAKE(rdmnet_disc_platform_unregister_broker);
  RESET_FAKE(discovered_broker_free_platform_resources);

  RESET_FAKE(monitorcb_broker_found);
  RESET_FAKE(monitorcb_broker_updated);
  RESET_FAKE(monitorcb_broker_lost);

  RESET_FAKE(regcb_broker_registered);
  RESET_FAKE(regcb_broker_register_error);
  RESET_FAKE(regcb_other_broker_found);
  RESET_FAKE(regcb_other_broker_lost);
}

#endif /* TEST_DISC_COMMON_FAKES_H_ */
