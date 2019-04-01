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

#pragma once

#include "RDMnetLibInterface.h"
#include "ControllerLog.h"

// RDMnetLibWrapper: C++ wrapper for the RDMnet Library interface.

// These functions mirror the callback functions from the C library.
class RDMnetLibNotifyInternal
{
public:
  virtual void Connected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                         const RdmnetClientConnectedInfo *info) = 0;
  virtual void NotConnected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                            const RdmnetClientNotConnectedInfo *info) = 0;
  virtual void ClientListUpdate(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                client_list_action_t list_action, const ClientList *list) = 0;
  virtual void RdmResponseReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                   const RemoteRdmResponse *resp) = 0;
  virtual void RdmCommandReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                  const RemoteRdmCommand *cmd) = 0;
  virtual void StatusReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                              const RemoteRptStatus *status) = 0;
};

class RDMnetLibWrapper : public RDMnetLibInterface, public RDMnetLibNotifyInternal
{
public:
  RDMnetLibWrapper(ControllerLog *log);

  bool Startup(RDMnetLibNotify *notify) override;
  void Shutdown() override;

  rdmnet_client_scope_t AddScope(const std::string &scope,
                                 StaticBrokerConfig static_broker = StaticBrokerConfig()) override;
  bool RemoveScope(rdmnet_client_scope_t scope_handle) override;

  bool SendRdmCommand(rdmnet_client_scope_t scope_handle, const LocalRdmCommand &cmd) override;
  bool SendRdmResponse(rdmnet_client_scope_t scope_handle, const LocalRdmResponse &resp) override;

protected:
  // RDMnetLibNotifyInternal overrides
  virtual void Connected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                         const RdmnetClientConnectedInfo *info) override;
  virtual void NotConnected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                            const RdmnetClientNotConnectedInfo *info) override;
  virtual void ClientListUpdate(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                client_list_action_t list_action, const ClientList *list) override;
  virtual void RdmResponseReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                   const RemoteRdmResponse *resp) override;
  virtual void RdmCommandReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                  const RemoteRdmCommand *cmd) override;
  virtual void StatusReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                              const RemoteRptStatus *status) override;

private:
  LwpaUuid my_cid_;

  bool running_{false};
  rdmnet_controller_t controller_handle_{nullptr};

  ControllerLog *log_{nullptr};
  RDMnetLibNotify *notify_{nullptr};
};
