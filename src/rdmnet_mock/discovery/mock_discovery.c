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

#include "rdmnet_mock/core/discovery.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_init, const RdmnetNetintConfig*);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_deinit);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_init_broker_info, RdmnetBrokerDiscInfo*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_start_monitoring, const RdmnetScopeMonitorConfig*,
                       rdmnet_scope_monitor_t*, int*);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_stop_monitoring, rdmnet_scope_monitor_t);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_stop_monitoring_all);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_register_broker, const RdmnetBrokerRegisterConfig*,
                       rdmnet_registered_broker_t*);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_unregister_broker, rdmnet_registered_broker_t);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_tick);
