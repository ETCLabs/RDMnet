#include "rdmnet_lib_wrapper.h"

extern "C" {

static void devicecb_connected(rdmnet_device_t handle, const RdmnetClientConnectedInfo *info, void *context)
{
  RdmnetLibWrapper *wrapper = static_cast<RdmnetLibWrapper *>(context);
  if (wrapper)
  {
    wrapper->LibNotifyConnected(handle, info);
  }
}

static void devicecb_connect_failed(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo *info, void *context)
{
  RdmnetLibWrapper *wrapper = static_cast<RdmnetLibWrapper *>(context);
  if (wrapper)
  {
    wrapper->LibNotifyConnectFailed(handle, info);
  }
}

static void devicecb_disconnected(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo *info, void *context)
{
  RdmnetLibWrapper *wrapper = static_cast<RdmnetLibWrapper *>(context);
  if (wrapper)
  {
    wrapper->LibNotifyDisconnected(handle, info);
  }
}

static void devicecb_rdm_command_received(rdmnet_device_t handle, const RemoteRdmCommand *cmd, void *context)
{
  RdmnetLibWrapper *wrapper = static_cast<RdmnetLibWrapper *>(context);
  if (wrapper)
  {
    wrapper->LibNotifyRdmCommandReceived(handle, cmd);
  }
}

static void devicecb_llrp_rdm_command_received(rdmnet_device_t handle, const LlrpRemoteRdmCommand *cmd, void *context)
{
  RdmnetLibWrapper *wrapper = static_cast<RdmnetLibWrapper *>(context);
  if (wrapper)
  {
    wrapper->LibNotifyLlrpRdmCommandReceived(handle, cmd);
  }
}
}

lwpa_error_t RdmnetLibWrapper::Startup(const LwpaUuid &cid, const RdmnetScopeConfig &scope_config,
                                       RdmnetLibNotify *notify, FakewayLog *log)
{
  my_cid_ = cid;
  notify_ = notify;
  log_ = log;

  // Initialize the Rdmnet device library
  lwpa_error_t res = rdmnet_device_init(log_ ? log_->params() : nullptr);
  if (res != kLwpaErrOk)
  {
    if (log_)
      log_->Log(LWPA_LOG_ERR, "Error initializing RDMnet core library: '%s'", lwpa_strerror(res));
    return res;
  }

  // Create our device handle in the RDMnet library
  RdmnetDeviceConfig config;
  RDMNET_DEVICE_CONFIG_INIT(&config, 0x6574);
  config.cid = my_cid_;
  config.scope_config = scope_config;
  // clang-format off
  config.callbacks = {
    devicecb_connected,
    devicecb_connect_failed,
    devicecb_disconnected,
    devicecb_rdm_command_received,
    devicecb_llrp_rdm_command_received
  };
  // clang-format on
  config.callback_context = this;

  res = rdmnet_device_create(&config, &device_handle_);
  if (res != kLwpaErrOk)
  {
    if (log_)
      log_->Log(LWPA_LOG_ERR, "Error creating an RDMnet Device handle: '%s'", lwpa_strerror(res));
    rdmnet_device_deinit();
    return res;
  }

  return res;
}

void RdmnetLibWrapper::Shutdown()
{
  rdmnet_device_destroy(device_handle_);
  rdmnet_device_deinit();
  notify_ = nullptr;
  my_cid_ = kLwpaNullUuid;
}

bool RdmnetLibWrapper::SendRdmResponse(const LocalRdmResponse &resp)
{
  return (kLwpaErrOk == rdmnet_device_send_rdm_response(device_handle_, &resp));
}

bool RdmnetLibWrapper::SendStatus(const LocalRptStatus &status)
{
  return (kLwpaErrOk == rdmnet_device_send_status(device_handle_, &status));
}

bool RdmnetLibWrapper::SendLlrpResponse(const LlrpLocalRdmResponse &resp)
{
  return (kLwpaErrOk == rdmnet_device_send_llrp_response(device_handle_, &resp));
}

bool RdmnetLibWrapper::ChangeScope(const RdmnetScopeConfig &new_scope_config, rdmnet_disconnect_reason_t reason)
{
  return (kLwpaErrOk == rdmnet_device_change_scope(device_handle_, &new_scope_config, reason));
}

bool RdmnetLibWrapper::ChangeSearchDomain(const std::string &new_search_domain, rdmnet_disconnect_reason_t reason)
{
  return (kLwpaErrOk == rdmnet_device_change_search_domain(device_handle_, new_search_domain.c_str(), reason));
}

void RdmnetLibWrapper::LibNotifyConnected(rdmnet_device_t handle, const RdmnetClientConnectedInfo *info)
{
  if (notify_ && handle == device_handle_ && info)
  {
    notify_->Connected(*info);
  }
}

void RdmnetLibWrapper::LibNotifyConnectFailed(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo *info)
{
  if (notify_ && handle == device_handle_ && info)
  {
    notify_->ConnectFailed(*info);
  }
}

void RdmnetLibWrapper::LibNotifyDisconnected(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo *info)
{
  if (notify_ && handle == device_handle_ && info)
  {
    notify_->Disconnected(*info);
  }
}

void RdmnetLibWrapper::LibNotifyRdmCommandReceived(rdmnet_device_t handle, const RemoteRdmCommand *cmd)
{
  if (notify_ && handle == device_handle_ && cmd)
  {
    notify_->RdmCommandReceived(*cmd);
  }
}

void RdmnetLibWrapper::LibNotifyLlrpRdmCommandReceived(rdmnet_device_t handle, const LlrpRemoteRdmCommand *cmd)
{
  if (notify_ && handle == device_handle_ && cmd)
  {
    notify_->LlrpRdmCommandReceived(*cmd);
  }
}
