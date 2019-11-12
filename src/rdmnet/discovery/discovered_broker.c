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

#include "discovered_broker.h"

DiscoveredBroker* discovered_broker_new(const char* service_name, const char* full_service_name)
{
  DiscoveredBroker* new_db = (DiscoveredBroker*)malloc(sizeof(DiscoveredBroker));
  if (new_db)
  {
    rdmnet_disc_init_broker_info(&new_db->info);
    rdmnet_safe_strncpy(new_db->info.service_name, service_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(new_db->full_service_name, full_service_name, RDMNET_DISC_SERVICE_NAME_MAX_LENGTH);
    memset(&new_db->platform_data, 0, sizeof(RdmnetDiscoveredBrokerPlatformData));
    new_db->next = NULL;
  }
  return new_db;
}

/* Adds broker discovery information into brokers.
 * Assumes a lock is already taken.*/
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

/* Searches for a DiscoveredBroker instance by full name in a list.
 * Returns the found instance or NULL if no match was found.
 * Assumes a lock is already taken.
 */
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

/* Removes a DiscoveredBroker instance from a list.
 * Assumes a lock is already taken.*/
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
  BrokerListenAddr* listen_addr = db->info.listen_addr_list;
  while (listen_addr)
  {
    BrokerListenAddr* next_listen_addr = listen_addr->next;
    free(listen_addr);
    listen_addr = next_listen_addr;
  }
  discovered_broker_free_platform_resources(db);
  free(db);
}
