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

#ifndef RDMNET_LIB_WRAPPER_H_
#define RDMNET_LIB_WRAPPER_H_

#include <string>
#include "rdmnet/device.h"
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/uuid.h"
#include "fakeway_log.h"

class RdmnetLibNotify
{
public:
  virtual void Connected(const RdmnetClientConnectedInfo& info) = 0;
  virtual void ConnectFailed(const RdmnetClientConnectFailedInfo& info) = 0;
  virtual void Disconnected(const RdmnetClientDisconnectedInfo& info) = 0;
  virtual void RdmCommandReceived(const RemoteRdmCommand& cmd) = 0;
  virtual void LlrpRdmCommandReceived(const LlrpRemoteRdmCommand& cmd) = 0;
};

class RdmnetLibInterface
{
public:
  virtual etcpal::Result Startup(const etcpal::Uuid& cid, const RdmnetScopeConfig& scope_config,
                                 RdmnetLibNotify* notify, FakewayLog* log) = 0;
  virtual void Shutdown() = 0;

  virtual etcpal::Result SendRdmResponse(const LocalRdmResponse& resp) = 0;
  virtual etcpal::Result SendStatus(const LocalRptStatus& status) = 0;
  virtual etcpal::Result SendLlrpResponse(const LlrpLocalRdmResponse& resp) = 0;
  virtual etcpal::Result ChangeScope(const RdmnetScopeConfig& new_scope_config, rdmnet_disconnect_reason_t reason) = 0;
  virtual etcpal::Result ChangeSearchDomain(const std::string& new_search_domain,
                                            rdmnet_disconnect_reason_t reason) = 0;
};

class RdmnetLibWrapper : public RdmnetLibInterface
{
public:
  etcpal::Result Startup(const etcpal::Uuid& cid, const RdmnetScopeConfig& scope_config, RdmnetLibNotify* notify,
                         FakewayLog* log) override;
  void Shutdown() override;

  etcpal::Result SendRdmResponse(const LocalRdmResponse& resp) override;
  etcpal::Result SendStatus(const LocalRptStatus& status) override;
  etcpal::Result SendLlrpResponse(const LlrpLocalRdmResponse& resp) override;
  etcpal::Result ChangeScope(const RdmnetScopeConfig& new_scope_config, rdmnet_disconnect_reason_t reason) override;
  etcpal::Result ChangeSearchDomain(const std::string& new_search_domain, rdmnet_disconnect_reason_t reason) override;

  void LibNotifyConnected(rdmnet_device_t handle, const RdmnetClientConnectedInfo* info);
  void LibNotifyConnectFailed(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo* info);
  void LibNotifyDisconnected(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo* info);
  void LibNotifyRdmCommandReceived(rdmnet_device_t handle, const RemoteRdmCommand* cmd);
  void LibNotifyLlrpRdmCommandReceived(rdmnet_device_t handle, const LlrpRemoteRdmCommand* cmd);

private:
  etcpal::Uuid my_cid_;

  rdmnet_device_t device_handle_{nullptr};

  FakewayLog* log_{nullptr};
  RdmnetLibNotify* notify_{nullptr};
};

#endif  // RDMNET_LIB_WRAPPER_H_
