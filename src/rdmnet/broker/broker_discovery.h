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

/// @file broker_discovery.h
/// @brief Handles the Broker's DNS registration and discovery of other Brokers.

#ifndef BROKER_DISCOVERY_H_
#define BROKER_DISCOVERY_H_

#include <cstdint>
#include <string>
#include <vector>
#include "etcpal/cpp/error.h"
#include "rdmnet/cpp/broker.h"
#include "rdmnet/discovery.h"

/// A callback interface for notifications from the broker discovery subsystem.
class BrokerDiscoveryNotify
{
public:
  /// A broker was registered with the information indicated by broker_info.
  virtual void HandleBrokerRegistered(const std::string& assigned_service_name) = 0;
  /// A broker was found at the same scope as the one which was previously registered.
  virtual void HandleOtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info) = 0;
  /// A previously-found non-local broker has gone away.
  virtual void HandleOtherBrokerLost(const std::string& scope, const std::string& service_name) = 0;
  /// An error occurred while registering a broker's service instance.
  virtual void HandleBrokerRegisterError(int platform_error) = 0;
};

class BrokerDiscoveryInterface
{
public:
  virtual ~BrokerDiscoveryInterface() = default;

  virtual void SetNotify(BrokerDiscoveryNotify* notify) = 0;

  virtual etcpal::Error RegisterBroker(const rdmnet::Broker::Settings& settings, const rdm::Uid& my_uid) = 0;
  virtual void          UnregisterBroker() = 0;

  virtual bool BrokerShouldDeregister(const etcpal::Uuid& this_broker_cid, const etcpal::Uuid& other_broker_cid) = 0;
};

/// A wrapper for the RDMnet Discovery library for use by Brokers.
class BrokerDiscoveryManager : public BrokerDiscoveryInterface
{
public:
  void SetNotify(BrokerDiscoveryNotify* notify) override { notify_ = notify; }

  // Registration actions
  etcpal::Error RegisterBroker(const rdmnet::Broker::Settings& settings, const rdm::Uid& my_uid) override;
  void          UnregisterBroker() override;

  bool BrokerShouldDeregister(const etcpal::Uuid& this_broker_cid, const etcpal::Uuid& other_broker_cid);

  // Accessors
  std::string assigned_service_name() const { return assigned_service_name_; }

  // Callbacks from the C library, do not call directly.
  void LibNotifyBrokerRegistered(rdmnet_registered_broker_t handle, const char* assigned_service_name);
  void LibNotifyBrokerRegisterError(rdmnet_registered_broker_t handle, int platform_error);
  void LibNotifyOtherBrokerFound(rdmnet_registered_broker_t handle, const RdmnetBrokerDiscInfo* broker_info);
  void LibNotifyOtherBrokerLost(rdmnet_registered_broker_t handle, const char* scope, const char* service_name);

private:
  BrokerDiscoveryNotify*     notify_{nullptr};
  rdmnet_registered_broker_t handle_{RDMNET_REGISTERED_BROKER_INVALID};
  std::string                assigned_service_name_;
};

#endif  // BROKER_DISCOVERY_H_
