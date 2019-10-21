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

#ifndef _BROKER_SHELL_H_
#define _BROKER_SHELL_H_

#include <array>
#include <atomic>
#include <set>
#include <string>
#include <vector>
#include "etcpal/inet.h"
#include "etcpal/log.h"
#include "rdmnet/broker.h"

// BrokerShell : Platform-neutral wrapper around the Broker library from a generic console
// application. Instantiates and drives the Broker library.

class BrokerShell : public rdmnet::BrokerNotify
{
public:
  using MacAddress = rdmnet::BrokerSettings::MacAddress;

  // Returns the code the app should exit with.
  int Run(rdmnet::BrokerLog* log);
  static void PrintVersion();

  // Options to set from the command line; must be set BEFORE Run() is called.
  void SetInitialScope(const std::string& scope) { initial_data_.scope = scope; }
  void SetInitialIfaceList(const std::set<EtcPalIpAddr>& ifaces) { initial_data_.ifaces = ifaces; }
  void SetInitialMacList(const std::set<MacAddress>& macs) { initial_data_.macs = macs; }
  void SetInitialPort(uint16_t port) { initial_data_.port = port; }
  void SetInitialLogLevel(int level) { initial_data_.log_mask = ETCPAL_LOG_UPTO(level); }

  void NetworkChanged();
  void AsyncShutdown();

private:
  void HandleScopeChanged(const std::string& new_scope) override;
  void PrintWarningMessage();

  void ApplySettingsChanges(rdmnet::BrokerSettings& settings);

  struct InitialData
  {
    std::string scope{E133_DEFAULT_SCOPE};
    std::set<EtcPalIpAddr> ifaces;
    std::set<MacAddress> macs;
    uint16_t port{0};
    int log_mask{ETCPAL_LOG_UPTO(ETCPAL_LOG_INFO)};
  } initial_data_;

  rdmnet::BrokerLog* log_{nullptr};
  std::atomic<bool> restart_requested_{false};
  bool shutdown_requested_{false};
  std::string new_scope_;
};

#endif  // _BROKER_SHELL_H_
