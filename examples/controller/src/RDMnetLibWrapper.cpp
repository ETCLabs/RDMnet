/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "RDMnetLibWrapper.h"

extern "C" {

static void controllercb_connected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                   const RdmnetClientConnectedInfo* info, void* context)
{
  RDMnetLibNotifyInternal* notify = static_cast<RDMnetLibNotifyInternal*>(context);
  if (notify)
    notify->Connected(handle, scope, info);
}

static void controllercb_connect_failed(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                        const RdmnetClientConnectFailedInfo* info, void* context)
{
  RDMnetLibNotifyInternal* notify = static_cast<RDMnetLibNotifyInternal*>(context);
  if (notify)
    notify->ConnectFailed(handle, scope, info);
}

static void controllercb_disconnected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                      const RdmnetClientDisconnectedInfo* info, void* context)
{
  RDMnetLibNotifyInternal* notify = static_cast<RDMnetLibNotifyInternal*>(context);
  if (notify)
    notify->Disconnected(handle, scope, info);
}

static void controllercb_client_list_update(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                            client_list_action_t list_action, const ClientList* list, void* context)
{
  RDMnetLibNotifyInternal* notify = static_cast<RDMnetLibNotifyInternal*>(context);
  if (notify)
    notify->ClientListUpdate(handle, scope, list_action, list);
}

static void controllercb_rdm_response_received(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                               const RemoteRdmResponse* resp, void* context)
{
  RDMnetLibNotifyInternal* notify = static_cast<RDMnetLibNotifyInternal*>(context);
  if (notify)
    notify->RdmResponseReceived(handle, scope, resp);
}

static void controllercb_rdm_command_received(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                              const RemoteRdmCommand* cmd, void* context)
{
  RDMnetLibNotifyInternal* notify = static_cast<RDMnetLibNotifyInternal*>(context);
  if (notify)
    notify->RdmCommandReceived(handle, scope, cmd);
}

static void controllercb_status_received(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                         const RemoteRptStatus* status, void* context)
{
  RDMnetLibNotifyInternal* notify = static_cast<RDMnetLibNotifyInternal*>(context);
  if (notify)
    notify->StatusReceived(handle, scope, status);
}

static void controllercb_llrp_rdm_command_received(rdmnet_controller_t handle, const LlrpRemoteRdmCommand* cmd,
                                                   void* context)
{
  RDMnetLibNotifyInternal* notify = static_cast<RDMnetLibNotifyInternal*>(context);
  if (notify)
    notify->LlrpRdmCommandReceived(handle, cmd);
}
}  // extern "C"

RDMnetLibWrapper::RDMnetLibWrapper(ControllerLog* log) : log_(log)
{
}

bool RDMnetLibWrapper::Startup(const LwpaUuid& cid, RDMnetLibNotify* notify)
{
  if (!running_)
  {
    my_cid_ = cid;
    notify_ = notify;

    // Initialize the RDMnet controller library
    lwpa_error_t res = rdmnet_controller_init(log_ ? log_->GetLogParams() : nullptr);
    if (res != kLwpaErrOk)
    {
      if (log_)
        log_->Log(LWPA_LOG_ERR, "Error initializing RDMnet core library: '%s'", lwpa_strerror(res));
      return false;
    }

    // Create our controller handle in the RDMnet library
    RdmnetControllerConfig config;
    RDMNET_CONTROLLER_CONFIG_INIT(&config, 0x6574);
    config.cid = my_cid_;
    // clang-format off
    config.callbacks = {
      controllercb_connected,
      controllercb_connect_failed,
      controllercb_disconnected,
      controllercb_client_list_update,
      controllercb_rdm_response_received,
      controllercb_rdm_command_received,
      controllercb_status_received,
      controllercb_llrp_rdm_command_received
    };
    // clang-format on
    config.callback_context = static_cast<RDMnetLibNotifyInternal*>(this);

    res = rdmnet_controller_create(&config, &controller_handle_);
    if (res != kLwpaErrOk)
    {
      if (log_)
        log_->Log(LWPA_LOG_ERR, "Error creating an RDMnet Controller handle: '%s'", lwpa_strerror(res));
      rdmnet_controller_deinit();
      return false;
    }

    running_ = true;
  }
  return true;
}

void RDMnetLibWrapper::Shutdown()
{
  if (running_)
  {
    rdmnet_controller_destroy(controller_handle_);
    rdmnet_controller_deinit();
    running_ = false;
    notify_ = nullptr;
    my_cid_ = kLwpaNullUuid;
  }
}

rdmnet_client_scope_t RDMnetLibWrapper::AddScope(const std::string& scope, StaticBrokerConfig static_broker)
{
  // Check if the scope is too long
  if (scope.length() >= E133_SCOPE_STRING_PADDED_LENGTH)
    return RDMNET_CLIENT_SCOPE_INVALID;

  RdmnetScopeConfig config;
  RDMNET_CLIENT_SET_SCOPE(&config, scope.c_str());
  if (static_broker.valid)
  {
    config.has_static_broker_addr = true;
    config.static_broker_addr = static_broker.addr;
  }
  else
  {
    config.has_static_broker_addr = false;
  }

  rdmnet_client_scope_t new_scope_handle;
  lwpa_error_t res = rdmnet_controller_add_scope(controller_handle_, &config, &new_scope_handle);
  if (res == kLwpaErrOk)
  {
    if (log_)
      log_->Log(LWPA_LOG_INFO, "RDMnet scope '%s' added with handle %d.", scope.c_str(), new_scope_handle);
    return new_scope_handle;
  }
  else
  {
    if (log_)
      log_->Log(LWPA_LOG_ERR, "Error adding new RDMnet scope '%s': '%s'", scope.c_str(), lwpa_strerror(res));
    return RDMNET_CLIENT_SCOPE_INVALID;
  }
}

bool RDMnetLibWrapper::RemoveScope(rdmnet_client_scope_t scope_handle, rdmnet_disconnect_reason_t reason)
{
  lwpa_error_t res = rdmnet_controller_remove_scope(controller_handle_, scope_handle, reason);
  if (res == kLwpaErrOk)
  {
    if (log_)
      log_->Log(LWPA_LOG_INFO, "RDMnet scope with handle %d removed.", scope_handle);
    return true;
  }
  else
  {
    if (log_)
      log_->Log(LWPA_LOG_ERR, "Error removing RDMnet scope with handle %d: '%s'", scope_handle, lwpa_strerror(res));
    return false;
  }
}

bool RDMnetLibWrapper::SendRdmCommand(rdmnet_client_scope_t scope_handle, const LocalRdmCommand& cmd)
{
  return (kLwpaErrOk == rdmnet_controller_send_rdm_command(controller_handle_, scope_handle, &cmd, nullptr));
}

bool RDMnetLibWrapper::SendRdmCommand(rdmnet_client_scope_t scope_handle, const LocalRdmCommand& cmd, uint32_t& seq_num)
{
  return (kLwpaErrOk == rdmnet_controller_send_rdm_command(controller_handle_, scope_handle, &cmd, &seq_num));
}

bool RDMnetLibWrapper::SendRdmResponse(rdmnet_client_scope_t scope_handle, const LocalRdmResponse& resp)
{
  return (kLwpaErrOk == rdmnet_controller_send_rdm_response(controller_handle_, scope_handle, &resp));
}

bool RDMnetLibWrapper::SendLlrpResponse(const LlrpLocalRdmResponse& resp)
{
  return (kLwpaErrOk == rdmnet_controller_send_llrp_response(controller_handle_, &resp));
}

bool RDMnetLibWrapper::RequestClientList(rdmnet_client_scope_t scope_handle)
{
  return (kLwpaErrOk == rdmnet_controller_request_client_list(controller_handle_, scope_handle));
}

void RDMnetLibWrapper::Connected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                 const RdmnetClientConnectedInfo* info)
{
  if (notify_ && handle == controller_handle_ && info)
  {
    notify_->Connected(scope, *info);
  }
}

void RDMnetLibWrapper::ConnectFailed(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                     const RdmnetClientConnectFailedInfo* info)
{
  if (notify_ && handle == controller_handle_ && info)
  {
    notify_->ConnectFailed(scope, *info);
  }
}

void RDMnetLibWrapper::Disconnected(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                    const RdmnetClientDisconnectedInfo* info)
{
  if (notify_ && handle == controller_handle_ && info)
  {
    notify_->Disconnected(scope, *info);
  }
}

void RDMnetLibWrapper::ClientListUpdate(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                        client_list_action_t list_action, const ClientList* list)
{
  if (notify_ && handle == controller_handle_ && list)
  {
    notify_->ClientListUpdate(scope, list_action, *list);
  }
}

void RDMnetLibWrapper::RdmResponseReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                           const RemoteRdmResponse* resp)
{
  if (notify_ && handle == controller_handle_ && resp)
  {
    notify_->RdmResponseReceived(scope, *resp);
  }
}

void RDMnetLibWrapper::RdmCommandReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                          const RemoteRdmCommand* cmd)
{
  if (notify_ && handle == controller_handle_ && cmd)
  {
    notify_->RdmCommandReceived(scope, *cmd);
  }
}

void RDMnetLibWrapper::StatusReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope,
                                      const RemoteRptStatus* status)
{
  if (notify_ && handle == controller_handle_ && status)
  {
    notify_->StatusReceived(scope, *status);
  }
}

void RDMnetLibWrapper::LlrpRdmCommandReceived(rdmnet_controller_t handle, const LlrpRemoteRdmCommand* cmd)
{
  if (notify_ && handle == controller_handle_ && cmd)
  {
    notify_->LlrpRdmCommandReceived(*cmd);
  }
}
