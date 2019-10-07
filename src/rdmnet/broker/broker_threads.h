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
#ifndef _BROKER_THREADS_H_
#define _BROKER_THREADS_H_

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
  virtual bool NewConnection(etcpal_socket_t new_sock, const EtcPalSockaddr& remote_addr) = 0;

  // A notification from a client service thread to process each client queue, sending out the next
  // message from each queue if one is available. Return false if no messages or partial messages
  // were sent.
  virtual bool ServiceClients() = 0;
};

class BrokerThreadInterface
{
public:
  virtual bool AddListenThread(etcpal_socket_t listen_sock) = 0;

  virtual bool AddClientServiceThread() = 0;
};

class BrokerThreadManager
{
  // Listens for TCP connections
  ListenThread(etcpal_socket_t listen_sock, ListenThreadNotify* pnotify, rdmnet::BrokerLog* log);
  virtual ~ListenThread();

  // Creates the listening socket and starts the thread.
  bool Start();

  // Destroys the listening socket and stops the thread.
  void Stop();

  // The thread function
  void Run();

protected:
  bool terminated_{true};
  ListenThreadNotify* notify_{nullptr};

  struct ListenThread
  {
    etcpal_thread_t handle{};
    etcpal_socket_t socket{ETCPAL_SOCKET_INVALID};
  };
  std::vector<ListenThread> listen_threads_;

  rdmnet::BrokerLog* log_{nullptr};
};

// This is the thread that processes the controller queues and device states
class ClientServiceThread
{
public:
  explicit ClientServiceThread(unsigned int sleep_ms);
  virtual ~ClientServiceThread();

  bool Start();
  void Stop();

  void SetNotify(ClientServiceThreadNotify* pnotify) { notify_ = pnotify; }

  void Run();

protected:
  bool terminated_;
  etcpal_thread_t thread_handle_;
  int sleep_ms_;
  ClientServiceThreadNotify* notify_;
};

#endif  // _BROKER_THREADS_H_
