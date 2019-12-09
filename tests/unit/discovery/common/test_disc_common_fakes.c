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

#include "test_disc_common_fakes.h"

DEFINE_FFF_GLOBALS;

DEFINE_FAKE_VALUE_FUNC(bool, rdmnet_core_initialized);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_init, const RdmnetNetintConfig*);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_platform_deinit);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_platform_tick);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_start_monitoring, const RdmnetScopeMonitorConfig*,
                       RdmnetScopeMonitorRef*, int*);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_platform_stop_monitoring, RdmnetScopeMonitorRef*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_register_broker, const RdmnetBrokerDiscInfo*,
                       RdmnetBrokerRegisterRef*, int*);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_platform_unregister_broker, rdmnet_registered_broker_t);
DEFINE_FAKE_VOID_FUNC(discovered_broker_free_platform_resources, DiscoveredBroker*);
DEFINE_FAKE_VOID_FUNC(monitorcb_broker_found, rdmnet_scope_monitor_t, const RdmnetBrokerDiscInfo*, void*);
DEFINE_FAKE_VOID_FUNC(monitorcb_broker_lost, rdmnet_scope_monitor_t, const char*, const char*, void*);
DEFINE_FAKE_VOID_FUNC(monitorcb_scope_monitor_error, rdmnet_scope_monitor_t, const char*, int, void*);
DEFINE_FAKE_VOID_FUNC(regcb_broker_registered, rdmnet_registered_broker_t, const char*, void*);
DEFINE_FAKE_VOID_FUNC(regcb_broker_register_error, rdmnet_registered_broker_t, int, void*);
DEFINE_FAKE_VOID_FUNC(regcb_broker_found, rdmnet_registered_broker_t, const RdmnetBrokerDiscInfo*, void*);
DEFINE_FAKE_VOID_FUNC(regcb_broker_lost, rdmnet_registered_broker_t, const char*, const char*, void*);
DEFINE_FAKE_VOID_FUNC(regcb_scope_monitor_error, rdmnet_registered_broker_t, const char*, int, void*);
