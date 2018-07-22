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

/*! \file rdmnet/discovery.h
 *  \brief Functions to discover a Broker and/or register a Broker for
 *         discovery. Uses mDNS and DNS-SD under the hood.
 */
#ifndef _RDMNET_DISCOVERY_H_
#define _RDMNET_DISCOVERY_H_

#include "lwpa_error.h"
#include "lwpa_cid.h"
#include "lwpa_socket.h"

#include "estardmnet.h"

#define SRV_TYPE_PADDED_LENGTH \
  32  // does not appear to have a standardized size in E133, current default
      // value that is being used is "_draft-e133._tcp."

#define ARRAY_SIZE_DEFAULT \
  100  // defines the size of the arrays, i.e. operation_map,
       // m_brokers_being_discovered, etc

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScopeMonitorInfo
{
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  char domain[E133_DOMAIN_STRING_PADDED_LENGTH];
} ScopeMonitorInfo;

typedef struct BrokerDiscInfo
{
  LwpaCid cid;
  char service_name[E133_SERVICE_NAME_STRING_PADDED_LENGTH];
  uint16_t port;
  LwpaSockaddr listen_addrs[ARRAY_SIZE_DEFAULT];
  size_t listen_addrs_count;
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  char model[E133_MODEL_STRING_PADDED_LENGTH];
  char manufacturer[E133_MANUFACTURER_STRING_PADDED_LENGTH];
} BrokerDiscInfo;

typedef struct RdmnetDiscCallbacks
{
  void (*broker_found)(const char *scope, const BrokerDiscInfo *broker_info, void *context);
  void (*broker_lost)(const char *service_name, void *context);
  void (*scope_monitor_error)(const ScopeMonitorInfo *scope_info, int platform_error, void *context);
  void (*broker_registered)(const BrokerDiscInfo *broker_info, const char *assigned_service_name, void *context);
  void (*broker_register_error)(const BrokerDiscInfo *broker_info, int platform_error, void *context);
} RdmnetDiscCallbacks;

lwpa_error_t rdmnetdisc_init(RdmnetDiscCallbacks *callbacks);
void rdmnetdisc_deinit();

void fill_default_scope_info(ScopeMonitorInfo *scope_info);
void fill_default_broker_info(BrokerDiscInfo *broker_info);

lwpa_error_t rdmnetdisc_startmonitoring(const ScopeMonitorInfo *scope_info, int *platform_specific_error,
                                        void *context);

void rdmnetdisc_stopmonitoring(const ScopeMonitorInfo *scope_info);
void rdmnetdisc_stopmonitoring_all_scopes();

lwpa_error_t rdmnetdisc_registerbroker(const BrokerDiscInfo *broker_info, void *context);
void rdmnetdisc_unregisterbroker();

void rdmnetdisc_tick();

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_DISCOVERY_H_ */
