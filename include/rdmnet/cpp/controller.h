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

/// \ingroup rdmnet_controller_cpp
/// \brief A base class for a class that receives RDM commands addressed to a controller.
///
/// This is an optional portion of the controller API. See \ref using_controller for details.
class ControllerRdmCommandHandler
{
public:
  virtual void HandleRdmCommand(Controller& controller, ScopeHandle scope, const RemoteRdmCommand& cmd) = 0;
  virtual void HandleLlrpRdmCommand(Controller& controller, const LlrpRemoteRdmCommand& cmd) = 0;
};

/// \ingroup rdmnet_controller_cpp
/// \brief A base class for a class that receives notification callbacks from a controller.
///
/// See \ref using_controller for details of how to use this API.
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
  virtual void HandleRptStatus(Controller& controller, ScopeHandle scope, const RemoteRptStatus& status) = 0;
};

struct ControllerData
{
  etcpal::Uuid cid;
  rdm::Uid uid;
  std::string search_domain;

  ControllerData() = default;
  ControllerData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in, const std::string& search_domain_in);
  static ControllerData Default(uint16_t manufacturer_id);
};

inline ControllerData::ControllerData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in,
                                      const std::string& search_domain_in)
    : cid(cid_in), uid(uid_in), search_domain(search_domain_in)
{
}

inline ControllerData ControllerData::Default(uint16_t manufacturer_id)
{
  return ControllerData(etcpal::Uuid::OsPreferred(), rdm::Uid::DynamicUidRequest(manufacturer_id), E133_DEFAULT_DOMAIN);
}

struct ControllerRdmData
{
  std::string manufacturer_label;
  std::string device_model_description;
  std::string software_version_label;
  std::string device_label;
  bool device_label_settable{true};

  ControllerRdmData() = default;
  ControllerRdmData(const std::string& manufacturer_label_in, const std::string& device_model_description_in,
                    const std::string& software_version_label_in, const std::string& device_label_in);

  bool IsValid() const;
};

inline ControllerRdmData::ControllerRdmData(const std::string& manufacturer_label_in,
                                            const std::string& device_model_description_in,
                                            const std::string& software_version_label_in,
                                            const std::string& device_label_in)
    : manufacturer_label(manufacturer_label_in)
    , device_model_description(device_model_description_in)
    , software_version_label(software_version_label_in)
    , device_label(device_label_in)
{
}

inline bool ControllerRdmData::IsValid() const
{
  return !(manufacturer_label.empty() && device_model_description.empty() && software_version_label.empty() &&
           device_label.empty());
}

/// \ingroup rdmnet_controller_cpp
/// \brief An instance of RDMnet Controller functionality.
///
/// See \ref using_controller for details of how to use this API.
class Controller
{
public:
  etcpal::Result Startup(ControllerNotifyHandler& notify_handler, const ControllerRdmData& rdm_data,
                         const ControllerData& data);
  etcpal::Result Startup(ControllerNotifyHandler& notify_handler, ControllerRdmCommandHandler& rdm_handler,
                         const ControllerData& data);
  void Shutdown();

  etcpal::Expected<ScopeHandle> AddDefaultScope(const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Expected<ScopeHandle> AddScope(const Scope& scope);
  etcpal::Result RemoveScope(ScopeHandle handle);

  etcpal::Expected<uint32_t> SendRdmCommand(ScopeHandle scope, const LocalRdmCommand& cmd);
  etcpal::Result SendRdmResponse(ScopeHandle scope, const LocalRdmResponse& resp);
  etcpal::Result SendLlrpResponse(const LlrpLocalRdmResponse& resp);
  etcpal::Result RequestClientList(ScopeHandle scope);

  ControllerHandle handle() const;
  const ControllerData& data() const;
  const ControllerRdmData& rdm_data() const;

  void UpdateRdmData(const ControllerRdmData& new_data);

  static etcpal::Result Init(
      const EtcPalLogParams* log_params = nullptr,
      const std::vector<RdmnetMcastNetintId>& mcast_netints = std::vector<RdmnetMcastNetintId>{});
  static etcpal::Result Init(const etcpal::Logger& logger, const std::vector<RdmnetMcastNetintId>& mcast_netints =
                                                               std::vector<RdmnetMcastNetintId>{});
  static void Deinit();

private:
  ControllerHandle handle_{RDMNET_CONTROLLER_INVALID};
  ControllerData my_data_;
  ControllerRdmData my_rdm_data_;

  ControllerRdmCommandHandler* rdm_cmd_handler_{nullptr};
  ControllerNotifyHandler* notify_{nullptr};
};

inline etcpal::Result Controller::Startup(ControllerNotifyHandler& notify_handler, const ControllerRdmData& rdm_data,
                                          const ControllerData& data)
{
  return kEtcPalErrNotImpl;
}

inline etcpal::Result Controller::Startup(ControllerNotifyHandler& notify_handler,
                                          ControllerRdmCommandHandler& rdm_handler, const ControllerData& data)
{
  return kEtcPalErrNotImpl;
}

inline void Controller::Shutdown()
{
}

/// \brief Initialize the RDMnet Controller library.
/// \param log_params Optional logging configuration for the RDMnet library. If nullptr, no
///                   messages will be logged.
/// \param mcast_netints Optional set of network interfaces on which to operate RDMnet's multicast
///                      protocols. If left at default, all interfaces will be used.
/// \return kEtcPalErrOk: Initialization successful.
/// \return Error codes from rdmnet_client_init().
inline etcpal::Result Controller::Init(const EtcPalLogParams* log_params,
                                       const std::vector<RdmnetMcastNetintId>& mcast_netints)
{
  return kEtcPalErrNotImpl;
}

inline etcpal::Result Controller::Init(const etcpal::Logger& logger,
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

inline etcpal::Expected<ScopeHandle> Controller::AddScope(const Scope& scope)
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

inline ControllerHandle Controller::handle() const
{
  return handle_;
}

inline const ControllerData& Controller::data() const
{
  return my_data_;
}
};  // namespace rdmnet

#endif  // RDMNET_CPP_CONTROLLER_H_
