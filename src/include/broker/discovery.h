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

/*! \file broker/discovery.h
 *  \brief Handles the Broker's DNS registration and discovery of other
 *         Brokers.
 *  \author Sam Kearney
 */
#ifndef _BROKER_DISCOVERY_H_
#define _BROKER_DISCOVERY_H_

#include <string>
#include <vector>
#include "lwpa_error.h"
#include "lwpa_int.h"
#include "estardmnet.h"
#include "rdmnet/discovery.h"

/// Settings for the Broker's DNS Discovery functionality.
struct BrokerDiscoveryAttributes
{
  /// Your unique name for this %Broker DNS-SD service instance. The discovery library uses
  /// standard mechanisms to ensure that this service instance name is actually unique;
  /// however, the application should make a reasonable effort to provide a name that will
  /// not conflict with other %Brokers.
  std::string dns_service_instance_name;

  /// A string to identify the manufacturer of this %Broker instance.
  std::string dns_manufacturer;
  /// A string to identify the model of product in which the %Broker instance is included.
  std::string dns_model;

  /// The Scope on which this %Broker should operate. If empty, the default RDMnet scope is used.
  std::string scope;

  BrokerDiscoveryAttributes() : scope(E133_DEFAULT_SCOPE) {}
};

/// A callback interface for notifications from a BrokerDiscoveryManager.
class IBrokerDiscoveryManager_Notify
{
public:
  /// A %Broker was registered with the information indicated by broker_info.
  virtual void BrokerRegistered(const BrokerDiscInfo &broker_info, const std::string &assigned_service_name) = 0;
  /// A %Broker was found at the same scope as the one which was previously registered.
  virtual void OtherBrokerFound(const BrokerDiscInfo &broker_info) = 0;
  /// A previously-found non-local %Broker has gone away.
  virtual void OtherBrokerLost(const std::string &service_name) = 0;
  /// An error occurred while registering a %Broker's service instance.
  virtual void BrokerRegisterError(const BrokerDiscInfo &broker_info, int platform_error) = 0;
};

/// A wrapper for the RDMnet Discovery library for use by Brokers.
class BrokerDiscoveryManager
{
public:
  BrokerDiscoveryManager(IBrokerDiscoveryManager_Notify *notify);
  virtual ~BrokerDiscoveryManager();

  static lwpa_error_t InitLibrary();
  static void DeinitLibrary();
  static void LibraryTick();

  lwpa_error_t RegisterBroker(const BrokerDiscoveryAttributes &disc_attributes, const LwpaCid &local_cid,
                              const std::vector<LwpaIpAddr> &listen_addrs, uint16_t listen_port);
  void UnregisterBroker();

protected:
  static void BrokerFound(const char *scope, const BrokerDiscInfo *broker_info, void *context);
  static void BrokerLost(const char *service_name, void *context);
  static void ScopeMonitorError(const ScopeMonitorInfo *scope_info, int platform_error, void *context);
  static void BrokerRegistered(const BrokerDiscInfo *broker_info, const char *assigned_service_name, void *context);
  static void BrokerRegisterError(const BrokerDiscInfo *broker_info, int platform_error, void *context);

  IBrokerDiscoveryManager_Notify *notify_;
};

#endif  // _BROKER_DISCOVERY_H_
