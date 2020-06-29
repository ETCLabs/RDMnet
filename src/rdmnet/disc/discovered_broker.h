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

#ifndef RDMNET_DISC_DISCOVERED_BROKER_H_
#define RDMNET_DISC_DISCOVERED_BROKER_H_

#include "rdmnet/discovery.h"

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/uuid.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/core/util.h"
#include "rdmnet/defs.h"
#include "rdmnet/disc/dns_txt_record_item.h"
#include "rdmnet_disc_platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DiscoveredBroker DiscoveredBroker;
struct DiscoveredBroker
{
  /////////////////////////////////////////////////////////////////////////////
  // Broker discovery info
  EtcPalUuid cid;
  RdmUid     uid;
  int        e133_version;
  char       service_instance_name[E133_SERVICE_NAME_STRING_PADDED_LENGTH];
  char       full_service_name[RDMNET_DISC_SERVICE_NAME_MAX_LENGTH];
  uint16_t   port;
#if RDMNET_DYNAMIC_MEM
  EtcPalIpAddr* listen_addr_array;
#else
  EtcPalIpAddr             listen_addr_array[RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER];
#endif
  size_t num_listen_addrs;
  char   scope[E133_SCOPE_STRING_PADDED_LENGTH];
  char   model[E133_MODEL_STRING_PADDED_LENGTH];
  char   manufacturer[E133_MANUFACTURER_STRING_PADDED_LENGTH];

#if RDMNET_DYNAMIC_MEM
  RdmnetDnsTxtRecordItem*   additional_txt_items_array;
  DnsTxtRecordItemInternal* additional_txt_items_data;
#else
  RdmnetDnsTxtRecordItem   additional_txt_items_array[MAX_TXT_RECORD_ITEMS_PER_BROKER];
  DnsTxtRecordItemInternal additional_txt_items_data[MAX_TXT_RECORD_ITEMS_PER_BROKER];
#endif
  size_t num_additional_txt_items;

  /////////////////////////////////////////////////////////////////////////////

  rdmnet_scope_monitor_t             monitor_ref;
  RdmnetDiscoveredBrokerPlatformData platform_data;
  DiscoveredBroker*                  next;
};

typedef bool (*DiscoveredBrokerPredicateFunction)(DiscoveredBroker* db, const void* context);

// None of these functions are thread-safe and as such they should always be called within
// appropriate locks.

etcpal_error_t    discovered_broker_module_init(void);
DiscoveredBroker* discovered_broker_new(rdmnet_scope_monitor_t monitor_ref,
                                        const char*            service_name,
                                        const char*            full_service_name);
void              discovered_broker_insert(DiscoveredBroker** list_head_ptr, DiscoveredBroker* new_db);
bool              discovered_broker_add_listen_addr(DiscoveredBroker* db, const EtcPalIpAddr* addr);
bool              discovered_broker_add_txt_record_item(DiscoveredBroker* db,
                                                        const char*       key,
                                                        const uint8_t*    value,
                                                        uint8_t           value_len);
void              discovered_broker_fill_disc_info(const DiscoveredBroker* db, RdmnetBrokerDiscInfo* broker_info);
DiscoveredBroker* discovered_broker_find(DiscoveredBroker*                 list_head,
                                         DiscoveredBrokerPredicateFunction predicate,
                                         const void*                       context);
DiscoveredBroker* discovered_broker_find_by_name(DiscoveredBroker* list_head, const char* full_name);
void              discovered_broker_remove(DiscoveredBroker** list_head_ptr, const DiscoveredBroker* db);
void              discovered_broker_delete(DiscoveredBroker* db);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_DISC_DISCOVERED_BROKER_H_ */
