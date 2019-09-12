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

// epoll() is a scalabile mechanism for watching many file descriptors (including sockets) in the
// Linux kernel. For this app, we use a single thread polling all of the currently-open sockets.
//
// Further reading:
// "man epoll" from a Linux distribution command line
// https://linux.die.net/man/4/epoll

#include "linux_socket_manager.h"

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr int kMaxEvents = 100;
constexpr int kEpollTimeout = 200;

// Function for the worker thread which does all the socket reading.
void* SocketWorkerThread(void* arg)
{
  LinuxBrokerSocketManager* sock_mgr = reinterpret_cast<LinuxBrokerSocketManager*>(arg);
  if (!sock_mgr)
    return reinterpret_cast<void*>(1);

  auto events = std::make_unique<struct epoll_event[]>(kMaxEvents);

  while (sock_mgr->keep_running())
  {
    int epoll_result = epoll_wait(sock_mgr->epoll_fd(), events.get(), kMaxEvents, kEpollTimeout);
    for (int i = 0; i < epoll_result && sock_mgr->keep_running(); ++i)
    {
      if (events[i].events & EPOLLERR)
      {
        // Notify that this socket is bad
        sock_mgr->WorkerNotifySocketBad(events[i].data.fd);
      }
      else if (events[i].events & EPOLLIN)
      {
        // Do the read on the socket
        sock_mgr->WorkerNotifySocketReadEvent(events[i].data.fd);
      }
    }
  }
  return reinterpret_cast<void*>(0);
}

bool LinuxBrokerSocketManager::Startup(BrokerSocketManagerNotify* notify)
{
  notify_ = notify;

  // Per the man page, the size argument is ignored but must be greater than zero. Random value was
  // chosen
  epoll_fd_ = epoll_create(42);
  if (epoll_fd_ < 0)
    return false;

  if (0 != pthread_create(&thread_handle_, NULL, SocketWorkerThread, this))
  {
    close(epoll_fd_);
    epoll_fd_ = -1;
    return false;
  }

  return true;
}

bool LinuxBrokerSocketManager::Shutdown()
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

  if (epoll_fd_ >= 0)
    close(epoll_fd_);

  // Shutdown the worker thread
  pthread_join(thread_handle_, NULL);

  {  // Write lock scope
    etcpal::WriteGuard socket_write(socket_lock_);
    sockets_.clear();
  }

  return true;
}

bool LinuxBrokerSocketManager::AddSocket(rdmnet_conn_t conn_handle, etcpal_socket_t socket)
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
      struct epoll_event new_event;
      new_event.events = EPOLLIN;
      new_event.data.fd = conn_handle;
      if (0 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket, &new_event))
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

void LinuxBrokerSocketManager::RemoveSocket(rdmnet_conn_t conn_handle)
{
  etcpal::ReadGuard socket_write(socket_lock_);

  auto sock_data = sockets_.find(conn_handle);
  if (sock_data != sockets_.end())
  {
    // Per the epoll man page, deregister is not necessary before closing the socket.
    shutdown(sock_data->second->socket, SHUT_RDWR);
    close(sock_data->second->socket);
    sockets_.erase(sock_data);
  }
}

void LinuxBrokerSocketManager::WorkerNotifySocketBad(rdmnet_conn_t conn_handle)
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

void LinuxBrokerSocketManager::WorkerNotifySocketReadEvent(rdmnet_conn_t conn_handle)
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

// Instantiate a LinuxBrokerSocketManager
std::unique_ptr<BrokerSocketManager> CreateBrokerSocketManager()
{
  return std::make_unique<LinuxBrokerSocketManager>();
}
