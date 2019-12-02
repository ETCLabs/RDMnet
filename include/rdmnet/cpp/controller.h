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

/// \file rdmnet/cpp/controller.h
/// \brief C++ wrapper for the RDMnet Controller API

#ifndef RDMNET_CPP_CONTROLLER_H_
#define RDMNET_CPP_CONTROLLER_H_

#include <string>
#include <vector>
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/log.h"
#include "rdmnet/controller.h"

namespace rdmnet
{
/// \defgroup rdmnet_controller_cpp Controller C++ Language API
/// \ingroup rdmnet_controller
/// \brief C++ Language version of the Controller API
/// @{

using ControllerHandle = rdmnet_controller_t;
using ScopeHandle = rdmnet_client_scope_t;

/// @}

/// \brief A base class for a class that receives notification callbacks from a controller.
/// \ingroup rdmnet_controller_cpp
class ControllerNotify
{
public:
  virtual void Connected(ScopeHandle scope, const RdmnetClientConnectedInfo& info) = 0;
  virtual void ConnectFailed(ScopeHandle scope, const RdmnetClientConnectFailedInfo& info) = 0;
  virtual void Disconnected(ScopeHandle scope, const RdmnetClientDisconnectedInfo& info) = 0;
  virtual void ClientListUpdateReceived(ScopeHandle scope, client_list_action_t list_action,
                                        const ClientList& list) = 0;
  virtual void RdmResponseReceived(ScopeHandle scope, const RemoteRdmResponse& resp) = 0;
  virtual void RdmCommandReceived(Scopehandle scope, const RemoteRdmCommand& cmd) = 0;
  virtual void StatusReceived(ScopeHandle scope, const RemoteRptStatus& status) = 0;
  virtual void LlrpRdmCommandReceived(const LlrpRemoteRdmCommand& cmd) = 0;
};

/// \brief An instance of RDMnet Controller functionality.
/// \ingroup rdmnet_controller_cpp
class Controller
{
public:
  etcpal::Result Startup(const etcpal::Uuid& cid, ControllerNotify& notify, RdmUid& uid = RdmUid{},
                         const std::string& search_domain = std::string{},
                         const std::vector<LlrpNetintId>& llrp_netints = std::vector<LlrpNetintId>{});
  void Shutdown();

  etcpal::Expected<ScopeHandle> AddDefaultScope(const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Expected<ScopeHandle> AddScope(const std::string& scope_str,
                                         const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Result RemoveScope(ScopeHandle handle);

  etcpal::Expected<uint32_t> SendRdmCommand(ScopeHandle scope, const LocalRdmCommand& cmd);
  etcpal::Result SendRdmResponse(ScopeHandle scope, const LocalRdmResponse& resp);
  etcpal::Result SendLlrpResponse(const LlrpLocalRdmResponse& resp);
  etcpal::Result RequestClientList(ScopeHandle scope);

  ControllerHandle handle() const;
  const etcpal::Uuid& cid() const;

  static etcpal::Result Init(const EtcPalLogParams* log_params);
  static void Deinit();

private:
  ControllerHandle handle_{};
  etcpal::Uuid cid_;
};

inline etcpal::Result Controller::Startup(const etcpal::Uuid& cid);
{
  return kEtcPalErrNotImpl;
}

inline void Controller::Shutdown()
{
}

/// \brief Initialize the RDMnet Controller library.
/// \param log_params Optional logging configuration for the RDMnet library.
/// \return kEtcPalErrOk: Initialization successful.
/// \return Error codes from rdmnet_client_init().
inline etcpal::Result Controller::Init(const EtcPalLogParams* log_params)
{
  return kEtcPalErrNotImpl;
}

/// \brief Deinitialize the RDMnet Controller library.
inline void Controller::Deinit()
{
}

inline etcpal::Expected<ScopeHandle> Controller::AddDefaultScope(const etcpal::SockAddr& static_broker_addr)
{
  return kEtcPalErrNotImpl;
}

inline etcpal::Expected<ScopeHandle> Controller::AddScope(const std::string& scope_str,
                                                          const etcpal::SockAddr& static_broker_addr)
{
  return kEtcPalErrNotImpl;
}

inline etcpal::Result Controller::RemoveScope(ScopeHandle handle)
{
  return kEtcPalErrNotImpl;
}

inline etcpal::Expected<uint32_t> Controller::SendRdmCommand(ScopeHandle scope, const LocalRdmCommand& cmd)
{
  return kEtcPalErrNotImpl;
}

inline etcpal::Result Controller::SendRdmResponse(ScopeHandle scope, const LocalRdmResponse& resp)
{
  return kEtcPalErrNotImpl;
}

inline etcpal::Result Controller::SendLlrpResponse(const LlrpLocalRdmResponse& resp)
{
  return kEtcPalErrNotImpl;
}

inline etcpal::Result Controller::RequestClientList(ScopeHandle scope)
{
  return kEtcPalErrNotImpl;
}

inline ControllerHandle Controller::handle()
{
  return handle_;
}

inline const etcpal::Uuid& Controller::cid() const
{
  return cid_;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_CONTROLLER_H_
