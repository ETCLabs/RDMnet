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

#include "lwpa_lock.h"
#include "dns_sd.h"

/*From dns_sd.h :
 *  For most applications, DNS - SD TXT records are generally
 *  less than 100 bytes, so in most cases a simple fixed - sized
 *  256 - byte buffer will be more than sufficient.*/
#define TXT_RECORD_BUFFER_LENGTH 256
#define REGISTRATION_STRING_PADDED_LENGTH SRV_TYPE_PADDED_LENGTH + E133_SCOPE_STRING_PADDED_LENGTH + 4

enum BROKER_STATE
{
  kBrokerNotRegistered,
  kBrokerInfoSet,
  kBrokeRegisterStarted,
  kBrokerRegistered
};

typedef struct OperationData
{
  lwpa_socket_t socket;
  DNSServiceRef search_ref;
  char full_name[kDNSServiceMaxDomainName];
} OperationData;

typedef struct Operations
{
  DNSServiceRef refs[ARRAY_SIZE_DEFAULT];
  OperationData op_data[ARRAY_SIZE_DEFAULT];
  size_t count;
} Operations;

typedef struct ScopesMonitored
{
  DNSServiceRef refs[ARRAY_SIZE_DEFAULT];
  ScopeMonitorInfo monitor_info[ARRAY_SIZE_DEFAULT];
  size_t count;
} ScopesMonitored;

typedef struct BrokersBeingDiscovered
{
  char fullnames[ARRAY_SIZE_DEFAULT][kDNSServiceMaxDomainName];
  BrokerDiscInfo info[ARRAY_SIZE_DEFAULT];
  size_t count;
} BrokersBeingDiscovered;

#endif /* _RDMNET_DISCOVERY_BONJOUR_H_ */
