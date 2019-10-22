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

/// \file broker_threads.h
/// \brief Classes to represent threads used by the Broker.
/// \author Nick Ballhorn-Wagner and Sam Kearney

#ifndef BROKER_THREADS_H_
#define BROKER_THREADS_H_

#include <string>
#include <memory>
#include <vector>

#include "etcpal/thread.h"
#include "etcpal/lock.h"
#include "etcpal/inet.h"
#include "etcpal/socket.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/broker.h"
#include "broker_util.h"

// The interface for callbacks from threads managed by the broker.
class BrokerThreadNotify
{
public:
  // Called when a listen thread gets a new connection. Return false to close the connection
  // immediately.
  virtual bool HandleNewConnection(etcpal_socket_t new_sock, const EtcPalSockaddr& remote_addr) = 0;

  // A notification from a client service thread to process each client queue, sending out the next
  // message from each queue if one is available. Return false if no messages or partial messages
  // were sent.
  virtual bool ServiceClients() = 0;
};

class BrokerThreadInterface
{
public:
  virtual ~BrokerThreadInterface() = default;

  virtual void SetNotify(BrokerThreadNotify* notify) = 0;

  virtual bool AddListenThread(etcpal_socket_t listen_sock) = 0;

  virtual bool AddClientServiceThread() = 0;

  virtual void StopThreads() = 0;
};

class BrokerThread
{
public:
  BrokerThread(BrokerThreadNotify* notify) : notify_(notify) {}
  virtual ~BrokerThread() {}

  virtual bool Start() = 0;
  virtual void Run() = 0;

  bool terminated() const { return terminated_; }

protected:
  etcpal_thread_t handle_{};
  BrokerThreadNotify* notify_{nullptr};
  bool terminated_{true};
};

class ListenThread : public BrokerThread
{
public:
  ListenThread(etcpal_socket_t listen_sock, BrokerThreadNotify* notify, rdmnet::BrokerLog* log)
      : BrokerThread(notify), socket_(listen_sock), log_(log)
  {
  }
  ~ListenThread() override;

  bool Start() override;
  void Run() override;

  void ReadSocket();

private:
  etcpal_socket_t socket_{ETCPAL_SOCKET_INVALID};
  rdmnet::BrokerLog* log_{nullptr};
};

class ClientServiceThread : public BrokerThread
{
public:
  ClientServiceThread(BrokerThreadNotify* notify) : BrokerThread(notify) {}
  ~ClientServiceThread() override;

  bool Start() override;
  void Run() override;

protected:
  static constexpr int kSleepMs{1};
};

class BrokerThreadManager : public BrokerThreadInterface
{
public:
  void SetNotify(BrokerThreadNotify* notify) override { notify_ = notify; }

  bool AddListenThread(etcpal_socket_t listen_sock) override;
  bool AddClientServiceThread() override;

  void StopThreads() override;

  std::vector<std::unique_ptr<BrokerThread>>& threads();

private:
  BrokerThreadNotify* notify_{nullptr};

  std::vector<std::unique_ptr<BrokerThread>> threads_;

  rdmnet::BrokerLog* log_{nullptr};
};

#endif  // BROKER_THREADS_H_
