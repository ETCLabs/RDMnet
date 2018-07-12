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
#include "broker/discovery.h"

static BrokerDiscoveryManager *g_instance;

/*********************** Private function prototypes *************************/

static void disccb_broker_found(const char *scope, const BrokerDiscInfo *broker_info);
static void disccb_broker_lost(const char *service_name);
static void disccb_scope_monitor_error(const ScopeMonitorInfo *scope_info, int platform_error);
static void disccb_broker_registered(const BrokerDiscInfo *broker_info, const char *assigned_service_name);
static void disccb_broker_register_error(const BrokerDiscInfo *broker_info, int platform_error);

/*************************** Function definitions ****************************/

lwpa_error_t BrokerDiscoveryManager::InitLibrary()
{
  RdmnetDiscCallbacks callbacks = {disccb_broker_found, disccb_broker_lost, disccb_scope_monitor_error,
                                   disccb_broker_registered, disccb_broker_register_error};

  return rdmnetdisc_init(&callbacks);
}

void BrokerDiscoveryManager::DeinitLibrary()
{
  rdmnetdisc_deinit();
}

BrokerDiscoveryManager::BrokerDiscoveryManager()
{
  // TODO BIG HACK, waiting on the context pointers in the discovery library
  g_instance = this;
}

BrokerDiscoveryManager::~BrokerDiscoveryManager()
{
  // TODO BIG HACK, waiting on the context pointers in the discovery library
  g_instance = nullptr;
}

lwpa_error_t BrokerDiscoveryManager::RegisterBroker(const BrokerDiscoveryAttributes &disc_attributes)
{
}
