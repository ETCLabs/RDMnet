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

// Linux override of BrokerSocketManager.
// Uses epoll, currently the most efficient and scalable socket management tool available from the
// Linux API.

#ifndef LINUX_SOCKET_MANAGER_H_
#define LINUX_SOCKET_MANAGER_H_

#include <map>
#include <vector>
#include <memory>
#include <pthread.h>

#include "etcpal/cpp/lock.h"
#include "rdmnet/core/msg_buf.h"
#include "broker_socket_manager.h"

// Wrapper around Linux thread functions to increase the testability of this module.
// TODO on Linux

// class LinuxThreadInterface
//{
// public:
//  virtual HANDLE StartThread(_beginthreadex_proc_type start_address, void* arg_list) = 0;
//  virtual DWORD WaitForThreadsCompletion(DWORD count, const HANDLE* handle_arr, BOOL wait_all, DWORD milliseconds) =
//  0; virtual BOOL CleanupThread(HANDLE thread_handle) = 0;
//};
//
// class DefaultLinuxThreads : public LinuxThreadInterface
//{
// public:
//  virtual HANDLE StartThread(_beginthreadex_proc_type start_address, void* arg_list) override
//  {
//    return reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, start_address, arg_list, 0, NULL));
//  }
//  virtual DWORD WaitForThreadsCompletion(DWORD count, const HANDLE* handle_arr, BOOL wait_all, DWORD milliseconds)
//  {
//    return WaitForMultipleObjects(count, handle_arr, wait_all, milliseconds);
//  }
//  virtual BOOL CleanupThread(HANDLE thread_handle) override { return CloseHandle(thread_handle); }
//};

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

// A class to manage RDMnet Broker sockets on Linux.
// This handles receiving data on all RDMnet client connections, using epoll for maximum
// performance. Sending on connections is done in the core Broker library through the EtcPal
// interface. Other miscellaneous Broker socket operations like LLRP are also handled in the core
// library.
class LinuxBrokerSocketManager : public BrokerSocketManager
{
public:
  LinuxBrokerSocketManager(/* LinuxThreadInterface* thread_interface = new DefaultLinuxThreads */)
  //: thread_interface_(thread_interface)
  {
  }
  virtual ~LinuxBrokerSocketManager() = default;

  // BrokerSocketManager interface
  bool Startup() override;
  bool Shutdown() override;
  void SetNotify(BrokerSocketNotify* notify) override { notify_ = notify; }
  bool AddSocket(BrokerClient::Handle client_handle, etcpal_socket_t socket) override;
  void RemoveSocket(BrokerClient::Handle client_handle) override;

  // Callback functions called from worker threads
  void WorkerNotifySocketReadEvent(BrokerClient::Handle client_handle);
  void WorkerNotifySocketBad(BrokerClient::Handle client_handle);

  // Accessors
  bool keep_running() const { return !shutting_down_; }
  int  epoll_fd() const { return epoll_fd_; }

private:
  bool      shutting_down_{false};
  pthread_t thread_handle_;
  int       epoll_fd_{-1};
  // std::unique_ptr<LinuxThreadInterface> thread_interface_;

  // The set of sockets being managed.
  std::map<BrokerClient::Handle, std::unique_ptr<SocketData>> sockets_;
  etcpal::Mutex                                               socket_lock_;

  // The callback instance
  BrokerSocketNotify* notify_{nullptr};
};

#endif  // LINUX_SOCKET_MANAGER_H_
