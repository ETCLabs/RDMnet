/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

// macOS override of RDMnet::BrokerSocketManager.
// Uses epoll, the most efficient and scalable socket management tool available from the Mac API.

#ifndef _MACOS_SOCKET_MANAGER_H_
#define _MACOS_SOCKET_MANAGER_H_

#include <map>
#include <vector>
#include <memory>
#include <pthread.h>

#include "lwpa/lock.h"
#include "rdmnet/broker/socket_manager.h"

// The set of data allocated per-socket.
struct SocketData
{
  SocketData(rdmnet_conn_t conn_handle_in, lwpa_socket_t socket_in) : conn_handle(conn_handle_in), socket(socket_in) {}

  rdmnet_conn_t conn_handle{RDMNET_CONN_INVALID};
  int socket{-1};

  // Receive buffer for socket recv operations
  uint8_t recv_buf[RDMNET_RECV_DATA_MAX_SIZE];
};

// A class to manage RDMnet Broker sockets on Mac.
// This handles receiving data on all RDMnet client connections, using epoll for maximum
// performance. Sending on connections is done in the core Broker library through the lwpa
// interface. Other miscellaneous Broker socket operations like LLRP are also handled in the core
// library.
class MacBrokerSocketManager : public RDMnet::BrokerSocketManager
{
public:
  MacBrokerSocketManager() { lwpa_rwlock_create(&socket_lock_); }
  virtual ~MacBrokerSocketManager() { lwpa_rwlock_destroy(&socket_lock_); }

  // RDMnet::BrokerSocketManager interface
  bool Startup(RDMnet::BrokerSocketManagerNotify* notify) override;
  bool Shutdown() override;
  bool AddSocket(rdmnet_conn_t conn_handle, lwpa_socket_t socket) override;
  void RemoveSocket(rdmnet_conn_t conn_handle) override;

  // Callback functions called from worker threads
  void WorkerNotifySocketReadEvent(rdmnet_conn_t conn_handle);
  void WorkerNotifySocketBad(rdmnet_conn_t conn_handle);

  // Accessors
  bool keep_running() const { return !shutting_down_; }
  int kqueue_fd() const { return kqueue_fd_; }

private:
  bool shutting_down_{false};
  pthread_t thread_handle_;
  int kqueue_fd_{-1};

  // The set of sockets being managed.
  std::map<rdmnet_conn_t, std::unique_ptr<SocketData>> sockets_;
  lwpa_rwlock_t socket_lock_;

  // The callback instance
  RDMnet::BrokerSocketManagerNotify* notify_{nullptr};
};

#endif  // _MACOS_SOCKET_MANAGER_H_
