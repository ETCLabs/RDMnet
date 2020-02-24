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

/* Implementation of the discovered_broker.h functions. */

#include "discovered_broker.h"

#include "rdmnet/core/util.h"
#include "rdmnet/private/opts.h"
#include "disc_platform_api.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/****************************** Private macros *******************************/

#if RDMNET_DYNAMIC_MEM
#define ALLOC_DISCOVERED_BROKER() (DiscoveredBroker*)malloc(sizeof(DiscoveredBroker))
#define FREE_DISCOVERED_BROKER(ptr) free(ptr)
#else
#define ALLOC_DISCOVERED_BROKER() (DiscoveredBroker*)etcpal_mempool_alloc(discovered_brokers)
#define FREE_DISCOVERED_BROKER(ptr) etcpal_mempool_free(discovered_brokers, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(discovered_brokers, DiscoveredBroker, RDMNET_MAX_DISCOVERED_BROKERS);
#endif

etcpal_error_t discovered_broker_init(void)
{
#if RDMNET_DYNAMIC_MEM
  return kEtcPalErrOk;
#else
  return etcpal_mempool_init(discovered_brokers);
#endif
}

DiscoveredBroker* discovered_broker_new(rdmnet_scope_monitor_t monitor_ref, const char* service_name,
                                        const char* full_service_name)
{
  DiscoveredBroker* new_db = ALLOC_DISCOVERED_BROKER();
  if (new_db)
  {
    rdmnet_disc_init_broker_info(&new_db->info);
#if !RDMNET_DYNAMIC_MEM
    new_db->info.listen_addrs = new_db->listen_addr_array;
#endif
    new_db->monitor_ref = monitor_ref;
    rdmnet_safe_strncpy(new_db->info.service_name, service_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(new_db->full_service_name, full_service_name, RDMNET_DISC_SERVICE_NAME_MAX_LENGTH);
    memset(&new_db->platform_data, 0, sizeof(RdmnetDiscoveredBrokerPlatformData));
    new_db->next = NULL;
  }
  return new_db;
}

void discovered_broker_insert(DiscoveredBroker** list_head_ptr, DiscoveredBroker* new_db)
{
  if (*list_head_ptr)
  {
    DiscoveredBroker* cur = *list_head_ptr;
    for (; cur->next; cur = cur->next)
      ;
    cur->next = new_db;
  }
  else
  {
    *list_head_ptr = new_db;
  }
}

bool discovered_broker_add_listen_addr(DiscoveredBroker* db, const EtcPalIpAddr* addr)
{
#if RDMNET_DYNAMIC_MEM
  if (!db->info.listen_addrs)
  {
    db->info.listen_addrs = (EtcPalIpAddr*)malloc(sizeof(EtcPalIpAddr));
    if (db->info.listen_addrs)
    {
      db->info.num_listen_addrs = 1;
      db->info.listen_addrs[db->info.num_listen_addrs - 1] = *addr;
      return true;
    }
  }
  else
  {
    EtcPalIpAddr* new_arr =
        (EtcPalIpAddr*)realloc(db->info.listen_addrs, sizeof(EtcPalIpAddr) * (db->info.num_listen_addrs + 1));
    if (new_arr)
    {
      db->info.listen_addrs = new_arr;
      ++db->info.num_listen_addrs;
      db->info.listen_addrs[db->info.num_listen_addrs - 1] = *addr;
      return true;
    }
  }
  return false;
#else
  if (db->info.num_listen_addrs < RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER)
  {
    db->listen_addr_array[db->info.num_listen_addrs] = *addr;
    ++db->info.num_listen_addrs;
    return true;
  }
  return false;
#endif
}

DiscoveredBroker* discovered_broker_lookup_by_name(DiscoveredBroker* list_head, const char* full_name)
{
  for (DiscoveredBroker* current = list_head; current; current = current->next)
  {
    if (strcmp(current->full_service_name, full_name) == 0)
    {
      return current;
    }
  }
  return NULL;
}

void discovered_broker_remove(DiscoveredBroker** list_head_ptr, const DiscoveredBroker* db)
{
  if (!(*list_head_ptr))
    return;

  if (*list_head_ptr == db)
  {
    // Remove from the head of the list
    *list_head_ptr = (*list_head_ptr)->next;
  }
  else
  {
    // Find in the list and remove.
    for (DiscoveredBroker* prev_db = *list_head_ptr; prev_db->next; prev_db = prev_db->next)
    {
      if (prev_db->next == db)
      {
        prev_db->next = prev_db->next->next;
        break;
      }
    }
  }
}

void discovered_broker_delete(DiscoveredBroker* db)
{
#if RDMNET_DYNAMIC_MEM
  free(db->info.listen_addrs);
#endif
  discovered_broker_free_platform_resources(db);
  FREE_DISCOVERED_BROKER(db);
}
