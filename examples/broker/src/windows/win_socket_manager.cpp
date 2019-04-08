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

// I/O completion ports use a pool of worker threads to process data from a separate pool of
// sockets. Each time there is activity on a socket, one of the threads waiting in the call to
// GetQueuedCompletionStatus() wakes up.
//
// The I/O completion port will not wake up a number of threads greater than its concurrency value,
// which is specified on creation. The default is the number of processors on the system.
// The Microsoft docs recommend keeping this default and using a pool of threads equal to twice this
// number to wait on the port. This is because more threads can run when one of the threads
// processing data enters a waiting state for another reason, e.g. sleeping or waiting on a mutex.
//
// Further reading:
// https://docs.microsoft.com/en-us/windows/desktop/fileio/i-o-completion-ports
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa364986(v=vs.85).aspx
// https://xania.org/200807/iocp

#include "win_socket_manager.h"

enum class MessageKey
{
  kNormalRecv,
  kStartRecv,
  kShutdown
};

// Function for the worker threads which make up the thread pool.
unsigned __stdcall SocketWorkerThread(void *arg)
{
  WinBrokerSocketManager *sock_mgr = reinterpret_cast<WinBrokerSocketManager *>(arg);
  if (!sock_mgr)
    return 1;

  bool keep_running = true;
  while (keep_running)
  {
    DWORD bytes_read = 0;
    ULONG_PTR cmd;
    OVERLAPPED *overlapped = nullptr;

    BOOL result = GetQueuedCompletionStatus(sock_mgr->iocp(), &bytes_read, &cmd, &overlapped, INFINITE);

    SocketData *sock_data = nullptr;
    if (overlapped)
      sock_data = CONTAINING_RECORD(overlapped, SocketData, overlapped);

    // The matrix of possible output parameter and return values from GetQueuedCompletionStatus()
    // indicate a number of possible conditions:
    // (thank you Matt Godbolt, https://xania.org/200807/iocp)
    //
    // result | overlapped | meaning
    // -------|------------|-----------------------------------------------------------------------
    //  FALSE |    null    | Call failed with no accompanying socket data. Usually indicates a bug
    //        |            | in usage of the function (invalid argument, etc.).
    // -------|------------|-----------------------------------------------------------------------
    //  FALSE |  non-null  | There is an error condition on a socket, e.g. ungraceful close.
    // -------|------------|-----------------------------------------------------------------------
    //  TRUE  |    null    | Usually a result of custom user messages sent using
    //        |            | PostQueuedCompletionStatus() which don't include OVERLAPPED structures.
    //        |            | Our app shouldn't encounter this as we always include an OVERLAPPED
    //        |            | struct with our custom messages.
    // -------|------------|-----------------------------------------------------------------------
    //  TRUE  |  non-null  | Non-error result of a previous overlapped operation on a socket. Handle
    //        |            | accordingly.
    if (!result)
    {
      if (!sock_data)
      {
        // Unlikely error case of error return with no socket reference. Should not happen if the
        // program is operating normally. Sleep to avoid busy loop.
        Sleep(10);
      }
      else
      {
        // Error occurred on the socket.
        sock_mgr->WorkerNotifySocketBad(sock_data->conn_handle, false);
      }
    }
    else
    {
      switch (static_cast<MessageKey>(cmd))
      {
        case MessageKey::kNormalRecv:
          if (!sock_data)
          {
            // Bad state combo, shouldn't get here. Sleep to avoid busy loop.
            Sleep(10);
            break;
          }
          else
          {
            if (bytes_read == 0)
            {
              sock_mgr->WorkerNotifySocketBad(sock_data->conn_handle, true);
              break;
            }
            else
            {
              sock_mgr->WorkerNotifyRecvData(sock_data->conn_handle);
              // Intentional fallthrough to start an overlapped receive operation again
            }
          }
        case MessageKey::kStartRecv:
          if (sock_data)
          {
            // Begin a new overlapped receive operation
            DWORD recv_flags = 0;
            int recv_result = WSARecv(sock_data->socket, &sock_data->ws_recv_buf, 1, nullptr, &recv_flags,
                                      &sock_data->overlapped, nullptr);
            // Check for errors. Otherwise, we will be notified asynchronously through the I/O
            // completion port.
            if (recv_result != 0 && WSA_IO_PENDING != WSAGetLastError())
            {
              sock_mgr->WorkerNotifySocketBad(sock_data->conn_handle, false);
            }
          }
          break;
        case MessageKey::kShutdown:
        default:
          // The thread has been signaled to shut down.
          keep_running = false;
          break;
      }
    }
  }
  return 0;
}

bool WinBrokerSocketManager::Startup(RDMnet::BrokerSocketManagerNotify *notify)
{
  bool ok = true;
  notify_ = notify;

  WSADATA wsadata;
  if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata))
  {
    return false;
  }

  iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  ok = (iocp_ != nullptr);

  if (ok)
  {
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    // Start up a number of worker threads equal to double the number of processors on the system.
    // This is the recommended number from the Microsoft docs.
    for (DWORD i = 0; i < info.dwNumberOfProcessors * 2; ++i)
    {
      HANDLE thread_handle = thread_interface_->StartThread(SocketWorkerThread, this);
      if (thread_handle != nullptr)
      {
        worker_threads_.push_back(thread_handle);
      }
      else
      {
        ok = false;
        break;
      }
    }
  }

  if (!ok)
    Shutdown();
  return ok;
}

bool WinBrokerSocketManager::Shutdown()
{
  // TODO
  return true;
}

bool WinBrokerSocketManager::AddSocket(rdmnet_conn_t conn_handle, lwpa_socket_t socket)
{
  lwpa::WriteGuard socket_write(socket_lock_);

  // Create the data structure for the new socket
  SocketData new_sock_data = SocketData(conn_handle, socket);
  // Add it to the socket map
  auto result = sockets_.insert(std::make_pair(conn_handle, new_sock_data));
  if (result.second)
  {
    // Add the socket to our I/O completion port
    if (NULL != CreateIoCompletionPort((HANDLE)socket, iocp_, static_cast<ULONG_PTR>(MessageKey::kNormalRecv), 0))
    {
      // Notify a worker thread to begin a receive operation
      if (PostQueuedCompletionStatus(iocp_, 0, static_cast<ULONG_PTR>(MessageKey::kStartRecv),
                                     &result.first->second.overlapped))
      {
        return true;
      }
      else
      {
        sockets_.erase(conn_handle);
      }
    }
    else
    {
      sockets_.erase(conn_handle);
    }
  }
  return false;
}

void WinBrokerSocketManager::RemoveSocket(rdmnet_conn_t conn_handle)
{
  lwpa::ReadGuard socket_read(socket_lock_);

  auto sock_data = sockets_.find(conn_handle);
  if (sock_data != sockets_.end())
  {
    if (!sock_data->second.close_requested)
    {
      sock_data->second.close_requested = true;
      // This will cause a worker to wake up and notify that the socket was closed,
      // triggering the rest of the destruction.
      closesocket(sock_data->second.socket);
    }
  }
}

void WinBrokerSocketManager::WorkerNotifySocketBad(rdmnet_conn_t conn_handle, bool graceful)
{
  bool notify_socket_closed = false;

  {  // Write lock scope
    lwpa::WriteGuard socket_write(socket_lock_);

    auto sock_data = sockets_.find(conn_handle);
    if (sock_data != sockets_.end())
    {
      if (!sock_data->second.close_requested)
      {
        notify_socket_closed = true;
      }
      closesocket(sock_data->second.socket);
      sockets_.erase(sock_data);
    }
  }

  if (notify_socket_closed && notify_)
    notify_->SocketClosed(conn_handle, graceful);
}

void WinBrokerSocketManager::WorkerNotifyRecvData(rdmnet_conn_t conn_handle)
{
  lwpa::ReadGuard socket_read(socket_lock_);

  auto sock_data = sockets_.find(conn_handle);
  if (sock_data != sockets_.end())
  {
    if (!sock_data->second.close_requested)
    {
    }
  }
}
