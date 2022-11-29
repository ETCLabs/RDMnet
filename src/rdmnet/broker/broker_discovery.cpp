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

#include "broker_discovery.h"

#include <algorithm>
#include <iterator>
#include "etcpal/common.h"

/*************************** Function definitions ****************************/

// C library callback shims
extern "C" {

void disccb_broker_registered(rdmnet_registered_broker_t handle, const char* assigned_service_name, void* context)
{
  if (!RDMNET_ASSERT_VERIFY(assigned_service_name))
    return;

  BrokerDiscoveryManager* disc = static_cast<BrokerDiscoveryManager*>(context);
  if (disc)
  {
    disc->LibNotifyBrokerRegistered(handle, assigned_service_name);
  }
}

void disccb_broker_register_error(rdmnet_registered_broker_t handle, int platform_error, void* context)
{
  BrokerDiscoveryManager* disc = static_cast<BrokerDiscoveryManager*>(context);
  if (disc)
  {
    disc->LibNotifyBrokerRegisterError(handle, platform_error);
  }
}

void disccb_other_broker_found(rdmnet_registered_broker_t  handle,
                               const RdmnetBrokerDiscInfo* broker_info,
                               void*                       context)
{
  if (!RDMNET_ASSERT_VERIFY(broker_info))
    return;

  BrokerDiscoveryManager* disc = static_cast<BrokerDiscoveryManager*>(context);
  if (disc)
  {
    disc->LibNotifyOtherBrokerFound(handle, broker_info);
  }
}

void disccb_other_broker_lost(rdmnet_registered_broker_t handle,
                              const char*                scope,
                              const char*                service_name,
                              void*                      context)
{
  if (!RDMNET_ASSERT_VERIFY(scope) || !RDMNET_ASSERT_VERIFY(service_name))
    return;

  BrokerDiscoveryManager* disc = static_cast<BrokerDiscoveryManager*>(context);
  if (disc)
  {
    disc->LibNotifyOtherBrokerLost(handle, scope, service_name);
  }
}

}  // extern "C"

etcpal::Error BrokerDiscoveryManager::RegisterBroker(const rdmnet::Broker::Settings&  settings,
                                                     const rdm::Uid&                  my_uid,
                                                     const std::vector<unsigned int>& resolved_interface_indexes)
{
  RdmnetBrokerRegisterConfig config = RDMNET_BROKER_REGISTER_CONFIG_DEFAULT_INIT;

  config.cid = settings.cid.get();
  config.uid = my_uid.get();
  config.service_instance_name = settings.dns.service_instance_name.c_str();
  config.port = settings.listen_port;
  if (!resolved_interface_indexes.empty())
  {
    config.netints = resolved_interface_indexes.data();
    config.num_netints = resolved_interface_indexes.size();
  }
  config.scope = settings.scope.c_str();
  config.model = settings.dns.model.c_str();
  config.manufacturer = settings.dns.manufacturer.c_str();

  std::vector<RdmnetDnsTxtRecordItem> additional_txt_items;
  if (!settings.dns.additional_txt_record_items.empty())
  {
    additional_txt_items.reserve(settings.dns.additional_txt_record_items.size());
    std::transform(
        settings.dns.additional_txt_record_items.begin(), settings.dns.additional_txt_record_items.end(),
        std::back_inserter(additional_txt_items), [](const rdmnet::DnsTxtRecordItem& item) {
          return RdmnetDnsTxtRecordItem{item.key.c_str(), item.value.data(), static_cast<uint8_t>(item.value.size())};
        });
  }
  if (!additional_txt_items.empty())
  {
    config.additional_txt_items = additional_txt_items.data();
    config.num_additional_txt_items = additional_txt_items.size();
  }

  // clang-format off
  config.callbacks = {
    disccb_broker_registered,
    disccb_broker_register_error,
    disccb_other_broker_found,
    disccb_other_broker_lost,
    this
  };
  // clang-format on

  return rdmnet_disc_register_broker(&config, &handle_);
}

void BrokerDiscoveryManager::UnregisterBroker()
{
  assigned_service_name_.clear();
  rdmnet_disc_unregister_broker(handle_);
}

void BrokerDiscoveryManager::LibNotifyBrokerRegistered(rdmnet_registered_broker_t handle,
                                                       const char*                assigned_service_name)
{
  if (handle == handle_ && assigned_service_name)
  {
    assigned_service_name_ = assigned_service_name;
    if (notify_)
      notify_->HandleBrokerRegistered(assigned_service_name_);
  }
}

void BrokerDiscoveryManager::LibNotifyBrokerRegisterError(rdmnet_registered_broker_t handle, int platform_error)
{
  if (handle == handle_ && notify_)
    notify_->HandleBrokerRegisterError(platform_error);
}

void BrokerDiscoveryManager::LibNotifyOtherBrokerFound(rdmnet_registered_broker_t  handle,
                                                       const RdmnetBrokerDiscInfo* broker_info)
{
  if (handle == handle_ && notify_ && broker_info)
    notify_->HandleOtherBrokerFound(*broker_info);
}

void BrokerDiscoveryManager::LibNotifyOtherBrokerLost(rdmnet_registered_broker_t handle,
                                                      const char*                scope,
                                                      const char*                service_name)
{
  if (handle == handle_ && notify_ && scope && service_name)
    notify_->HandleOtherBrokerLost(scope, service_name);
}
