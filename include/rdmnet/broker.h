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

/// \file rdmnet/broker.h
/// \brief A platform-neutral RDMnet Broker implementation.

#ifndef RDMNET_BROKER_H_
#define RDMNET_BROKER_H_

#include <memory>
#include <string>
#include "etcpal/cpp/log.h"
#include "rdmnet/broker/settings.h"

class BrokerCore;

namespace rdmnet
{
/// \defgroup rdmnet_broker Broker API
/// \ingroup rdmnet_cpp_api
/// \brief Implementation of RDMnet broker functionality; see \ref using_broker.

/// \ingroup rdmnet_broker
/// \brief A callback interface for notifications from the Broker.
class BrokerNotify
{
public:
  /// The Scope of the Broker has changed via RDMnet configuration. The Broker should be restarted.
  virtual void HandleScopeChanged(const std::string& new_scope) = 0;
};

/// \ingroup rdmnet_broker
/// \brief Defines an instance of RDMnet broker functionality.
///
/// Use the BrokerSettings struct to configure the behavior of the broker. After instantiatiation,
/// call Startup() to start broker services on a set of network interfaces.
///
/// Starts some threads to handle messages and connections. The current breakdown (pending
/// concurrency optimization) is:
///   * Either:
///     + One thread per explicitly-specified network interface being listened on, or
///     + One thread, if listening on all interfaces
///   * A platform-dependent number of threads to receive messages from clients, depending on the
///     most efficient way to read large number of sockets on a given platform
///   * One thread to handle message routing between clients
///   * One thread to dispatch log messages
///
/// Periodically call Tick() to handle some cleanup and housekeeping. Call Shutdown() at exit, when
/// Broker services are no longer needed, or when a setting has changed. The Broker may send
/// notifications through the BrokerNotify interface.
class Broker
{
public:
  Broker();
  virtual ~Broker();

  Broker(const Broker& other) = delete;
  Broker& operator=(const Broker& other) = delete;
  Broker(Broker&& other) = default;
  Broker& operator=(Broker&& other) = default;

  bool Startup(const BrokerSettings& settings, BrokerNotify* notify, etcpal::Logger* logger);
  void Shutdown();
  void Tick();

  BrokerSettings GetSettings() const;

private:
  std::unique_ptr<BrokerCore> core_;
};

};  // namespace rdmnet

/// @}

#endif  // RDMNET_BROKER_H_
