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

/*
 * An assumption is made that this module will only be used on platforms where dynamic memory
 * allocation is available. Thus the functionality is disabled if RDMNET_DYNAMIC_MEM is defined to
 * 0.
 */

#include "rdmnet/disc/registered_broker.h"

#include <string.h>
#include "etcpal/common.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/core/util.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif

/**************************** Private variables ******************************/

RC_DECLARE_REF_LIST(registered_brokers, 1);

/*********************** Private function prototypes *************************/

static void registered_broker_delete_ref_cb(void* rb, const void* context);

/*************************** Function definitions ****************************/

etcpal_error_t registered_broker_module_init(void)
{
  return (rc_ref_list_init(&registered_brokers) ? kEtcPalErrOk : kEtcPalErrNoMem);
}

void registered_broker_module_deinit(void)
{
  registered_broker_delete_all();
  rc_ref_list_cleanup(&registered_brokers);
}

RdmnetBrokerRegisterRef* registered_broker_new(const RdmnetBrokerRegisterConfig* config)
{
  if (!RDMNET_ASSERT_VERIFY(config))
    return NULL;

#if RDMNET_DYNAMIC_MEM
  RdmnetBrokerRegisterRef* new_rb = (RdmnetBrokerRegisterRef*)calloc(1, sizeof(RdmnetBrokerRegisterRef));
  if (new_rb)
  {
    if (config->num_netints != 0)
    {
      new_rb->netints = calloc(config->num_netints, sizeof(unsigned int));
      if (!new_rb->netints)
      {
        free(new_rb);
        return NULL;
      }
    }
    else
    {
      new_rb->netints = NULL;
    }

    if (config->num_additional_txt_items != 0)
    {
      new_rb->additional_txt_items = calloc(config->num_additional_txt_items, sizeof(DnsTxtRecordItemInternal));
      if (!new_rb->additional_txt_items)
      {
        if (new_rb->netints)
          free(new_rb->netints);
        free(new_rb);
        return NULL;
      }
    }
    else
    {
      new_rb->additional_txt_items = NULL;
    }

    new_rb->cid = config->cid;
    new_rb->uid = config->uid;
    rdmnet_safe_strncpy(new_rb->service_instance_name, config->service_instance_name,
                        E133_SERVICE_NAME_STRING_PADDED_LENGTH);
    new_rb->port = config->port;
    if (config->num_netints != 0)
      memcpy(new_rb->netints, config->netints, config->num_netints * sizeof(unsigned int));
    new_rb->num_netints = config->num_netints;
    rdmnet_safe_strncpy(new_rb->scope, config->scope, E133_SCOPE_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(new_rb->model, config->model, E133_MODEL_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(new_rb->manufacturer, config->manufacturer, E133_MANUFACTURER_STRING_PADDED_LENGTH);
    new_rb->num_additional_txt_items = 0;
    if (config->num_additional_txt_items != 0)
    {
      if (!RDMNET_ASSERT_VERIFY(config->additional_txt_items))
        return NULL;

      for (size_t i = 0; i < config->num_additional_txt_items; ++i)
      {
        const RdmnetDnsTxtRecordItem* txt_item = &config->additional_txt_items[i];
        if (txt_item->key)
        {
          DnsTxtRecordItemInternal* txt_item_dest = &new_rb->additional_txt_items[i];
          rdmnet_safe_strncpy(txt_item_dest->key, txt_item->key, DNS_TXT_RECORD_COMPONENT_MAX_LENGTH);
          // Values are optional
          if (txt_item->value && txt_item->value_len)
            memcpy(txt_item_dest->value, txt_item->value, txt_item->value_len);
          txt_item_dest->value_len = txt_item->value_len;
          ++new_rb->num_additional_txt_items;
        }
      }
    }
    new_rb->callbacks = config->callbacks;

    new_rb->scope_monitor_handle = NULL;
    new_rb->state = kBrokerStateNotRegistered;
    new_rb->full_service_name[0] = '\0';
    memset(&new_rb->platform_data, 0, sizeof(RdmnetBrokerRegisterPlatformData));
  }
  return new_rb;
#else
  return NULL;
#endif
}

void registered_broker_insert(RdmnetBrokerRegisterRef* ref)
{
  if (!RDMNET_ASSERT_VERIFY(ref))
    return;

  rc_ref_list_add_ref(&registered_brokers, ref);
}

bool broker_register_ref_is_valid(const RdmnetBrokerRegisterRef* ref)
{
  if (!RDMNET_ASSERT_VERIFY(ref))
    return false;

  return (rc_ref_list_find_ref_index(&registered_brokers, ref) != -1);
}

void registered_broker_for_each(BrokerRefFunction func)
{
  if (!RDMNET_ASSERT_VERIFY(func))
    return;

  for (void** ref_ptr = registered_brokers.refs; ref_ptr < registered_brokers.refs + registered_brokers.num_refs;
       ++ref_ptr)
  {
    if (!RDMNET_ASSERT_VERIFY(ref_ptr))
      return;

    func(*ref_ptr);
  }
}

/* Removes an entry from broker_ref_list. Assumes a lock is already taken. */
void registered_broker_remove(const RdmnetBrokerRegisterRef* ref)
{
  if (!RDMNET_ASSERT_VERIFY(ref))
    return;

  rc_ref_list_remove_ref(&registered_brokers, ref);
}

void registered_broker_delete(RdmnetBrokerRegisterRef* rb)
{
#if RDMNET_DYNAMIC_MEM
  if (!RDMNET_ASSERT_VERIFY(rb))
    return;

  if (rb->netints)
    free(rb->netints);
  if (rb->additional_txt_items)
    free(rb->additional_txt_items);
  free(rb);
#else
  ETCPAL_UNUSED_ARG(rb);
#endif
}

void registered_broker_delete_ref_cb(void* rb, const void* context)
{
  ETCPAL_UNUSED_ARG(context);

  if (!RDMNET_ASSERT_VERIFY(rb))
    return;

  registered_broker_delete(rb);
}

void registered_broker_delete_all(void)
{
  rc_ref_list_remove_all(&registered_brokers, registered_broker_delete_ref_cb, NULL);
}
