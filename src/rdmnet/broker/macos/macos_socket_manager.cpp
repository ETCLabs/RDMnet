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

#include "macos_socket_manager.h"

#include <memory>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

constexpr int kMaxEvents = 100;
constexpr int kEventTimeout = 200;

// Function for the worker thread which does all the socket reading.
void* SocketWorkerThread(void* arg)
{
  MacBrokerSocketManager* sock_mgr = reinterpret_cast<MacBrokerSocketManager*>(arg);
  if (!sock_mgr)
    return reinterpret_cast<void*>(1);

  auto kevent_list = std::make_unique<struct kevent[]>(kMaxEvents);

  while (sock_mgr->keep_running())
  {
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = kEventTimeout * 1000;

    int kevent_result = kevent(sock_mgr->kqueue_fd(), NULL, 0, kevent_list.get(), kMaxEvents, &tv);
    for (int i = 0; i < kevent_result && sock_mgr->keep_running(); ++i)
    {
      if (kevent_list[i].filter == EVFILT_READ)
      {
        if (kevent_list[i].data > 0)
        {
          // Do the read on the socket
          sock_mgr->WorkerNotifySocketReadEvent(
              static_cast<rdmnet_conn_t>(reinterpret_cast<intptr_t>(kevent_list[i].udata)));
        }
        if (kevent_list[i].flags & EV_EOF)
        {
          // Notify that this socket is bad
          sock_mgr->WorkerNotifySocketBad(static_cast<rdmnet_conn_t>(reinterpret_cast<intptr_t>(kevent_list[i].udata)));
        }
      }
    }
  }
  return reinterpret_cast<void*>(0);
}

bool MacBrokerSocketManager::Startup(BrokerSocketManagerNotify* notify)
{
  notify_ = notify;

  kqueue_fd_ = kqueue();
  if (kqueue_fd_ < 0)
    return false;

  if (0 != pthread_create(&thread_handle_, NULL, SocketWorkerThread, this))
  {
    close(kqueue_fd_);
    kqueue_fd_ = -1;
    return false;
  }

  return true;
}

bool MacBrokerSocketManager::Shutdown()
{
  shutting_down_ = true;

  {  // Read lock scope
    etcpal::ReadGuard socket_read(socket_lock_);
    for (auto& sock_data : sockets_)
    {
      // Close each socket. Doesn't affect the epoll operation.
      close(sock_data.second->socket);
    }
  }

  if (kqueue_fd_ >= 0)
    close(kqueue_fd_);

  // Shutdown the worker thread
  pthread_join(thread_handle_, NULL);

  {  // Write lock scope
    etcpal::WriteGuard socket_write(socket_lock_);
    sockets_.clear();
  }

  return true;
}

bool MacBrokerSocketManager::AddSocket(rdmnet_conn_t conn_handle, etcpal_socket_t socket)
{
  etcpal::WriteGuard socket_write(socket_lock_);

  // Create the data structure for the new socket
  auto new_sock_data = std::make_unique<SocketData>(conn_handle, socket);
  if (new_sock_data)
  {
    // Add it to the socket map
    auto result = sockets_.insert(std::make_pair(conn_handle, std::move(new_sock_data)));
    if (result.second)
    {
      // Add the socket to our epoll fd
      struct kevent new_event;
      EV_SET(&new_event, socket, EVFILT_READ, EV_ADD, 0, 0, reinterpret_cast<void*>(conn_handle));

      if (0 == kevent(kqueue_fd_, &new_event, 1, NULL, 0, NULL))
      {
        return true;
      }
      else
      {
        sockets_.erase(conn_handle);
      }
    }
  }
  return false;
}

void MacBrokerSocketManager::RemoveSocket(rdmnet_conn_t conn_handle)
{
  etcpal::ReadGuard socket_write(socket_lock_);

  auto sock_data = sockets_.find(conn_handle);
  if (sock_data != sockets_.end())
  {
    // Per the kqueue man page, kevents associated with a closed descriptor are cleaned up
    // automatically.
    shutdown(sock_data->second->socket, SHUT_RDWR);
    close(sock_data->second->socket);
    sockets_.erase(sock_data);
  }
}

void MacBrokerSocketManager::WorkerNotifySocketBad(rdmnet_conn_t conn_handle)
{
  {  // Write lock scope
    etcpal::WriteGuard socket_write(socket_lock_);

    auto sock_data = sockets_.find(conn_handle);
    if (sock_data != sockets_.end())
    {
      close(sock_data->second->socket);
      sockets_.erase(sock_data);
    }
  }

  if (notify_)
    notify_->SocketClosed(conn_handle, false);
}

void MacBrokerSocketManager::WorkerNotifySocketReadEvent(rdmnet_conn_t conn_handle)
{
  etcpal::ReadGuard socket_read(socket_lock_);

  auto sock_data = sockets_.find(conn_handle);
  if (sock_data != sockets_.end())
  {
    ssize_t recv_result = recv(sock_data->second->socket, sock_data->second->recv_buf, RDMNET_RECV_DATA_MAX_SIZE, 0);
    if (recv_result <= 0)
    {
      // The socket was closed, either gracefully or ungracefully.
      close(sock_data->second->socket);
      sockets_.erase(sock_data);
      if (notify_)
        notify_->SocketClosed(conn_handle, (recv_result == 0));
    }
    else
    {
      notify_->SocketDataReceived(conn_handle, sock_data->second->recv_buf, recv_result);
    }
  }
}

std::unique_ptr<BrokerSocketManager> CreateBrokerSocketManager()
{
  return std::make_unique<MacBrokerSocketManager>();
}
