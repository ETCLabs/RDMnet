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

/* rdmnet_mock/core/discovery.h
 * Mocking the functions of rdmnet/core/discovery.h
 */
#ifndef _RDMNET_MOCK_CORE_DISCOVERY_H_
#define _RDMNET_MOCK_CORE_DISCOVERY_H_

#include "rdmnet/core/discovery.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnetdisc_init);
DECLARE_FAKE_VOID_FUNC(rdmnetdisc_deinit);
DECLARE_FAKE_VOID_FUNC(rdmnetdisc_fill_default_broker_info, RdmnetBrokerDiscInfo *);
DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnetdisc_start_monitoring, const RdmnetScopeMonitorConfig *,
                        rdmnet_scope_monitor_t *, int *);
DECLARE_FAKE_VOID_FUNC(rdmnetdisc_stop_monitoring, rdmnet_scope_monitor_t);
DECLARE_FAKE_VOID_FUNC(rdmnetdisc_stop_monitoring_all);
DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnetdisc_register_broker, const RdmnetBrokerRegisterConfig *,
                        rdmnet_registered_broker_t *);
DECLARE_FAKE_VOID_FUNC(rdmnetdisc_unregister_broker, rdmnet_registered_broker_t);
DECLARE_FAKE_VOID_FUNC(rdmnetdisc_tick);

#define RDMNET_CORE_DISCOVERY_DO_FOR_ALL_FAKES(operation) \
  operation(rdmnetdisc_init);                             \
  operation(rdmnetdisc_deinit);                           \
  operation(rdmnetdisc_fill_default_broker_info);         \
  operation(rdmnetdisc_start_monitoring);                 \
  operation(rdmnetdisc_stop_monitoring);                  \
  operation(rdmnetdisc_stop_monitoring_all);              \
  operation(rdmnetdisc_register_broker);                  \
  operation(rdmnetdisc_unregister_broker);                \
  operation(rdmnetdisc_tick)

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_MOCK_CORE_DISCOVERY_H_ */
