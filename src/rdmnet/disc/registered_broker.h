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
 * Manage references to locally-registered brokers.
 *
 * An assumption is made that this module will only be used on platforms where dynamic memory
 * allocation is available.
 */

#ifndef RDMNET_DISC_REGISTERED_BROKER_H_
#define RDMNET_DISC_REGISTERED_BROKER_H_

#include "etcpal/timer.h"
#include "rdmnet/discovery.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/disc/dns_txt_record_item.h"
#include "rdmnet_disc_platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  kBrokerStateNotRegistered,
  kBrokerStateQuerying,
  kBrokerStateRegistered
} broker_state_t;

typedef struct RdmnetBrokerRegisterRef RdmnetBrokerRegisterRef;
struct RdmnetBrokerRegisterRef
{
  /////////////////////////////////////////////////////////////////////////////
  // The broker's registration information

  EtcPalUuid                cid;
  RdmUid                    uid;
  char                      service_instance_name[E133_SERVICE_NAME_STRING_PADDED_LENGTH];
  uint16_t                  port;
  unsigned int*             netints;
  size_t                    num_netints;
  char                      scope[E133_SCOPE_STRING_PADDED_LENGTH];
  char                      model[E133_MODEL_STRING_PADDED_LENGTH];
  char                      manufacturer[E133_MANUFACTURER_STRING_PADDED_LENGTH];
  DnsTxtRecordItemInternal* additional_txt_items;
  size_t                    num_additional_txt_items;

  RdmnetDiscBrokerCallbacks callbacks;

  /////////////////////////////////////////////////////////////////////////////

  rdmnet_scope_monitor_t scope_monitor_handle;
  broker_state_t         state;
  char                   full_service_name[RDMNET_DISC_SERVICE_NAME_MAX_LENGTH];

  EtcPalTimer query_timer;

  RdmnetBrokerRegisterPlatformData platform_data;
};

typedef void (*BrokerRefFunction)(RdmnetBrokerRegisterRef* ref);

etcpal_error_t           registered_broker_module_init(void);
void                     registered_broker_module_deinit(void);
RdmnetBrokerRegisterRef* registered_broker_new(const RdmnetBrokerRegisterConfig* config);
void                     registered_broker_insert(RdmnetBrokerRegisterRef* ref);
bool                     broker_register_ref_is_valid(const RdmnetBrokerRegisterRef* ref);
void                     registered_broker_for_each(BrokerRefFunction func);
void                     registered_broker_remove(const RdmnetBrokerRegisterRef* ref);
void                     registered_broker_delete(RdmnetBrokerRegisterRef* rb);
void                     registered_broker_delete_all(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_DISC_REGISTERED_BROKER_H_ */
