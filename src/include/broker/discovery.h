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
#include "lwpa_error.h"
#include "rdmnet/discovery.h"

struct BrokerDiscoveryAttributes
{
  std::string mdns_service_instance_name;  // Your unique name for the broker
                                           // service instance
  std::string mdns_manufacturer;
  std::string mdns_model;

  std::string scope;        // If empty, the default RDMnet scope is used.
  std::string mdns_domain;  // If empty, the default RDMnet domain is used.

  BrokerDiscoveryAttributes() : scope(E133_DEFAULT_SCOPE), mdns_domain(E133_DEFAULT_DOMAIN) {}
};

class IBrokerDiscoveryManager_Notify
{
public:
  virtual void BrokerRegistered(const BrokerDiscInfo *broker_info) = 0;
};

class BrokerDiscoveryManager
{
public:
  BrokerDiscoveryManager();
  virtual ~BrokerDiscoveryManager();

  static lwpa_error_t InitLibrary();
  static void DeinitLibrary();

  lwpa_error_t RegisterBroker(const BrokerDiscoveryAttributes &disc_attributes);
};

#endif  // _BROKER_DISCOVERY_H_
