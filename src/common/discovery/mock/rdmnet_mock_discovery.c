/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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

/*! \file rdmnet_mock_discovery.c
 *  \brief Provides a mock implementation of the discovery functions.
 *  These are currently just empty shims to allow RDMnet to be built by CI.
 *  \author Sam Kearney
 */

#include "rdmnet/common/discovery.h"

lwpa_error_t rdmnetdisc_init(RdmnetDiscCallbacks *callbacks)
{
  (void)callbacks;
  return LWPA_NOTIMPL;
}

void rdmnetdisc_deinit()
{
}

void fill_default_scope_info(ScopeMonitorInfo *scope_info)
{
  (void)scope_info;
}

void fill_default_broker_info(BrokerDiscInfo *broker_info)
{
  (void)broker_info;
}

lwpa_error_t rdmnetdisc_startmonitoring(const ScopeMonitorInfo *scope_info, int *platform_specific_error, void *context)
{
  (void)scope_info;
  (void)platform_specific_error;
  (void)context;
  return LWPA_NOTIMPL;
}

void rdmnetdisc_stopmonitoring(const ScopeMonitorInfo *scope_info)
{
  (void)scope_info;
}

void rdmnetdisc_stopmonitoring_all_scopes()
{
}

lwpa_error_t rdmnetdisc_registerbroker(const BrokerDiscInfo *broker_info, bool monitor_scope, void *context)
{
  (void)broker_info;
  (void)monitor_scope;
  (void)context;
  return LWPA_NOTIMPL;
}

void rdmnetdisc_unregisterbroker(bool stop_monitoring_scope)
{
  (void)stop_monitoring_scope;
}

void rdmnetdisc_tick()
{
}
