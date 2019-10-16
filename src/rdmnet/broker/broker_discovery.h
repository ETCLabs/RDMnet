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

/// \file broker_discovery.h
/// \brief Handles the Broker's DNS registration and discovery of other Brokers.

#ifndef BROKER_DISCOVERY_H_
#define BROKER_DISCOVERY_H_

#include <string>
#include <vector>
#include "etcpal/cpp/error.h"
#include "etcpal/int.h"
#include "rdmnet/broker.h"
#include "rdmnet/core/discovery.h"

/// A callback interface for notifications from the broker discovery subsystem.
class BrokerDiscoveryNotify
{
public:
  /// A broker was registered with the information indicated by broker_info.
  virtual void HandleBrokerRegistered(const std::string& scope, const std::string& requested_service_name,
                                      const std::string& assigned_service_name) = 0;
  /// A broker was found at the same scope as the one which was previously registered.
  virtual void HandleOtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info) = 0;
  /// A previously-found non-local broker has gone away.
  virtual void HandleOtherBrokerLost(const std::string& scope, const std::string& service_name) = 0;
  /// An error occurred while registering a broker's service instance.
  virtual void HandleBrokerRegisterError(const std::string& scope, const std::string& requested_service_name,
                                         int platform_error) = 0;
  /// An error occurred while monitoring a given scope for other brokers.
  virtual void HandleScopeMonitorError(const std::string& scope, int platform_error) = 0;
};

class BrokerDiscoveryInterface
{
public:
  virtual void SetNotify(BrokerDiscoveryNotify* notify) = 0;

  virtual etcpal::Result RegisterBroker(const rdmnet::BrokerSettings& settings) = 0;
  virtual void UnregisterBroker() = 0;
};

/// A wrapper for the RDMnet Discovery library for use by Brokers.
class BrokerDiscoveryManager : public BrokerDiscoveryInterface
{
public:
  BrokerDiscoveryManager();

  void SetNotify(BrokerDiscoveryNotify* notify) override { notify_ = notify; }

  // Registration actions
  etcpal::Result RegisterBroker(const rdmnet::BrokerSettings& settings) override;
  void UnregisterBroker() override;

  // Accessors
  std::string scope() const { return cur_config_valid_ ? cur_config_.my_info.scope : std::string(); }
  std::string requested_service_name() const
  {
    return cur_config_valid_ ? cur_config_.my_info.service_name : std::string();
  }
  std::string assigned_service_name() const { return assigned_service_name_; }

  // Callbacks from the C library, do not call directly.
  void LibNotifyBrokerRegistered(rdmnet_registered_broker_t handle, const char* assigned_service_name);
  void LibNotifyBrokerRegisterError(rdmnet_registered_broker_t handle, int platform_error);
  void LibNotifyBrokerFound(rdmnet_registered_broker_t handle, const RdmnetBrokerDiscInfo* broker_info);
  void LibNotifyBrokerLost(rdmnet_registered_broker_t handle, const char* scope, const char* service_name);
  void LibNotifyScopeMonitorError(rdmnet_registered_broker_t handle, const char* scope, int platform_error);

  BrokerDiscoveryNotify* notify_{nullptr};
  RdmnetBrokerRegisterConfig cur_config_{};
  bool cur_config_valid_{false};
  rdmnet_registered_broker_t handle_{RDMNET_REGISTERED_BROKER_INVALID};
  std::string assigned_service_name_;
};

#endif  // BROKER_DISCOVERY_H_
