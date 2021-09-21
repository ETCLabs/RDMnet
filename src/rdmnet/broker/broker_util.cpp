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

#include "broker_util.h"

extern "C" {
bool IntHandleMgrValueInUse(int handle, void* context)
{
  auto func = static_cast<ClientHandleGenerator*>(context)->GetValueInUseFunc();
  if (func)
    return func(handle);
  else
    return false;
}
}

ClientHandleGenerator::ClientHandleGenerator()
{
  init_int_handle_manager(&handle_mgr_, -1, IntHandleMgrValueInUse, this);
}

void ClientHandleGenerator::SetValueInUseFunc(const ValueInUseFunc& value_in_use_func)
{
  value_in_use_ = value_in_use_func;
}

void ClientHandleGenerator::SetNextHandle(BrokerClient::Handle next_handle)
{
  handle_mgr_.last_handle = next_handle - 1;
}

ClientHandleGenerator::ValueInUseFunc ClientHandleGenerator::GetValueInUseFunc() const
{
  return value_in_use_;
}

BrokerClient::Handle ClientHandleGenerator::GetClientHandle()
{
  return get_next_int_handle(&handle_mgr_);
}

RptHeader SwapHeaderData(const RptHeader& source)
{
  RptHeader swapped_header;

  swapped_header.seqnum = source.seqnum;
  swapped_header.dest_endpoint_id = source.source_endpoint_id;
  swapped_header.dest_uid = source.source_uid;
  swapped_header.source_endpoint_id = source.dest_endpoint_id;
  swapped_header.source_uid = source.dest_uid;
  return swapped_header;
}
