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

// Linux override of RDMnet::BrokerSocketManager.
// Uses epoll, the most efficient and scalable socket management tool available from the Linux API.

#ifndef _LINUX_SOCKET_MANAGER_H_
#define _LINUX_SOCKET_MANAGER_H_

#include <map>
#include <vector>
#include <memory>
#include <pthread.h>

#include "lwpa/lock.h"
#include "rdmnet/broker/socket_manager.h"

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
  SocketData(rdmnet_conn_t conn_handle_in, lwpa_socket_t socket_in) : conn_handle(conn_handle_in), socket(socket_in) {}

  rdmnet_conn_t conn_handle{RDMNET_CONN_INVALID};
  int socket{-1};

  // Receive buffer for socket recv operations
  uint8_t recv_buf[RDMNET_RECV_DATA_MAX_SIZE];
};

// A class to manage RDMnet Broker sockets on Linux.
// This handles receiving data on all RDMnet client connections, using epoll for maximum
// performance. Sending on connections is done in the core Broker library through the lwpa
// interface. Other miscellaneous Broker socket operations like LLRP are also handled in the core
// library.
class LinuxBrokerSocketManager : public RDMnet::BrokerSocketManager
{
public:
  LinuxBrokerSocketManager(/* LinuxThreadInterface* thread_interface = new DefaultLinuxThreads */)
  //: thread_interface_(thread_interface)
  {
    lwpa_rwlock_create(&socket_lock_);
  }
  virtual ~LinuxBrokerSocketManager() { lwpa_rwlock_destroy(&socket_lock_); }

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
  int epoll_fd() const { return epoll_fd_; }

private:
  bool shutting_down_{false};
  pthread_t thread_handle_;
  int epoll_fd_{-1};
  // std::unique_ptr<LinuxThreadInterface> thread_interface_;

  // The set of sockets being managed.
  std::map<rdmnet_conn_t, std::unique_ptr<SocketData>> sockets_;
  lwpa_rwlock_t socket_lock_;

  // The callback instance
  RDMnet::BrokerSocketManagerNotify* notify_{nullptr};
};

#endif  // _LINUX_SOCKET_MANAGER_H_