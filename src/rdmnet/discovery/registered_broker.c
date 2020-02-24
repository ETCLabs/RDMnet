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

/* An assumption is made that this module will only be used on platforms where dynamic memory
 * allocation is available. Thus the functionality is disabled if RDMNET_DYNAMIC_MEM is defined to
 * 0.
 */

#include "registered_broker.h"

#include <string.h>
#include "rdmnet/private/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif

/****************************** Private macros *******************************/

#if RDMNET_DYNAMIC_MEM
#define ALLOC_REGISTERED_BROKER() (RdmnetBrokerRegisterRef*)malloc(sizeof(RdmnetBrokerRegisterRef))
#define FREE_REGISTERED_BROKER(ptr) free(ptr)
#else
#define ALLOC_REGISTERED_BROKER() NULL
#define FREE_REGISTERED_BROKER(ptr)
#endif

/**************************** Private variables ******************************/

static RdmnetBrokerRegisterRef* broker_ref_list;

/*************************** Function definitions ****************************/

RdmnetBrokerRegisterRef* registered_broker_new(const RdmnetBrokerRegisterConfig* config)
{
  RdmnetBrokerRegisterRef* new_rb = ALLOC_REGISTERED_BROKER();
  if (new_rb)
  {
    new_rb->config = *config;
    new_rb->scope_monitor_handle = NULL;
    new_rb->state = kBrokerStateNotRegistered;
    new_rb->full_service_name[0] = '\0';
    new_rb->query_timeout_expired = false;
    memset(&new_rb->platform_data, 0, sizeof(RdmnetBrokerRegisterPlatformData));
    new_rb->next = NULL;
  }
  return new_rb;
}

void registered_broker_insert(RdmnetBrokerRegisterRef* ref)
{
  if (ref)
  {
    ref->next = NULL;

    if (!broker_ref_list)
    {
      // Make the new scope the head of the list.
      broker_ref_list = ref;
    }
    else
    {
      // Insert the new registered broker at the end of the list.
      RdmnetBrokerRegisterRef* last_ref = broker_ref_list;
      for (; last_ref->next; last_ref = last_ref->next)
        ;
      last_ref->next = ref;
    }
  }
}

bool broker_register_ref_is_valid(const RdmnetBrokerRegisterRef* ref)
{
  for (const RdmnetBrokerRegisterRef* compare_ref = broker_ref_list; compare_ref; compare_ref = compare_ref->next)
  {
    if (ref == compare_ref)
      return true;
  }
  return false;
}

void registered_broker_for_each(void (*for_each_func)(RdmnetBrokerRegisterRef* ref))
{
  if (for_each_func)
  {
    for (RdmnetBrokerRegisterRef* ref = broker_ref_list; ref; ref = ref->next)
      for_each_func(ref);
  }
}

/* Removes an entry from broker_ref_list. Assumes a lock is already taken. */
void registered_broker_remove(const RdmnetBrokerRegisterRef* ref)
{
  if (!broker_ref_list)
    return;

  if (ref == broker_ref_list)
  {
    // Remove the element at the head of the list
    broker_ref_list = ref->next;
  }
  else
  {
    for (RdmnetBrokerRegisterRef* prev_ref = broker_ref_list; prev_ref->next; prev_ref = prev_ref->next)
    {
      if (prev_ref->next == ref)
      {
        prev_ref->next = ref->next;
        break;
      }
    }
  }
}

void registered_broker_delete(RdmnetBrokerRegisterRef* rb)
{
  FREE_REGISTERED_BROKER(rb);
}

void registered_broker_delete_all(void)
{
  if (!broker_ref_list)
    return;

  RdmnetBrokerRegisterRef* ref = broker_ref_list;
  while (ref)
  {
    RdmnetBrokerRegisterRef* next_ref = ref->next;
    registered_broker_delete(ref);
    ref = next_ref;
  }
  broker_ref_list = NULL;
}
