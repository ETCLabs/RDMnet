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

#ifndef DISCOVERED_BROKER_H_
#define DISCOVERED_BROKER_H_

#include "rdmnet/core/discovery.h"

#include "etcpal/bool.h"
#include "rdmnet/private/opts.h"
#include "disc_platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DiscoveredBroker DiscoveredBroker;
struct DiscoveredBroker
{
  char full_service_name[RDMNET_DISC_SERVICE_NAME_MAX_LENGTH];
  RdmnetBrokerDiscInfo info;
  rdmnet_scope_monitor_t monitor_ref;
#if !RDMNET_DYNAMIC_MEM
  EtcPalIpAddr listen_addr_array[RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER];
#endif
  RdmnetDiscoveredBrokerPlatformData platform_data;
  DiscoveredBroker* next;
};

/*
 * None of these functions are thread-safe and as such they should always be called within
 * appropriate locks.
 */

/*
 * Initialize the discovered_broker module.
 */
etcpal_error_t discovered_broker_init();

/*
 * Allocates a new discovered broker reference for a given monitored scope. service_name is the
 * service instance name to populate the new broker reference with. full_service_name is the
 * service name combined with the service type and domain. Call discovered_broker_insert() to
 * insert the broker into a list.
 */
DiscoveredBroker* discovered_broker_new(rdmnet_scope_monitor_t monitor_ref, const char* service_name,
                                        const char* full_service_name);

/*
 * Adds a discovered broker reference to a linked list pointed to by *list_head_ptr (works if
 * *list_head_ptr is NULL, in that case creates the first list entry).
 */
void discovered_broker_insert(DiscoveredBroker** list_head_ptr, DiscoveredBroker* new_db);

/*
 * Adds a new broker listen address to the array for a given discovered broker.
 */
bool discovered_broker_add_listen_addr(DiscoveredBroker* db, const EtcPalIpAddr* addr);

/*
 * Searches for a DiscoveredBroker instance by full name in a list. Returns the found instance or
 * NULL if no match was found.
 */
DiscoveredBroker* discovered_broker_lookup_by_name(DiscoveredBroker* list_head, const char* full_name);

/*
 * Removes a DiscoveredBroker instance from a list. If the instance was at the head of the list,
 * updates *list_head_ptr.
 */
void discovered_broker_remove(DiscoveredBroker** list_head_ptr, const DiscoveredBroker* db);

/*
 * Deallocates a discovered broker reference.
 */
void discovered_broker_delete(DiscoveredBroker* db);

#ifdef __cplusplus
}
#endif

#endif /* DISCOVERED_BROKER_H_ */
