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

#ifndef _RDMNET_DISCOVERY_AVAHI_H_
#define _RDMNET_DISCOVERY_AVAHI_H_

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>

#include "rdmnet/core/discovery.h"

typedef struct DiscoveredBroker DiscoveredBroker;
struct DiscoveredBroker
{
  char full_service_name[kDNSServiceMaxDomainName];
  RdmnetBrokerDiscInfo info;

  // State information for this broker.
  resolve_state_t state;
  //DNSServiceRef dnssd_ref;

  DiscoveredBroker* next;
};

typedef struct RdmnetScopeMonitorRef RdmnetScopeMonitorRef;
struct RdmnetScopeMonitorRef
{
  // The configuration data that the user provided.
  RdmnetScopeMonitorConfig config;
  // The Avahi browse handle
  AvahiServiceBrowser* avahi_browser;
  // If this ScopeMonitorRef is associated with a registered Broker, that is tracked here. Otherwise
  // NULL.
  rdmnet_registered_broker_t broker_handle;
  // The list of Brokers discovered or being discovered on this scope.
  DiscoveredBroker* broker_list;
  // The next ref in the list of scopes being monitored.
  RdmnetScopeMonitorRef* next;
};

typedef enum
{
  kBrokerStateNotRegistered,
  kBrokerStateInfoSet,
  kBrokerStateRegisterStarted,
  kBrokerStateRegistered
} broker_state_t;

typedef struct RdmnetBrokerRegisterRef
{
  RdmnetBrokerRegisterConfig config;
  rdmnet_scope_monitor_t scope_monitor_handle;
  broker_state_t state;
  char full_service_name[kDNSServiceMaxDomainName];

  // For hooking up to the DNS-SD API
  DNSServiceRef dnssd_ref;
} RdmnetBrokerRegisterRef;

#endif /* _RDMNET_DISCOVERY_AVAHI_H_ */

