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
#define FREE_DISCOVERED_BROKER(ptr) \
  if (RDMNET_ASSERT_VERIFY(ptr))    \
  {                                 \
    free(ptr);                      \
  }
#elif RDMNET_MAX_DISCOVERED_BROKERS_PER_SCOPE
#define ALLOC_DISCOVERED_BROKER() (DiscoveredBroker*)etcpal_mempool_alloc(discovered_brokers)
#define FREE_DISCOVERED_BROKER(ptr)               \
  if (RDMNET_ASSERT_VERIFY(ptr))                  \
  {                                               \
    etcpal_mempool_free(discovered_brokers, ptr); \
  }
#else
#define ALLOC_DISCOVERED_BROKER() NULL
#define FREE_DISCOVERED_BROKER(ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM && RDMNET_MAX_DISCOVERED_BROKERS_PER_SCOPE
ETCPAL_MEMPOOL_DEFINE(discovered_brokers,
                      DiscoveredBroker,
                      (RDMNET_MAX_DISCOVERED_BROKERS_PER_SCOPE * RDMNET_MAX_MONITORED_SCOPES));
#endif

etcpal_error_t discovered_broker_module_init(void)
{
#if RDMNET_DYNAMIC_MEM
  return kEtcPalErrOk;
#else
  return etcpal_mempool_init(discovered_brokers);
#endif
}

/*********************** Private function prototypes *************************/

static bool find_txt_item(DiscoveredBroker*          db,
                          const uint8_t*             key,
                          uint8_t                    key_len,
                          RdmnetDnsTxtRecordItem**   item_ptr,
                          DnsTxtRecordItemInternal** item_data_ptr);
static bool get_next_unused_txt_item(DiscoveredBroker*          db,
                                     RdmnetDnsTxtRecordItem**   item_ptr,
                                     DnsTxtRecordItemInternal** item_data_ptr);
#if RDMNET_DYNAMIC_MEM
static bool expand_txt_record_arrays(DiscoveredBroker* db);
#endif

/*************************** Function definitions ****************************/

DiscoveredBroker* discovered_broker_new(rdmnet_scope_monitor_t monitor_ref,
                                        const char*            service_name,
                                        const char*            full_service_name)
{
  if (!RDMNET_ASSERT_VERIFY(monitor_ref) || !RDMNET_ASSERT_VERIFY(service_name) ||
      !RDMNET_ASSERT_VERIFY(full_service_name))
  {
    return NULL;
  }

  DiscoveredBroker* new_db = ALLOC_DISCOVERED_BROKER();
  if (new_db)
  {
    memset(new_db, 0, sizeof(DiscoveredBroker));
    new_db->monitor_ref = monitor_ref;
    rdmnet_safe_strncpy(new_db->service_instance_name, service_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(new_db->full_service_name, full_service_name, RDMNET_DISC_SERVICE_NAME_MAX_LENGTH);
#if RDMNET_DYNAMIC_MEM
    new_db->listen_addr_array = NULL;
    new_db->listen_addr_netint_array = NULL;
#endif
    new_db->num_listen_addrs = 0;
    memset(&new_db->platform_data, 0, sizeof(RdmnetDiscoveredBrokerPlatformData));
    new_db->next = NULL;
  }
  return new_db;
}

void discovered_broker_insert(DiscoveredBroker** list_head_ptr, DiscoveredBroker* new_db)
{
  if (!RDMNET_ASSERT_VERIFY(list_head_ptr) || !RDMNET_ASSERT_VERIFY(new_db))
    return;

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

bool discovered_broker_add_listen_addr(DiscoveredBroker* db, const EtcPalIpAddr* addr, unsigned int netint)
{
  if (!RDMNET_ASSERT_VERIFY(db) || !RDMNET_ASSERT_VERIFY(addr))
    return false;

#if RDMNET_DYNAMIC_MEM
  if (!db->listen_addr_array)
  {
    if (!RDMNET_ASSERT_VERIFY(!db->listen_addr_netint_array))
      return false;

    db->listen_addr_array = (EtcPalIpAddr*)malloc(sizeof(EtcPalIpAddr));
    db->listen_addr_netint_array = (unsigned int*)malloc(sizeof(unsigned int));
    if (db->listen_addr_array && db->listen_addr_netint_array)
    {
      db->listen_addr_array[0] = *addr;
      db->listen_addr_netint_array[0] = netint;
      db->num_listen_addrs = 1;
      return true;
    }
  }
  else
  {
    if (!RDMNET_ASSERT_VERIFY(db->listen_addr_netint_array))
      return false;

    EtcPalIpAddr* new_ip_arr =
        (EtcPalIpAddr*)realloc(db->listen_addr_array, sizeof(EtcPalIpAddr) * (db->num_listen_addrs + 1));
    unsigned int* net_netint_arr =
        (unsigned int*)realloc(db->listen_addr_netint_array, sizeof(unsigned int) * (db->num_listen_addrs + 1));

    if (new_ip_arr)
    {
      db->listen_addr_array = new_ip_arr;
      db->listen_addr_array[db->num_listen_addrs] = *addr;
    }

    if (net_netint_arr)
    {
      db->listen_addr_netint_array = net_netint_arr;
      db->listen_addr_netint_array[db->num_listen_addrs] = netint;
    }

    if (new_ip_arr && net_netint_arr)
    {
      ++db->num_listen_addrs;
      return true;
    }
  }
  return false;
#else
  if (db->num_listen_addrs < RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER)
  {
    db->listen_addr_array[db->num_listen_addrs] = *addr;
    db->listen_addr_netint_array[db->num_listen_addrs] = netint;
    ++db->num_listen_addrs;
    return true;
  }
  return false;
#endif
}

bool discovered_broker_add_txt_record_item(DiscoveredBroker* db,
                                           const char*       key,
                                           const uint8_t*    value,
                                           uint8_t           value_len)
{
  if (!RDMNET_ASSERT_VERIFY(db) || !RDMNET_ASSERT_VERIFY(key) || !RDMNET_ASSERT_VERIFY(value))
    return false;

  RdmnetDnsTxtRecordItem*   item = NULL;
  DnsTxtRecordItemInternal* item_data = NULL;

  // If the item already exists, just update its value
  if (find_txt_item(db, (const uint8_t*)key, (uint8_t)strlen(key), &item, &item_data))
  {
    if (!RDMNET_ASSERT_VERIFY(item_data))
      return false;

    if (value_len == item_data->value_len && memcmp(value, item_data->value, value_len) == 0)
    {
      // Same value
      return false;
    }
    memcpy(item_data->value, value, value_len);
    item_data->value_len = value_len;
    return true;
  }
  else if (get_next_unused_txt_item(db, &item, &item_data))
  {
    if (!RDMNET_ASSERT_VERIFY(item) || !RDMNET_ASSERT_VERIFY(item_data))
      return false;

    rdmnet_safe_strncpy(item_data->key, key, DNS_TXT_RECORD_COMPONENT_MAX_LENGTH);
    memcpy(item_data->value, value, value_len);
    item_data->value_len = value_len;

    // Assign the references from the RdmnetDnsTxtRecordItem
    item->key = item_data->key;
    item->value = item_data->value;
    item->value_len = item_data->value_len;
    return true;
  }
  return false;
}

bool discovered_broker_add_binary_txt_record_item(DiscoveredBroker* db,
                                                  const uint8_t*    key,
                                                  uint8_t           key_len,
                                                  const uint8_t*    value,
                                                  uint8_t           value_len)
{
  if (!RDMNET_ASSERT_VERIFY(db) || !RDMNET_ASSERT_VERIFY(key))
    return false;

  // Key must be 100% PRINTUSASCII
  for (const uint8_t* key_char = key; key_char < key + key_len; ++key_char)
  {
    if (*key_char < 0x20 || *key_char > 0x7e)
      return false;
  }

  RdmnetDnsTxtRecordItem*   item = NULL;
  DnsTxtRecordItemInternal* item_data = NULL;

  // If the item already exists, just update its value
  if (find_txt_item(db, key, key_len, &item, &item_data))
  {
    if (!RDMNET_ASSERT_VERIFY(item_data))
      return false;

    if ((value_len == item_data->value_len) &&
        ((value_len == 0) || (RDMNET_ASSERT_VERIFY(value) && (memcmp(value, item_data->value, value_len) == 0))))
    {
      // Same value
      return false;
    }

    if (value_len > 0)
      memcpy(item_data->value, value, value_len);

    item_data->value_len = value_len;

    return true;
  }
  else if (get_next_unused_txt_item(db, &item, &item_data))
  {
    if (!RDMNET_ASSERT_VERIFY(item) || !RDMNET_ASSERT_VERIFY(item_data))
      return false;

    memcpy(item_data->key, key, key_len);
    item_data->key[key_len] = '\0';

    if (value_len > 0)
      memcpy(item_data->value, value, value_len);

    item_data->value_len = value_len;

    // Assign the references from the RdmnetDnsTxtRecordItem
    item->key = item_data->key;
    item->value = item_data->value;
    item->value_len = item_data->value_len;
    return true;
  }
  return false;
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
    broker_info->listen_addr_netints = db->listen_addr_netint_array;
    broker_info->num_listen_addrs = db->num_listen_addrs;
    broker_info->scope = db->scope;
    broker_info->model = db->model;
    broker_info->manufacturer = db->manufacturer;
    broker_info->additional_txt_items = db->additional_txt_items_array;
    broker_info->num_additional_txt_items = db->num_additional_txt_items;
  }
}

DiscoveredBroker* discovered_broker_find(DiscoveredBroker*                 list_head,
                                         DiscoveredBrokerPredicateFunction predicate,
                                         const void*                       context)
{
  if (!RDMNET_ASSERT_VERIFY(predicate))
    return NULL;

  for (DiscoveredBroker* current = list_head; current; current = current->next)
  {
    if (predicate(current, context))
      return current;
  }
  return NULL;
}

DiscoveredBroker* discovered_broker_find_by_name(DiscoveredBroker* list_head, const char* full_name)
{
  if (!RDMNET_ASSERT_VERIFY(full_name))
    return NULL;

  for (DiscoveredBroker* current = list_head; current; current = current->next)
  {
    if (strcmp(current->full_service_name, full_name) == 0)
      return current;
  }
  return NULL;
}

void discovered_broker_remove(DiscoveredBroker** list_head_ptr, const DiscoveredBroker* db)
{
  if (!RDMNET_ASSERT_VERIFY(list_head_ptr) || !RDMNET_ASSERT_VERIFY(db))
    return;

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
  if (!RDMNET_ASSERT_VERIFY(db))
    return;

#if RDMNET_DYNAMIC_MEM
  if (db->additional_txt_items_data)
    free(db->additional_txt_items_data);
  if (db->additional_txt_items_array)
    free(db->additional_txt_items_array);
  if (db->listen_addr_array)
    free(db->listen_addr_array);
  if (db->listen_addr_netint_array)
    free(db->listen_addr_netint_array);
#endif
  discovered_broker_free_platform_resources(db);
  FREE_DISCOVERED_BROKER(db);
}

bool find_txt_item(DiscoveredBroker*          db,
                   const uint8_t*             key,
                   uint8_t                    key_len,
                   RdmnetDnsTxtRecordItem**   item_ptr,
                   DnsTxtRecordItemInternal** item_data_ptr)
{
  if (!RDMNET_ASSERT_VERIFY(db) || !RDMNET_ASSERT_VERIFY(key) || !RDMNET_ASSERT_VERIFY(item_ptr) ||
      !RDMNET_ASSERT_VERIFY(item_data_ptr))
  {
    return false;
  }

  for (size_t i = 0; i < db->num_additional_txt_items; ++i)
  {
    if (!RDMNET_ASSERT_VERIFY(db->additional_txt_items_data))
      return false;

    DnsTxtRecordItemInternal* item_data = &db->additional_txt_items_data[i];
    if (key_len == strlen(item_data->key) && memcmp(item_data->key, key, key_len) == 0)
    {
      *item_data_ptr = item_data;
      *item_ptr = &db->additional_txt_items_array[i];
      return true;
    }
  }
  return false;
}

bool get_next_unused_txt_item(DiscoveredBroker*          db,
                              RdmnetDnsTxtRecordItem**   item_ptr,
                              DnsTxtRecordItemInternal** item_data_ptr)
{
  if (!RDMNET_ASSERT_VERIFY(db) || !RDMNET_ASSERT_VERIFY(item_ptr) || !RDMNET_ASSERT_VERIFY(item_data_ptr))
    return false;

#if RDMNET_DYNAMIC_MEM
  if (expand_txt_record_arrays(db))
  {
    if (!RDMNET_ASSERT_VERIFY(db->additional_txt_items_array))
      return false;

    *item_ptr = &db->additional_txt_items_array[db->num_additional_txt_items - 1];
    *item_data_ptr = &db->additional_txt_items_data[db->num_additional_txt_items - 1];
    return true;
  }
  else
  {
    return false;
  }
#else
  if (db->num_additional_txt_items >= RDMNET_MAX_ADDITIONAL_TXT_ITEMS_PER_DISCOVERED_BROKER)
    return false;
  *item_ptr = &db->additional_txt_items_array[db->num_additional_txt_items];
  *item_data_ptr = &db->additional_txt_items_data[db->num_additional_txt_items];
  ++db->num_additional_txt_items;
  return true;
#endif
}

#if RDMNET_DYNAMIC_MEM
bool expand_txt_record_arrays(DiscoveredBroker* db)
{
  if (!RDMNET_ASSERT_VERIFY(db))
    return false;

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
