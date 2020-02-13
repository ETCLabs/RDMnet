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

/// \file rdmnet/cpp/message.h

#ifndef RDMNET_CPP_MESSAGE_H_
#define RDMNET_CPP_MESSAGE_H_

#include <cstdint>
#include "rdm/cpp/uid.h"
#include "rdmnet/core/message.h"

namespace rdmnet
{
class RemoteRdmCommand
{
private:
  RdmnetRemoteRdmCommand cmd_;
};

class LocalRdmResponse
{
  LocalRdmResponse(const rdm::Uid& rdmnet_dest_uid, uint16_t source_endpoint, uint32_t seq_num,
                   const RdmResponse* responses, size_t num_responses);

private:
  RdmnetLocalRdmResponse resp_;
};

class LocalRdmCommand
{
public:
  LocalRdmCommand(const rdm::Uid& rdmnet_dest_uid, uint16_t dest_endpoint, const rdm::Uid& rdm_dest_uid,
                  rdm_command_class_t command_class, uint16_t param_id, const uint8_t* data = nullptr,
                  uint8_t datalen = 0);
  LocalRdmCommand(const rdm::Uid& rdmnet_dest_uid, uint16_t dest_endpoint, const rdm::Uid& rdm_dest_uid,
                  uint16_t subdevice, rdm_command_class_t command_class, uint16_t param_id,
                  const uint8_t* data = nullptr, uint8_t datalen = 0);

  static RdmnetLocalRdmCommand& get() noexcept;
  const RdmnetLocalRdmCommand& get() const noexcept;

  static LocalRdmCommand Get(uint16_t param_id, const rdm::Uid& rdmnet_dest_uid, uint16_t dest_endpoint,
                             const rdm::Uid& rdm_dest_uid, const uint8_t* data = nullptr, uint8_t datalen = 0);
  static LocalRdmCommand Set(uint16_t param_id, const rdm::Uid& rdmnet_dest_uid, uint16_t dest_endpoint,
                             const rdm::Uid& rdm_dest_uid, const uint8_t* data = nullptr, uint8_t datalen = 0);

  static LocalRdmCommand ToDefaultResponder(const rdm::Uid& rdm_dest_uid, rdm_command_class_t command_class,
                                            uint16_t param_id, const uint8_t* data = nullptr, uint8_t datalen = 0);
  static LocalRdmCommand GetToDefaultResponder(uint16_t param_id, const rdm::Uid& rdm_dest_uid,
                                               const uint8_t* data = nullptr, uint8_t datalen = 0);
  static LocalRdmCommand SetToDefaultResponder(uint16_t param_id, const rdm::Uid& rdm_dest_uid,
                                               const uint8_t* data = nullptr, uint8_t datalen = 0);

private:
  RdmnetLocalRdmCommand cmd_;
};

class RemoteRdmResponse
{
private:
  RdmnetRemoteRdmResponse resp_;
};

// TODO Resolve

// template <class Message>
// rdm::Uid GetSourceUid(const Message& msg)
// {
//   return msg.source_uid;
// }
//
// template <class Message>
// rdm::Uid GetDestUid(const Message& msg)
// {
//   return msg.dest_uid;
// }
//
// template <class Message>
// uint16_t GetDestEndpoint(const Message& msg)
// {
//   return msg.dest_endpoint;
// }
//
// template <class Message>
// uint32_t GetSeqNum(const Message& msg)
// {
//   return msg.seq_num;
// }
//
// template <class Message>
// rdm::Command GetRdmCommand(const Message& msg)
// {
//   return msg.rdm_command;
// }
//
// template <class Message>
// rdm::Command GetOriginalCommand(const Message& msg)
// {
//   return msg.original_command;
// }

};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_H_
