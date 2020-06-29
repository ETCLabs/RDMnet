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

// test_operators.h
// Provide comparison operators not defined elsewhere for RDMnet public API types, for the purpose
// of testing.

#ifndef TEST_OPERATORS_H_
#define TEST_OPERATORS_H_

#include <cstring>
#include "rdmnet/discovery.h"

inline bool operator==(const RdmnetDnsTxtRecordItem& a, const RdmnetDnsTxtRecordItem& b)
{
  return ((std::strcmp(a.key, b.key) == 0) && (a.value_len == b.value_len) &&
          (std::memcmp(a.value, b.value, a.value_len) == 0));
}

inline bool operator!=(const RdmnetDnsTxtRecordItem& a, const RdmnetDnsTxtRecordItem& b)
{
  return !(a == b);
}

inline bool operator==(const RdmnetBrokerDiscInfo& a, const RdmnetBrokerDiscInfo& b)
{
  if ((a.cid == b.cid) && (a.uid == b.uid) && (a.e133_version == b.e133_version) &&
      (std::strncmp(a.service_instance_name, b.service_instance_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH) == 0) &&
      (a.port == b.port) && (a.num_listen_addrs == b.num_listen_addrs) &&
      (std::strncmp(a.scope, b.scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0) &&
      (std::strncmp(a.model, b.model, E133_MODEL_STRING_PADDED_LENGTH) == 0) &&
      (std::strncmp(a.manufacturer, b.manufacturer, E133_MANUFACTURER_STRING_PADDED_LENGTH) == 0) &&
      (a.num_additional_txt_items == b.num_additional_txt_items))
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

    if (a.num_additional_txt_items > 0 && (!a.additional_txt_items || !b.additional_txt_items))
      return false;

    for (size_t i = 0; i < a.num_additional_txt_items; ++i)
    {
      if (a.additional_txt_items[i] != b.additional_txt_items[i])
        return false;
    }
    return true;
  }
  return false;
}

inline bool operator==(const RdmnetBrokerRegisterConfig& a, const RdmnetBrokerRegisterConfig& b)
{
  if ((a.cid == b.cid) && (a.uid == b.uid) && (std::strcmp(a.service_instance_name, b.service_instance_name) == 0) &&
      (a.port == b.port) && (a.num_netints == b.num_netints) && (std::strcmp(a.scope, b.scope) == 0) &&
      (std::strcmp(a.model, b.model) == 0) && (std::strcmp(a.manufacturer, b.manufacturer) == 0) &&
      (a.num_additional_txt_items == b.num_additional_txt_items) &&
      (a.callbacks.broker_registered == b.callbacks.broker_registered) &&
      (a.callbacks.broker_register_failed == b.callbacks.broker_register_failed) &&
      (a.callbacks.other_broker_found == b.callbacks.other_broker_found) &&
      (a.callbacks.other_broker_lost == b.callbacks.other_broker_lost) && (a.callbacks.context == b.callbacks.context))
  {
    if (a.num_netints > 0 && (!a.netints || !b.netints))
      return false;

    for (size_t i = 0; i < a.num_netints; ++i)
    {
      if (a.netints[i] != b.netints[i])
        return false;
    }

    if (a.num_additional_txt_items > 0 && (!a.additional_txt_items || !b.additional_txt_items))
      return false;

    for (size_t i = 0; i < a.num_additional_txt_items; ++i)
    {
      if (a.additional_txt_items[i] != b.additional_txt_items[i])
        return false;
    }

    return true;
  }
  return false;
}

inline bool operator==(const RdmnetScopeMonitorConfig& a, const RdmnetScopeMonitorConfig& b)
{
  return ((std::strncmp(a.scope, b.scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0) &&
          (std::strncmp(a.domain, b.domain, E133_DOMAIN_STRING_PADDED_LENGTH) == 0) &&
          (a.callbacks.broker_found == b.callbacks.broker_found) &&
          (a.callbacks.broker_lost == b.callbacks.broker_lost) && (a.callbacks.context == b.callbacks.context));
}

#endif  // TEST_OPERATORS_H_
