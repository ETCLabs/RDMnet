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
#include "etcpal/cpp/log.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/controller.h"
#include "rdmnet/cpp/client.h"

namespace rdmnet
{
/// \defgroup rdmnet_controller_cpp Controller C++ Language API
/// \ingroup rdmnet_controller
/// \brief C++ Language version of the Controller API
/// @{

using ControllerHandle = rdmnet_controller_t;

/// @}

class Controller;

/// \brief A base class for a class that receives notification callbacks from a controller.
/// \ingroup rdmnet_controller_cpp
class ControllerNotifyHandler
{
public:
  virtual void HandleConnectedToBroker(Controller& controller, ScopeHandle scope,
                                       const RdmnetClientConnectedInfo& info) = 0;
  virtual void HandleBrokerConnectFailed(Controller& controller, ScopeHandle scope,
                                         const RdmnetClientConnectFailedInfo& info) = 0;
  virtual void HandleDisconnectedFromBroker(Controller& controller, ScopeHandle scope,
                                            const RdmnetClientDisconnectedInfo& info) = 0;
  virtual void HandleClientListUpdate(Controller& controller, ScopeHandle scope, client_list_action_t list_action,
                                      const ClientList& list) = 0;
  virtual void HandleRdmResponse(Controller& controller, ScopeHandle scope, const RemoteRdmResponse& resp) = 0;
  virtual void HandleRdmCommand(Controller& controller, ScopeHandle scope, const RemoteRdmCommand& cmd) = 0;
  virtual void HandleRptStatus(Controller& controller, ScopeHandle scope, const RemoteRptStatus& status) = 0;
  virtual void HandleLlrpRdmCommand(Controller& controller, const LlrpRemoteRdmCommand& cmd) = 0;
};

/// \brief An instance of RDMnet Controller functionality.
/// \ingroup rdmnet_controller_cpp
class Controller
{
public:
  etcpal::Result Startup(const etcpal::Uuid& cid, ControllerNotifyHandler& notify_handler);
  void Shutdown();

  etcpal::Expected<ScopeHandle> AddDefaultScope(const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Expected<ScopeHandle> AddScope(const Scope& scope);
  etcpal::Result RemoveScope(ScopeHandle handle);

  etcpal::Expected<uint32_t> SendRdmCommand(ScopeHandle scope, const LocalRdmCommand& cmd);
  etcpal::Result SendRdmResponse(ScopeHandle scope, const LocalRdmResponse& resp);
  etcpal::Result SendLlrpResponse(const LlrpLocalRdmResponse& resp);
  etcpal::Result RequestClientList(ScopeHandle scope);

  ControllerHandle handle() const;
  const etcpal::Uuid& cid() const;
  etcpal::Expected<Scope> scope(ScopeHandle handle) const;
  const rdm::Uid& uid() const;

  void SetUid(const rdm::Uid& uid);
  void SetSearchDomain(const std::string& search_domain);

  static etcpal::Result Init(
      const EtcPalLogParams* log_params = nullptr,
      const std::vector<RdmnetMcastNetintId>& mcast_netints = std::vector<RdmnetMcastNetintId>{});
  static etcpal::Result Init(const etcpal::Logger& logger, const std::vector<RdmnetMcastNetintId>& mcast_netints =
                                                               std::vector<RdmnetMcastNetintId>{});
  static void Deinit();

private:
  ControllerNotifyHandler* notify_{nullptr};
  ControllerHandle handle_{RDMNET_CONTROLLER_INVALID};
  etcpal::Uuid cid_;
  rdm::Uid uid_;
  std::string search_domain_;
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
inline etcpal::Result Controller::Init(const EtcPalLogParams* log_params,
                                       const std::vector<RdmnetMcastNetintId>& mcast_netints)
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

inline const rdm::Uid& Controller::uid() const
{
  return uid_;
}

inline Controller& SetUid(const rdm::Uid& uid)
{
  uid_ = uid;
  return *this;
}

inline Controller& SetSearchDomain(const std::string& search_domain)
{
  search_domain_ = search_domain;
  return *this;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_CONTROLLER_H_
