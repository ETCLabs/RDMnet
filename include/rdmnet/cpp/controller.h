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
#include "rdm/cpp/message.h"
#include "rdmnet/controller.h"
#include "rdmnet/cpp/client.h"

namespace rdmnet
{
/// \defgroup rdmnet_controller_cpp Controller API
/// \ingroup rdmnet_cpp_api
/// \brief Implementation of RDMnet controller functionality; see \ref using_controller.
///
/// RDMnet controllers are clients which originate RDM commands and receive responses. Controllers
/// can participate in multiple scopes; the default scope string "default" must be configured as a
/// default setting. This API provides classes tailored to the usage concerns of an RDMnet
/// controller.
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
  virtual void HandleRdmCommand(Controller& controller, ScopeHandle scope_handle,
                                const RdmnetRemoteRdmCommand& cmd) = 0;

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
  virtual void HandleRdmResponse(Controller& controller, ScopeHandle scope_handle,
                                 const RdmnetRemoteRdmResponse& resp) = 0;

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

  /// Create an empty, invalid data structure by default.
  ControllerData() = default;
  ControllerData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in, const std::string& search_domain_in);

  bool IsValid() const;

  static ControllerData Default(uint16_t manufacturer_id);
};

/// Create a ControllerData instance by passing all members explicitly.
inline ControllerData::ControllerData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in,
                                      const std::string& search_domain_in)
    : cid(cid_in), uid(uid_in), search_domain(search_domain_in)
{
}

/// Determine whether a ControllerData instance contains valid data for RDMnet operation.
bool ControllerData::IsValid() const
{
  return (!cid.IsNull() && (uid.IsStatic() || uid.IsDynamicUidRequest()) && !search_domain.empty());
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
/// \brief An instance of RDMnet controller functionality.
///
/// See \ref using_controller for details of how to use this API.
class Controller
{
public:
  etcpal::Error Startup(ControllerNotifyHandler& notify_handler, const ControllerData& data,
                        const ControllerRdmData& rdm_data);
  etcpal::Error Startup(ControllerNotifyHandler& notify_handler, const ControllerData& data,
                        ControllerRdmCommandHandler& rdm_handler);
  void Shutdown(rdmnet_disconnect_reason_t disconnect_reason = kRdmnetDisconnectShutdown);

  etcpal::Expected<ScopeHandle> AddDefaultScope(const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Expected<ScopeHandle> AddScope(const Scope& scope);
  etcpal::Expected<ScopeHandle> AddScope(const std::string& id,
                                         const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Error RemoveScope(ScopeHandle scope_handle, rdmnet_disconnect_reason_t disconnect_reason);

  etcpal::Expected<uint32_t> SendRdmCommand(ScopeHandle scope_handle, const RdmnetLocalRdmCommand& cmd);
  etcpal::Expected<uint32_t> SendRdmCommand(ScopeHandle scope_handle, const rdm::Uid& dest_uid, uint16_t dest_endpoint,
                                            const rdm::Command& rdm);
  etcpal::Error SendRdmResponse(ScopeHandle scope_handle, const RdmnetLocalRdmResponse& resp);
  etcpal::Error SendLlrpResponse(const LlrpLocalRdmResponse& resp);
  etcpal::Error RequestClientList(ScopeHandle scope_handle);

  ControllerHandle handle() const;
  const ControllerData& data() const;
  const ControllerRdmData& rdm_data() const;
  ControllerNotifyHandler* notify_handler() const;
  ControllerRdmCommandHandler* rdm_command_handler() const;
  etcpal::Expected<Scope> scope(ScopeHandle scope_handle) const;

  void UpdateRdmData(const ControllerRdmData& new_data);

  static etcpal::Error Init(const EtcPalLogParams* log_params = nullptr,
                            const std::vector<RdmnetMcastNetintId>& mcast_netints = std::vector<RdmnetMcastNetintId>{});
  static etcpal::Error Init(const etcpal::Logger& logger,
                            const std::vector<RdmnetMcastNetintId>& mcast_netints = std::vector<RdmnetMcastNetintId>{});
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
                                                          const RdmnetRemoteRdmResponse* resp, void* context)
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
                                                         const RdmnetRemoteRdmCommand* cmd, void* context)
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
/// \return etcpal::Error::Ok(): Controller started successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return Errors forwarded from rdmnet_controller_create().
inline etcpal::Error Controller::Startup(ControllerNotifyHandler& notify_handler, const ControllerData& data,
                                         const ControllerRdmData& rdm_data)
{
  if (!data.IsValid() || !rdm_data.IsValid())
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
                                 rdm_data.device_label.c_str(), rdm_data.device_label_settable);
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
/// \return etcpal::Error::Ok(): Controller started successfully.
/// \return Errors forwarded from rdmnet_controller_create().
inline etcpal::Error Controller::Startup(ControllerNotifyHandler& notify_handler, const ControllerData& data,
                                         ControllerRdmCommandHandler& rdm_handler)
{
  if (!data.IsValid())
    return kEtcPalErrInvalid;

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

/// \brief Shortcut to add the default RDMnet scope to a controller instance.
///
/// \param static_broker_addr [optional] A static broker address to configure for the default scope.
/// \return On success, a handle to the new scope, to be used with subsequent API calls.
/// \return On failure, error codes from rdmnet_controller_add_scope().
inline etcpal::Expected<ScopeHandle> Controller::AddDefaultScope(const etcpal::SockAddr& static_broker_addr)
{
  return kEtcPalErrNotImpl;
}

/// \brief Add a new scope to this controller instance.
///
/// The library will attempt to discover and connect to a broker for the scope (or just connect if
/// a static broker address is given); the status of these attempts will be communicated via
/// associated ControllerNotifyHandler.
///
/// \param scope Configuration information for the new scope.
/// \return On success, a handle to the new scope, to be used with subsequent API calls.
/// \return On failure, error codes from rdmnet_controller_add_scope().
inline etcpal::Expected<ScopeHandle> Controller::AddScope(const Scope& scope)
{
  return AddScope(scope.id(), scope.static_broker_addr());
}

/// \brief Add a new scope to this controller instance.
///
/// The library will attempt to discover and connect to a broker for the scope (or just connect if
/// a static broker address is given); the status of these attempts will be communicated via
/// associated ControllerNotifyHandler.
///
/// \param id The scope ID string.
/// \param static_broker_addr [optional] A static IP address and port at which to connect to the
///                           broker for this scope.
/// \return On success, a handle to the new scope, to be used with subsequent API calls.
/// \return On failure, error codes from rdmnet_controller_add_scope().
inline etcpal::Expected<ScopeHandle> Controller::AddScope(const std::string& id,
                                                          const etcpal::SockAddr& static_broker_addr)
{
  RdmnetScopeConfig scope_config = {id.c_str(), static_broker_addr.ip().IsValid(), static_broker_addr.get()};
  rdmnet_client_scope_t scope_handle;
  auto result = rdmnet_controller_add_scope(handle_, &scope_config, &scope_handle);
  return (result == kEtcPalErrOk ? scope_handle : result);
}

/// \brief Remove a previously-added scope from this controller instance.
///
/// After this call completes, scope_handle will no longer be valid.
///
/// \param scope_handle Handle to scope to remove.
/// \param disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
/// \return etcpal::Error::Ok(): Scope removed successfully.
/// \return Error codes from from rdmnet_controller_remove_scope().
inline etcpal::Error Controller::RemoveScope(ScopeHandle scope_handle, rdmnet_disconnect_reason_t disconnect_reason)
{
  return rdmnet_controller_remove_scope(handle_, scope_handle, disconnect_reason);
}

/// \brief Send an RDM command from a controller on a scope.
///
/// The response will be delivered via the ControllerNotifyHandler::HandleRdmResponse() callback.
///
/// \param scope_handle Handle to the scope on which to send the RDM command.
/// \param cmd The RDM command data to send, including its addressing information.
/// \return On success, a sequence number which can be used to match the command with a response.
/// \return On failure, error codes from rdmnet_controller_send_rdm_command().
inline etcpal::Expected<uint32_t> Controller::SendRdmCommand(ScopeHandle scope_handle, const RdmnetLocalRdmCommand& cmd)
{
  return kEtcPalErrNotImpl;
}

/// \brief Send an RDM command from a controller on a scope.
///
/// The response will be delivered via the ControllerNotifyHandler::HandleRdmResponse() callback.
///
/// \param scope_handle Handle to the scope on which to send the RDM command.
/// \param dest_uid The UID of the component to which to send the RDM command.
/// \param dest_endpoint The endpoint on the device to which to send the RDM command.
///                      E133_NULL_ENDPOINT addresses the default responder. If addressing another
///                      controller, this must always be E133_NULL_ENDPOINT.
/// \param rdm The RDM command data to send.
/// \return On success, a sequence number which can be used to match the command with a response.
/// \return On failure, error codes from rdmnet_controller_send_rdm_command().
inline etcpal::Expected<uint32_t> Controller::SendRdmCommand(ScopeHandle scope_handle, const rdm::Uid& dest_uid,
                                                             uint16_t dest_endpoint, const rdm::Command& rdm)
{
  return kEtcPalErrNotImpl;
}

/// \brief Send an RDM response from a controller on a scope.
/// \param scope_handle Handle to the scope on which to send the RDM response.
/// \param resp The RDM response data to send, including its addressing information.
/// \return etcpal::Error::Ok(): Response sent successfully.
/// \return Error codes from from rdmnet_controller_send_rdm_response().
inline etcpal::Error Controller::SendRdmResponse(ScopeHandle scope_handle, const RdmnetLocalRdmResponse& resp)
{
  return kEtcPalErrNotImpl;
}

/// \brief Send an LLRP RDM response from a controller on a scope.
/// \param resp The LLRP RDM response data to send, including its addressing information.
/// \return etcpal::Error::Ok(): Response sent successfully.
/// \return Error codes from from rdmnet_controller_send_llrp_rdm_response().
inline etcpal::Error Controller::SendLlrpResponse(const LlrpLocalRdmResponse& resp)
{
  return kEtcPalErrNotImpl;
}

/// \brief Request a client list from a broker.
///
/// The response will be delivered via the ControllerNotifyHandler::HandleClientListUpdate()
/// callback.
///
/// \param[in] scope_handle Handle to the scope on which to request the client list.
/// \return etcpal::Error::Ok(): Request sent successfully.
/// \return Errors codes from rdmnet_controller_request_client_list().
inline etcpal::Error Controller::RequestClientList(ScopeHandle scope_handle)
{
  return kEtcPalErrNotImpl;
}

/// \brief Retrieve the handle of a controller instance.
///
/// The handle is mostly used for interacting with the C controller API.
inline ControllerHandle Controller::handle() const
{
  return handle_;
}

/// \brief Retrieve the data that this controller was configured with on startup.
inline const ControllerData& Controller::data() const
{
  return my_data_;
}

/// \brief Retrieve the RDM data that this controller was configured with on startup.
/// \return The data, or an invalid ControllerRdmData instance if it was not provided.
inline const ControllerRdmData& Controller::rdm_data() const
{
  return my_rdm_data_;
}

/// \brief Retrieve the ControllerNotifyHandler reference that this controller was configured with.
inline ControllerNotifyHandler* Controller::notify_handler() const
{
  return notify_;
}

/// \brief Retrieve the ControllerRdmCommandHandler reference that this controller was configured with.
/// \return A pointer to the handler, or nullptr if it was not provided.
inline ControllerRdmCommandHandler* Controller::rdm_command_handler() const
{
  return rdm_cmd_handler_;
}

/// \brief Initialize the RDMnet Controller library.
/// \param log_params Optional logging configuration for the RDMnet library. If nullptr, no
///                   messages will be logged.
/// \param mcast_netints Optional set of network interfaces on which to operate RDMnet's multicast
///                      protocols. If left at default, all interfaces will be used.
/// \return etcpal::Error::Ok(): Initialization successful.
/// \return Error codes from rdmnet_client_init().
inline etcpal::Error Controller::Init(const EtcPalLogParams* log_params,
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
/// \return etcpal::Error::Ok(): Initialization successful.
/// \return Error codes from rdmnet_client_init().
inline etcpal::Error Controller::Init(const etcpal::Logger& logger,
                                      const std::vector<RdmnetMcastNetintId>& mcast_netints)
{
  return Init(&logger.log_params(), mcast_netints);
}

/// \brief Deinitialize the RDMnet Controller library.
inline void Controller::Deinit()
{
  rdmnet_controller_deinit();
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_CONTROLLER_H_
