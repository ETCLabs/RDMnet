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

#ifndef BROKER_SOCKET_MANAGER_H_
#define BROKER_SOCKET_MANAGER_H_

#include <memory>
#include "etcpal/socket.h"
#include "rdmnet/core/message.h"
#include "broker_client.h"

// The corresponding sources for this file are found in the platform-specific subfolders for each
// Broker platform.

/// @brief A status returned by HandleSocketMessageReceived.
///
/// This is used to determine if the worker thread should move on to the next message, or call
/// HandleSocketMessageReceived with the same message later (potentially throttling the TCP connection).
enum class HandleMessageResult
{
  kRetryLater,
  kGetNextMessage
};

class BrokerSocketNotify
{
public:
  /// @brief An RDMnet message was received on a socket.
  ///
  /// If the message can't be processed right now, set throttle to true to delay this message to a future notification.
  /// Otherwise, the data should be handled immediately. The socket manager keeps ownership of the message and will
  /// reuse it when the callback finishes.
  ///
  /// @param[in] handle The client handle on which data was received.
  /// @param[in] message The parsed message which was received on the socket.
  /// @return #kRetryLater: The message couldn't be processed and should be delayed to a future notification.
  /// @return #kGetNextMessage: Ready to move on to the next message.
  virtual HandleMessageResult HandleSocketMessageReceived(BrokerClient::Handle handle,
                                                          const RdmnetMessage& message) = 0;

  /// @brief A socket was closed remotely.
  ///
  /// The socket is no longer valid after this callback finishes. Do not call
  /// BrokerSocketManager::RemoveSocket() or any other API function from this callback as it is
  /// unnecessary and may cause a deadlock.
  ///
  /// @param[in] handle The client handle for which the socket was closed.
  /// @param[in] graceful Whether the TCP connection was closed gracefully.
  virtual void HandleSocketClosed(BrokerClient::Handle handle, bool graceful) = 0;
};

class BrokerSocketManager
{
public:
  virtual ~BrokerSocketManager() = default;

  virtual bool Startup() = 0;
  virtual bool Shutdown() = 0;

  virtual void SetNotify(BrokerSocketNotify* notify) = 0;

  virtual bool AddSocket(BrokerClient::Handle handle, etcpal_socket_t sock) = 0;
  virtual void RemoveSocket(BrokerClient::Handle handle) = 0;
};

std::unique_ptr<BrokerSocketManager> CreateBrokerSocketManager();

#endif  // BROKER_SOCKET_MANAGER_H_
