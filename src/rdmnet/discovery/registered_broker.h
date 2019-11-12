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

#ifndef REGISTERED_BROKER_H_
#define REGISTERED_BROKER_H_

#include "etcpal/timer.h"
#include "rdmnet/core/discovery.h"
#include "disc_platform_defs.h"

// How long we monitor the registered scope before doing the actual DNS registration of a broker.
#define BROKER_REG_QUERY_TIMEOUT 3000

typedef enum
{
  kBrokerStateNotRegistered,
  kBrokerStateQuerying,
  kBrokerStateRegisterStarted,
  kBrokerStateRegistered
} broker_state_t;

typedef struct RdmnetBrokerRegisterRef RdmnetBrokerRegisterRef;
struct RdmnetBrokerRegisterRef
{
  RdmnetBrokerRegisterConfig config;
  rdmnet_scope_monitor_t scope_monitor_handle;
  broker_state_t state;
  char full_service_name[RDMNET_DISC_SERVICE_NAME_MAX_LENGTH];

  EtcPalTimer query_timer;
  bool query_timeout_expired;

  RdmnetBrokerRegisterPlatformData platform_data;

  RdmnetBrokerRegisterRef* next;
};

#endif /* REGISTERED_BROKER_H_ */
