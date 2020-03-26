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

/// \file broker_client.h

#ifndef BROKER_CLIENT_H_
#define BROKER_CLIENT_H_

#include <queue>
#include <memory>
#include <map>
#include <stdexcept>

#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/lock.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/message.h"
#include "rdmnet/core/client.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/rpt_prot.h"
#include "broker_threads.h"

struct MessageRef
{
  MessageRef() : size_sent(0) {}

  std::unique_ptr<uint8_t[]> data;
  size_t size;
  size_t size_sent;
};

// RPT RDM messages are two sets of data, the RPT header and the RDM message.
struct RPTMessageRef
{
  RPTMessageRef() {}
  RPTMessageRef(const RptHeader& new_header, const RdmBuffer& new_msg) : header(new_header), msg(new_msg) {}

  RptHeader header;
  RdmBuffer msg;
};

/*! \brief A generic Client.
 *  Each Component that connects to a Broker is a Client. The Broker uses the
 *  common functionality defined in this class to handle each Client to which
 *  it is connected. */
class BrokerClient
{
public:
  BrokerClient(rdmnet_conn_t conn) : conn_(conn) {}
  BrokerClient(rdmnet_conn_t conn, size_t max_q_size) : conn_(conn), max_q_size_(max_q_size) {}
  // Non-default copy constructor to avoid copying the message queue and lock.
  BrokerClient(const BrokerClient& other)
      : cid(other.cid)
      , client_protocol(other.client_protocol)
      , addr(other.addr)
      , conn_(other.conn_)
      , max_q_size_(other.max_q_size_)
  {
  }
  virtual ~BrokerClient() = default;

  virtual bool Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg);
  virtual bool Send() { return false; }

  // Read/write lock functions. Prefer use of ClientReadGuard and
  // ClientWriteGuard to these functions where possible.
  bool ReadLock() const { return lock_.ReadLock(); }
  void ReadUnlock() const { lock_.ReadUnlock(); }
  bool WriteLock() const { return lock_.WriteLock(); }
  void WriteUnlock() const { lock_.WriteUnlock(); }

  etcpal::Uuid cid{};
  client_protocol_t client_protocol{kClientProtocolUnknown};
  etcpal::SockAddr addr{};

protected:
  bool PushPostSizeCheck(const etcpal::Uuid& sender_cid, const BrokerMessage& msg);

  mutable etcpal::RwLock lock_;
  rdmnet_conn_t conn_{RDMNET_CONN_INVALID};
  size_t max_q_size_{0};
  std::queue<MessageRef> broker_msgs_;
};

class ClientReadGuard
{
public:
  explicit ClientReadGuard(BrokerClient& client) : client_(client)
  {
    if (!client_.ReadLock())
    {
      throw std::runtime_error("Broker failed to take a read lock on a client.");
    }
  }
  ~ClientReadGuard() { client_.ReadUnlock(); }

private:
  BrokerClient& client_;
};

class ClientWriteGuard
{
public:
  explicit ClientWriteGuard(BrokerClient& client) : client_(client)
  {
    if (!client_.WriteLock())
    {
      throw std::runtime_error("Broker failed to take a write lock on a client.");
    }
  }
  ~ClientWriteGuard() { client_.WriteUnlock(); }

private:
  BrokerClient& client_;
};

class RPTClient : public BrokerClient
{
public:
  RPTClient(const RptClientEntry& client_entry, const BrokerClient& prev_client)
      : BrokerClient(prev_client)
      , uid(client_entry.uid)
      , client_type(client_entry.type)
      , binding_cid(client_entry.binding_cid)
  {
    client_protocol = kClientProtocolRPT;
    cid = client_entry.cid;
  }
  virtual ~RPTClient() {}

  virtual bool Push(rdmnet_conn_t /*from_conn*/, const etcpal::Uuid& /*sender_cid*/, const RptMessage& /*msg*/)
  {
    return false;
  }
  virtual bool Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;

  RdmUid uid{};
  rpt_client_type_t client_type{kRPTClientTypeUnknown};
  etcpal::Uuid binding_cid{};

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
  RPTController(size_t max_q_size, const RptClientEntry& cli_entry, const BrokerClient& prev_client)
      : RPTClient(cli_entry, prev_client)
  {
    max_q_size_ = max_q_size;
  }
  virtual ~RPTController() {}

  virtual bool Push(rdmnet_conn_t from_conn, const etcpal::Uuid& sender_cid, const RptMessage& msg) override;
  virtual bool Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;
  virtual bool Push(const etcpal::Uuid& sender_cid, const RptHeader& header, const RptStatusMsg& msg);
  virtual bool Send() override;

  std::queue<MessageRef> rpt_msgs_;
};

// State data about each device
class RPTDevice : public RPTClient
{
public:
  RPTDevice(size_t max_q_size, const RptClientEntry& cli_entry, const BrokerClient& prev_client)
      : RPTClient(cli_entry, prev_client)
  {
    max_q_size_ = max_q_size;
  }
  virtual ~RPTDevice() {}

  virtual bool Push(rdmnet_conn_t from_conn, const etcpal::Uuid& sender_cid, const RptMessage& msg) override;
  virtual bool Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg) override;
  virtual bool Send() override;

protected:
  rdmnet_conn_t last_controller_serviced_{RDMNET_CONN_INVALID};
  size_t rpt_msgs_total_size_{0};
  std::map<rdmnet_conn_t, std::queue<MessageRef>> rpt_msgs_;
};

#endif  // BROKER_CLIENT_H_
