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

// epoll() is a scalabile mechanism for watching many file descriptors (including sockets) in the
// Linux kernel. For this app, we use a single thread polling all of the currently-open sockets.
//
// Further reading:
// "man epoll" from a Linux distribution command line
// https://linux.die.net/man/4/epoll

#include "linux_socket_manager.h"

#include <algorithm>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "rdmnet/core/message.h"

constexpr int kMaxEvents = 100;
constexpr int kEpollTimeout = 200;

// Function for the worker thread which does all the socket reading.
void* SocketWorkerThread(void* arg)
{
  LinuxBrokerSocketManager* sock_mgr = reinterpret_cast<LinuxBrokerSocketManager*>(arg);
  if (!sock_mgr)
    return reinterpret_cast<void*>(1);

  std::unique_ptr<struct epoll_event[]> events(new struct epoll_event[kMaxEvents]);

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

bool LinuxBrokerSocketManager::Startup()
{
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

  if (epoll_fd_ >= 0)
    close(epoll_fd_);

  // Shutdown the worker thread
  pthread_join(thread_handle_, NULL);

  etcpal::MutexGuard socket_guard(socket_lock_);
  for (auto& sock_data : sockets_)
    close(sock_data.second->socket);
  sockets_.clear();

  return true;
}

bool LinuxBrokerSocketManager::AddSocket(BrokerClient::Handle client_handle, etcpal_socket_t socket)
{
  etcpal::MutexGuard socket_guard(socket_lock_);

  // Create the data structure for the new socket
  std::unique_ptr<SocketData> new_sock_data(new SocketData(client_handle, socket));
  if (new_sock_data)
  {
    // Add it to the socket map
    auto result = sockets_.insert(std::make_pair(client_handle, std::move(new_sock_data)));
    if (result.second)
    {
      // Add the socket to our epoll fd
      struct epoll_event new_event;
      new_event.events = EPOLLIN;
      new_event.data.fd = client_handle;
      if (0 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket, &new_event))
      {
        return true;
      }
      else
      {
        sockets_.erase(client_handle);
      }
    }
  }
  return false;
}

void LinuxBrokerSocketManager::RemoveSocket(BrokerClient::Handle client_handle)
{
  etcpal::MutexGuard socket_guard(socket_lock_);

  auto sock_data = sockets_.find(client_handle);
  if (sock_data != sockets_.end())
  {
    // Per the epoll man page, deregister is not necessary before closing the socket.
    shutdown(sock_data->second->socket, SHUT_RDWR);
    close(sock_data->second->socket);
    sockets_.erase(sock_data);
  }
}

void LinuxBrokerSocketManager::WorkerNotifySocketBad(BrokerClient::Handle client_handle)
{
  {  // Lock scope
    etcpal::MutexGuard socket_guard(socket_lock_);

    auto sock_data = sockets_.find(client_handle);
    if (sock_data != sockets_.end())
    {
      close(sock_data->second->socket);
      sockets_.erase(sock_data);
    }
  }

  if (notify_)
    notify_->HandleSocketClosed(client_handle, false);
}

void LinuxBrokerSocketManager::WorkerNotifySocketReadEvent(BrokerClient::Handle client_handle)
{
  etcpal::MutexGuard socket_guard(socket_lock_);

  auto sock_data_iter = sockets_.find(client_handle);
  if (sock_data_iter != sockets_.end())
  {
    SocketData* sock_data = sock_data_iter->second.get();
    void*       recv_buf = &sock_data->recv_buf.buf[sock_data->recv_buf.cur_data_size];
    size_t      recv_buf_size =
        std::min<size_t>(RDMNET_RECV_DATA_MAX_SIZE, (RC_MSG_BUF_SIZE - sock_data->recv_buf.cur_data_size));

    ssize_t recv_result = recv(sock_data->socket, recv_buf, recv_buf_size, 0);
    if (recv_result <= 0)
    {
      // The socket was closed, either gracefully or ungracefully.
      close(sock_data->socket);
      sockets_.erase(sock_data_iter);
      if (notify_)
        notify_->HandleSocketClosed(client_handle, (recv_result == 0));
    }
    else
    {
      sock_data->recv_buf.cur_data_size += recv_result;
      etcpal_error_t res = rc_msg_buf_parse_data(&sock_data->recv_buf);
      while (res == kEtcPalErrOk)
      {
        if (notify_)
          notify_->HandleSocketMessageReceived(client_handle, sock_data->recv_buf.msg);
        rc_free_message_resources(&sock_data->recv_buf.msg);
        res = rc_msg_buf_parse_data(&sock_data->recv_buf);
      }
    }
  }
}

// Instantiate a LinuxBrokerSocketManager
std::unique_ptr<BrokerSocketManager> CreateBrokerSocketManager()
{
  return std::unique_ptr<BrokerSocketManager>(new LinuxBrokerSocketManager);
}
