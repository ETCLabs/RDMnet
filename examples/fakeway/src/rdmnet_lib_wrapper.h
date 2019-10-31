#include "rdmnet/device.h"
#include "fakeway_log.h"

class RdmnetLibNotify
{
public:
  virtual void Connected(const RdmnetClientConnectedInfo &info) = 0;
  virtual void ConnectFailed(const RdmnetClientConnectFailedInfo &info) = 0;
  virtual void Disconnected(const RdmnetClientDisconnectedInfo &info) = 0;
  virtual void RdmCommandReceived(const RemoteRdmCommand &cmd) = 0;
  virtual void LlrpRdmCommandReceived(const LlrpRemoteRdmCommand &cmd) = 0;
};

class RdmnetLibInterface
{
public:
  virtual lwpa_error_t Startup(const LwpaUuid &cid, const RdmnetScopeConfig &scope_config, RdmnetLibNotify *notify,
                               FakewayLog *log) = 0;
  virtual void Shutdown() = 0;

  virtual bool SendRdmResponse(const LocalRdmResponse &resp) = 0;
  virtual bool SendStatus(const LocalRptStatus &status) = 0;
  virtual bool SendLlrpResponse(const LlrpLocalRdmResponse &resp) = 0;
  virtual bool ChangeScope(const RdmnetScopeConfig &new_scope_config, rdmnet_disconnect_reason_t reason) = 0;
  virtual bool ChangeSearchDomain(const std::string &new_search_domain, rdmnet_disconnect_reason_t reason) = 0;
};

class RdmnetLibWrapper : public RdmnetLibInterface
{
public:
  lwpa_error_t Startup(const LwpaUuid &cid, const RdmnetScopeConfig &scope_config, RdmnetLibNotify *notify,
                       FakewayLog *log) override;
  void Shutdown() override;

  bool SendRdmResponse(const LocalRdmResponse &resp) override;
  bool SendStatus(const LocalRptStatus &status) override;
  bool SendLlrpResponse(const LlrpLocalRdmResponse &resp) override;
  bool ChangeScope(const RdmnetScopeConfig &new_scope_config, rdmnet_disconnect_reason_t reason) override;
  bool ChangeSearchDomain(const std::string &new_search_domain, rdmnet_disconnect_reason_t reason) override;

  void LibNotifyConnected(rdmnet_device_t handle, const RdmnetClientConnectedInfo *info);
  void LibNotifyConnectFailed(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo *info);
  void LibNotifyDisconnected(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo *info);
  void LibNotifyRdmCommandReceived(rdmnet_device_t handle, const RemoteRdmCommand *cmd);
  void LibNotifyLlrpRdmCommandReceived(rdmnet_device_t handle, const LlrpRemoteRdmCommand *cmd);

private:
  LwpaUuid my_cid_;

  rdmnet_device_t device_handle_{nullptr};

  FakewayLog *log_{nullptr};
  RdmnetLibNotify *notify_{nullptr};
};
