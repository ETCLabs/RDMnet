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

#ifndef LWMDNS_SEND_
#define LWMDNS_SEND_

#include "etcpal/error.h"
#include "rdmnet/common.h"
#include "rdmnet/disc/monitored_scope.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t lwmdns_send_module_init(const RdmnetNetintConfig* netint_config);
void           lwmdns_send_module_deinit(void);

void lwmdns_send_ptr_query(const RdmnetScopeMonitorRef* ref);
void lwmdns_send_any_query_on_service(const DiscoveredBroker* db);
void lwmdns_send_any_query_on_hostname(const DiscoveredBroker* db);

#ifdef __cplusplus
}
#endif

#endif /* LWMDNS_SEND_ */
