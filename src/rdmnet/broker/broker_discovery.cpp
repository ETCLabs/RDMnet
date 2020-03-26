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

#include "broker_discovery.h"

#include "etcpal/common.h"

/*************************** Function definitions ****************************/

// C library callback shims
extern "C" {

void disccb_broker_registered(rdmnet_registered_broker_t handle, const char* assigned_service_name, void* context)
{
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

void disccb_broker_found(rdmnet_registered_broker_t handle, const RdmnetBrokerDiscInfo* broker_info, void* context)
{
  BrokerDiscoveryManager* disc = static_cast<BrokerDiscoveryManager*>(context);
  if (disc)
  {
    disc->LibNotifyBrokerFound(handle, broker_info);
  }
}

void disccb_broker_lost(rdmnet_registered_broker_t handle, const char* scope, const char* service_name, void* context)
{
  BrokerDiscoveryManager* disc = static_cast<BrokerDiscoveryManager*>(context);
  if (disc)
  {
    disc->LibNotifyBrokerLost(handle, scope, service_name);
  }
}

void disccb_scope_monitor_error(rdmnet_registered_broker_t handle, const char* scope, int platform_error, void* context)
{
  BrokerDiscoveryManager* disc = static_cast<BrokerDiscoveryManager*>(context);
  if (disc)
  {
    disc->LibNotifyScopeMonitorError(handle, scope, platform_error);
  }
}
}  // extern "C"

BrokerDiscoveryManager::BrokerDiscoveryManager()
{
  // clang-format off
  cur_config_.callbacks = {
    disccb_broker_registered,
    disccb_broker_register_error,
    disccb_broker_found,
    disccb_broker_lost,
    disccb_scope_monitor_error
  };
  // clang-format on
  cur_config_.callback_context = this;
}

etcpal::Error BrokerDiscoveryManager::RegisterBroker(const rdmnet::BrokerSettings& settings)
{
  // Start with the default information.
  RdmnetBrokerDiscInfo* my_info = &cur_config_.my_info;
  rdmnet_disc_init_broker_info(my_info);

  my_info->cid = settings.cid.get();
  std::vector<EtcPalIpAddr> listen_addr_list;
  listen_addr_list.reserve(settings.listen_addrs.size());
  for (const auto& listen_addr : settings.listen_addrs)
  {
    listen_addr_list.push_back(listen_addr.get());
  }
  my_info->listen_addrs = listen_addr_list.data();
  my_info->num_listen_addrs = listen_addr_list.size();
  my_info->port = settings.listen_port;

  ETCPAL_MSVC_BEGIN_NO_DEP_WARNINGS()
  strncpy(my_info->manufacturer, settings.dns.manufacturer.c_str(), E133_MANUFACTURER_STRING_PADDED_LENGTH);
  strncpy(my_info->model, settings.dns.model.c_str(), E133_MODEL_STRING_PADDED_LENGTH);
  strncpy(my_info->scope, settings.scope.c_str(), E133_SCOPE_STRING_PADDED_LENGTH);
  strncpy(my_info->service_name, settings.dns.service_instance_name.c_str(), E133_SERVICE_NAME_STRING_PADDED_LENGTH);
  ETCPAL_MSVC_END_NO_DEP_WARNINGS()

  etcpal_error_t res = rdmnet_disc_register_broker(&cur_config_, &handle_);
  if (res == kEtcPalErrOk)
    cur_config_valid_ = true;
  return res;
}

void BrokerDiscoveryManager::UnregisterBroker()
{
  cur_config_valid_ = false;
  assigned_service_name_.clear();
  rdmnet_disc_unregister_broker(handle_);
}

void BrokerDiscoveryManager::LibNotifyBrokerRegistered(rdmnet_registered_broker_t handle,
                                                       const char* assigned_service_name)
{
  if (handle == handle_ && assigned_service_name)
  {
    assigned_service_name_ = assigned_service_name;
    if (notify_)
      notify_->HandleBrokerRegistered(cur_config_.my_info.scope, cur_config_.my_info.service_name,
                                      assigned_service_name_);
  }
}

void BrokerDiscoveryManager::LibNotifyBrokerRegisterError(rdmnet_registered_broker_t handle, int platform_error)
{
  if (handle == handle_ && notify_)
    notify_->HandleBrokerRegisterError(cur_config_.my_info.scope, cur_config_.my_info.service_name, platform_error);
}

void BrokerDiscoveryManager::LibNotifyBrokerFound(rdmnet_registered_broker_t handle,
                                                  const RdmnetBrokerDiscInfo* broker_info)
{
  if (handle == handle_ && notify_ && broker_info)
    notify_->HandleOtherBrokerFound(*broker_info);
}

void BrokerDiscoveryManager::LibNotifyBrokerLost(rdmnet_registered_broker_t handle, const char* scope,
                                                 const char* service_name)
{
  if (handle == handle_ && notify_ && scope && service_name)
    notify_->HandleOtherBrokerLost(scope, service_name);
}

void BrokerDiscoveryManager::LibNotifyScopeMonitorError(rdmnet_registered_broker_t handle, const char* scope,
                                                        int platform_error)
{
  if (handle == handle_ && notify_ && scope)
    notify_->HandleScopeMonitorError(scope, platform_error);
}
