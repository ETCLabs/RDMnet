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

#include "etcpal/common.h"
#include "disc_common.h"
#include "disc_platform_api.h"
#include "discovered_broker.h"
#include "registered_broker.h"
#include "monitored_scope.h"

etcpal_error_t rdmnet_disc_platform_init(const RdmnetNetintConfig* netint_config)
{
  ETCPAL_UNUSED_ARG(netint_config);
  return kEtcPalErrOk;
}

void rdmnet_disc_platform_deinit(void)
{
}

etcpal_error_t rdmnet_disc_platform_start_monitoring(const RdmnetScopeMonitorConfig* config,
                                                     RdmnetScopeMonitorRef* handle, int* platform_specific_error)
{
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(platform_specific_error);
  return kEtcPalErrNotImpl;
}

void rdmnet_disc_platform_stop_monitoring(RdmnetScopeMonitorRef* handle)
{
  ETCPAL_UNUSED_ARG(handle);
}

void rdmnet_disc_platform_unregister_broker(rdmnet_registered_broker_t handle)
{
  ETCPAL_UNUSED_ARG(handle);
}

void discovered_broker_free_platform_resources(DiscoveredBroker* db)
{
  ETCPAL_UNUSED_ARG(db);
}

/* If returns !0, this was an error from Bonjour.  Reset the state and notify the callback.*/
etcpal_error_t rdmnet_disc_platform_register_broker(const RdmnetBrokerDiscInfo* info,
                                                    RdmnetBrokerRegisterRef* broker_ref, int* platform_specific_error)
{
  ETCPAL_UNUSED_ARG(info);
  ETCPAL_UNUSED_ARG(broker_ref);
  ETCPAL_UNUSED_ARG(platform_specific_error);
  return kEtcPalErrNotImpl;
}

void rdmnet_disc_platform_tick(void)
{
}
