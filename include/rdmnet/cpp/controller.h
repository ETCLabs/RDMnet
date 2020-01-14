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
///
/// See \ref using_controller for details of how to use this API.
///
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
  /// \brief An RDM command has been received addressed to a controller.
  /// \param controller Controller instance which has received the RDM command.
  /// \param scope_handle Handle to the scope on which the RDM command was received.
  /// \param cmd The RDM command data.
  virtual void HandleRdmCommand(Controller& controller, ScopeHandle scope_handle, const RemoteRdmCommand& cmd) = 0;

  /// \brief An RDM command has been received over LLRP, addressed to a controller.
  /// \param controller Controller instance which has received the RDM command.
  /// \param cmd The RDM command data.
  virtual void HandleLlrpRdmCommand(Controller& controller, const LlrpRemoteRdmCommand& cmd) = 0;
};

/// \ingroup rdmnet_controller_cpp
/// \brief A base class for a class that receives notification callbacks from a controller.
///
/// See \ref using_controller for details of how to use this API.
class ControllerNotifyHandler
{
public:
  /// \brief A controller has successfully connected to a broker.
  /// \param controller Controller instance which has connected.
  /// \param scope_handle Handle to the scope on which the controller has connected.
  /// \param info More information about the successful connection.
  virtual void HandleConnectedToBroker(Controller& controller, ScopeHandle scope_handle,
                                       const RdmnetClientConnectedInfo& info) = 0;

  /// \brief A connection attempt failed between a controller and a broker.
  /// \param controller Controller instance which has failed to connect.
  /// \param scope_handle Handle to the scope on which the connection failed.
  /// \param info More information about the failed connection.
  virtual void HandleBrokerConnectFailed(Controller& controller, ScopeHandle scope_handle,
                                         const RdmnetClientConnectFailedInfo& info) = 0;

  /// \brief A controller which was previously connected to a broker has disconnected.
  /// \param controller Controller instance which has disconnected.
  /// \param scope_handle Handle to the scope on which the disconnect occurred.
  /// \param info More information about the disconnect event.
  virtual void HandleDisconnectedFromBroker(Controller& controller, ScopeHandle scope_handle,
                                            const RdmnetClientDisconnectedInfo& info) = 0;

  /// \brief A client list update has been received from a broker.
  /// \param controller Controller instance which has received the client list update.
  /// \param scope_handle Handle to the scope on which the client list update was received.
  /// \param list_action The way the updates in client_list should be applied to the controller's
  ///                    cached list.
  /// \param list The list of updates.
  virtual void HandleClientListUpdate(Controller& controller, ScopeHandle scope_handle,
                                      client_list_action_t list_action, const RptClientList& list) = 0;

  /// \brief An RDM response has been received.
  /// \param controller Controller instance which has received the RDM response.
  /// \param scope_handle Handle to the scope on which the RDM response was received.
  /// \param resp The RDM response data.
  virtual void HandleRdmResponse(Controller& controller, ScopeHandle scope_handle, const RemoteRdmResponse& resp) = 0;

  /// \brief An RPT status message has been received in response to a previously-sent RDM command.
  /// \param controller Controller instance which has received the RPT status message.
  /// \param scope_handle Handle to the scope on which the RPT status message was received.
  /// \param status The RPT status data.
  virtual void HandleRptStatus(Controller& controller, ScopeHandle scope_handle, const RemoteRptStatus& status) = 0;
};

/// A set of configuration data that a controller needs to initialize.
struct ControllerData
{
  etcpal::Uuid cid;           ///< The controller's Component Identifier (CID).
  rdm::Uid uid;               ///< The controller's RDM UID. For a dynamic UID, use ::rdm::Uid::DynamicUidRequest().
  std::string search_domain;  ///< The controller's search domain for discovering brokers.

  ControllerData() = default;
  ControllerData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in, const std::string& search_domain_in);
  static ControllerData Default(uint16_t manufacturer_id);
};

/// Create a ControllerData instance by passing all members explicitly.
inline ControllerData::ControllerData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in,
                                      const std::string& search_domain_in)
    : cid(cid_in), uid(uid_in), search_domain(search_domain_in)
{
}

/// \brief Create a default set of controller data.
///
/// Generates an ephemeral UUID to use as a CID and creates a dynamic UID based on the provided
/// ESTA manufacturer ID.
inline ControllerData ControllerData::Default(uint16_t manufacturer_id)
{
  return ControllerData(etcpal::Uuid::OsPreferred(), rdm::Uid::DynamicUidRequest(manufacturer_id), E133_DEFAULT_DOMAIN);
}

/// A set of initial identifying RDM data to use for a controller.
struct ControllerRdmData
{
  std::string manufacturer_label;        ///< The manufacturer name of the controller.
  std::string device_model_description;  ///< The name of the product model which implements the controller.
  std::string software_version_label;    ///< The software version of the controller as a string.
  std::string device_label;              ///< A user-settable name for this controller instance.
  bool device_label_settable{true};      ///< Whether the library should allow device_label to be changed remotely.

  /// Create an empty, invalid structure by default - must be filled in before passing to Controller::Startup().
  ControllerRdmData() = default;
  ControllerRdmData(const std::string& manufacturer_label_in, const std::string& device_model_description_in,
                    const std::string& software_version_label_in, const std::string& device_label_in);

  bool IsValid() const;
};

/// Create a ControllerRdmData instance by passing all members explicitly.
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

/// Whether this data is valid (all string members are non-empty).
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
  etcpal::Result Startup(ControllerNotifyHandler& notify_handler, const ControllerData& data,
                         const ControllerRdmData& rdm_data);
  etcpal::Result Startup(ControllerNotifyHandler& notify_handler, const ControllerData& data,
                         ControllerRdmCommandHandler& rdm_handler);
  void Shutdown(rdmnet_disconnect_reason_t disconnect_reason = kRdmnetDisconnectShutdown);

  etcpal::Expected<ScopeHandle> AddDefaultScope(const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Expected<ScopeHandle> AddScope(const Scope& scope);
  etcpal::Result RemoveScope(ScopeHandle handle);

  etcpal::Expected<uint32_t> SendRdmCommand(ScopeHandle scope, const LocalRdmCommand& cmd);
  etcpal::Expected<uint32_t> SendRdmCommand(ScopeHandle scope, const rdm::Uid& dest_uid, uint16_t dest_endpoint,
                                            const RdmCommand& rdm);
  etcpal::Result SendRdmResponse(ScopeHandle scope, const LocalRdmResponse& resp);
  etcpal::Result SendLlrpResponse(const LlrpLocalRdmResponse& resp);
  etcpal::Result RequestClientList(ScopeHandle scope);

  ControllerHandle handle() const;
  const ControllerData& data() const;
  const ControllerRdmData& rdm_data() const;
  ControllerNotifyHandler* notify_handler() const;
  ControllerRdmCommandHandler* rdm_command_handler() const;

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

/// \cond controller_c_callbacks
/// Callbacks from underlying controllerlibrary to be forwarded

#define RDMNET_GET_CONTROLLER_REF(handle_in, context_in)            \
  Controller& controller = *(static_cast<Controller*>(context_in)); \
  assert((handle_in) == controller.handle());

extern "C" inline void ControllerLibCbConnected(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                const RdmnetClientConnectedInfo* info, void* context)
{
  if (info && context)
  {
    RDMNET_GET_CONTROLLER_REF(handle, context);
    controller.notify_handler()->HandleConnectedToBroker(controller, scope_handle, *info);
  }
}

extern "C" inline void ControllerLibCbConnectFailed(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                    const RdmnetClientConnectFailedInfo* info, void* context)
{
  if (info && context)
  {
    RDMNET_GET_CONTROLLER_REF(handle, context);
    controller.notify_handler()->HandleBrokerConnectFailed(controller, scope_handle, *info);
  }
}

extern "C" inline void ControllerLibCbDisconnected(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                   const RdmnetClientDisconnectedInfo* info, void* context)
{
  if (info && context)
  {
    RDMNET_GET_CONTROLLER_REF(handle, context);
    controller.notify_handler()->HandleDisconnectedFromBroker(controller, scope_handle, *info);
  }
}

extern "C" inline void ControllerLibCbClientListUpdate(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                       client_list_action_t list_action, const RptClientList* list,
                                                       void* context)
{
  if (list && context)
  {
    RDMNET_GET_CONTROLLER_REF(handle, context);
    controller.notify_handler()->HandleClientListUpdate(controller, scope_handle, list_action, *list);
  }
}

extern "C" inline void ControllerLibCbRdmResponseReceived(rdmnet_controller_t handle,
                                                          rdmnet_client_scope_t scope_handle,
                                                          const RemoteRdmResponse* resp, void* context)
{
  if (resp && context)
  {
    RDMNET_GET_CONTROLLER_REF(handle, context);
    controller.notify_handler()->HandleRdmResponse(controller, scope_handle, *resp);
  }
}

extern "C" inline void ControllerLibCbStatusReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                     const RemoteRptStatus* status, void* context)
{
  if (status && context)
  {
    RDMNET_GET_CONTROLLER_REF(handle, context);
    controller.notify_handler()->HandleRptStatus(controller, scope_handle, *status);
  }
}

extern "C" inline void ControllerLibCbRdmCommandReceived(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                         const RemoteRdmCommand* cmd, void* context)
{
  if (cmd && context)
  {
    RDMNET_GET_CONTROLLER_REF(handle, context);
    controller.rdm_command_handler()->HandleRdmCommand(controller, scope_handle, *cmd);
  }
}

extern "C" inline void ControllerLibCbLlrpRdmCommandReceived(rdmnet_controller_t handle,
                                                             const LlrpRemoteRdmCommand* cmd, void* context)
{
  if (cmd && context)
  {
    RDMNET_GET_CONTROLLER_REF(handle, context);
    controller.rdm_command_handler()->HandleLlrpRdmCommand(controller, *cmd);
  }
}

/// \endcond

/// \brief Allocate resources and startup this controller with the given configuration.
///
/// This overload provides a set of RDM data to the library to use for the controller's RDM
/// responder. RDM commands addressed to the controller will be handled internally by the library.
///
/// \param notify_handler A class instance to handle callback notifications from this controller.
/// \param data RDMnet protocol data used by this controller. In most instances,
///             the value produced by ControllerData::Default() is sufficient.
/// \param rdm_data Data to identify this controller to other controllers on the network.
/// \return etcpal::Result::Ok(): Controller started successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return Errors forwarded from rdmnet_controller_create().
inline etcpal::Result Controller::Startup(ControllerNotifyHandler& notify_handler, const ControllerData& data,
                                          const ControllerRdmData& rdm_data)
{
  if (!rdm_data.IsValid())
    return kEtcPalErrInvalid;

  notify_ = &notify_handler;
  my_data_ = data;
  my_rdm_data_ = rdm_data;

  RdmnetControllerConfig config;
  rdmnet_controller_config_init(&config, data.uid.manufacturer_id());
  config.optional.uid = data.uid.get();
  config.optional.search_domain = data.search_domain.c_str();
  config.cid = data.cid.get();
  RDMNET_CONTROLLER_SET_CALLBACKS(&config, ControllerLibCbConnected, ControllerLibCbConnectFailed,
                                  ControllerLibCbDisconnected, ControllerLibCbClientListUpdate,
                                  ControllerLibCbRdmResponseReceived, ControllerLibCbStatusReceived, this);
  RDMNET_CONTROLLER_SET_RDM_DATA(&config, rdm_data.manufacturer_label.c_str(),
                                 rdm_data.device_model_description.c_str(), rdm_data.software_version_label.c_str(),
                                 rdm_data.device_label.c_str());
  return rdmnet_controller_create(&config, &handle_);
}

/// \brief Allocate resources and startup this controller with the given configuration.
///
/// This overload provides a notification handler to respond to RDM commands addressed to the
/// controller. You must implement a core set of RDM commands - see \ref using_controller for more
/// information.
///
/// \param notify_handler A class instance to handle callback notifications from this controller.
/// \param data RDMnet protocol data used by this controller. In most instances,
///             the value produced by ControllerData::Default() is sufficient.
/// \param rdm_handler A class instance to handle RDM commands addressed to this controller.
/// \return etcpal::Result::Ok(): Controller started successfully.
/// \return Errors forwarded from rdmnet_controller_create().
inline etcpal::Result Controller::Startup(ControllerNotifyHandler& notify_handler, const ControllerData& data,
                                          ControllerRdmCommandHandler& rdm_handler)
{
  notify_ = &notify_handler;
  rdm_cmd_handler_ = &rdm_handler;
  my_data_ = data;

  RdmnetControllerConfig config;
  rdmnet_controller_config_init(&config, data.uid.manufacturer_id());
  config.optional.uid = data.uid.get();
  config.optional.search_domain = data.search_domain.c_str();
  config.cid = data.cid.get();
  RDMNET_CONTROLLER_SET_CALLBACKS(&config, ControllerLibCbConnected, ControllerLibCbConnectFailed,
                                  ControllerLibCbDisconnected, ControllerLibCbClientListUpdate,
                                  ControllerLibCbRdmResponseReceived, ControllerLibCbStatusReceived, this);
  RDMNET_CONTROLLER_SET_RDM_CMD_CALLBACKS(&config, ControllerLibCbRdmCommandReceived,
                                          ControllerLibCbLlrpRdmCommandReceived);
  return rdmnet_controller_create(&config, &handle_);
}

/// \brief Shutdown this controller and deallocate resources.
///
/// Will disconnect all scopes to which this controller is currently connected, sending the
/// disconnect reason provided in the disconnect_reason parameter.
///
/// \param disconnect_reason Reason code for disconnecting from each scope.
inline void Controller::Shutdown(rdmnet_disconnect_reason_t disconnect_reason)
{
  rdmnet_controller_destroy(handle_, disconnect_reason);
  handle_ = RDMNET_CONTROLLER_INVALID;
}

/// \brief Initialize the RDMnet Controller library.
/// \param log_params Optional logging configuration for the RDMnet library. If nullptr, no
///                   messages will be logged.
/// \param mcast_netints Optional set of network interfaces on which to operate RDMnet's multicast
///                      protocols. If left at default, all interfaces will be used.
/// \return etcpal::Result::Ok(): Initialization successful.
/// \return Error codes from rdmnet_client_init().
inline etcpal::Result Controller::Init(const EtcPalLogParams* log_params,
                                       const std::vector<RdmnetMcastNetintId>& mcast_netints)
{
  if (!mcast_netints.empty())
  {
    RdmnetNetintConfig netint_config;
    netint_config.netint_arr = mcast_netints.data();
    netint_config.num_netints = mcast_netints.size();
    return rdmnet_controller_init(log_params, &netint_config);
  }
  else
  {
    return rdmnet_controller_init(log_params, nullptr);
  }
}

/// \brief Initialize the RDMnet Controller library.
/// \param logger Logger instance to gather log messages from the RDMnet library.
/// \param mcast_netints Optional set of network interfaces on which to operate RDMnet's multicast
///                      protocols. If left at default, all interfaces will be used.
/// \return etcpal::Result::Ok(): Initialization successful.
/// \return Error codes from rdmnet_client_init().
inline etcpal::Result Controller::Init(const etcpal::Logger& logger,
                                       const std::vector<RdmnetMcastNetintId>& mcast_netints)
{
  return Init(&logger.log_params(), mcast_netints);
}

/// \brief Deinitialize the RDMnet Controller library.
inline void Controller::Deinit()
{
  rdmnet_controller_deinit();
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

inline const ControllerRdmData& Controller::rdm_data() const
{
  return my_rdm_data_;
}

inline ControllerNotifyHandler* Controller::notify_handler() const
{
  return notify_;
}

inline ControllerRdmCommandHandler* Controller::rdm_command_handler() const
{
  return rdm_cmd_handler_;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_CONTROLLER_H_
