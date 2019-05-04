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

#include "broker_shell.h"

#include <iostream>
#include <cstring>
#include "lwpa/netint.h"
#include "lwpa/thread.h"
#include "rdmnet/version.h"

void BrokerShell::ScopeChanged(const std::string &new_scope)
{
  if (log_)
    log_->Log(LWPA_LOG_INFO, "Scope change detected, restarting broker and applying changes");

  new_scope_ = new_scope;
  restart_requested_ = true;
}

void BrokerShell::NetworkChanged()
{
  if (log_)
    log_->Log(LWPA_LOG_INFO, "Network change detected, restarting broker and applying changes");

  restart_requested_ = true;
}

void BrokerShell::AsyncShutdown()
{
  if (log_)
    log_->Log(LWPA_LOG_INFO, "Shutdown requested, Broker shutting down...");

  shutdown_requested_ = true;
}

void BrokerShell::ApplySettingsChanges(RDMnet::BrokerSettings &settings, std::vector<LwpaIpAddr> &new_addrs)
{
  new_addrs = GetInterfacesToListen();

  if (!new_scope_.empty())
  {
    settings.disc_attributes.scope = new_scope_;
    new_scope_.clear();
  }
}

std::vector<LwpaIpAddr> BrokerShell::GetInterfacesToListen()
{
  if (!initial_data_.macs.empty())
  {
    return ConvertMacsToInterfaces(initial_data_.macs);
  }
  else if (!initial_data_.ifaces.empty())
  {
    return initial_data_.ifaces;
  }
  else
  {
    return std::vector<LwpaIpAddr>();
  }
}

std::vector<LwpaIpAddr> BrokerShell::ConvertMacsToInterfaces(const std::vector<MacAddr> &macs)
{
  std::vector<LwpaIpAddr> to_return;

  size_t num_netints = lwpa_netint_get_num_interfaces();
  auto netints = std::make_unique<LwpaNetintInfo[]>(num_netints);
  if (netints)
  {
    size_t netints_retrieved = lwpa_netint_get_interfaces(netints.get(), num_netints);
    for (const auto &mac : macs)
    {
      for (size_t i = 0; i < netints_retrieved; ++i)
      {
        if (0 == memcmp(netints[i].mac, mac.data(), LWPA_NETINTINFO_MAC_LEN))
        {
          to_return.push_back(netints[i].addr);
          break;
        }
      }
    }
  }

  return to_return;
}

void BrokerShell::Run(RDMnet::BrokerLog *log, RDMnet::BrokerSocketManager *sock_mgr)
{
  PrintWarningMessage();

  log_ = log;
  log_->Startup(initial_data_.log_mask);

  RDMnet::BrokerSettings broker_settings(0x6574);
  broker_settings.disc_attributes.scope = initial_data_.scope;

  std::vector<LwpaIpAddr> ifaces = GetInterfacesToListen();

  lwpa_generate_v4_uuid(&broker_settings.cid);

  broker_settings.disc_attributes.dns_manufacturer = "ETC";
  broker_settings.disc_attributes.dns_service_instance_name = "UNIQUE NAME";
  broker_settings.disc_attributes.dns_model = "E1.33 Broker Prototype";

  RDMnet::Broker broker(log, sock_mgr, this);
  broker.Startup(broker_settings, initial_data_.port, ifaces);

  // We want this to run forever if a console
  while (true)
  {
    broker.Tick();

    if (shutdown_requested_)
    {
      break;
    }
    else if (restart_requested_)
    {
      broker.GetSettings(broker_settings);
      broker.Shutdown();

      ApplySettingsChanges(broker_settings, ifaces);
      broker.Startup(broker_settings, initial_data_.port, ifaces);
    }

    lwpa_thread_sleep(300);
  }

  broker.Shutdown();
  log_->Shutdown();
}

void BrokerShell::PrintVersion()
{
  std::cout << "ETC Prototype RDMnet Broker\n";
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
