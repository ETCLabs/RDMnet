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

#include "broker/threads.h"

#include <map>
#include "lwpa_socket.h"
#include "rdmnet/connection.h"
#include "broker/util.h"

/**************************** Private constants ******************************/

// The amount of time we'll block until we get an accept
#define LISTEN_TIMEOUT_MS 200

/*************************** Function definitions ****************************/

static void listen_thread_fn(void *arg)
{
  ListenThread *lt = static_cast<ListenThread *>(arg);
  if (lt)
  {
    lt->Run();
  }
}

ListenThread::ListenThread(const LwpaSockaddr &listen_addr, IListenThread_Notify *pnotify)
    : addr_(listen_addr), terminated_(true), notify_(pnotify), listen_socket_(LWPA_SOCKET_INVALID)
{
}

ListenThread::~ListenThread()
{
  Stop();
}

// Creates the listening socket.
bool ListenThread::Start()
{
  if (listen_socket_ != LWPA_SOCKET_INVALID)
    return false;

  listen_socket_ = lwpa_socket(lwpaip_is_v4(&addr_.ip) ? LWPA_AF_INET : LWPA_AF_INET6, LWPA_STREAM);
  if (listen_socket_ == LWPA_SOCKET_INVALID)
  {
    if (notify_)
      notify_->LogError("ListenThread: Failed to create listen socket.");
    return false;
  }

  lwpa_error_t err = lwpa_bind(listen_socket_, &addr_);
  if (err != LWPA_OK)
  {
    lwpa_close(listen_socket_);
    listen_socket_ = LWPA_SOCKET_INVALID;
    if (notify_)
    {
      char addrstr[LWPA_INET6_ADDRSTRLEN];
      lwpa_inet_ntop(&addr_.ip, addrstr, LWPA_INET6_ADDRSTRLEN);
      notify_->LogError("ListenThread: Bind to " + std::string(addrstr) +
                        " failed on listen socket with error: " + std::string(lwpa_strerror(err)) + ".");
    }
    return false;
  }

  err = lwpa_listen(listen_socket_, 0);
  if (err != LWPA_OK)
  {
    lwpa_close(listen_socket_);
    listen_socket_ = LWPA_SOCKET_INVALID;
    if (notify_)
      notify_->LogError(
          "ListenThread: Listen failed on listen socket with "
          "error: " +
          std::string(lwpa_strerror(err)) + ".");
    return false;
  }

  terminated_ = false;
  LwpaThreadParams tparams = {LWPA_THREAD_DEFAULT_PRIORITY, LWPA_THREAD_DEFAULT_STACK, "ListenThread", NULL};
  if (!lwpa_thread_create(&thread_handle_, &tparams, listen_thread_fn, this))
  {
    lwpa_close(listen_socket_);
    listen_socket_ = LWPA_SOCKET_INVALID;
    if (notify_)
      notify_->LogError("ListenThread: Failed to start thread.");
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
    lwpa_thread_stop(&thread_handle_, 10000);

    if (listen_socket_ != LWPA_SOCKET_INVALID)
    {
      lwpa_close(listen_socket_);
      listen_socket_ = LWPA_SOCKET_INVALID;
    }
  }
}

void ListenThread::Run()
{
  // Wait on our listening thread for new sockets or timeout. Since we heavily
  // block on the accept, we'll keep accepting as long as the listen socket is
  // valid.
  while (!terminated_)
  {
    if (listen_socket_ != LWPA_SOCKET_INVALID)
    {
      lwpa_socket_t conn_sock;
      LwpaSockaddr new_addr;
      std::string error_str;

      lwpa_error_t err = lwpa_accept(listen_socket_, &new_addr, &conn_sock);

      if (err != LWPA_OK)
      {
        notify_->LogError("ListenThread: Accept failed with error: " + std::string(lwpa_strerror(err)) + ".");
        terminated_ = true;
        return;
      }

      bool keep_socket = false;
      if (notify_)
        keep_socket = notify_->NewConnection(conn_sock, new_addr);
      if (!keep_socket)
        lwpa_close(conn_sock);
    }
    else
      lwpa_thread_sleep(10);
  }
}

/************************************/

static void conn_poll_thread_fn(void *arg)
{
  ConnPollThread *cpt = static_cast<ConnPollThread *>(arg);
  if (cpt)
    cpt->Run();
}

ConnPollThread::ConnPollThread(size_t max_sockets, IConnPollThread_Notify *pnotify)
    : terminated_(true), max_count_(max_sockets), notify_(pnotify)
{
  lwpa_rwlock_create(&conn_lock_);
}

ConnPollThread::~ConnPollThread()
{
  Stop();
  lwpa_rwlock_destroy(&conn_lock_);
}

bool ConnPollThread::Start()
{
  terminated_ = false;
  LwpaThreadParams tparams = {LWPA_THREAD_DEFAULT_PRIORITY, LWPA_THREAD_DEFAULT_STACK, "ConnPollThread", NULL};
  return lwpa_thread_create(&thread_handle_, &tparams, conn_poll_thread_fn, this);
}

void ConnPollThread::Stop()
{
  if (!terminated_)
  {
    terminated_ = true;
    lwpa_thread_stop(&thread_handle_, 10000);
  }
}

// Use this to add a connection to the next run of rdmnet_poll().
// Returns false if the maximum count has already been reached
bool ConnPollThread::AddConnection(int conn)
{
  BrokerWriteGuard conns_write(conn_lock_);
  if (conns_.size() < max_count_)
  {
    conns_.push_back(conn);
    if (!poll_arr_ || conns_.size() > poll_arr_->size())
    {
      poll_arr_ = std::make_shared<std::vector<RdmnetPoll>>(conns_.size());
    }
    return true;
  }
  return false;
}

// Use this to remove a socket from the next run of rdmnet_poll().
// Returns the number of sockets left in the list, and whether or not
// the cookie was in this object.
// If there are no sockets left, this thread should probably be shut down.
size_t ConnPollThread::RemoveConnection(int conn)
{
  BrokerWriteGuard conns_write(conn_lock_);
  for (auto it = conns_.begin(); it != conns_.end(); ++it)
  {
    if (*it == conn)
    {
      conns_.erase(it);
      break;
    }
  }
  return conns_.size();
}

// The thread function
void ConnPollThread::Run()
{
  if (!notify_)
    return;

  // Because select does our sleeping for us, there is no need to sleep as
  // long as we have sockets.
  while (!terminated_)
  {
    bool do_poll = false;
    std::vector<int> conn_snapshot;
    std::shared_ptr<std::vector<RdmnetPoll>> poll_arr_snapshot;

    {
      BrokerReadGuard conns_read(conn_lock_);
      do_poll = (conns_.size() != 0);
      conn_snapshot = conns_;
      poll_arr_snapshot = poll_arr_;
    }

    if (do_poll)
      notify_->PollConnections(conn_snapshot, poll_arr_snapshot->data());
    else
      lwpa_thread_sleep(50);
  }
}

/************************************/

static void controller_device_service_thread_fn(void *arg)
{
  ClientServiceThread *cdt = static_cast<ClientServiceThread *>(arg);
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
    lwpa_thread_stop(&thread_handle_, 10000);
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
