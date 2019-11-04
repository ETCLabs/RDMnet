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

/* The rdmnetdisc_platform_defs.h specialization for Bonjour. */
#ifndef RDMNETDISC_PLATFORM_DEFS_H_
#define RDMNETDISC_PLATFORM_DEFS_H_

#include "dns_sd.h"

#define RDMNETDISC_SERVICE_NAME_MAX_LENGTH kDNSServiceMaxDomainName

typedef enum
{
  kResolveStateServiceResolve,
  kResolveStateGetAddrInfo,
  kResolveStateDone
} resolve_state_t;

typedef struct RdmnetDiscoveredBrokerPlatformData
{
  // State information for this broker.
  resolve_state_t state;
  DNSServiceRef dnssd_ref;
} RdmnetDiscoveredBrokerPlatformData;

typedef struct RdmnetScopeMonitorPlatformData
{
  // The Bonjour handle
  DNSServiceRef dnssd_ref;
} RdmnetScopeMonitorPlatformData;

typedef struct RdmnetBrokerRegisterPlatformData
{
  // For hooking up to the DNS-SD API
  DNSServiceRef dnssd_ref;
} RdmnetBrokerRegisterPlatformData;

#endif /* RDMNETDISC_PLATFORM_DEFS_H_ */
