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

#ifndef RDMNET_DISC_MONITORED_SCOPE_H_
#define RDMNET_DISC_MONITORED_SCOPE_H_

#include "rdmnet/discovery.h"
#include "rdmnet/disc/discovered_broker.h"
#include "rdmnet_disc_platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RdmnetScopeMonitorRef RdmnetScopeMonitorRef;
struct RdmnetScopeMonitorRef
{
  /////////////////////////////////////////////////////////////////////////////
  // The configuration data that the user provided.

  char                        scope[E133_SCOPE_STRING_PADDED_LENGTH];
  char                        domain[E133_DOMAIN_STRING_PADDED_LENGTH];
  RdmnetScopeMonitorCallbacks callbacks;

  /////////////////////////////////////////////////////////////////////////////

  // If this ScopeMonitorRef is associated with a registered Broker, that is tracked here. Otherwise
  // NULL.
  rdmnet_registered_broker_t broker_handle;
  // The list of Brokers discovered or being discovered on this scope.
  DiscoveredBroker* broker_list;
  // Platform-specific data stored with this monitor ref
  RdmnetScopeMonitorPlatformData platform_data;
};

typedef void (*ScopeMonitorRefFunction)(RdmnetScopeMonitorRef* ref);
typedef bool (*ScopeMonitorRefPredicateFunction)(const RdmnetScopeMonitorRef* ref, const void* context);
typedef bool (*ScopeMonitorAndDBPredicateFunction)(const RdmnetScopeMonitorRef* ref,
                                                   const DiscoveredBroker*      db,
                                                   const void*                  context);

etcpal_error_t         monitored_scope_module_init(void);
void                   monitored_scope_module_deinit(void);
RdmnetScopeMonitorRef* scope_monitor_new(const RdmnetScopeMonitorConfig* config);
void                   scope_monitor_insert(RdmnetScopeMonitorRef* scope_ref);
bool                   scope_monitor_ref_is_valid(const RdmnetScopeMonitorRef* ref);
void                   scope_monitor_for_each(ScopeMonitorRefFunction func);
RdmnetScopeMonitorRef* scope_monitor_find(ScopeMonitorRefPredicateFunction predicate, const void* context);
bool                   scope_monitor_and_discovered_broker_find(ScopeMonitorAndDBPredicateFunction predicate,
                                                                const void*                        context,
                                                                RdmnetScopeMonitorRef**            found_ref,
                                                                DiscoveredBroker**                 found_db);
void                   scope_monitor_remove(const RdmnetScopeMonitorRef* ref);
void                   scope_monitor_delete(RdmnetScopeMonitorRef* ref);
void                   scope_monitor_delete_all(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_DISC_MONITORED_SCOPE_H_ */
