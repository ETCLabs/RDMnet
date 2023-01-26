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

#include "rdmnet/disc/monitored_scope.h"

#include "rdmnet/core/opts.h"
#include "rdmnet/core/util.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/****************************** Private macros *******************************/

#if RDMNET_DYNAMIC_MEM
#define ALLOC_SCOPE_MONITOR_REF() (RdmnetScopeMonitorRef*)malloc(sizeof(RdmnetScopeMonitorRef))
#define FREE_SCOPE_MONITOR_REF(ptr) \
  if (RDMNET_ASSERT_VERIFY(ptr))    \
  {                                 \
    free(ptr);                      \
  }
#elif RDMNET_MAX_MONITORED_SCOPES
#define ALLOC_SCOPE_MONITOR_REF() (RdmnetScopeMonitorRef*)etcpal_mempool_alloc(scope_monitor_refs)
#define FREE_SCOPE_MONITOR_REF(ptr)               \
  if (RDMNET_ASSERT_VERIFY(ptr))                  \
  {                                               \
    etcpal_mempool_free(scope_monitor_refs, ptr); \
  }
#else
#define ALLOC_SCOPE_MONITOR_REF() NULL
#define FREE_SCOPE_MONITOR_REF(ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM && RDMNET_MAX_MONITORED_SCOPES
ETCPAL_MEMPOOL_DEFINE(scope_monitor_refs, RdmnetScopeMonitorRef, RDMNET_MAX_MONITORED_SCOPES);
#endif

#if RDMNET_DYNAMIC_MEM || RDMNET_MAX_MONITORED_SCOPES
RC_DECLARE_REF_LIST(scope_monitor_refs, RDMNET_MAX_MONITORED_SCOPES);
#endif

/*************************** Function definitions ****************************/

etcpal_error_t monitored_scope_module_init(void)
{
#if !RDMNET_DYNAMIC_MEM && RDMNET_MAX_MONITORED_SCOPES
  etcpal_error_t res = etcpal_mempool_init(scope_monitor_refs);
  if (res != kEtcPalErrOk)
    return res;
#endif

  return (rc_ref_list_init(&scope_monitor_refs) ? kEtcPalErrOk : kEtcPalErrNoMem);
}

void monitored_scope_module_deinit(void)
{
  scope_monitor_delete_all();
  rc_ref_list_cleanup(&scope_monitor_refs);
}

/* Allocate and initialize a new scope monitor ref. */
RdmnetScopeMonitorRef* scope_monitor_new(const RdmnetScopeMonitorConfig* config)
{
  if (!RDMNET_ASSERT_VERIFY(config))
    return NULL;

  RdmnetScopeMonitorRef* new_monitor = ALLOC_SCOPE_MONITOR_REF();
  if (new_monitor)
  {
    rdmnet_safe_strncpy(new_monitor->scope, config->scope, E133_SCOPE_STRING_PADDED_LENGTH);
    if (config->domain)
      rdmnet_safe_strncpy(new_monitor->domain, config->domain, E133_DOMAIN_STRING_PADDED_LENGTH);
    else
      new_monitor->domain[0] = '\0';
    new_monitor->callbacks = config->callbacks;

    new_monitor->broker_handle = NULL;
    new_monitor->broker_list = NULL;
    memset(&new_monitor->platform_data, 0, sizeof(RdmnetScopeMonitorPlatformData));
  }
  return new_monitor;
}

/* Adds a new scope monitor ref to the global scope_ref_list. Assumes a lock is already taken. */
void scope_monitor_insert(RdmnetScopeMonitorRef* scope_ref)
{
  if (!RDMNET_ASSERT_VERIFY(scope_ref))
    return;

  rc_ref_list_add_ref(&scope_monitor_refs, scope_ref);
}

bool scope_monitor_ref_is_valid(const RdmnetScopeMonitorRef* ref)
{
  if (!RDMNET_ASSERT_VERIFY(ref))
    return false;

  return (rc_ref_list_find_ref_index(&scope_monitor_refs, ref) != -1);
}

void scope_monitor_for_each(ScopeMonitorRefFunction func)
{
  if (!RDMNET_ASSERT_VERIFY(func))
    return;

  for (void** ref_ptr = scope_monitor_refs.refs; ref_ptr < scope_monitor_refs.refs + scope_monitor_refs.num_refs;
       ++ref_ptr)
  {
    func(*ref_ptr);
  }
}

RdmnetScopeMonitorRef* scope_monitor_find(ScopeMonitorRefPredicateFunction predicate, const void* context)
{
  if (!RDMNET_ASSERT_VERIFY(predicate))
    return NULL;

  for (void** ref_ptr = scope_monitor_refs.refs; ref_ptr < scope_monitor_refs.refs + scope_monitor_refs.num_refs;
       ++ref_ptr)
  {
    if (!RDMNET_ASSERT_VERIFY(ref_ptr))
      return NULL;

    if (predicate(*ref_ptr, context))
      return *ref_ptr;
  }
  return NULL;
}

bool scope_monitor_and_discovered_broker_find(ScopeMonitorAndDBPredicateFunction predicate,
                                              const void*                        context,
                                              RdmnetScopeMonitorRef**            found_ref,
                                              DiscoveredBroker**                 found_db)
{
  if (!RDMNET_ASSERT_VERIFY(predicate) || !RDMNET_ASSERT_VERIFY(found_ref) || !RDMNET_ASSERT_VERIFY(found_db))
    return false;

  for (void** ref_ptr = scope_monitor_refs.refs; ref_ptr < scope_monitor_refs.refs + scope_monitor_refs.num_refs;
       ++ref_ptr)
  {
    if (!RDMNET_ASSERT_VERIFY(ref_ptr))
      return false;

    RdmnetScopeMonitorRef* ref = *(RdmnetScopeMonitorRef**)ref_ptr;
    if (!RDMNET_ASSERT_VERIFY(ref))
      return false;

    for (DiscoveredBroker* db = ref->broker_list; db; db = db->next)
    {
      if (predicate(ref, db, context))
      {
        *found_ref = ref;
        *found_db = db;
        return true;
      }
    }
  }
  return false;
}

/* Removes an entry from scope_ref_list. Assumes a lock is already taken. */
void scope_monitor_remove(const RdmnetScopeMonitorRef* ref)
{
  if (!RDMNET_ASSERT_VERIFY(ref))
    return;

  rc_ref_list_remove_ref(&scope_monitor_refs, ref);
}

/*
 * Deallocate a scope_monitor_ref. Also deallocates all DiscoveredBrokers attached to this
 * scope_monitor_ref.
 */
void scope_monitor_delete(RdmnetScopeMonitorRef* ref)
{
  if (!RDMNET_ASSERT_VERIFY(ref))
    return;

  DiscoveredBroker* db = ref->broker_list;
  DiscoveredBroker* next_db = NULL;
  while (db)
  {
    next_db = db->next;
    discovered_broker_delete(db);
    db = next_db;
  }
  FREE_SCOPE_MONITOR_REF(ref);
}

void scope_monitor_delete_ref_cb(void* ref, const void* context)
{
  ETCPAL_UNUSED_ARG(context);

  if (!RDMNET_ASSERT_VERIFY(ref))
    return;

  scope_monitor_delete(ref);
}

void scope_monitor_delete_all(void)
{
  rc_ref_list_remove_all(&scope_monitor_refs, scope_monitor_delete_ref_cb, NULL);
}
