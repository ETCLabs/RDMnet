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

#ifndef RDMNET_CPP_MESSAGE_H_
#define RDMNET_CPP_MESSAGE_H_

#include "rdm/cpp/uid.h"
#include "rdm/cpp/message.h"
#include "rdmnet/core/message.h"

namespace rdmnet
{
template <class Message>
rdm::Uid GetSourceUid(const Message& msg)
{
  return msg.source_uid;
}

template <class Message>
rdm::Uid GetDestUid(const Message& msg)
{
  return msg.dest_uid;
}

template <class Message>
uint16_t GetDestEndpoint(const Message& msg)
{
  return msg.dest_endpoint;
}

template <class Message>
uint32_t GetSeqNum(const Message& msg)
{
  return msg.seq_num;
}

template <class Message>
rdm::Command GetRdmCommand(const Message& msg)
{
  return msg.rdm_command;
}

template <class Message>
rdm::Command GetOriginalCommand(const Message& msg)
{
  return msg.original_command;
}

};      // namespace rdmnet
#endif  // RDMNET_CPP_MESSAGE_H_
