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

#include "broker_shell.h"

#include <iostream>
#include <cstring>
#include "etcpal/netint.h"
#include "etcpal/thread.h"
#include "rdmnet/version.h"

void BrokerShell::HandleScopeChanged(const std::string& new_scope)
{
  if (log_)
    log_->Info("Scope change detected, restarting broker and applying changes");

  new_scope_ = new_scope;
  restart_requested_ = true;
}

void BrokerShell::NetworkChanged()
{
  if (log_)
    log_->Info("Network change detected, restarting broker and applying changes");

  restart_requested_ = true;
}

void BrokerShell::AsyncShutdown()
{
  if (log_)
    log_->Info("Shutdown requested, Broker shutting down...");

  shutdown_requested_ = true;
}

void BrokerShell::ApplySettingsChanges(rdmnet::BrokerSettings& settings)
{
  if (!new_scope_.empty())
  {
    settings.scope = new_scope_;
    new_scope_.clear();
  }
}

int BrokerShell::Run(rdmnet::BrokerLog& log)
{
  PrintWarningMessage();

  log_ = &log;

  rdmnet::BrokerSettings broker_settings(etcpal::Uuid::V4(), 0x6574);

  broker_settings.scope = initial_data_.scope;
  broker_settings.dns.manufacturer = "ETC";
  broker_settings.dns.model = "RDMnet Broker Example App";
  broker_settings.listen_port = initial_data_.port;
  broker_settings.listen_macs = initial_data_.macs;
  broker_settings.listen_addrs = initial_data_.ifaces;

  rdmnet::Broker broker;

  if (!broker.Startup(broker_settings, this, log_))
  {
    std::cout << "Broker failed to start.\n";
    return 1;
  }

  // We want this to run forever in a console
  while (true)
  {
    broker.Tick();

    if (shutdown_requested_)
    {
      break;
    }
    else if (restart_requested_)
    {
      restart_requested_ = false;

      broker_settings = broker.GetSettings();
      broker.Shutdown();

      ApplySettingsChanges(broker_settings);
      broker.Startup(broker_settings, this, log_);
    }

    etcpal_thread_sleep(300);
  }

  broker.Shutdown();
  return 0;
}

void BrokerShell::PrintVersion()
{
  std::cout << "ETC Example RDMnet Broker\n";
  std::cout << "Version " << RDMNET_VERSION_STRING << "\n\n";
  std::cout << RDMNET_VERSION_COPYRIGHT << "\n";
  std::cout << "License: Apache License v2.0 <http://www.apache.org/licenses/LICENSE-2.0>\n";
  std::cout << "Unless required by applicable law or agreed to in writing, this software is\n";
  std::cout << "provided \"AS IS\", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express\n";
  std::cout << "or implied.\n";
}

void BrokerShell::PrintWarningMessage()
{
  std::cout << "*******************************************************************************\n";
  std::cout << "*******************************************************************************\n";
  std::cout << "This is an RDMnet Broker example application. This app is suitable for testing\n";
  std::cout << "other RDMnet components against, but it is not designed to be deployed in\n";
  std::cout << "production. DO NOT USE THIS APP IN A SHIPPING PRODUCT. You have been warned.\n";
  std::cout << "*******************************************************************************\n";
  std::cout << "*******************************************************************************\n";
}
