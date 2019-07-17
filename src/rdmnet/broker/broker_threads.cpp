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

#include "broker_threads.h"

#include "lwpa/socket.h"
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

ListenThread::ListenThread(lwpa_socket_t listen_sock, ListenThreadNotify* pnotify, RDMnet::BrokerLog* log)
    : notify_(pnotify), listen_socket_(listen_sock), log_(log)
{
}

ListenThread::~ListenThread()
{
  Stop();
}

bool ListenThread::Start()
{
  if (listen_socket_ == LWPA_SOCKET_INVALID)
    return false;

  terminated_ = false;
  LwpaThreadParams tparams = {LWPA_THREAD_DEFAULT_PRIORITY, LWPA_THREAD_DEFAULT_STACK, "ListenThread", NULL};
  if (!lwpa_thread_create(&thread_handle_, &tparams, listen_thread_fn, this))
  {
    lwpa_close(listen_socket_);
    listen_socket_ = LWPA_SOCKET_INVALID;
    if (log_)
      log_->Log(LWPA_LOG_ERR, "ListenThread: Failed to start thread.");
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

    if (listen_socket_ != LWPA_SOCKET_INVALID)
    {
      lwpa_shutdown(listen_socket_, LWPA_SHUT_RD);
      lwpa_close(listen_socket_);
      listen_socket_ = LWPA_SOCKET_INVALID;
    }

    lwpa_thread_join(&thread_handle_);
  }
}

void ListenThread::Run()
{
  // Wait on our listening thread for new sockets or timeout. Since we heavily block on the accept,
  // we'll keep accepting as long as the listen socket is valid.
  while (!terminated_)
  {
    if (listen_socket_ != LWPA_SOCKET_INVALID)
    {
      lwpa_socket_t conn_sock;
      LwpaSockaddr new_addr;

      lwpa_error_t err = lwpa_accept(listen_socket_, &new_addr, &conn_sock);

      if (err != kLwpaErrOk)
      {
        // If terminated_ is set, the socket has been closed because the thread is being stopped
        // externally. Otherwise, it's a real error.
        if (!terminated_)
        {
          if (log_)
            log_->Log(LWPA_LOG_ERR, "ListenThread: Accept failed with error: %s.", lwpa_strerror(err));
          terminated_ = true;
        }
        return;
      }

      bool keep_socket = false;
      if (notify_)
        keep_socket = notify_->NewConnection(conn_sock, new_addr);
      if (!keep_socket)
        lwpa_close(conn_sock);
    }
    else
    {
      lwpa_thread_sleep(10);
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
  LwpaThreadParams tparams = {LWPA_THREAD_DEFAULT_PRIORITY, LWPA_THREAD_DEFAULT_STACK, "ClientServiceThread", NULL};
  return lwpa_thread_create(&thread_handle_, &tparams, controller_device_service_thread_fn, this);
}

void ClientServiceThread::Stop()
{
  if (!terminated_)
  {
    terminated_ = true;
    lwpa_thread_join(&thread_handle_);
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
    lwpa_thread_sleep(sleep_ms_);
  }
}

/************************************/
