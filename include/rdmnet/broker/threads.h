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

/*! \file broker/threads.h
 * \brief Classes to represent threads used by the Broker.
 * \author Nick Ballhorn-Wagner and Sam Kearney
 */
#ifndef _BROKER_THREADS_H_
#define _BROKER_THREADS_H_

#include <vector>
#include <deque>
#include <memory>
#include "lwpa_thread.h"
#include "lwpa_lock.h"
#include "lwpa_inet.h"
#include "lwpa_socket.h"
#include "rdmnet/common/connection.h"
#include "rdmnet/broker/util.h"

// The interface for the listener callback.
class IListenThread_Notify
{
public:
  // Called when the listen thread gets a new connection.  If you return false,
  // the connection is severed. Presumably you were just about to stop the
  // listener threads anyway...
  // The address & port fields of addr are used.
  // Do NOT stop the listening thread in this callback!
  virtual bool NewConnection(lwpa_socket_t new_sock, const LwpaSockaddr &remote_addr) = 0;

  // Called to log an error. You may want to stop the listening thread if
  // errors keep occurring, but you should NOT do it in this callback!
  virtual void LogError(const std::string &err) = 0;
};

// Listens for TCP connections
class ListenThread
{
public:
  ListenThread(const LwpaSockaddr &listen_addr, IListenThread_Notify *pnotify);
  virtual ~ListenThread();

  // Creates the listening socket and starts the thread.
  bool Start();

  // Destroys the listening socket and stops the thread.
  void Stop();

  // Returns the address and port we were requested to listen to (not the bound
  // port)
  LwpaSockaddr GetAddr() const { return addr_; }

  // The thread function
  void Run();

protected:
  LwpaSockaddr addr_;
  bool terminated_;
  IListenThread_Notify *notify_;

  lwpa_thread_t thread_handle_;
  lwpa_socket_t listen_socket_;
};

/************************************/

class IConnPollThread_Notify
{
public:
  virtual void PollConnections(const std::vector<int> &conn_handles, RdmnetPoll *poll_arr) = 0;
};

// Used to poll RDMnet connections for incoming data.
// You can add & remove as needed.
class ConnPollThread
{
public:
  ConnPollThread(size_t max_sockets, IConnPollThread_Notify *pnotify);
  virtual ~ConnPollThread();

  bool Start();
  void Stop();

  bool AddConnection(int conn);
  size_t RemoveConnection(int conn);

  void Run();

protected:
  bool terminated_;
  lwpa_thread_t thread_handle_;
  size_t max_count_;
  IConnPollThread_Notify *notify_;

  mutable lwpa_rwlock_t conn_lock_;
  std::vector<int> conns_;
  std::shared_ptr<std::vector<RdmnetPoll>> poll_arr_;
};

/************************************/

class IClientServiceThread_Notify
{
public:
  // Process each client queue, sending out the next message from each queue if
  // clients are available. Return false if no messages or partial messages
  // were sent.
  virtual bool ServiceClients() = 0;
};

// This is the thread that processes the controller queues and device states
class ClientServiceThread
{
public:
  explicit ClientServiceThread(unsigned int sleep_ms);
  virtual ~ClientServiceThread();

  bool Start();
  void Stop();

  void SetNotify(IClientServiceThread_Notify *pnotify) { notify_ = pnotify; }

  void Run();

protected:
  bool terminated_;
  lwpa_thread_t thread_handle_;
  int sleep_ms_;
  IClientServiceThread_Notify *notify_;
};

#endif  // _BROKER_THREADS_H_
