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

#ifndef _RDMNET_DISCOVERY_AVAHI_H_
#define _RDMNET_DISCOVERY_AVAHI_H_

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/domain.h>

#include "etcpal/timer.h"
#include "rdmnet/core/discovery.h"

#define SERVICE_STR_PADDED_LENGTH E133_DNSSD_SRV_TYPE_PADDED_LENGTH + E133_SCOPE_STRING_PADDED_LENGTH + 10

typedef struct RdmnetScopeMonitorRef RdmnetScopeMonitorRef;

typedef struct DiscoveredBroker DiscoveredBroker;
struct DiscoveredBroker
{
  char full_service_name[AVAHI_DOMAIN_NAME_MAX];
  RdmnetBrokerDiscInfo info;
  RdmnetScopeMonitorRef* monitor_ref;

  // State information for this broker.
  int num_outstanding_resolves;
  int num_successful_resolves;

  DiscoveredBroker* next;
};

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
  kBrokerStateQuerying,
  kBrokerStateRegisterStarted,
  kBrokerStateRegistered
} broker_state_t;

typedef struct RdmnetBrokerRegisterRef
{
  RdmnetBrokerRegisterConfig config;
  rdmnet_scope_monitor_t scope_monitor_handle;
  broker_state_t state;
  char full_service_name[AVAHI_DOMAIN_NAME_MAX];

  EtcPalTimer query_timer;
  bool query_timeout_expired;

  // For hooking up to the DNS-SD API
  AvahiEntryGroup* avahi_entry_group;
} RdmnetBrokerRegisterRef;

#endif /* _RDMNET_DISCOVERY_AVAHI_H_ */
