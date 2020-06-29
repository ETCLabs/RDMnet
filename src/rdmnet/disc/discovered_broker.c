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

/* Implementation of the discovered_broker.h functions. */

#include "rdmnet/disc/discovered_broker.h"

#include <string.h>
#include "rdmnet/core/util.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/disc/platform_api.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/****************************** Private macros *******************************/

#if RDMNET_DYNAMIC_MEM
#define ALLOC_DISCOVERED_BROKER() (DiscoveredBroker*)malloc(sizeof(DiscoveredBroker))
#define FREE_DISCOVERED_BROKER(ptr) free(ptr)
#elif RDMNET_MAX_DISCOVERED_BROKERS
#define ALLOC_DISCOVERED_BROKER() (DiscoveredBroker*)etcpal_mempool_alloc(discovered_brokers)
#define FREE_DISCOVERED_BROKER(ptr) etcpal_mempool_free(discovered_brokers, ptr)
#else
#define ALLOC_DISCOVERED_BROKER() NULL
#define FREE_DISCOVERED_BROKER(ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM && RDMNET_MAX_DISCOVERED_BROKERS
ETCPAL_MEMPOOL_DEFINE(discovered_brokers, DiscoveredBroker, RDMNET_MAX_DISCOVERED_BROKERS);
#endif

etcpal_error_t discovered_broker_module_init(void)
{
#if RDMNET_DYNAMIC_MEM
  return kEtcPalErrOk;
#else
  return etcpal_mempool_init(discovered_brokers);
#endif
}

DiscoveredBroker* discovered_broker_new(rdmnet_scope_monitor_t monitor_ref,
                                        const char*            service_name,
                                        const char*            full_service_name)
{
  DiscoveredBroker* new_db = ALLOC_DISCOVERED_BROKER();
  if (new_db)
  {
    memset(new_db, 0, sizeof(DiscoveredBroker));
    new_db->monitor_ref = monitor_ref;
    rdmnet_safe_strncpy(new_db->service_instance_name, service_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
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
  if (!db->listen_addr_array)
  {
    db->listen_addr_array = (EtcPalIpAddr*)malloc(sizeof(EtcPalIpAddr));
    if (db->listen_addr_array)
    {
      db->listen_addr_array[0] = *addr;
      db->num_listen_addrs = 1;
      return true;
    }
  }
  else
  {
    EtcPalIpAddr* new_arr =
        (EtcPalIpAddr*)realloc(db->listen_addr_array, sizeof(EtcPalIpAddr) * (db->num_listen_addrs + 1));
    if (new_arr)
    {
      db->listen_addr_array = new_arr;
      db->listen_addr_array[db->num_listen_addrs++] = *addr;
      return true;
    }
  }
  return false;
#else
  if (db->num_listen_addrs < RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER)
  {
    db->listen_addr_array[db->num_listen_addrs++] = *addr;
    return true;
  }
  return false;
#endif
}

#if RDMNET_DYNAMIC_MEM
static bool expand_txt_record_arrays(DiscoveredBroker* db)
{
  if (!db->additional_txt_items_array)
  {
    db->additional_txt_items_array = (RdmnetDnsTxtRecordItem*)malloc(sizeof(RdmnetDnsTxtRecordItem));
    db->additional_txt_items_data = (DnsTxtRecordItemInternal*)malloc(sizeof(DnsTxtRecordItemInternal));
    if (db->additional_txt_items_array && db->additional_txt_items_data)
    {
      ++db->num_additional_txt_items;
      return true;
    }
    else
    {
      if (db->additional_txt_items_array)
        free(db->additional_txt_items_array);
      if (db->additional_txt_items_data)
        free(db->additional_txt_items_data);
    }
  }
  else
  {
    RdmnetDnsTxtRecordItem* new_arr = (RdmnetDnsTxtRecordItem*)realloc(
        db->additional_txt_items_array, sizeof(RdmnetDnsTxtRecordItem) * (db->num_additional_txt_items + 1));
    if (new_arr)
    {
      db->additional_txt_items_array = new_arr;
      DnsTxtRecordItemInternal* new_data_arr = (DnsTxtRecordItemInternal*)realloc(
          db->additional_txt_items_data, sizeof(DnsTxtRecordItemInternal) * (db->num_additional_txt_items + 1));
      if (new_data_arr)
      {
        db->additional_txt_items_data = new_data_arr;
        // Reset the references from additional_txt_items_array to additional_txt_items_data
        for (size_t i = 0; i < db->num_additional_txt_items; ++i)
        {
          db->additional_txt_items_array[i].key = db->additional_txt_items_data[i].key;
          db->additional_txt_items_array[i].value = db->additional_txt_items_data[i].value;
        }
        ++db->num_additional_txt_items;
        return true;
      }
      // db->additional_txt_items_array is not leaked if the second realloc fails, it's just bigger
      // than it needs to be until the next change is made to db.
    }
  }
  return false;
}
#endif

bool discovered_broker_add_txt_record_item(DiscoveredBroker* db,
                                           const char*       key,
                                           const uint8_t*    value,
                                           uint8_t           value_len)
{
  RdmnetDnsTxtRecordItem*   new_item = NULL;
  DnsTxtRecordItemInternal* new_item_data = NULL;

#if RDMNET_DYNAMIC_MEM
  if (expand_txt_record_arrays(db))
  {
    new_item = &db->additional_txt_items_array[db->num_additional_txt_items - 1];
    new_item_data = &db->additional_txt_items_data[db->num_additional_txt_items - 1];
  }
#else
  if (db->num_additional_txt_items >= MAX_TXT_RECORD_ITEMS_PER_BROKER)
    return false;
  new_item = &db->additional_txt_items_array[db->num_additional_txt_items];
  new_item_data = &db->additional_txt_items_data[db->num_additional_txt_items];
  ++db->num_additional_txt_items;
#endif

  rdmnet_safe_strncpy(new_item_data->key, key, DNS_TXT_RECORD_COMPONENT_MAX_LENGTH);
  memcpy(new_item_data->value, value, value_len);
  new_item_data->value_len = value_len;

  // Assign the references from the RdmnetDnsTxtRecordItem
  new_item->key = new_item_data->key;
  new_item->value = new_item_data->value;
  new_item->value_len = new_item_data->value_len;
  return true;
}

void discovered_broker_fill_disc_info(const DiscoveredBroker* db, RdmnetBrokerDiscInfo* broker_info)
{
  if (db && broker_info)
  {
    broker_info->cid = db->cid;
    broker_info->uid = db->uid;
    broker_info->e133_version = db->e133_version;
    broker_info->service_instance_name = db->service_instance_name;
    broker_info->port = db->port;
    broker_info->listen_addrs = db->listen_addr_array;
    broker_info->num_listen_addrs = db->num_listen_addrs;
    broker_info->scope = db->scope;
    broker_info->model = db->model;
    broker_info->manufacturer = db->manufacturer;
    broker_info->additional_txt_items = db->additional_txt_items_array;
    broker_info->num_additional_txt_items = db->num_additional_txt_items;
  }
}

DiscoveredBroker* discovered_broker_find_by_name(DiscoveredBroker* list_head, const char* full_name)
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
  if (db->additional_txt_items_data)
    free(db->additional_txt_items_data);
  if (db->additional_txt_items_array)
    free(db->additional_txt_items_array);
  if (db->listen_addr_array)
    free(db->listen_addr_array);
#endif
  discovered_broker_free_platform_resources(db);
  FREE_DISCOVERED_BROKER(db);
}
