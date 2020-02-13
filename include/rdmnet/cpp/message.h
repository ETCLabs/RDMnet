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
#include <vector>
#include "rdm/cpp/uid.h"
#include "rdm/cpp/message.h"
#include "rdmnet/core/message.h"

namespace rdmnet
{
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
  rdm::Command rdm_command() const noexcept;

  static LocalRdmCommand Get(uint16_t param_id, const rdm::Uid& rdmnet_dest_uid, uint16_t dest_endpoint,
                             const rdm::Uid& rdm_dest_uid, const uint8_t* data = nullptr, uint8_t datalen = 0);
  static LocalRdmCommand Set(uint16_t param_id, const rdm::Uid& rdmnet_dest_uid, uint16_t dest_endpoint,
                             const rdm::Uid& rdm_dest_uid, const uint8_t* data = nullptr, uint8_t datalen = 0);

  static LocalRdmCommand ToDefaultResponder(const rdm::Uid& rdmnet_dest_uid, rdm_command_class_t command_class,
                                            uint16_t param_id, const uint8_t* data = nullptr, uint8_t datalen = 0);
  static LocalRdmCommand ToDefaultResponder(const rdm::Uid& rdmnet_dest_uid, const rdm::Command& rdm_command);
  static LocalRdmCommand GetToDefaultResponder(uint16_t param_id, const rdm::Uid& rdmnet_dest_uid,
                                               const uint8_t* data = nullptr, uint8_t datalen = 0);
  static LocalRdmCommand SetToDefaultResponder(uint16_t param_id, const rdm::Uid& rdmnet_dest_uid,
                                               const uint8_t* data = nullptr, uint8_t datalen = 0);

  static LocalRdmCommand FromRdm(const rdm::Uid& rdmnet_dest_uid, uint16_t dest_endpoint,
                                 const rdm::Command& rdm_command);

private:
  RdmnetLocalRdmCommand cmd_;
};

class RemoteRdmCommand
{
private:
  RdmnetRemoteRdmCommand cmd_;
};

class LocalRdmResponse
{
  LocalRdmResponse(const rdm::Uid& rdmnet_dest_uid, uint16_t source_endpoint, uint32_t seq_num,
                   const RdmResponse* responses, size_t num_responses);

  LocalRdmResponse FromCommand(const RemoteRdmCommand& received_cmd, const RdmResponse& response);
  LocalRdmResponse FromCommand(const RemoteRdmCommand& received_cmd, const RdmResponse* responses,
                               size_t num_responses);
  LocalRdmResponse FromCommand(const RemoteRdmCommand& received_cmd, const rdm::Response& response);
  LocalRdmResponse FromCommand(const RemoteRdmCommand& received_cmd, const rdm::Response* responses,
                               size_t num_responses);
  LocalRdmResponse FromCommand(const RemoteRdmCommand& received_cmd, const std::vector<rdm::Response>& responses);

private:
  RdmnetLocalRdmResponse resp_;
};

class RemoteRdmResponse
{
private:
  RdmnetRemoteRdmResponse resp_;
};

};  // namespace rdmnet

namespace llrp
{
class LocalRdmCommand
{
private:
  LlrpLocalRdmCommand cmd_;
}
};  // namespace llrp

#endif  // RDMNET_CPP_MESSAGE_H_
