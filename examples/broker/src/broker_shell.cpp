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
#include "rdmnet/version.h"

bool g_set_new_scope = false;
std::string g_scope_to_set;

void BrokerShell::ScopeChanged(const std::string &new_scope)
{
  scope_changed_ = true;
  new_scope_ = new_scope;
}

bool ShouldApplyChanges(HANDLE net_handle, LPOVERLAPPED net_overlap, bool &bOverlapped)
{
  DWORD temp;
  bOverlapped = (GetOverlappedResult(net_handle, net_overlap, &temp, false) ? true : false);
  return (bOverlapped || g_set_new_scope);
}

void PrepForSettingsChange(RDMnet::Broker &broker, RDMnet::BrokerSettings &settings)
{
  broker.Shutdown();
  broker.GetSettings(settings);
}

void ApplySettingsChanges(RDMnet::BrokerLog &log, bool bOverlapped, RDMnet::BrokerSettings &settings, HANDLE net_handle,
                          LPOVERLAPPED net_overlap, std::vector<IFList::iflist_entry> &interfaces,
                          std::vector<LwpaIpAddr> &useaddrs)
{
  // If we detect the network changed, restart the broker core
  if (bOverlapped)
  {
    log.Log(LWPA_LOG_INFO, "Network change detected, restarting broker and applying changes");

    // We need to reset the useaddrs vector
    IFList::FindIFaces(log, interfaces);
    GetMyIfaceKey(useaddrs, interfaces);
    memset(net_overlap, 0, sizeof(OVERLAPPED));
    net_handle = NULL;
    NotifyAddrChange(&net_handle, net_overlap);
  }

  // If there are other new settings, apply them here.
  if (g_set_new_scope)
  {
    g_set_new_scope = false;
    log.Log(LWPA_LOG_INFO, "Scope change detected, restarting broker and applying changes");
    settings.disc_attributes.scope = g_scope_to_set;
  }
}

void BrokerShell::Run(RDMnet::BrokerLog *log, RDMnet::BrokerSocketManager *sock_mgr)
{
  log->Startup(initial_data_.log_level);

  RDMnet::BrokerSettings broker_settings(0x6574);
  broker_settings.disc_attributes.scope = initial_data_.scope;

  std::vector<LwpaIpAddr> ifaces;
  if (!initial_data_.macs.empty())
  {
    ifaces = ConvertMacsToInterfaces(
  }
  else if (!initial_data_.ifaces.empty())
  {
  }
  std::vector<IFList::iflist_entry> interfaces;
  IFList::FindIFaces(broker_log, interfaces);

  // Given the first network interface found, generate the cid and UID
  if (!interfaces.empty())
  {
    // The cid will be based on the scope, in case we want to run different instances on the same
    // machine
    std::string cidstr("ETC E133 BROKER for scope: ");
    cidstr += broker_settings.disc_attributes.scope;
    lwpa_generate_v3_uuid(&broker_settings.cid, cidstr.c_str(), interfaces.front().mac, 1);
  }

  broker_settings.disc_attributes.dns_manufacturer = "ETC";
  broker_settings.disc_attributes.dns_service_instance_name = "UNIQUE NAME";
  broker_settings.disc_attributes.dns_model = "E1.33 Broker Prototype";

  std::vector<LwpaIpAddr> useaddrs;
  GetMyIfaceKey(useaddrs, interfaces);

  RDMnet::Broker broker(log, sock_mgr, this);
  broker.Startup(broker_settings, initial_data_.port, initial_data_.ifaces);

  PrintWarningMessage();

  // We want this to run forever if a console, otherwise run for how long the service manager allows
  // it
  while (!g_shell || !g_shell->exitServiceThread)
  {
    // Do the main service work here
    bool bOverlapped = false;

    broker.Tick();

    if (ShouldApplyChanges(net_handle, &net_overlap, bOverlapped))
    {
      PrepForSettingsChange(broker, broker_settings);
      ApplySettingsChanges(broker_log, bOverlapped, broker_settings, net_handle, &net_overlap, interfaces, useaddrs);
      broker.Startup(broker_settings, GetPortKey(), useaddrs);
    }

    Sleep(300);
  }

  broker.Shutdown();
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