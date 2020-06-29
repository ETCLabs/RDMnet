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

// Implementation of the public rdmnet/cpp/broker API.

#include "rdmnet/cpp/broker.h"
#include "broker_core.h"

/*************************** Function definitions ****************************/

/// Constructs a broker instance. Broker is not running until Broker::Startup() is called.
rdmnet::Broker::Broker() : core_(new BrokerCore)
{
}

/// Destroys a broker instance. Call Broker::Shutdown() first.
rdmnet::Broker::~Broker()
{
}

/// @brief Start all broker functionality and threads.
///
/// If listen_addrs is empty, this returns false.  Otherwise, the broker uses the address fields to
/// set up the listening sockets. If the listen_port is 0 and their is only one listen_addr, an
/// ephemeral port is chosen. If there are more listen_addrs, listen_port must not be 0.
///
/// @param settings Settings for the broker to use for this session.
/// @param logger (optional) A class instance that the broker will use to log messages.
/// @param notify (optional) A class instance that the broker will use to send asynchronous
///               notifications about its state.
/// @return etcpal::Error::Ok(): Broker started successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return #kEtcPalErrNotInit: RDMnet library not initialized.
/// @return #kEtcPalErrSys: An internal library or system call error occurred.
/// @return Other codes translated from system error codes are possible.
etcpal::Error rdmnet::Broker::Startup(const Settings& settings, etcpal::Logger* logger, NotifyHandler* notify)
{
  return core_->Startup(settings, notify, logger);
}

/// @brief Shut down all broker functionality and threads.
///
/// Sends disconnect messages to all connected clients, joins all threads and deallocates
/// resources.
///
/// @param disconnect_reason Disconnect reason code to send to all connected clients.
void rdmnet::Broker::Shutdown(rdmnet_disconnect_reason_t disconnect_reason)
{
  core_->Shutdown(disconnect_reason);
}

/// @brief Change the scope on which a broker operates.
///
/// This function is for changing the scope after Broker::Startup() has been called. To configure
/// the initial scope, use the Broker::Settings::scope member. Sends disconnect messages to all
/// connected clients with the reason given before disconnecting and beginning operations on the
/// new scope.
///
/// @param new_scope The new scope on which the broker should operate.
/// @param disconnect_reason Disconnect reason code to send to all connected clients on the current
///                          scope.
/// @return etcpal::Error::Ok(): Scope changed successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return #kEtcPalErrNotInit: Broker not started.
/// @return #kEtcPalErrSys: An internal library or system call error occurred.
etcpal::Error rdmnet::Broker::ChangeScope(const std::string& new_scope, rdmnet_disconnect_reason_t disconnect_reason)
{
  return core_->ChangeScope(new_scope, disconnect_reason);
}

/// @brief Get the current settings the broker is using.
///
/// Can be called even after Shutdown. Useful if you want to shutdown & restart the broker for any
/// reason.
const rdmnet::Broker::Settings& rdmnet::Broker::settings() const
{
  return core_->settings();
}
