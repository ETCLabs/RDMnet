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

#include <string>
#include "rdmnet/controller.h"
#include "ControllerUtils.h"

class RDMnetLibNotify
{
public:
  virtual void Connected(rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo &info) = 0;
  virtual void ConnectFailed(rdmnet_client_scope_t scope_handle, const RdmnetClientConnectFailedInfo &info) = 0;
  virtual void Disconnected(rdmnet_client_scope_t scope_handle, const RdmnetClientDisconnectedInfo &info) = 0;
  virtual void ClientListUpdate(rdmnet_client_scope_t scope_handle, client_list_action_t action,
                                const ClientList &list) = 0;
  virtual void RdmCommandReceived(rdmnet_client_scope_t scope_handle, const RemoteRdmCommand &cmd) = 0;
  virtual void RdmResponseReceived(rdmnet_client_scope_t scope_handle, const RemoteRdmResponse &resp) = 0;
  virtual void StatusReceived(rdmnet_client_scope_t scope_handle, const RemoteRptStatus &status) = 0;
};

class RDMnetLibInterface
{
public:
  virtual bool Startup(RDMnetLibNotify *notify) = 0;
  virtual void Shutdown() = 0;

  virtual rdmnet_client_scope_t AddScope(const std::string &scope,
                                         StaticBrokerConfig static_broker = StaticBrokerConfig()) = 0;
  virtual bool RemoveScope(rdmnet_client_scope_t scope_handle, rdmnet_disconnect_reason_t reason) = 0;
  virtual bool SendRdmCommand(rdmnet_client_scope_t scope_handle, const LocalRdmCommand &cmd) = 0;
  virtual bool SendRdmCommand(rdmnet_client_scope_t scope_handle, const LocalRdmCommand &cmd, uint32_t &seq_num) = 0;
  virtual bool SendRdmResponse(rdmnet_client_scope_t scope_handle, const LocalRdmResponse &resp) = 0;
  virtual bool RequestClientList(rdmnet_client_scope_t scope_handle) = 0;
};
