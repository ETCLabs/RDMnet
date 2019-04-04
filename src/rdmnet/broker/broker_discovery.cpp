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
#include "broker_discovery.h"

#include "rdmnet/core/util.h"

/*************************** Function definitions ****************************/

BrokerDiscoveryManager::BrokerDiscoveryManager(BrokerDiscoveryManagerNotify *notify) : notify_(notify)
{
}

BrokerDiscoveryManager::~BrokerDiscoveryManager()
{
}

lwpa_error_t BrokerDiscoveryManager::RegisterBroker(const RDMnet::BrokerDiscoveryAttributes &disc_attributes,
                                                    const LwpaUuid &local_cid,
                                                    const std::vector<LwpaIpAddr> &listen_addrs, uint16_t listen_port)
{
  // Start with the default information.
  rdmnetdisc_fill_default_broker_info(&cur_info_);

  cur_info_.cid = local_cid;
  for (size_t i = 0; i < listen_addrs.size(); i++)
  {
    // TODO: make sure lwpa_sockaddr is what we want on the library's side of things
    cur_info_.listen_addrs[i].ip = listen_addrs[i];
  }
  cur_info_.listen_addrs_count = listen_addrs.size();
  cur_info_.port = listen_port;

  RDMNET_MSVC_BEGIN_NO_DEP_WARNINGS()
  strncpy(cur_info_.manufacturer, disc_attributes.dns_manufacturer.c_str(), E133_MANUFACTURER_STRING_PADDED_LENGTH);
  strncpy(cur_info_.model, disc_attributes.dns_model.c_str(), E133_MODEL_STRING_PADDED_LENGTH);
  strncpy(cur_info_.scope, disc_attributes.scope.c_str(), E133_SCOPE_STRING_PADDED_LENGTH);
  strncpy(cur_info_.service_name, disc_attributes.dns_service_instance_name.c_str(),
          E133_SERVICE_NAME_STRING_PADDED_LENGTH);
  RDMNET_MSVC_END_NO_DEP_WARNINGS()

  lwpa_error_t res = rdmnetdisc_register_broker(&cur_info_, true, notify_);
  if (res == kLwpaErrOk)
    cur_info_valid_ = true;
  return res;
}

void BrokerDiscoveryManager::UnregisterBroker()
{
  cur_info_valid_ = false;
  rdmnetdisc_unregister_broker(true);
}

void BrokerDiscoveryManager::Standby()
{
  rdmnetdisc_unregister_broker(false);
}

lwpa_error_t BrokerDiscoveryManager::Resume()
{
  return rdmnetdisc_register_broker(&cur_info_, false, notify_);
}

void BrokerDiscoveryManager::BrokerFound(const char * /*scope*/, const RdmnetBrokerDiscInfo *broker_info, void *context)
{
  BrokerDiscoveryManagerNotify *notify = static_cast<BrokerDiscoveryManagerNotify *>(context);
  if (notify && broker_info)
    notify->OtherBrokerFound(*broker_info);
}

void BrokerDiscoveryManager::BrokerLost(const char *service_name, void *context)
{
  BrokerDiscoveryManagerNotify *notify = static_cast<BrokerDiscoveryManagerNotify *>(context);
  if (notify)
    notify->OtherBrokerLost(service_name);
}

void BrokerDiscoveryManager::ScopeMonitorError(const RdmnetScopeMonitorInfo * /*scope_info*/, int /*platform_error*/,
                                               void * /*context*/)
{
}

void BrokerDiscoveryManager::BrokerRegistered(const RdmnetBrokerDiscInfo *broker_info,
                                              const char *assigned_service_name, void *context)
{
  BrokerDiscoveryManagerNotify *notify = static_cast<BrokerDiscoveryManagerNotify *>(context);
  if (notify && broker_info)
    notify->BrokerRegistered(*broker_info, assigned_service_name);
}

void BrokerDiscoveryManager::BrokerRegisterError(const BrokerDiscInfo *broker_info, int platform_error, void *context)
{
  BrokerDiscoveryManagerNotify *notify = static_cast<BrokerDiscoveryManagerNotify *>(context);
  if (notify && broker_info)
    notify->BrokerRegisterError(*broker_info, platform_error);
}