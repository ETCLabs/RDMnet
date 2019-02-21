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

#ifndef _RDMNET_DISCOVERY_BONJOUR_H_
#define _RDMNET_DISCOVERY_BONJOUR_H_

#include "dns_sd.h"
#include "lwpa/lock.h"
#include "lwpa/socket.h"
#include "rdmnet/private/opts.h"

/*From dns_sd.h :
 *  For most applications, DNS - SD TXT records are generally
 *  less than 100 bytes, so in most cases a simple fixed - sized
 *  256 - byte buffer will be more than sufficient.*/
#define TXT_RECORD_BUFFER_LENGTH 256
#define REGISTRATION_STRING_PADDED_LENGTH E133_DNSSD_SRV_TYPE_PADDED_LENGTH + E133_SCOPE_STRING_PADDED_LENGTH + 4

#define MAX_SCOPES_MONITORED ((RDMNET_MAX_SCOPES_PER_CONTROLLER * RDMNET_MAX_CONTROLLERS) + RDMNET_MAX_DEVICES)

typedef enum
{
  kResolveStateServiceResolve,
  kResolveStateGetAddrInfo,
  kResolveStateDone
} resolve_state_t;

typedef struct DiscoveredBroker DiscoveredBroker;
struct DiscoveredBroker
{
  char full_service_name[kDNSServiceMaxDomainName];
  RdmnetBrokerDiscInfo info;

  // State information for this broker.
  resolve_state_t state;
  lwpa_socket_t sock;
  DNSServiceRef dnssd_ref;

  DiscoveredBroker *next;
};

typedef struct RdmnetScopeMonitorRef RdmnetScopeMonitorRef;
struct RdmnetScopeMonitorRef
{
  // The configuration data that the user provided.
  RdmnetScopeMonitorConfig config;
  // The Bonjour handle
  DNSServiceRef dnssd_ref;
  // If this ScopeMonitorRef is associated with a registered Broker, that is tracked here. Otherwise
  // NULL.
  rdmnet_registered_broker_t broker_handle;
  // The list of Brokers discovered or being discovered on this scope.
  DiscoveredBroker *broker_list;
  // The next ref in the list of scopes being monitored.
  RdmnetScopeMonitorRef *next;
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
  lwpa_socket_t socket;
} RdmnetBrokerRegisterRef;

#endif /* _RDMNET_DISCOVERY_BONJOUR_H_ */
