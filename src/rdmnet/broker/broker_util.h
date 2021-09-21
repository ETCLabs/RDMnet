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

/// @file broker_util.h

#ifndef BROKER_UTIL_H_
#define BROKER_UTIL_H_

#include <functional>
#include "etcpal/common.h"
#include "etcpal/handle_manager.h"
#include "rdmnet/core/rpt_prot.h"
#include "rdmnet/core/util.h"
#include "broker_client.h"

// A class to generate client handles using the algorithm of the RDMnet core library
// IntHandleManager.
class ClientHandleGenerator
{
public:
  using ValueInUseFunc = std::function<bool(BrokerClient::Handle)>;

  ClientHandleGenerator();

  void           SetValueInUseFunc(const ValueInUseFunc& value_in_use_func);
  ValueInUseFunc GetValueInUseFunc() const;
  void           SetNextHandle(BrokerClient::Handle next_handle);

  BrokerClient::Handle GetClientHandle();

private:
  ValueInUseFunc   value_in_use_;
  IntHandleManager handle_mgr_;
};

// Utility functions for manipulating messages
RptHeader SwapHeaderData(const RptHeader& source);

#endif  // BROKER_UTIL_H_
