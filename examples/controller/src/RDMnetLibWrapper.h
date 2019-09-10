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

#pragma once

#include "RDMnetLibInterface.h"
#include "ControllerLog.h"

// RDMnetLibWrapper: C++ wrapper for the RDMnet Library interface.

// These functions mirror the callback functions from the C library.
class RDMnetLibNotifyInternal
{
public:
  virtual void Connected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                         const RdmnetClientConnectedInfo* info) = 0;
  virtual void ConnectFailed(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                             const RdmnetClientConnectFailedInfo* info) = 0;
  virtual void Disconnected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                            const RdmnetClientDisconnectedInfo* info) = 0;
  virtual void ClientListUpdate(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                client_list_action_t list_action, const ClientList* list) = 0;
  virtual void RdmResponseReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                   const RemoteRdmResponse* resp) = 0;
  virtual void RdmCommandReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                  const RemoteRdmCommand* cmd) = 0;
  virtual void StatusReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                              const RemoteRptStatus* status) = 0;
  virtual void LlrpRdmCommandReceived(rdmnet_controller_t handle, const LlrpRemoteRdmCommand* cmd) = 0;
};

class RDMnetLibWrapper : public RDMnetLibInterface, public RDMnetLibNotifyInternal
{
public:
  RDMnetLibWrapper(ControllerLog* log);

  bool Startup(const EtcPalUuid& cid, RDMnetLibNotify* notify) override;
  void Shutdown() override;

  rdmnet_client_scope_t AddScope(const std::string& scope,
                                 StaticBrokerConfig static_broker = StaticBrokerConfig()) override;
  bool RemoveScope(rdmnet_client_scope_t scope_handle, rdmnet_disconnect_reason_t reason) override;

  bool SendRdmCommand(rdmnet_client_scope_t scope_handle, const LocalRdmCommand& cmd) override;
  bool SendRdmCommand(rdmnet_client_scope_t scope_handle, const LocalRdmCommand& cmd, uint32_t& seq_num) override;
  bool SendRdmResponse(rdmnet_client_scope_t scope_handle, const LocalRdmResponse& resp) override;
  bool SendLlrpResponse(const LlrpLocalRdmResponse& resp) override;
  bool RequestClientList(rdmnet_client_scope_t scope_handle) override;

protected:
  // RDMnetLibNotifyInternal overrides
  virtual void Connected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                         const RdmnetClientConnectedInfo* info) override;
  virtual void ConnectFailed(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                             const RdmnetClientConnectFailedInfo* info) override;
  virtual void Disconnected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                            const RdmnetClientDisconnectedInfo* info) override;
  virtual void ClientListUpdate(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                client_list_action_t list_action, const ClientList* list) override;
  virtual void RdmResponseReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                   const RemoteRdmResponse* resp) override;
  virtual void RdmCommandReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                  const RemoteRdmCommand* cmd) override;
  virtual void StatusReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                              const RemoteRptStatus* status) override;
  virtual void LlrpRdmCommandReceived(rdmnet_controller_t handle, const LlrpRemoteRdmCommand* cmd) override;

private:
  EtcPalUuid my_cid_;

  bool running_{false};
  rdmnet_controller_t controller_handle_{nullptr};

  ControllerLog* log_{nullptr};
  RDMnetLibNotify* notify_{nullptr};
};
