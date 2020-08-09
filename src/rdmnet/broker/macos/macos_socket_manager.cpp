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

#include "macos_socket_manager.h"

#include <algorithm>
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

  std::unique_ptr<struct kevent[]> kevent_list(new struct kevent[kMaxEvents]);

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
              static_cast<BrokerClient::Handle>(reinterpret_cast<intptr_t>(kevent_list[i].udata)));
        }
        if (kevent_list[i].flags & EV_EOF)
        {
          // Notify that this socket is bad
          sock_mgr->WorkerNotifySocketBad(
              static_cast<BrokerClient::Handle>(reinterpret_cast<intptr_t>(kevent_list[i].udata)));
        }
      }
    }
  }
  return reinterpret_cast<void*>(0);
}

bool MacBrokerSocketManager::Startup()
{
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
      shutdown(sock_data.second->socket, SHUT_RDWR);
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

bool MacBrokerSocketManager::AddSocket(BrokerClient::Handle client_handle, etcpal_socket_t socket)
{
  etcpal::WriteGuard socket_write(socket_lock_);

  // Create the data structure for the new socket
  std::unique_ptr<SocketData> new_sock_data(new SocketData(client_handle, socket));
  if (new_sock_data)
  {
    // Add it to the socket map
    auto result = sockets_.insert(std::make_pair(client_handle, std::move(new_sock_data)));
    if (result.second)
    {
      // Add the socket to our epoll fd
      struct kevent new_event;
      EV_SET(&new_event, socket, EVFILT_READ, EV_ADD, 0, 0, reinterpret_cast<void*>(client_handle));

      if (0 == kevent(kqueue_fd_, &new_event, 1, NULL, 0, NULL))
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

void MacBrokerSocketManager::RemoveSocket(BrokerClient::Handle client_handle)
{
  etcpal::ReadGuard socket_write(socket_lock_);

  auto sock_data = sockets_.find(client_handle);
  if (sock_data != sockets_.end())
  {
    // Per the kqueue man page, kevents associated with a closed descriptor are cleaned up
    // automatically.
    shutdown(sock_data->second->socket, SHUT_RDWR);
    close(sock_data->second->socket);
    sockets_.erase(sock_data);
  }
}

void MacBrokerSocketManager::WorkerNotifySocketBad(BrokerClient::Handle client_handle)
{
  {  // Write lock scope
    etcpal::WriteGuard socket_write(socket_lock_);

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

void MacBrokerSocketManager::WorkerNotifySocketReadEvent(BrokerClient::Handle client_handle)
{
  etcpal::ReadGuard socket_read(socket_lock_);

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

std::unique_ptr<BrokerSocketManager> CreateBrokerSocketManager()
{
  return std::unique_ptr<BrokerSocketManager>(new MacBrokerSocketManager);
}
