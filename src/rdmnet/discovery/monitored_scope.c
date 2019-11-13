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

#include "monitored_scope.h"

#include "rdmnet/private/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/****************************** Private macros *******************************/

#if RDMNET_DYNAMIC_MEM
#define ALLOC_SCOPE_MONITOR_REF() (RdmnetScopeMonitorRef*)malloc(sizeof(RdmnetScopeMonitorRef))
#define FREE_SCOPE_MONITOR_REF(ptr) free(ptr)
#else
#define ALLOC_SCOPE_MONITOR_REF() (RdmnetScopeMonitorRef*)etcpal_mempool_alloc(scope_monitor_refs)
#define FREE_SCOPE_MONITOR_REF(ptr) etcpal_mempool_free(scope_monitor_refs, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(scope_monitor_refs, RdmnetScopeMonitorRef, RDMNET_MAX_MONITORED_SCOPES);
#endif

static RdmnetScopeMonitorRef* scope_ref_list;

/*************************** Function definitions ****************************/

/* Allocate and initialize a new scope monitor ref. */
RdmnetScopeMonitorRef* scope_monitor_new(const RdmnetScopeMonitorConfig* config)
{
  RdmnetScopeMonitorRef* new_monitor = ALLOC_SCOPE_MONITOR_REF();
  if (new_monitor)
  {
    new_monitor->config = *config;
    new_monitor->broker_handle = NULL;
    new_monitor->broker_list = NULL;
    new_monitor->next = NULL;
  }
  return new_monitor;
}

/* Adds a new scope monitor ref to the global scope_ref_list. Assumes a lock is already taken. */
void scope_monitor_insert(RdmnetScopeMonitorRef* scope_ref)
{
  if (scope_ref)
  {
    scope_ref->next = NULL;

    if (!scope_ref_list)
    {
      // Make the new scope the head of the list.
      scope_ref_list = scope_ref;
    }
    else
    {
      // Insert the new scope at the end of the list.
      RdmnetScopeMonitorRef* ref = scope_ref_list;
      for (; ref->next; ref = ref->next)
        ;
      ref->next = scope_ref;
    }
  }
}

bool scope_monitor_ref_is_valid(const RdmnetScopeMonitorRef* ref)
{
  for (const RdmnetScopeMonitorRef* compare_ref = scope_ref_list; compare_ref; compare_ref = compare_ref->next)
  {
    if (ref == compare_ref)
      return true;
  }
  return false;
}

void scope_monitor_for_each(void (*for_each_func)(RdmnetScopeMonitorRef*))
{
  if (for_each_func)
  {
    for (RdmnetScopeMonitorRef* ref = scope_ref_list; ref; ref = ref->next)
      for_each_func(ref);
  }
}

/* Removes an entry from scope_ref_list. Assumes a lock is already taken. */
void scope_monitor_remove(const RdmnetScopeMonitorRef* ref)
{
  if (!scope_ref_list)
    return;

  if (ref == scope_ref_list)
  {
    // Remove the element at the head of the list
    scope_ref_list = ref->next;
  }
  else
  {
    for (RdmnetScopeMonitorRef* prev_ref = scope_ref_list; prev_ref->next; prev_ref = prev_ref->next)
    {
      if (prev_ref->next == ref)
      {
        prev_ref->next = ref->next;
        break;
      }
    }
  }
}

/* Deallocate a scope_monitor_ref. Also deallocates all DiscoveredBrokers attached to this
 * scope_monitor_ref. */
void scope_monitor_delete(RdmnetScopeMonitorRef* ref)
{
  DiscoveredBroker* db = ref->broker_list;
  DiscoveredBroker* next_db;
  while (db)
  {
    next_db = db->next;
    discovered_broker_delete(db);
    db = next_db;
  }
  FREE_SCOPE_MONITOR_REF(ref);
}

void scope_monitor_delete_all()
{
  if (!scope_ref_list)
    return;

  RdmnetScopeMonitorRef* ref = scope_ref_list;
  while (ref)
  {
    RdmnetScopeMonitorRef* next_ref = ref->next;
    scope_monitor_delete(ref);
    ref = next_ref;
  }
  scope_ref_list = NULL;
}
