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
#include <queue>
#include <stdexcept>
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/lock.h"
#include "etcpal/cpp/timer.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/socket.h"
#include "rdm/message.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/rpt_prot.h"
#include "rdmnet/defs.h"

struct MessageRef
{
  MessageRef() : size_sent(0) {}

  std::unique_ptr<uint8_t[]> data;
  size_t                     size;
  size_t                     size_sent;
};

// RPT RDM messages are two sets of data, the RPT header and the RDM message.
struct RPTMessageRef
{
  RPTMessageRef() {}
  RPTMessageRef(const RptHeader& new_header, const RdmBuffer& new_msg) : header(new_header), msg(new_msg) {}

  RptHeader header;
  RdmBuffer msg;
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

  virtual bool Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg);
  virtual bool Send(const etcpal::Uuid& broker_cid);

  bool TcpConnExpired() const { return heartbeat_timer_.IsExpired(); }
  void MessageReceived() { heartbeat_timer_.Reset(); }

  etcpal::Uuid           cid{};
  client_protocol_t      client_protocol{kClientProtocolUnknown};
  etcpal::SockAddr       addr{};
  Handle                 handle{kInvalidHandle};
  mutable etcpal::RwLock lock;
  etcpal_socket_t        socket{ETCPAL_SOCKET_INVALID};
  size_t                 max_q_size{0};

protected:
  bool PushPostSizeCheck(const etcpal::Uuid& sender_cid, const BrokerMessage& msg);
  bool SendNull(const etcpal::Uuid& broker_cid);

  std::queue<MessageRef> broker_msgs_;
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

  virtual bool Push(Handle /*from_conn*/, const etcpal::Uuid& /*sender_cid*/, const RptMessage& /*msg*/)
  {
    return false;
  }
  virtual bool Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;

  RdmUid            uid{};
  rpt_client_type_t client_type{kRPTClientTypeUnknown};
  etcpal::Uuid      binding_cid{};

protected:
  bool PushPostSizeCheck(const etcpal::Uuid& sender_cid, const RptHeader& header, const RptStatusMsg& msg);

  std::queue<MessageRef> status_msgs_;
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

  virtual bool Push(Handle from_conn, const etcpal::Uuid& sender_cid, const RptMessage& msg) override;
  virtual bool Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;
  virtual bool Push(const etcpal::Uuid& sender_cid, const RptHeader& header, const RptStatusMsg& msg);
  virtual bool Send(const etcpal::Uuid& broker_cid) override;

protected:
  std::queue<MessageRef> rpt_msgs_;
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

  virtual bool Push(Handle from_conn, const etcpal::Uuid& sender_cid, const RptMessage& msg) override;
  virtual bool Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;
  virtual bool Send(const etcpal::Uuid& broker_cid) override;

protected:
  Handle                                   last_controller_serviced_{kInvalidHandle};
  size_t                                   rpt_msgs_total_size_{0};
  std::map<Handle, std::queue<MessageRef>> rpt_msgs_;
};

#endif  // BROKER_CLIENT_H_
