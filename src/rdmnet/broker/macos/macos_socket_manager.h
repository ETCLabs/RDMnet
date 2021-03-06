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

// macOS override of BrokerSocketManager.
// Uses epoll, currently the most efficient and scalable socket management tool available from the
// macOS Darwin API.

#ifndef MACOS_SOCKET_MANAGER_H_
#define MACOS_SOCKET_MANAGER_H_

#include <map>
#include <vector>
#include <memory>
#include <pthread.h>

#include "etcpal/cpp/rwlock.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/msg_buf.h"
#include "broker_socket_manager.h"

// The set of data allocated per-socket.
struct SocketData
{
  SocketData(BrokerClient::Handle client_handle_in, etcpal_socket_t socket_in)
      : client_handle(client_handle_in), socket(socket_in)
  {
    rc_msg_buf_init(&recv_buf);
  }

  BrokerClient::Handle client_handle{BrokerClient::kInvalidHandle};
  int                  socket{-1};

  // Receive buffer for socket recv operations
  RCMsgBuf recv_buf;
};

// A class to manage RDMnet Broker sockets on Mac.
// This handles receiving data on all RDMnet client connections, using epoll for maximum
// performance. Sending on connections is done in the core Broker library through the EtcPal
// interface. Other miscellaneous Broker socket operations like LLRP are also handled in the core
// library.
class MacBrokerSocketManager : public BrokerSocketManager
{
public:
  virtual ~MacBrokerSocketManager() = default;

  // BrokerSocketManager interface
  bool Startup() override;
  bool Shutdown() override;
  void SetNotify(BrokerSocketNotify* notify) override { notify_ = notify; }
  bool AddSocket(BrokerClient::Handle client_handle, etcpal_socket_t socket) override;
  void RemoveSocket(BrokerClient::Handle client_handle) override;

  // Callback functions called from worker threads
  void WorkerNotifySocketReadEvent(BrokerClient::Handle client_handle);
  void WorkerNotifySocketBad(BrokerClient::Handle conn_handle);

  // Accessors
  bool keep_running() const { return !shutting_down_; }
  int  kqueue_fd() const { return kqueue_fd_; }

private:
  bool      shutting_down_{false};
  pthread_t thread_handle_;
  int       kqueue_fd_{-1};

  // The set of sockets being managed.
  std::map<BrokerClient::Handle, std::unique_ptr<SocketData>> sockets_;
  etcpal::RwLock                                              socket_lock_;

  // The callback instance
  BrokerSocketNotify* notify_{nullptr};
};

#endif  // MACOS_SOCKET_MANAGER_H_
