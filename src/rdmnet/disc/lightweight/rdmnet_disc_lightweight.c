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

#include "etcpal/common.h"
#include "rdmnet/disc/common.h"
#include "rdmnet/disc/platform_api.h"
#include "rdmnet/disc/discovered_broker.h"
#include "rdmnet/disc/registered_broker.h"
#include "rdmnet/disc/monitored_scope.h"
#include "lwmdns_common.h"
#include "lwmdns_send.h"
#include "lwmdns_recv.h"

#define INITIAL_QUERY_INTERVAL 1000
#define QUERY_BACKOFF_FACTOR 3

etcpal_error_t rdmnet_disc_platform_init(const RdmnetNetintConfig* netint_config)
{
  etcpal_error_t res = lwmdns_common_module_init();
  if (res != kEtcPalErrOk)
    return res;

  res = lwmdns_recv_module_init(netint_config);
  if (res != kEtcPalErrOk)
  {
    lwmdns_common_module_deinit();
    return res;
  }

  res = lwmdns_send_module_init(netint_config);
  if (res != kEtcPalErrOk)
  {
    lwmdns_recv_module_deinit();
    lwmdns_common_module_deinit();
  }
  return res;
}

void rdmnet_disc_platform_deinit(void)
{
  lwmdns_send_module_deinit();
  lwmdns_recv_module_deinit();
  lwmdns_common_module_deinit();
}

etcpal_error_t rdmnet_disc_platform_start_monitoring(RdmnetScopeMonitorRef* handle, int* platform_specific_error)
{
  ETCPAL_UNUSED_ARG(platform_specific_error);
  lwmdns_send_query(handle);
  handle->platform_data.sent_first_query = true;
  etcpal_timer_start(&handle->platform_data.query_timer, INITIAL_QUERY_INTERVAL);
  return kEtcPalErrOk;
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

etcpal_error_t rdmnet_disc_platform_register_broker(RdmnetBrokerRegisterRef* broker_ref, int* platform_specific_error)
{
  ETCPAL_UNUSED_ARG(broker_ref);
  ETCPAL_UNUSED_ARG(platform_specific_error);
  return kEtcPalErrNotImpl;
}

void process_monitored_scope(RdmnetScopeMonitorRef* monitor_ref)
{
  if (etcpal_timer_is_expired(&monitor_ref->platform_data.query_timer))
  {
    lwmdns_send_query(monitor_ref);
    uint32_t new_interval = monitor_ref->platform_data.query_timer.interval * QUERY_BACKOFF_FACTOR;
    if (new_interval > 360000)
      new_interval = 360000;
    etcpal_timer_start(&monitor_ref->platform_data.query_timer, new_interval);
  }
}

void rdmnet_disc_platform_tick(void)
{
  scope_monitor_for_each(process_monitored_scope);
}
