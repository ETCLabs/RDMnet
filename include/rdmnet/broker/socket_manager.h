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

/// \file rdmnet/broker/socket_manager.h
#ifndef _RDMNET_BROKER_SOCKET_MANAGER_H_
#define _RDMNET_BROKER_SOCKET_MANAGER_H_

#include "lwpa/socket.h"
#include "rdmnet/core/connection.h"

namespace RDMnet
{
class BrokerSocketManagerNotify
{
public:
  /// \brief Data was received on a socket.
  ///
  /// The data should be handled immediately - the socket manager keeps ownership of the data buffer
  /// and will reuse it when the callback finishes.
  ///
  /// \param[in] conn_handle The RDMnet connection handle on which data was received.
  /// \param[in] data Pointer to received data buffer.
  /// \param[in] data_size Size of received data buffer.
  virtual void SocketDataReceived(rdmnet_conn_t conn_handle, const uint8_t *data, size_t data_size) = 0;

  /// \brief A socket was closed remotely.
  ///
  /// The socket is no longer valid after this callback finishes. Do not call
  /// BrokerSocketManager::RemoveSocket() or any other API function from this callback as it is
  /// unnecessary and may cause a deadlock.
  ///
  /// \param[in] conn_handle The RDMnet connection handle for which the socket was closed.
  /// \param[in] graceful Whether the TCP connection was closed gracefully.
  virtual void SocketClosed(rdmnet_conn_t conn_handle, bool graceful) = 0;
};

class BrokerSocketManager
{
public:
  virtual bool Startup(BrokerSocketManagerNotify *notify) = 0;
  virtual bool Shutdown() = 0;

  virtual bool AddSocket(rdmnet_conn_t conn_handle, lwpa_socket_t sock) = 0;
  virtual void RemoveSocket(rdmnet_conn_t conn_handle) = 0;
};

};  // namespace RDMnet

#endif  // _RDMNET_BROKER_SOCKET_MANAGER_H_
