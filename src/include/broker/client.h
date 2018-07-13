/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

/*! \file broker/client.h
 *
 */
#ifndef _BROKER_CLIENT_H_
#define _BROKER_CLIENT_H_

#include <set>
#include <queue>
#include <memory>
#include <map>
#include <cassert>
#include <stdexcept>
#include "lwpa_lock.h"
#include "lwpa_inet.h"
#include "rdmnet/rdmtypes.h"
#include "rdmnet/client.h"
#include "rdmnet/message.h"
#include "rdmnet/rptprot.h"
#include "broker/threads.h"

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
  RPTMessageRef(const RptHeader &new_header, const RdmBuffer &new_msg) : header(new_header), msg(new_msg) {}

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
  BrokerClient(int conn) : marked_for_destruction(false), conn_(conn), max_q_size_(0) { lwpa_rwlock_create(&lock_); }
  BrokerClient(int conn, size_t max_q_size) : marked_for_destruction(false), conn_(conn), max_q_size_(max_q_size)
  {
    lwpa_rwlock_create(&lock_);
  }
  // Non-default copy constructor to avoid copying the message queue and lock.
  BrokerClient(const BrokerClient &other)
      : cid(other.cid)
      , client_protocol(other.client_protocol)
      , addr(other.addr)
      , poll_thread(other.poll_thread)
      , marked_for_destruction(other.marked_for_destruction)
      , conn_(other.conn_)
      , max_q_size_(other.max_q_size_)
  {
    lwpa_rwlock_create(&lock_);
  }
  virtual ~BrokerClient() { lwpa_rwlock_destroy(&lock_); }

  virtual bool Push(const LwpaCid &sender_cid, const BrokerMessage &msg);
  virtual bool Send() { return false; }

  // Read/write lock functions. Prefer use of ClientReadGuard and
  // ClientWriteGuard to these functions where possible.
  bool ReadLock() const { return lwpa_rwlock_readlock(&lock_, LWPA_WAIT_FOREVER); }
  void ReadUnlock() const { lwpa_rwlock_readunlock(&lock_); }
  bool WriteLock() const { return lwpa_rwlock_writelock(&lock_, LWPA_WAIT_FOREVER); }
  void WriteUnlock() const { lwpa_rwlock_writeunlock(&lock_); }

  LwpaCid cid;
  client_protocol_t client_protocol;
  LwpaSockaddr addr;
  std::shared_ptr<ConnPollThread> poll_thread;
  bool marked_for_destruction;

protected:
  bool PushPostSizeCheck(const LwpaCid &sender_cid, const BrokerMessage &msg);

  mutable lwpa_rwlock_t lock_;
  int conn_;
  size_t max_q_size_;
  std::queue<MessageRef> broker_msgs_;
};

class ClientReadGuard
{
public:
  explicit ClientReadGuard(BrokerClient &client) : client_(client)
  {
    if (!client_.ReadLock())
    {
      throw std::runtime_error("Broker failed to take a read lock on a client.");
    }
  }
  ~ClientReadGuard() { client_.ReadUnlock(); }

private:
  BrokerClient &client_;
};

class ClientWriteGuard
{
public:
  explicit ClientWriteGuard(BrokerClient &client) : client_(client)
  {
    if (!client_.WriteLock())
    {
      throw std::runtime_error("Broker failed to take a write lock on a client.");
    }
  }
  ~ClientWriteGuard() { client_.WriteUnlock(); }

private:
  BrokerClient &client_;
};

class RPTClient : public BrokerClient
{
public:
  RPTClient(rpt_client_type_t new_ctype, const LwpaUid &new_uid, const BrokerClient &prev_client)
      : BrokerClient(prev_client), uid(new_uid), client_type(new_ctype)
  {
  }
  virtual ~RPTClient() {}

  virtual bool Push(int /*from_conn*/, const LwpaCid & /*sender_cid*/, const RptMessage & /*msg*/) { return false; }
  virtual bool Push(const LwpaCid &sender_cid, const BrokerMessage &msg) override;

  LwpaUid uid;
  rpt_client_type_t client_type;
  LwpaCid binding_cid;

protected:
  bool PushPostSizeCheck(const LwpaCid &sender_cid, const RptHeader &header, const RptStatusMsg &msg);

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
  RPTController(size_t max_q_size, const ClientEntryData &cli_entry, const BrokerClient &prev_client)
      : RPTClient(cli_entry.data.rpt_data.client_type, cli_entry.data.rpt_data.client_uid, prev_client)
  {
    max_q_size_ = max_q_size;
    cid = cli_entry.client_cid;
    client_protocol = cli_entry.client_protocol;
  }
  virtual ~RPTController() {}

  virtual bool Push(int from_conn, const LwpaCid &sender_cid, const RptMessage &msg) override;
  virtual bool Push(const LwpaCid &sender_cid, const BrokerMessage &msg) override;
  virtual bool Push(const LwpaCid &sender_cid, const RptHeader &header, const RptStatusMsg &msg);
  virtual bool Send() override;

  std::queue<MessageRef> rpt_msgs_;
};

// State data about each device
class RPTDevice : public RPTClient
{
public:
  RPTDevice(size_t max_q_size, const ClientEntryData &cli_entry, const BrokerClient &prev_client)
      : RPTClient(cli_entry.data.rpt_data.client_type, cli_entry.data.rpt_data.client_uid, prev_client)
      , last_controller_serviced_(-1)
      , rpt_msgs_total_size_(0)
  {
    max_q_size_ = max_q_size;
    cid = cli_entry.client_cid;
    client_protocol = cli_entry.client_protocol;
  }
  virtual ~RPTDevice() {}

  virtual bool Push(int from_conn, const LwpaCid &sender_cid, const RptMessage &msg) override;
  virtual bool Push(const LwpaCid &sender_cid, const BrokerMessage &msg) override;
  virtual bool Send() override;

protected:
  int last_controller_serviced_;
  size_t rpt_msgs_total_size_;
  std::map<int, std::queue<MessageRef>> rpt_msgs_;
};

#endif  // _BROKER_CLIENT_H_
