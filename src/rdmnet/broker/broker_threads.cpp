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

#include "broker_threads.h"

#include "etcpal/socket.h"
#include "rdmnet/core/connection.h"
#include "broker_util.h"

/*************************** Function definitions ****************************/

extern "C" static void listen_thread_fn(void* arg)
{
  ListenThread* lt = static_cast<ListenThread*>(arg);
  if (lt)
  {
    lt->Run();
  }
}

bool ListenThread::Start()
{
  if (socket_ == ETCPAL_SOCKET_INVALID)
    return false;

  terminated_ = false;
  EtcPalThreadParams tparams = {ETCPAL_THREAD_DEFAULT_PRIORITY, ETCPAL_THREAD_DEFAULT_STACK, "ListenThread", NULL};
  if (!etcpal_thread_create(&handle_, &tparams, listen_thread_fn, this))
  {
    terminated_ = true;
    etcpal_close(socket_);
    socket_ = ETCPAL_SOCKET_INVALID;
    if (log_)
      log_->Critical("ListenThread: Failed to start thread.");
    return false;
  }

  return true;
}

void ListenThread::Run()
{
  // Wait on our listening thread for new sockets or timeout. Since we heavily block on the accept,
  // we'll keep accepting as long as the listen socket is valid.
  while (!terminated_)
  {
    ReadSocket();
  }
}

void ListenThread::ReadSocket()
{
  if (socket_ != ETCPAL_SOCKET_INVALID)
  {
    etcpal_socket_t conn_sock;
    EtcPalSockaddr new_addr;

    etcpal_error_t err = etcpal_accept(socket_, &new_addr, &conn_sock);

    if (err != kEtcPalErrOk)
    {
      // If terminated_ is set, the socket has been closed because the thread is being stopped
      // externally. Otherwise, it's a real error.
      if (!terminated_)
      {
        if (log_)
          log_->Critical("ListenThread: Accept failed with error: %s.", etcpal_strerror(err));
        terminated_ = true;
      }
      return;
    }

    bool keep_socket = false;
    if (notify_)
      keep_socket = notify_->HandleNewConnection(conn_sock, new_addr);
    if (!keep_socket)
      etcpal_close(conn_sock);
  }
  else
  {
    etcpal_thread_sleep(10);
  }
}

// Destroys the listening socket.
ListenThread::~ListenThread()
{
  if (!terminated_)
  {
    terminated_ = true;

    if (socket_ != ETCPAL_SOCKET_INVALID)
    {
      etcpal_shutdown(socket_, ETCPAL_SHUT_RD);
      etcpal_close(socket_);
      socket_ = ETCPAL_SOCKET_INVALID;
    }

    etcpal_thread_join(&handle_);
  }
}

extern "C" static void client_service_thread_fn(void* arg)
{
  ClientServiceThread* cst = static_cast<ClientServiceThread*>(arg);
  if (cst)
    cst->Run();
}

bool ClientServiceThread::Start()
{
  terminated_ = false;
  EtcPalThreadParams tparams = {ETCPAL_THREAD_DEFAULT_PRIORITY, ETCPAL_THREAD_DEFAULT_STACK, "ClientServiceThread",
                                NULL};
  return etcpal_thread_create(&handle_, &tparams, client_service_thread_fn, this);
}

void ClientServiceThread::Run()
{
  if (!notify_)
    return;

  while (!terminated_)
  {
    // As long as clients need to be processed, we won't sleep.
    while (notify_->ServiceClients())
      ;
    etcpal_thread_sleep(kSleepMs);
  }
}

ClientServiceThread::~ClientServiceThread()
{
  if (!terminated_)
  {
    terminated_ = true;
    etcpal_thread_join(&handle_);
  }
}

bool BrokerThreadManager::AddListenThread(etcpal_socket_t listen_sock)
{
  auto new_thread = std::make_unique<ListenThread>(listen_sock, notify_, log_);
  if (new_thread->Start())
  {
    threads_.push_back(std::move(new_thread));
    return true;
  }
  else
  {
    return false;
  }
}

bool BrokerThreadManager::AddClientServiceThread()
{
  auto new_thread = std::make_unique<ClientServiceThread>(notify_);
  if (new_thread->Start())
  {
    threads_.push_back(std::move(new_thread));
    return true;
  }
  else
  {
    return false;
  }
}

void BrokerThreadManager::StopThreads()
{
  threads_.clear();
}
