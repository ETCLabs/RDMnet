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

/// @file broker_client.h

#ifndef BROKER_CLIENT_H_
#define BROKER_CLIENT_H_

#include <chrono>
#include <memory>
#include <map>
#include <deque>
#include <stdexcept>
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/rwlock.h"
#include "etcpal/cpp/timer.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/socket.h"
#include "rdm/cpp/uid.h"
#include "rdm/message.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/rpt_prot.h"
#include "rdmnet/defs.h"

struct MessageRef
{
  MessageRef() = default;
  MessageRef(size_t alloc_size) : data(new uint8_t[alloc_size]) {}

  std::unique_ptr<uint8_t[]> data;
  size_t                     size{0};
  size_t                     size_sent{0};
};

// RPT RDM messages are two sets of data, the RPT header and the RDM message.
struct RPTMessageRef
{
  RPTMessageRef() {}
  RPTMessageRef(const RptHeader& new_header, const RdmBuffer& new_msg) : header(new_header), msg(new_msg) {}

  RptHeader header;
  RdmBuffer msg;
};

// Represents an action to take before destroying a client. The default-constructed object means
// take no action.
class ClientDestroyAction
{
public:
  enum class Action
  {
    DoNothing,
    SendDisconnect,
    SendConnectReply,
    MarkSocketInvalid
  };

  ClientDestroyAction() = default;

  static ClientDestroyAction DoNothing();
  static ClientDestroyAction SendConnectReply(rdmnet_connect_status_t connect_status);
  static ClientDestroyAction SendDisconnect(rdmnet_disconnect_reason_t reason);
  static ClientDestroyAction MarkSocketInvalid();

  Action                     action() const noexcept { return action_; }
  rdmnet_disconnect_reason_t disconnect_reason() const noexcept { return data_.disconnect_reason; }
  rdmnet_connect_status_t    connect_status() const noexcept { return data_.connect_status; }

private:
  Action action_{Action::DoNothing};
  union
  {
    rdmnet_disconnect_reason_t disconnect_reason;
    rdmnet_connect_status_t    connect_status;
  } data_{};
};

inline ClientDestroyAction ClientDestroyAction::DoNothing()
{
  return ClientDestroyAction{};
}

inline ClientDestroyAction ClientDestroyAction::SendConnectReply(rdmnet_connect_status_t connect_status)
{
  ClientDestroyAction to_return;
  to_return.action_ = Action::SendConnectReply;
  to_return.data_.connect_status = connect_status;
  return to_return;
}

inline ClientDestroyAction ClientDestroyAction::SendDisconnect(rdmnet_disconnect_reason_t reason)
{
  ClientDestroyAction to_return;
  to_return.action_ = Action::SendDisconnect;
  to_return.data_.disconnect_reason = reason;
  return to_return;
}

inline ClientDestroyAction ClientDestroyAction::MarkSocketInvalid()
{
  ClientDestroyAction to_return;
  to_return.action_ = Action::MarkSocketInvalid;
  return to_return;
}

// The result of attempting to push to a client's outgoing send queue.
enum class ClientPushResult
{
  Ok,         // Push successful
  QueueFull,  // The send queue is full
  Error       // Other classes of error, e.g. could not allocate memory
};

// A generic client.
// Each component that connects to a broker is a client. The broker uses the common functionality
// defined in this class to handle each client to which it is connected.
//
// A BrokerClient on its own is only capable of sending broker protocol messages and is intended to
// represent clients that have not yet sent an RDMnet connect request (aka "pending" clients).
class BrokerClient
{
public:
  using Handle = int;
  static constexpr Handle kInvalidHandle = -1;

  BrokerClient(Handle new_handle, etcpal_socket_t new_socket, size_t new_max_q_size = 0)
      : handle(new_handle), socket(new_socket), max_q_size(new_max_q_size)
  {
  }
  // Non-default copy constructor to avoid copying the message queue and lock.
  BrokerClient(const BrokerClient& other)
      : cid(other.cid)
      , client_protocol(other.client_protocol)
      , addr(other.addr)
      , handle(other.handle)
      , socket(other.socket)
      , max_q_size(other.max_q_size)
  {
  }
  virtual ~BrokerClient() = default;

  virtual ClientPushResult Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg);
  virtual bool             Send(const etcpal::Uuid& broker_cid);
  void                     MarkForDestruction(const etcpal::Uuid&        broker_cid,
                                              const rdm::Uid&            broker_uid,
                                              const ClientDestroyAction& destroy_action);

  bool TcpConnExpired() const { return heartbeat_timer_.IsExpired(); }
  void MessageReceived() { heartbeat_timer_.Reset(); }

  etcpal::Uuid           cid{};
  client_protocol_t      client_protocol{kClientProtocolUnknown};
  etcpal::SockAddr       addr{};
  Handle                 handle{kInvalidHandle};
  mutable etcpal::RwLock lock;
  etcpal_socket_t        socket{ETCPAL_SOCKET_INVALID};
  size_t                 max_q_size{0};
  bool                   marked_for_destruction{false};

protected:
  ClientPushResult PushPostSizeCheck(const etcpal::Uuid& sender_cid, const BrokerMessage& msg);
  bool             SendNull(const etcpal::Uuid& broker_cid);
  void             ApplyDestroyAction(const etcpal::Uuid&        broker_cid,
                                      const rdm::Uid&            broker_uid,
                                      const ClientDestroyAction& destroy_action);

  virtual void ClearAllQueues() { broker_msgs_.clear(); }

  std::deque<MessageRef> broker_msgs_;
  etcpal::Timer          send_timer_{std::chrono::seconds(E133_TCP_HEARTBEAT_INTERVAL_SEC)};
  etcpal::Timer          heartbeat_timer_{std::chrono::seconds(E133_HEARTBEAT_TIMEOUT_SEC)};
};

class ClientReadGuard
{
public:
  explicit ClientReadGuard(BrokerClient& client) : rg_(client.lock) {}

private:
  etcpal::ReadGuard rg_;
};

class ClientWriteGuard
{
public:
  explicit ClientWriteGuard(BrokerClient& client) : wg_(client.lock) {}

private:
  etcpal::WriteGuard wg_;
};

class RPTClient : public BrokerClient
{
public:
  RPTClient(const RdmnetRptClientEntry& client_entry, const BrokerClient& prev_client)
      : BrokerClient(prev_client)
      , uid(client_entry.uid)
      , client_type(client_entry.type)
      , binding_cid(client_entry.binding_cid)
  {
    client_protocol = kClientProtocolRPT;
    cid = client_entry.cid;
  }
  virtual ~RPTClient() {}

  virtual ClientPushResult Push(Handle /*from_conn*/, const etcpal::Uuid& /*sender_cid*/, const RptMessage& /*msg*/)
  {
    return ClientPushResult::Error;
  }
  virtual ClientPushResult Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;

  RdmUid            uid{};
  rpt_client_type_t client_type{kRPTClientTypeUnknown};
  etcpal::Uuid      binding_cid{};

protected:
  ClientPushResult PushPostSizeCheck(const etcpal::Uuid& sender_cid, const RptHeader& header, const RptStatusMsg& msg);
  virtual void     ClearAllQueues();

  std::deque<MessageRef> status_msgs_;
};

struct EPTClient : public BrokerClient
{
};

// State data about each controller
class RPTController : public RPTClient
{
public:
  // TODO max queue size
  RPTController(size_t new_max_q_size, const RdmnetRptClientEntry& cli_entry, const BrokerClient& prev_client)
      : RPTClient(cli_entry, prev_client)
  {
    max_q_size = new_max_q_size;
  }
  virtual ~RPTController() {}

  virtual ClientPushResult Push(Handle from_conn, const etcpal::Uuid& sender_cid, const RptMessage& msg) override;
  virtual ClientPushResult Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;
  virtual ClientPushResult Push(const etcpal::Uuid& sender_cid, const RptHeader& header, const RptStatusMsg& msg);
  virtual bool             Send(const etcpal::Uuid& broker_cid) override;

protected:
  bool         HasRoomToPush();
  virtual void ClearAllQueues();

  std::deque<MessageRef> rpt_msgs_;
};

// State data about each device
class RPTDevice : public RPTClient
{
public:
  RPTDevice(size_t new_max_q_size, const RdmnetRptClientEntry& cli_entry, const BrokerClient& prev_client)
      : RPTClient(cli_entry, prev_client)
  {
    max_q_size = new_max_q_size;
  }
  virtual ~RPTDevice() {}

  virtual ClientPushResult Push(Handle from_conn, const etcpal::Uuid& sender_cid, const RptMessage& msg) override;
  virtual ClientPushResult Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;
  virtual bool             Send(const etcpal::Uuid& broker_cid) override;

protected:
  bool         HasRoomToPush();
  virtual void ClearAllQueues();

  // A special queue-like class that organizes messages by source controller for fair scheduling.
  class RptMsgQ
  {
  public:
    bool        empty() const;
    MessageRef* front();
    void        pop_front();
    void        push_back(Handle controller, MessageRef&& value);
    size_t      size() const;
    void        clear();

    void RemoveCurrentController();

  private:
    size_t                                   total_msg_count_{0};
    std::map<Handle, std::deque<MessageRef>> rpt_msgs_;
    Handle                                   current_controller_{kInvalidHandle};
  };
  RptMsgQ rpt_msgs_;
};

#endif  // BROKER_CLIENT_H_
