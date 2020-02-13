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

// Implementation of the public rdmnet/broker API.

#include "rdmnet/broker.h"
#include "broker_core.h"

/*************************** Function definitions ****************************/

rdmnet::Broker::Broker() : core_(std::make_unique<BrokerCore>())
{
}

rdmnet::Broker::~Broker()
{
}

/// \brief Start all broker functionality and threads.
///
/// If listen_addrs is empty, this returns false.  Otherwise, the broker uses the address fields to
/// set up the listening sockets. If the listen_port is 0 and their is only one listen_addr, an
/// ephemeral port is chosen. If there are more listen_addrs, listen_port must not be 0.
///
/// \param[in] settings Settings for the broker to use for this session.
/// \param[in] notify A class instance that the broker will use to send asynchronous notifications
///                   about its state.
/// \param[in] log A class instance that the broker will use to log messages.
/// \return true (started broker successfully) or false (an error occurred starting broker).
bool rdmnet::Broker::Startup(const BrokerSettings& settings, rdmnet::BrokerNotifyHandler* notify, etcpal::Logger* log)
{
  return core_->Startup(settings, notify, log);
}

void rdmnet::Broker::Shutdown()
{
  core_->Shutdown();
}

void rdmnet::Broker::Tick()
{
  core_->Tick();
}

/// \brief Get the current settings the broker is using.
/// Can be called even after Shutdown. Useful if you want to shutdown & restart the broker for any
/// reason.
rdmnet::BrokerSettings rdmnet::Broker::GetSettings() const
{
  return core_->GetSettings();
}
