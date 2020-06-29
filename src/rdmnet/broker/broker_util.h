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
#include <stdexcept>
#include <queue>
#include "etcpal/common.h"
#include "rdm/cpp/uid.h"
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

// Represents an action to take before destroying a client. The default-constructed object means
// take no action.
class ClientDestroyAction
{
public:
  ClientDestroyAction() = default;

  static ClientDestroyAction SendConnectReply(rdmnet_connect_status_t connect_status);
  static ClientDestroyAction SendDisconnect(rdmnet_disconnect_reason_t reason);
  static ClientDestroyAction MarkSocketInvalid();

  void Apply(const rdm::Uid& broker_uid, const etcpal::Uuid& broker_cid, BrokerClient& client);

private:
  enum class Action
  {
    kDoNothing,
    kSendDisconnect,
    kSendConnectReply,
    kMarkSocketInvalid
  };
  Action action_{Action::kDoNothing};
  union
  {
    rdmnet_disconnect_reason_t disconnect_reason;
    rdmnet_connect_status_t    connect_status;
  } data_{};
};

inline ClientDestroyAction ClientDestroyAction::SendConnectReply(rdmnet_connect_status_t connect_status)
{
  ClientDestroyAction to_return;
  to_return.action_ = Action::kSendConnectReply;
  to_return.data_.connect_status = connect_status;
  return to_return;
}

inline ClientDestroyAction ClientDestroyAction::SendDisconnect(rdmnet_disconnect_reason_t reason)
{
  ClientDestroyAction to_return;
  to_return.action_ = Action::kSendDisconnect;
  to_return.data_.disconnect_reason = reason;
  return to_return;
}

inline ClientDestroyAction ClientDestroyAction::MarkSocketInvalid()
{
  ClientDestroyAction to_return;
  to_return.action_ = Action::kMarkSocketInvalid;
  return to_return;
}

inline void ClientDestroyAction::Apply(const rdm::Uid& broker_uid, const etcpal::Uuid& broker_cid, BrokerClient& client)
{
  if (action_ == Action::kSendConnectReply)
  {
    BrokerMessage msg{};
    msg.vector = VECTOR_BROKER_CONNECT_REPLY;
    BROKER_GET_CONNECT_REPLY_MSG(&msg)->broker_uid = broker_uid.get();
    BROKER_GET_CONNECT_REPLY_MSG(&msg)->connect_status = data_.connect_status;
    BROKER_GET_CONNECT_REPLY_MSG(&msg)->e133_version = E133_VERSION;
    client.Push(broker_cid, msg);
  }
  else if (action_ == Action::kSendDisconnect)
  {
    BrokerMessage msg{};
    msg.vector = VECTOR_BROKER_DISCONNECT;
    BROKER_GET_DISCONNECT_MSG(&msg)->disconnect_reason = data_.disconnect_reason;
    client.Push(broker_cid, msg);
  }
  else if (action_ == Action::kMarkSocketInvalid)
  {
    client.socket = ETCPAL_SOCKET_INVALID;
  }
}

// Utility functions for manipulating messages
RptHeader SwapHeaderData(const RptHeader& source);

#endif  // BROKER_UTIL_H_
