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

#include "rdmnet/core/discovery.h"

inline bool operator==(const RdmnetBrokerDiscInfo& a, const RdmnetBrokerDiscInfo& b)
{
  if ((a.cid == b.cid) && (std::strncmp(a.service_name, b.service_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH) == 0) &&
      (a.port == b.port) && (std::strncmp(a.scope, b.scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0) &&
      (std::strncmp(a.model, b.model, E133_MODEL_STRING_PADDED_LENGTH) == 0) &&
      (std::strncmp(a.manufacturer, b.manufacturer, E133_MANUFACTURER_STRING_PADDED_LENGTH) == 0))
  {
    // Check the BrokerListenAddrs
    BrokerListenAddr* a_addr = a.listen_addr_list;
    BrokerListenAddr* b_addr = b.listen_addr_list;
    while (a_addr && b_addr)
    {
      if (!(a_addr->addr == b_addr->addr) || (a_addr->next && !b_addr->next) || (!b_addr->next && a_addr->next))
      {
        return false;
      }
      a_addr = a_addr->next;
      b_addr = b_addr->next;
    }
    return true;
  }
  return false;
}

#endif  // TEST_OPERATORS_H_
