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

#include "rdmnet/controller.h"
#include "ControllerLog.h"

// RDMnetLibWrapper: C++ wrapper for the RDMnet Library interface.

class RDMnetLibNotify
{
public:
  virtual void Connected(const std::string &scope) = 0;
  virtual void NotConnected(const std::string &scope) = 0;
  virtual void ClientListUpdate(const std::string &scope, client_list_action_t action, const ClientList &list) = 0;
  virtual void RdmCmdReceived(const std::string &scope, const RemoteRdmCommand &cmd) = 0;
  virtual void RdmRespReceived(const std::string &scope, const RemoteRdmResponse &resp) = 0;
  virtual void StatusReceived(const std::string &scope, const RemoteRptStatus &status) = 0;
};

class RDMnetLibWrapper
{
public:
  RDMnetLibWrapper(ControllerLog *log);
  ~RDMnetLibWrapper();

  bool AddScope(const std::string &scope);
  bool RemoveScope(const std::string &scope);

  bool SendRdmCommand(const std::string &scope, const RdmCommand &cmd);

private:
  rdmnet_controller_t controller_handle_{nullptr};
  ControllerLog *log_{nullptr};
  LwpaUuid my_cid_;
};