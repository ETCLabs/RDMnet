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

/* rdmnet/discovery/common.h
 * Common functions and definitions used by all mDNS/DNS-SD providers across platforms.
 */
#ifndef RDMNET_DISCOVERY_COMMON_H_
#define RDMNET_DISCOVERY_COMMON_H_

#include "etcpal/timer.h"
#include "rdmnet/private/discovery.h"
#include "rdmnet_disc_platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// How long we monitor the registered scope before doing the actual DNS registration of a broker.
#define BROKER_REG_QUERY_TIMEOUT 3000

typedef struct RdmnetScopeMonitorRef RdmnetScopeMonitorRef;

typedef struct DiscoveredBroker DiscoveredBroker;
struct DiscoveredBroker
{
  char full_service_name[RDMNET_DISC_SERVICE_NAME_MAX_LENGTH];
  RdmnetBrokerDiscInfo info;
  RdmnetScopeMonitorRef* monitor_ref;
  RdmnetDiscoveredBrokerPlatformData platform_data;
  DiscoveredBroker* next;
};

struct RdmnetScopeMonitorRef
{
  // The configuration data that the user provided.
  RdmnetScopeMonitorConfig config;
  // If this ScopeMonitorRef is associated with a registered Broker, that is tracked here. Otherwise
  // NULL.
  rdmnet_registered_broker_t broker_handle;
  // The list of Brokers discovered or being discovered on this scope.
  DiscoveredBroker* broker_list;
  // Platform-specific data stored with this monitor ref
  RdmnetScopeMonitorPlatformData platform_data;
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

typedef struct RdmnetBrokerRegisterRef RdmnetBrokerRegisterRef;
struct RdmnetBrokerRegisterRef
{
  RdmnetBrokerRegisterConfig config;
  rdmnet_scope_monitor_t scope_monitor_handle;
  broker_state_t state;
  char full_service_name[RDMNET_DISC_SERVICE_NAME_MAX_LENGTH];

  EtcPalTimer query_timer;
  bool query_timeout_expired;

  RdmnetBrokerRegisterPlatformData platform_data;

  RdmnetBrokerRegisterRef* next;
};

/**************************************************************************************************
 * Access to the global discovery lock
 *************************************************************************************************/

extern etcpal_mutex_t rdmnet_disc_lock;
#define RDMNET_DISC_LOCK() etcpal_mutex_take(&rdmnet_disc_lock)
#define RDMNET_DISC_UNLOCK() etcpal_mutex_give(&rdmnet_disc_lock)

/**************************************************************************************************
 * Platform-specific functions called by discovery API functions
 *************************************************************************************************/

etcpal_error_t rdmnet_disc_platform_init(void);
void rdmnet_disc_platform_deinit(void);
void rdmnet_disc_platform_tick(void);
etcpal_error_t rdmnet_disc_platform_start_monitoring(const RdmnetScopeMonitorConfig* config,
                                                     RdmnetScopeMonitorRef* handle, int* platform_specific_error);
void rdmnet_disc_platform_stop_monitoring(RdmnetScopeMonitorRef* handle);
etcpal_error_t rdmnet_disc_platform_register_broker(const RdmnetBrokerDiscInfo* info,
                                                    RdmnetBrokerRegisterRef* broker_ref, int* platform_specific_error);
void rdmnet_disc_platform_unregister_broker(rdmnet_registered_broker_t handle);

void discovered_broker_free_platform_resources(DiscoveredBroker* db);

/**************************************************************************************************
 * Platform-neutral functions callable from both common.c and the platform-specific sources
 *************************************************************************************************/

bool scope_monitor_ref_is_valid(const RdmnetScopeMonitorRef* ref);
bool broker_register_ref_is_valid(const RdmnetBrokerRegisterRef* ref);

DiscoveredBroker* discovered_broker_lookup_by_name(DiscoveredBroker* list_head, const char* full_name);
DiscoveredBroker* discovered_broker_new(const char* service_name, const char* full_service_name);
void discovered_broker_insert(DiscoveredBroker** list_head_ptr, DiscoveredBroker* new_db);
void discovered_broker_remove(DiscoveredBroker** list_head_ptr, const DiscoveredBroker* db);
void discovered_broker_delete(DiscoveredBroker* db);

// Callbacks called from platform-specific code, must be called in a locked context
void notify_scope_monitor_error(RdmnetScopeMonitorRef* ref, int platform_specific_error);
void notify_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info);
void notify_broker_lost(rdmnet_scope_monitor_t handle, const char* service_name);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_DISCOVERY_COMMON_H_ */
