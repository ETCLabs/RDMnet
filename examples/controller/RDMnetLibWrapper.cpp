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

#include "RDMnetLibWrapper.h"

static void controllercb_connected(rdmnet_controller_t handle, const char *scope, const RdmnetClientConnectedInfo *info,
                                   void *context)
{
}

static void controllercb_not_connected(rdmnet_controller_t handle, const char *scope,
                                       const RdmnetClientNotConnectedInfo *info, void *context)
{
}

static void controllercb_client_list_update(rdmnet_controller_t handle, const char *scope,
                                            client_list_action_t list_action, const ClientList *list, void *context)
{
}

static void controllercb_rdm_response_received(rdmnet_controller_t handle, const char *scope,
                                               const RemoteRdmResponse *resp, void *context)
{
}

static void controllercb_rdm_command_received(rdmnet_controller_t handle, const char *scope,
                                              const RemoteRdmCommand *cmd, void *context)
{
}

static void controllercb_status_received(rdmnet_controller_t handle, const char *scope, const RemoteRptStatus *status,
                                         void *context)
{
}

RDMnetLibWrapper::RDMnetLibWrapper(ControllerLog *log) : log_(log)
{
  rdmnet_core_init(log_->GetLogParams());

  lwpa_generate_v4_uuid(&my_cid_);

  RdmnetScopeConfig default_scope;
  rdmnet_client_set_scope(&default_scope, E133_DEFAULT_SCOPE);

  RdmnetControllerConfig config;
  config.uid = RPT_CLIENT_DYNAMIC_UID(0x6574);
  config.cid = my_cid_;
  config.scope_arr = &default_scope;
  config.num_scopes = 1;
  config.callbacks = {controllercb_connected,
                      controllercb_not_connected,
                      controllercb_client_list_update,
                      controllercb_rdm_response_received,
                      controllercb_rdm_command_received,
                      controllercb_status_received};
  config.callback_context = this;

  rdmnet_controller_create(&config, &controller_handle_);
}

RDMnetLibWrapper::~RDMnetLibWrapper()
{
  rdmnet_core_deinit();
}