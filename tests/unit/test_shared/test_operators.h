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

// test_operators.h
// Provide comparison operators not defined elsewhere for RDMnet public API types, for the purpose
// of testing.

#ifndef TEST_OPERATORS_H_
#define TEST_OPERATORS_H_

#include <cstring>
#include "rdmnet/discovery.h"

inline bool operator==(const RdmnetBrokerDiscInfo& a, const RdmnetBrokerDiscInfo& b)
{
  if ((a.cid == b.cid) && (std::strncmp(a.service_name, b.service_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH) == 0) &&
      (a.port == b.port) && (a.num_listen_addrs == b.num_listen_addrs) &&
      (std::strncmp(a.scope, b.scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0) &&
      (std::strncmp(a.model, b.model, E133_MODEL_STRING_PADDED_LENGTH) == 0) &&
      (std::strncmp(a.manufacturer, b.manufacturer, E133_MANUFACTURER_STRING_PADDED_LENGTH) == 0))
  {
    if (a.num_listen_addrs > 0 && (!a.listen_addrs || !b.listen_addrs))
      return false;

    for (size_t i = 0; i < a.num_listen_addrs; ++i)
    {
      if (a.listen_addrs[i] != b.listen_addrs[i])
      {
        return false;
      }
    }
    return true;
  }
  return false;
}

inline bool operator==(const RdmnetBrokerRegisterConfig& a, const RdmnetBrokerRegisterConfig& b)
{
  return ((a.my_info == b.my_info) && (a.callbacks.broker_registered == b.callbacks.broker_registered) &&
          (a.callbacks.broker_register_error == b.callbacks.broker_register_error) &&
          (a.callbacks.broker_found == b.callbacks.broker_found) &&
          (a.callbacks.broker_lost == b.callbacks.broker_lost) &&
          (a.callbacks.scope_monitor_error == b.callbacks.scope_monitor_error) &&
          (a.callback_context == b.callback_context));
}

inline bool operator==(const RdmnetScopeMonitorConfig& a, const RdmnetScopeMonitorConfig& b)
{
  return ((std::strncmp(a.scope, b.scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0) &&
          (std::strncmp(a.domain, b.domain, E133_DOMAIN_STRING_PADDED_LENGTH) == 0) &&
          (a.callbacks.broker_found == b.callbacks.broker_found) &&
          (a.callbacks.broker_lost == b.callbacks.broker_lost) &&
          (a.callbacks.scope_monitor_error == b.callbacks.scope_monitor_error) &&
          (a.callback_context == b.callback_context));
}

#endif  // TEST_OPERATORS_H_
