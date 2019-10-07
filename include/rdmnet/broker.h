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
/// \author Nick Ballhorn-Wagner and Sam Kearney
#ifndef _RDMNET_BROKER_H_
#define _RDMNET_BROKER_H_

#include <memory>
#include <string>
#include "rdmnet/broker/log.h"
#include "rdmnet/broker/settings.h"

class BrokerCore;

/// \defgroup rdmnet_broker RDMnet Broker Library
/// \brief A platform-neutral RDMnet Broker implementation.
/// @{
///

/// A namespace to contain the public Broker classes
namespace rdmnet
{
/// A callback interface for notifications from the Broker.
class BrokerNotify
{
public:
  /// The Scope of the Broker has changed via RDMnet configuration. The Broker should be restarted.
  virtual void ScopeChanged(const std::string& new_scope) = 0;
};

/// \brief Defines an instance of RDMnet %Broker functionality.
///
/// After instantiatiation, call Startup() to start Broker services on a set of network interfaces.
/// Starts some threads (defined in broker/threads.h) to handle messages and connections.
/// Periodically call Tick() to handle some cleanup and housekeeping.
/// Call Shutdown() at exit, when Broker services are no longer needed, or when a setting has
/// changed.
class Broker
{
public:
  Broker();
  virtual ~Broker();

  bool Startup(const BrokerSettings& settings, BrokerNotify* notify, BrokerLog* log);
  void Shutdown();
  void Tick();

  BrokerSettings GetSettings() const;

private:
  std::unique_ptr<BrokerCore> core_;
};

};  // namespace rdmnet

/// @}

#endif  // _RDMNET_BROKER_H_
