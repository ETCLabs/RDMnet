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

/**************************** Private constants ******************************/

// The amount of time we'll block until we get an accept
#define LISTEN_TIMEOUT_MS 200

/*************************** Function definitions ****************************/

static void listen_thread_fn(void* arg)
{
  ListenThread* lt = static_cast<ListenThread*>(arg);
  if (lt)
  {
    lt->Run();
  }
}

ListenThread::ListenThread(etcpal_socket_t listen_sock, ListenThreadNotify* pnotify, rdmnet::BrokerLog* log)
    : notify_(pnotify), listen_socket_(listen_sock), log_(log)
{
}

ListenThread::~ListenThread()
{
  Stop();
}

bool ListenThread::Start()
{
  if (listen_socket_ == ETCPAL_SOCKET_INVALID)
    return false;

  terminated_ = false;
  EtcPalThreadParams tparams = {ETCPAL_THREAD_DEFAULT_PRIORITY, ETCPAL_THREAD_DEFAULT_STACK, "ListenThread", NULL};
  if (!etcpal_thread_create(&thread_handle_, &tparams, listen_thread_fn, this))
  {
    etcpal_close(listen_socket_);
    listen_socket_ = ETCPAL_SOCKET_INVALID;
    if (log_)
      log_->Log(ETCPAL_LOG_ERR, "ListenThread: Failed to start thread.");
    return false;
  }

  return true;
}

// Destroys the listening socket.
void ListenThread::Stop()
{
  if (!terminated_)
  {
    terminated_ = true;

    if (listen_socket_ != ETCPAL_SOCKET_INVALID)
    {
      etcpal_shutdown(listen_socket_, ETCPAL_SHUT_RD);
      etcpal_close(listen_socket_);
      listen_socket_ = ETCPAL_SOCKET_INVALID;
    }

    etcpal_thread_join(&thread_handle_);
  }
}

void ListenThread::Run()
{
  // Wait on our listening thread for new sockets or timeout. Since we heavily block on the accept,
  // we'll keep accepting as long as the listen socket is valid.
  while (!terminated_)
  {
    if (listen_socket_ != ETCPAL_SOCKET_INVALID)
    {
      etcpal_socket_t conn_sock;
      EtcPalSockaddr new_addr;

      etcpal_error_t err = etcpal_accept(listen_socket_, &new_addr, &conn_sock);

      if (err != kEtcPalErrOk)
      {
        // If terminated_ is set, the socket has been closed because the thread is being stopped
        // externally. Otherwise, it's a real error.
        if (!terminated_)
        {
          if (log_)
            log_->Log(ETCPAL_LOG_ERR, "ListenThread: Accept failed with error: %s.", etcpal_strerror(err));
          terminated_ = true;
        }
        return;
      }

      bool keep_socket = false;
      if (notify_)
        keep_socket = notify_->NewConnection(conn_sock, new_addr);
      if (!keep_socket)
        etcpal_close(conn_sock);
    }
    else
    {
      etcpal_thread_sleep(10);
    }
  }
}

/************************************/

static void controller_device_service_thread_fn(void* arg)
{
  ClientServiceThread* cdt = static_cast<ClientServiceThread*>(arg);
  if (cdt)
    cdt->Run();
}

ClientServiceThread::ClientServiceThread(unsigned int sleep_ms)
    : terminated_(true), sleep_ms_(sleep_ms), notify_(nullptr)
{
}

ClientServiceThread::~ClientServiceThread()
{
  Stop();
}

bool ClientServiceThread::Start()
{
  terminated_ = false;
  EtcPalThreadParams tparams = {ETCPAL_THREAD_DEFAULT_PRIORITY, ETCPAL_THREAD_DEFAULT_STACK, "ClientServiceThread",
                                NULL};
  return etcpal_thread_create(&thread_handle_, &tparams, controller_device_service_thread_fn, this);
}

void ClientServiceThread::Stop()
{
  if (!terminated_)
  {
    terminated_ = true;
    etcpal_thread_join(&thread_handle_);
  }
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
    etcpal_thread_sleep(sleep_ms_);
  }
}

/************************************/
