/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

/// \file broker_discovery.h
/// \brief Handles the Broker's DNS registration and discovery of other Brokers.
/// \author Sam Kearney
#ifndef _BROKER_DISCOVERY_H_
#define _BROKER_DISCOVERY_H_

#include <string>
#include <vector>
#include "etcpal/error.h"
#include "etcpal/int.h"
#include "rdmnet/broker.h"
#include "rdmnet/core/discovery.h"

/// A callback interface for notifications from a BrokerDiscoveryManager.
class BrokerDiscoveryManagerNotify
{
public:
  /// A %Broker was registered with the information indicated by broker_info.
  virtual void BrokerRegistered(const std::string& assigned_service_name) = 0;
  /// A %Broker was found at the same scope as the one which was previously registered.
  virtual void OtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info) = 0;
  /// A previously-found non-local %Broker has gone away.
  virtual void OtherBrokerLost(const std::string& service_name) = 0;
  /// An error occurred while registering a %Broker's service instance.
  virtual void BrokerRegisterError(int platform_error) = 0;
};

/// A wrapper for the RDMnet Discovery library for use by Brokers.
class BrokerDiscoveryManager
{
public:
  BrokerDiscoveryManager(BrokerDiscoveryManagerNotify* notify);
  virtual ~BrokerDiscoveryManager();

  // Registration actions
  etcpal_error_t RegisterBroker(const RDMnet::BrokerDiscoveryAttributes& disc_attributes, const EtcPalUuid& local_cid,
                              const std::vector<EtcPalIpAddr>& listen_addrs, uint16_t listen_port);
  void UnregisterBroker();

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

  BrokerDiscoveryManagerNotify* notify_{nullptr};
  RdmnetBrokerRegisterConfig cur_config_;
  bool cur_config_valid_{false};
  rdmnet_registered_broker_t handle_{RDMNET_REGISTERED_BROKER_INVALID};
  std::string assigned_service_name_;
};

#endif  // _BROKER_DISCOVERY_H_
