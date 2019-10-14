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

// Windows override of BrokerSocketManager.
// Uses Windows I/O completion ports, currently the most efficient and scalable socket management
// tool available from the Windows API.

#ifndef WIN_SOCKET_MANAGER_H_
#define WIN_SOCKET_MANAGER_H_

#include <winsock2.h>
#include <windows.h>
#include <process.h>

#include <map>
#include <vector>
#include <memory>

#include "etcpal/cpp/lock.h"
#include "broker_socket_manager.h"

// Wrapper around Windows thread functions to increase the testability of this module.
class WindowsThreadInterface
{
public:
  virtual HANDLE StartThread(_beginthreadex_proc_type start_address, void* arg_list) = 0;
  virtual DWORD WaitForThreadsCompletion(DWORD count, const HANDLE* handle_arr, BOOL wait_all, DWORD milliseconds) = 0;
  virtual BOOL CleanupThread(HANDLE thread_handle) = 0;
};

class DefaultWindowsThreads : public WindowsThreadInterface
{
public:
  virtual HANDLE StartThread(_beginthreadex_proc_type start_address, void* arg_list) override
  {
    return reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, start_address, arg_list, 0, NULL));
  }
  virtual DWORD WaitForThreadsCompletion(DWORD count, const HANDLE* handle_arr, BOOL wait_all, DWORD milliseconds)
  {
    return WaitForMultipleObjects(count, handle_arr, wait_all, milliseconds);
  }
  virtual BOOL CleanupThread(HANDLE thread_handle) override { return CloseHandle(thread_handle); }
};

// The set of data allocated per-socket.
struct SocketData
{
  SocketData(rdmnet_conn_t conn_handle_in, etcpal_socket_t socket_in) : conn_handle(conn_handle_in), socket(socket_in)
  {
    ws_recv_buf.buf = reinterpret_cast<char*>(&recv_buf);
    ws_recv_buf.len = RDMNET_RECV_DATA_MAX_SIZE;
  }

  rdmnet_conn_t conn_handle{RDMNET_CONN_INVALID};
  WSAOVERLAPPED overlapped{};
  SOCKET socket{INVALID_SOCKET};
  bool close_requested{false};

  // Socket receive data
  WSABUF ws_recv_buf;  // The variable Winsock uses for receive buffers
  // Receive buffer for socket recv operations
  uint8_t recv_buf[RDMNET_RECV_DATA_MAX_SIZE];
};

// A class to manage RDMnet Broker sockets on Windows.
// This handles receiving data on all RDMnet client connections, using I/O completion ports for
// maximum performance. Sending on connections is done in the core Broker library through the EtcPal
// interface. Other miscellaneous Broker socket operations like LLRP are also handled in the core
// library.
class WinBrokerSocketManager : public BrokerSocketManager
{
public:
  WinBrokerSocketManager(WindowsThreadInterface* thread_interface = new DefaultWindowsThreads)
      : thread_interface_(thread_interface)
  {
  }
  virtual ~WinBrokerSocketManager() = default;

  // rdmnet::BrokerSocketManager interface
  bool Startup() override;
  bool Shutdown() override;
  void SetNotify(BrokerSocketNotify* notify) override { notify_ = notify; }
  bool AddSocket(rdmnet_conn_t conn_handle, etcpal_socket_t socket) override;
  void RemoveSocket(rdmnet_conn_t conn_handle) override;

  // Callback functions called from worker threads
  void WorkerNotifyRecvData(rdmnet_conn_t conn_handle, size_t size);
  void WorkerNotifySocketBad(rdmnet_conn_t conn_handle, bool graceful);

  // Accessors
  HANDLE iocp() const { return iocp_; }

private:
  bool shutting_down_{false};

  // Thread pool management
  HANDLE iocp_{nullptr};
  std::vector<HANDLE> worker_threads_;
  std::unique_ptr<WindowsThreadInterface> thread_interface_;

  // The set of sockets being managed.
  std::map<rdmnet_conn_t, std::unique_ptr<SocketData>> sockets_;
  etcpal::RwLock socket_lock_;

  // The callback instance
  BrokerSocketNotify* notify_{nullptr};
};

#endif  // WIN_SOCKET_MANAGER_H_
