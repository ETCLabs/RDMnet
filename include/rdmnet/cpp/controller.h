/******************************************************************************
 * Copyright 2020 ETC Inc.
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

/// @file rdmnet/cpp/controller.h
/// @brief C++ wrapper for the RDMnet Controller API

#ifndef RDMNET_CPP_CONTROLLER_H_
#define RDMNET_CPP_CONTROLLER_H_

#include <algorithm>
#include <string>
#include <vector>
#include "etcpal/common.h"
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/log.h"
#include "etcpal/cpp/opaque_id.h"
#include "rdm/cpp/uid.h"
#include "rdm/cpp/message.h"
#include "rdmnet/cpp/client.h"
#include "rdmnet/cpp/common.h"
#include "rdmnet/cpp/message.h"
#include "rdmnet/controller.h"

namespace rdmnet
{
/// @defgroup rdmnet_controller_cpp Controller API
/// @ingroup rdmnet_cpp_api
/// @brief Implementation of RDMnet controller functionality; see @ref using_controller.
///
/// RDMnet controllers are clients which originate RDM commands and receive responses. Controllers
/// can participate in multiple scopes; the default scope string "default" must be configured as a
/// default setting. This API provides classes tailored to the usage concerns of an RDMnet
/// controller.
///
/// See @ref using_controller for details of how to use this API.

namespace detail
{
class ControllerHandleType
{
};
};  // namespace detail

/// @ingroup rdmnet_controller_cpp
/// @brief An instance of RDMnet controller functionality.
///
/// See @ref using_controller for details of how to use this API.
class Controller
{
public:
  /// A handle type used by the RDMnet library to identify controller instances.
  using Handle = etcpal::OpaqueId<detail::ControllerHandleType, rdmnet_controller_t, RDMNET_CONTROLLER_INVALID>;

  /// @ingroup rdmnet_controller_cpp
  /// @brief A base class for a class that receives notification callbacks from a controller.
  ///
  /// See @ref using_controller for details of how to use this API.
  class NotifyHandler
  {
  public:
    virtual ~NotifyHandler() = default;

    /// @brief A controller has successfully connected to a broker.
    /// @param controller_handle Handle to controller instance which has connected.
    /// @param scope_handle Handle to the scope on which the controller has connected.
    /// @param info More information about the successful connection.
    virtual void HandleConnectedToBroker(Handle                     controller_handle,
                                         ScopeHandle                scope_handle,
                                         const ClientConnectedInfo& info) = 0;

    /// @brief A connection attempt failed between a controller and a broker.
    /// @param controller_handle Handle to controller instance which has failed to connect.
    /// @param scope_handle Handle to the scope on which the connection failed.
    /// @param info More information about the failed connection.
    virtual void HandleBrokerConnectFailed(Handle                         controller_handle,
                                           ScopeHandle                    scope_handle,
                                           const ClientConnectFailedInfo& info) = 0;

    /// @brief A controller which was previously connected to a broker has disconnected.
    /// @param controller_handle Handle to controller instance which has disconnected.
    /// @param scope_handle Handle to the scope on which the disconnect occurred.
    /// @param info More information about the disconnect event.
    virtual void HandleDisconnectedFromBroker(Handle                        controller_handle,
                                              ScopeHandle                   scope_handle,
                                              const ClientDisconnectedInfo& info) = 0;

    /// @brief A client list update has been received from a broker.
    /// @param controller_handle Handle to controller instance which has received the client list update.
    /// @param scope_handle Handle to the scope on which the client list update was received.
    /// @param list_action The way the updates in client_list should be applied to the controller's
    ///                    cached list.
    /// @param list The list of updates.
    virtual void HandleClientListUpdate(Handle               controller_handle,
                                        ScopeHandle          scope_handle,
                                        client_list_action_t list_action,
                                        const RptClientList& list) = 0;

    /// @brief An RDM response has been received.
    /// @param controller_handle Handle to controller instance which has received the RDM response.
    /// @param scope_handle Handle to the scope on which the RDM response was received.
    /// @param resp The RDM response data.
    /// @return True if the response has been (or will be) processed and doesn't need to be notified for again.
    /// @return False if another notification for this response should be triggered later.
    virtual bool HandleRdmResponse(Handle controller_handle, ScopeHandle scope_handle, const RdmResponse& resp) = 0;

    /// @brief An RPT status message has been received in response to a previously-sent RDM command.
    /// @param controller_handle Handle to controller instance which has received the RPT status message.
    /// @param scope_handle Handle to the scope on which the RPT status message was received.
    /// @param status The RPT status data.
    virtual void HandleRptStatus(Handle controller_handle, ScopeHandle scope_handle, const RptStatus& status) = 0;

    /// @brief A set of previously-requested mappings of dynamic UIDs to responder IDs has been received.
    ///
    /// This callback does not need to be implemented if the controller implementation never intends
    /// to request responder IDs.
    ///
    /// @param controller_handle Handle to controller instance which has received the responder IDs.
    /// @param scope_handle Handle to the scope on which the responder IDs were received.
    /// @param list The list of dynamic UID to responder ID mappings.
    virtual void HandleResponderIdsReceived(Handle                          controller_handle,
                                            ScopeHandle                     scope_handle,
                                            const DynamicUidAssignmentList& list)
    {
      ETCPAL_UNUSED_ARG(controller_handle);
      ETCPAL_UNUSED_ARG(scope_handle);
      ETCPAL_UNUSED_ARG(list);
    }
  };

  /// @ingroup rdmnet_controller_cpp
  /// @brief A base class for a class that receives RDM commands addressed to a controller.
  ///
  /// This is an optional portion of the controller API. See @ref using_controller for details.
  class RdmCommandHandler
  {
  public:
    /// @brief An RDM command has been received addressed to a controller.
    /// @param controller_handle Handle to controller instance which has received the RDM command.
    /// @param scope_handle Handle to the scope on which the RDM command was received.
    /// @param cmd The RDM command data.
    /// @return The action to take in response to this RDM command.
    virtual RdmResponseAction HandleRdmCommand(Handle            controller_handle,
                                               ScopeHandle       scope_handle,
                                               const RdmCommand& cmd) = 0;

    /// @brief An RDM command has been received over LLRP, addressed to a controller.
    /// @param controller_handle Handle to controller instance which has received the RDM command.
    /// @param cmd The RDM command data.
    /// @return The action to take in response to this LLRP RDM command.
    virtual RdmResponseAction HandleLlrpRdmCommand(Handle controller_handle, const llrp::RdmCommand& cmd)
    {
      ETCPAL_UNUSED_ARG(controller_handle);
      ETCPAL_UNUSED_ARG(cmd);
      return rdmnet::RdmResponseAction::SendNack(kRdmNRActionNotSupported);
    }
  };

  /// @ingroup rdmnet_controller_cpp
  /// @brief A set of configuration settings that a controller needs to initialize.
  ///
  /// Note that network interfaces that the LLRP target of the controller should use are no longer specified here.
  /// Instead, the set of interfaces passed to rdmnet::Init() is used.
  struct Settings
  {
    etcpal::Uuid cid;            ///< The controller's Component Identifier (CID).
    rdm::Uid     uid;            ///< The controller's RDM UID. For a dynamic UID, use rdm::Uid::DynamicUidRequest().
    std::string  search_domain;  ///< (optional) The controller's search domain for discovering brokers.

    /// (optional) Whether to create an LLRP target associated with this controller.
    bool create_llrp_target{false};

    /// Create an empty, invalid data structure by default.
    Settings() = default;
    Settings(const etcpal::Uuid& new_cid, const rdm::Uid& new_uid);
    Settings(const etcpal::Uuid& new_cid, uint16_t manufacturer_id);

    bool IsValid() const;
  };

  /// @ingroup rdmnet_controller_cpp
  /// @brief A set of initial identifying RDM data to use for a controller.
  struct RdmData
  {
    /// @brief A number representing the product model which implements the controller.
    /// @details Should be unique per model for a given manufacturer.
    uint16_t model_id{0};
    /// @brief A number representing the version of the controller software.
    /// @details Should be unique per version of a given controller's application.
    uint32_t software_version_id{0};
    /// @brief A number representing the product's primary function.
    /// @details Valid values are defined in rdm/defs.h as E120_PRODUCT_CATEGORY_...
    uint16_t product_category{E120_PRODUCT_CATEGORY_CONTROL_CONTROLLER};

    std::string manufacturer_label;        ///< The manufacturer name of the controller.
    std::string device_model_description;  ///< The name of the product model which implements the controller.
    std::string software_version_label;    ///< The software version of the controller as a string.
    std::string device_label;              ///< A user-settable name for this controller instance.
    bool device_label_settable{false};     ///< Whether the library should allow device_label to be changed remotely.

    /// Create an empty, invalid structure by default - must be filled in before passing to Controller::Startup().
    RdmData() = default;
    RdmData(uint16_t    new_model_id,
            uint32_t    new_software_version_id,
            const char* new_manufacturer_label,
            const char* new_device_model_description,
            const char* new_software_version_label,
            const char* new_device_label);
    RdmData(uint16_t           new_model_id,
            uint32_t           new_software_version_id,
            const std::string& new_manufacturer_label,
            const std::string& new_device_model_description,
            const std::string& new_software_version_label,
            const std::string& new_device_label);

    bool IsValid() const;
  };

  Controller() = default;
  Controller(const Controller& other) = delete;
  Controller& operator=(const Controller& other) = delete;
  Controller(Controller&& other) = default;             ///< Move a controller instance.
  Controller& operator=(Controller&& other) = default;  ///< Move a controller instance.

  etcpal::Error Startup(NotifyHandler& notify_handler, const Settings& settings, const RdmData& rdm_data);
  etcpal::Error Startup(NotifyHandler&     notify_handler,
                        const Settings&    settings,
                        RdmCommandHandler& rdm_handler,
                        uint8_t*           rdm_response_buf = nullptr);
  void          Shutdown(rdmnet_disconnect_reason_t disconnect_reason = kRdmnetDisconnectShutdown);

  etcpal::Expected<ScopeHandle> AddScope(const char*             id,
                                         const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Expected<ScopeHandle> AddScope(const std::string&      id,
                                         const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Expected<ScopeHandle> AddScope(const Scope& scope_config);
  etcpal::Expected<ScopeHandle> AddDefaultScope(const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Error                 RemoveScope(ScopeHandle scope_handle, rdmnet_disconnect_reason_t disconnect_reason);
  etcpal::Error                 ChangeScope(ScopeHandle                scope_handle,
                                            const char*                new_scope_id_str,
                                            rdmnet_disconnect_reason_t disconnect_reason,
                                            const etcpal::SockAddr&    new_static_broker_addr = etcpal::SockAddr{});
  etcpal::Error                 ChangeScope(ScopeHandle                scope_handle,
                                            const Scope&               new_scope_config,
                                            rdmnet_disconnect_reason_t disconnect_reason);
  etcpal::Error ChangeSearchDomain(const char* new_search_domain, rdmnet_disconnect_reason_t disconnect_reason);

  etcpal::Expected<uint32_t> SendRdmCommand(ScopeHandle            scope_handle,
                                            const DestinationAddr& destination,
                                            rdmnet_command_class_t command_class,
                                            uint16_t               param_id,
                                            const uint8_t*         data = nullptr,
                                            uint8_t                data_len = 0);
  etcpal::Expected<uint32_t> SendGetCommand(ScopeHandle            scope_handle,
                                            const DestinationAddr& destination,
                                            uint16_t               param_id,
                                            const uint8_t*         data = nullptr,
                                            uint8_t                data_len = 0);
  etcpal::Expected<uint32_t> SendSetCommand(ScopeHandle            scope_handle,
                                            const DestinationAddr& destination,
                                            uint16_t               param_id,
                                            const uint8_t*         data = nullptr,
                                            uint8_t                data_len = 0);

  etcpal::Error RequestClientList(ScopeHandle scope_handle);
  etcpal::Error RequestResponderIds(ScopeHandle scope_handle, const rdm::Uid* uids, size_t num_uids);
  etcpal::Error RequestResponderIds(ScopeHandle scope_handle, const std::vector<rdm::Uid>& uids);

  etcpal::Error SendRdmAck(ScopeHandle            scope_handle,
                           const SavedRdmCommand& received_cmd,
                           const uint8_t*         response_data = nullptr,
                           size_t                 response_data_len = 0);
  etcpal::Error SendRdmNack(ScopeHandle            scope_handle,
                            const SavedRdmCommand& received_cmd,
                            rdm_nack_reason_t      nack_reason);
  etcpal::Error SendRdmNack(ScopeHandle scope_handle, const SavedRdmCommand& received_cmd, uint16_t raw_nack_reason);
  etcpal::Error SendRdmUpdate(ScopeHandle    scope_handle,
                              uint16_t       param_id,
                              const uint8_t* data = nullptr,
                              size_t         data_len = 0);

  etcpal::Error SendLlrpAck(const llrp::SavedRdmCommand& received_cmd,
                            const uint8_t*               response_data = nullptr,
                            uint8_t                      response_data_len = 0);
  etcpal::Error SendLlrpNack(const llrp::SavedRdmCommand& received_cmd, rdm_nack_reason_t nack_reason);
  etcpal::Error SendLlrpNack(const llrp::SavedRdmCommand& received_cmd, uint16_t raw_nack_reason);

  Handle                         handle() const;
  const RdmData&                 rdm_data() const;
  Controller::NotifyHandler*     notify_handler() const;
  Controller::RdmCommandHandler* rdm_command_handler() const;
  etcpal::Expected<Scope>        scope(ScopeHandle scope_handle) const;

  void UpdateRdmData(const RdmData& new_data);

private:
  Handle  handle_;
  RdmData my_rdm_data_;

  RdmCommandHandler* rdm_cmd_handler_{nullptr};
  NotifyHandler*     notify_{nullptr};
};

/// @cond controller_c_callbacks
/// Callbacks from underlying controller library to be forwarded

namespace internal
{
extern "C" inline void ControllerLibCbConnected(rdmnet_controller_t              controller_handle,
                                                rdmnet_client_scope_t            scope_handle,
                                                const RdmnetClientConnectedInfo* info,
                                                void*                            context)
{
  if (info && context)
  {
    static_cast<Controller::NotifyHandler*>(context)->HandleConnectedToBroker(Controller::Handle(controller_handle),
                                                                              ScopeHandle(scope_handle), *info);
  }
}

extern "C" inline void ControllerLibCbConnectFailed(rdmnet_controller_t                  controller_handle,
                                                    rdmnet_client_scope_t                scope_handle,
                                                    const RdmnetClientConnectFailedInfo* info,
                                                    void*                                context)
{
  if (info && context)
  {
    static_cast<Controller::NotifyHandler*>(context)->HandleBrokerConnectFailed(Controller::Handle(controller_handle),
                                                                                ScopeHandle(scope_handle), *info);
  }
}

extern "C" inline void ControllerLibCbDisconnected(rdmnet_controller_t                 controller_handle,
                                                   rdmnet_client_scope_t               scope_handle,
                                                   const RdmnetClientDisconnectedInfo* info,
                                                   void*                               context)
{
  if (info && context)
  {
    static_cast<Controller::NotifyHandler*>(context)->HandleDisconnectedFromBroker(
        Controller::Handle(controller_handle), ScopeHandle(scope_handle), *info);
  }
}

extern "C" inline void ControllerLibCbClientListUpdate(rdmnet_controller_t        controller_handle,
                                                       rdmnet_client_scope_t      scope_handle,
                                                       client_list_action_t       list_action,
                                                       const RdmnetRptClientList* list,
                                                       void*                      context)
{
  if (list && context)
  {
    static_cast<Controller::NotifyHandler*>(context)->HandleClientListUpdate(
        Controller::Handle(controller_handle), ScopeHandle(scope_handle), list_action, *list);
  }
}

extern "C" inline bool ControllerLibCbRdmResponseReceived(rdmnet_controller_t      controller_handle,
                                                          rdmnet_client_scope_t    scope_handle,
                                                          const RdmnetRdmResponse* resp,
                                                          void*                    context)
{
  if (resp && context)
  {
    return static_cast<Controller::NotifyHandler*>(context)->HandleRdmResponse(Controller::Handle(controller_handle),
                                                                               ScopeHandle(scope_handle), *resp);
  }

  return true;  // Error condition, drop response.
}

extern "C" inline void ControllerLibCbStatusReceived(rdmnet_controller_t    controller_handle,
                                                     rdmnet_client_scope_t  scope_handle,
                                                     const RdmnetRptStatus* status,
                                                     void*                  context)
{
  if (status && context)
  {
    static_cast<Controller::NotifyHandler*>(context)->HandleRptStatus(Controller::Handle(controller_handle),
                                                                      ScopeHandle(scope_handle), *status);
  }
}

extern "C" inline void ControllerLibCbResponderIdsReceived(rdmnet_controller_t                   controller_handle,
                                                           rdmnet_client_scope_t                 scope_handle,
                                                           const RdmnetDynamicUidAssignmentList* list,
                                                           void*                                 context)
{
  if (list && context)
  {
    static_cast<Controller::NotifyHandler*>(context)->HandleResponderIdsReceived(Controller::Handle(controller_handle),
                                                                                 ScopeHandle(scope_handle), *list);
  }
}

extern "C" inline void ControllerLibCbRdmCommandReceived(rdmnet_controller_t     controller_handle,
                                                         rdmnet_client_scope_t   scope_handle,
                                                         const RdmnetRdmCommand* cmd,
                                                         RdmnetSyncRdmResponse*  response,
                                                         void*                   context)
{
  if (cmd && response && context)
  {
    *response = static_cast<Controller::RdmCommandHandler*>(context)
                    ->HandleRdmCommand(Controller::Handle(controller_handle), ScopeHandle(scope_handle), *cmd)
                    .get();
  }
}

extern "C" inline void ControllerLibCbLlrpRdmCommandReceived(rdmnet_controller_t    controller_handle,
                                                             const LlrpRdmCommand*  cmd,
                                                             RdmnetSyncRdmResponse* response,
                                                             void*                  context)
{
  if (cmd && response && context)
  {
    *response = static_cast<Controller::RdmCommandHandler*>(context)
                    ->HandleLlrpRdmCommand(Controller::Handle(controller_handle), *cmd)
                    .get();
  }
}

};  // namespace internal

/// @endcond

/// @brief Create a controller Settings instance by passing the required members explicitly.
/// @details This version takes the fully-formed RDM UID that the controller will use.
inline Controller::Settings::Settings(const etcpal::Uuid& new_cid, const rdm::Uid& new_uid) : cid(new_cid), uid(new_uid)
{
}

/// @brief Create a controller Settings instance by passing the required members explicitly.
///
/// This version just takes the controller's ESTA manufacturer ID and uses it to generate
/// an RDMnet dynamic UID request.
inline Controller::Settings::Settings(const etcpal::Uuid& new_cid, uint16_t manufacturer_id)
    : cid(new_cid), uid(rdm::Uid::DynamicUidRequest(manufacturer_id))
{
}

/// Determine whether a controller Settings instance contains valid data for RDMnet operation.
inline bool Controller::Settings::IsValid() const
{
  return (!cid.IsNull() && (uid.IsStatic() || uid.IsDynamicUidRequest()));
}

/// Create a controller RdmData instance by passing all members which do not have a default value.
inline Controller::RdmData::RdmData(uint16_t    new_model_id,
                                    uint32_t    new_software_version_id,
                                    const char* new_manufacturer_label,
                                    const char* new_device_model_description,
                                    const char* new_software_version_label,
                                    const char* new_device_label)
    : model_id(new_model_id), software_version_id(new_software_version_id)
{
  if (new_manufacturer_label)
    manufacturer_label = std::string(new_manufacturer_label);

  if (new_device_model_description)
    device_model_description = std::string(new_device_model_description);

  if (new_software_version_label)
    software_version_label = std::string(new_software_version_label);

  if (new_device_label)
    device_label = std::string(new_device_label);
}

/// Create a controller RdmData instance by passing all members which do not have a default value.
inline Controller::RdmData::RdmData(uint16_t           new_model_id,
                                    uint32_t           new_software_version_id,
                                    const std::string& new_manufacturer_label,
                                    const std::string& new_device_model_description,
                                    const std::string& new_software_version_label,
                                    const std::string& new_device_label)
    : model_id(new_model_id)
    , software_version_id(new_software_version_id)
    , manufacturer_label(new_manufacturer_label)
    , device_model_description(new_device_model_description)
    , software_version_label(new_software_version_label)
    , device_label(new_device_label)
{
}

/// Whether this data is valid (all string members are non-empty).
inline bool Controller::RdmData::IsValid() const
{
  return ((!manufacturer_label.empty()) && (!device_model_description.empty()) && (!software_version_label.empty()) &&
          (!device_label.empty()));
}

/// @brief Allocate resources and start up this controller with the given configuration.
///
/// This overload provides a set of RDM data to the library to use for the controller's RDM
/// responder. RDM commands addressed to the controller will be handled internally by the library.
///
/// @param notify_handler A class instance to handle callback notifications from this controller.
/// @param settings Configuration settings used by this controller.
/// @param rdm_data Data to identify this controller to other controllers on the network.
/// @return etcpal::Error::Ok(): Controller started successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return Errors forwarded from rdmnet_controller_create().
inline etcpal::Error Controller::Startup(NotifyHandler&  notify_handler,
                                         const Settings& settings,
                                         const RdmData&  rdm_data)
{
  if (!settings.IsValid() || !rdm_data.IsValid())
    return kEtcPalErrInvalid;

  notify_ = &notify_handler;
  my_rdm_data_ = rdm_data;

  // clang-format off
  RdmnetControllerConfig config = {
    settings.cid.get(),             // CID
    {                               // Callback shims
      internal::ControllerLibCbConnected,
      internal::ControllerLibCbConnectFailed,
      internal::ControllerLibCbDisconnected,
      internal::ControllerLibCbClientListUpdate,
      internal::ControllerLibCbRdmResponseReceived,
      internal::ControllerLibCbStatusReceived,
      internal::ControllerLibCbResponderIdsReceived,
      &notify_handler               // Context
    },
    {                               // RDM command callback shims
      nullptr, nullptr, nullptr, nullptr
    },
    {                               // RDM data
      rdm_data.model_id,
      rdm_data.software_version_id,
      rdm_data.manufacturer_label.c_str(),
      rdm_data.device_model_description.c_str(),
      rdm_data.software_version_label.c_str(),
      rdm_data.device_label.c_str(),
      rdm_data.product_category,
      rdm_data.device_label_settable
    },
    settings.uid.get(),             // UID
    settings.search_domain.c_str(), // Search domain
    settings.create_llrp_target,    // Create LLRP target
  };
  // clang-format on

  rdmnet_controller_t c_handle = RDMNET_CONTROLLER_INVALID;
  etcpal::Error       result = rdmnet_controller_create(&config, &c_handle);

  handle_.SetValue(c_handle);

  return result;
}

/// @brief Allocate resources and start up this controller with the given configuration.
///
/// This overload provides a notification handler to respond to RDM commands addressed to the
/// controller. You must implement a core set of RDM commands - see @ref using_controller for more
/// information.
///
/// @param notify_handler A class instance to handle callback notifications from this controller.
/// @param settings Configuration settings used by this controller.
/// @param rdm_handler A class instance to handle RDM commands addressed to this controller.
/// @param rdm_response_buf (optional) A data buffer used to respond synchronously to RDM commands.
///                         See @ref handling_rdm_commands for more information.
/// @return etcpal::Error::Ok(): Controller started successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return Errors forwarded from rdmnet_controller_create().
inline etcpal::Error Controller::Startup(NotifyHandler&     notify_handler,
                                         const Settings&    settings,
                                         RdmCommandHandler& rdm_handler,
                                         uint8_t*           rdm_response_buf)
{
  if (!settings.IsValid())
    return kEtcPalErrInvalid;

  notify_ = &notify_handler;
  rdm_cmd_handler_ = &rdm_handler;

  // clang-format off
  RdmnetControllerConfig config = {
    settings.cid.get(),             // CID
    {                               // Callback shims
      internal::ControllerLibCbConnected,
      internal::ControllerLibCbConnectFailed,
      internal::ControllerLibCbDisconnected,
      internal::ControllerLibCbClientListUpdate,
      internal::ControllerLibCbRdmResponseReceived,
      internal::ControllerLibCbStatusReceived,
      internal::ControllerLibCbResponderIdsReceived,
      &notify_handler               // Context
    },
    {                               // RDM command callback shims
      internal::ControllerLibCbRdmCommandReceived,
      internal::ControllerLibCbLlrpRdmCommandReceived,
      rdm_response_buf,
      &rdm_handler                  // Context
    },
    RDMNET_CONTROLLER_RDM_DATA_DEFAULT_INIT, // RDM data
    settings.uid.get(),             // UID
    settings.search_domain.c_str(), // Search domain
    settings.create_llrp_target,    // Create LLRP target
  };
  // clang-format on

  rdmnet_controller_t c_handle = RDMNET_CONTROLLER_INVALID;
  etcpal::Error       result = rdmnet_controller_create(&config, &c_handle);

  handle_.SetValue(c_handle);

  return result;
}

/// @brief Shut down this controller and deallocate resources.
///
/// Will disconnect all scopes to which this controller is currently connected, sending the
/// disconnect reason provided in the disconnect_reason parameter.
///
/// @param disconnect_reason Reason code for disconnecting from each scope.
inline void Controller::Shutdown(rdmnet_disconnect_reason_t disconnect_reason)
{
  rdmnet_controller_destroy(handle_.value(), disconnect_reason);
  handle_.Clear();
}

/// @brief Add a new scope to this controller instance.
///
/// The library will attempt to discover and connect to a broker for the scope (or just connect if
/// a static broker address is given); the status of these attempts will be communicated via the
/// associated Controller::NotifyHandler.
///
/// @param id The scope ID string.
/// @param static_broker_addr [optional] A static IP address and port at which to connect to the
///                           broker for this scope.
/// @return On success, a handle to the new scope, to be used with subsequent API calls.
/// @return On failure, error codes from rdmnet_controller_add_scope(), or #kEtcPalErrInvalid on invalid parameter.
inline etcpal::Expected<ScopeHandle> Controller::AddScope(const char* id, const etcpal::SockAddr& static_broker_addr)
{
  if (!id)
    return kEtcPalErrInvalid;

  RdmnetScopeConfig     scope_config = {id, static_broker_addr.get()};
  rdmnet_client_scope_t scope_handle;
  auto                  result = rdmnet_controller_add_scope(handle_.value(), &scope_config, &scope_handle);
  if (result == kEtcPalErrOk)
    return ScopeHandle(scope_handle);
  else
    return result;
}

/// @brief Add a new scope to this controller instance.
///
/// The library will attempt to discover and connect to a broker for the scope (or just connect if
/// a static broker address is given); the status of these attempts will be communicated via the
/// associated Controller::NotifyHandler.
///
/// @param id The scope ID string.
/// @param static_broker_addr [optional] A static IP address and port at which to connect to the
///                           broker for this scope.
/// @return On success, a handle to the new scope, to be used with subsequent API calls.
/// @return On failure, error codes from rdmnet_controller_add_scope().
inline etcpal::Expected<ScopeHandle> Controller::AddScope(const std::string&      id,
                                                          const etcpal::SockAddr& static_broker_addr)
{
  return AddScope(id.c_str(), static_broker_addr);
}

/// @brief Add a new scope to this controller instance.
///
/// The library will attempt to discover and connect to a broker for the scope (or just connect if
/// a static broker address is given); the status of these attempts will be communicated via the
/// associated Controller::NotifyHandler.
///
/// @param scope_config Configuration information for the new scope.
/// @return On success, a handle to the new scope, to be used with subsequent API calls.
/// @return On failure, error codes from rdmnet_controller_add_scope().
inline etcpal::Expected<ScopeHandle> Controller::AddScope(const Scope& scope_config)
{
  return AddScope(scope_config.id_string().c_str(), scope_config.static_broker_addr());
}

/// @brief Shortcut to add the default RDMnet scope to a controller instance.
///
/// The library will attempt to discover and connect to a broker for the default scope (or just
/// connect if a static broker address is given); the status of these attempts will be communicated
/// via the associated Controller::NotifyHandler.
///
/// @param static_broker_addr [optional] A static broker address to configure for the default scope.
/// @return On success, a handle to the new scope, to be used with subsequent API calls.
/// @return On failure, error codes from rdmnet_controller_add_scope().
inline etcpal::Expected<ScopeHandle> Controller::AddDefaultScope(const etcpal::SockAddr& static_broker_addr)
{
  return AddScope(E133_DEFAULT_SCOPE, static_broker_addr);
}

/// @brief Remove a previously-added scope from this controller instance.
///
/// After this call completes, scope_handle will no longer be valid.
///
/// @param scope_handle Handle to scope to remove.
/// @param disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
/// @return etcpal::Error::Ok(): Scope removed successfully.
/// @return Error codes from from rdmnet_controller_remove_scope().
inline etcpal::Error Controller::RemoveScope(ScopeHandle scope_handle, rdmnet_disconnect_reason_t disconnect_reason)
{
  return rdmnet_controller_remove_scope(handle_.value(), scope_handle.value(), disconnect_reason);
}

/// @brief Change the configuration of a scope on a controller.
///
/// Will disconnect from any connected brokers and attempt connection again using the new
/// configuration given.
///
/// @param scope_handle Handle to the scope for which to change the configuration.
/// @param new_scope_id_str ID string to use for the new scope.
/// @param disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
/// @param new_static_broker_addr [optional] New static IP address and port at which to connect to
///                               the broker for this scope.
/// @return etcpal::Error::Ok(): Scope changed successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return #kEtcPalErrNotInit: Module not initialized.
/// @return #kEtcPalErrNotFound: scope_handle is not associated with a valid scope instance.
/// @return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Controller::ChangeScope(ScopeHandle                scope_handle,
                                             const char*                new_scope_id_str,
                                             rdmnet_disconnect_reason_t disconnect_reason,
                                             const etcpal::SockAddr&    new_static_broker_addr)
{
  if (!new_scope_id_str)
    return kEtcPalErrInvalid;

  RdmnetScopeConfig new_scope_config = {new_scope_id_str, new_static_broker_addr.get()};
  return rdmnet_controller_change_scope(handle_.value(), scope_handle.value(), &new_scope_config, disconnect_reason);
}

/// @brief Change the configuration of a scope on a controller.
///
/// Will disconnect from any connected brokers and attempt connection again using the new
/// configuration given.
///
/// @param scope_handle Handle to the scope for which to change the configuration.
/// @param new_scope_config New configuration parameters for the scope.
/// @param disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
/// @return etcpal::Error::Ok(): Scope changed successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return #kEtcPalErrNotInit: Module not initialized.
/// @return #kEtcPalErrNotFound: scope_handle is not associated with a valid scope instance.
/// @return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Controller::ChangeScope(ScopeHandle                scope_handle,
                                             const Scope&               new_scope_config,
                                             rdmnet_disconnect_reason_t disconnect_reason)
{
  return ChangeScope(scope_handle, new_scope_config.id_string().c_str(), disconnect_reason,
                     new_scope_config.static_broker_addr());
}

/// @brief Change the controller's DNS search domain.
///
/// Non-default search domains are considered advanced usage. Any added scopes which do not have a
/// static broker configuration will be disconnected, sending the disconnect reason provided in the
/// disconnect_reason parameter. Then discovery will be re-attempted on the new search domain.
///
/// @param new_search_domain New search domain to use for discovery.
/// @param disconnect_reason Disconnect reason to send to any connected brokers.
/// @return etcpal::Error::Ok(): Search domain changed successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return #kEtcPalErrNotInit: Module not initialized.
/// @return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Controller::ChangeSearchDomain(const char*                new_search_domain,
                                                    rdmnet_disconnect_reason_t disconnect_reason)
{
  if (!new_search_domain)
    return kEtcPalErrInvalid;

  return rdmnet_controller_change_search_domain(handle_.value(), new_search_domain, disconnect_reason);
}

/// @brief Send an RDM command from a controller on a scope.
///
/// The response will be delivered via the Controller::NotifyHandler::HandleRdmResponse() callback.
///
/// @param scope_handle Handle to the scope on which to send the RDM command.
/// @param destination The destination addressing information for the RDM command.
/// @param command_class The command's RDM command class (GET or SET).
/// @param param_id The command's RDM parameter ID.
/// @param data [optional] The command's RDM parameter data, if it has any.
/// @param data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// @return On success, a sequence number which can be used to match the command with a response.
/// @return On failure, error codes from rdmnet_controller_send_rdm_command().
inline etcpal::Expected<uint32_t> Controller::SendRdmCommand(ScopeHandle            scope_handle,
                                                             const DestinationAddr& destination,
                                                             rdmnet_command_class_t command_class,
                                                             uint16_t               param_id,
                                                             const uint8_t*         data,
                                                             uint8_t                data_len)
{
  uint32_t       seq_num;
  etcpal_error_t res = rdmnet_controller_send_rdm_command(handle_.value(), scope_handle.value(), &destination.get(),
                                                          command_class, param_id, data, data_len, &seq_num);
  if (res == kEtcPalErrOk)
    return seq_num;
  else
    return res;
}

/// @brief Send an RDM GET command from a controller on a scope.
///
/// The response will be delivered via the Controller::NotifyHandler::HandleRdmResponse() callback.
///
/// @param scope_handle Handle to the scope on which to send the RDM command.
/// @param destination The destination addressing information for the RDM command.
/// @param param_id The command's RDM parameter ID.
/// @param data [optional] The command's RDM parameter data, if it has any.
/// @param data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// @return On success, a sequence number which can be used to match the command with a response.
/// @return On failure, error codes from rdmnet_controller_send_get_command().
inline etcpal::Expected<uint32_t> Controller::SendGetCommand(ScopeHandle            scope_handle,
                                                             const DestinationAddr& destination,
                                                             uint16_t               param_id,
                                                             const uint8_t*         data,
                                                             uint8_t                data_len)
{
  uint32_t       seq_num;
  etcpal_error_t res = rdmnet_controller_send_get_command(handle_.value(), scope_handle.value(), &destination.get(),
                                                          param_id, data, data_len, &seq_num);
  if (res == kEtcPalErrOk)
    return seq_num;
  else
    return res;
}

/// @brief Send an RDM SET command from a controller on a scope.
///
/// The response will be delivered via the Controller::NotifyHandler::HandleRdmResponse() callback.
///
/// @param scope_handle Handle to the scope on which to send the RDM command.
/// @param destination The destination addressing information for the RDM command.
/// @param param_id The command's RDM parameter ID.
/// @param data [optional] The command's RDM parameter data, if it has any.
/// @param data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// @return On success, a sequence number which can be used to match the command with a response.
/// @return On failure, error codes from rdmnet_controller_send_set_command().
inline etcpal::Expected<uint32_t> Controller::SendSetCommand(ScopeHandle            scope_handle,
                                                             const DestinationAddr& destination,
                                                             uint16_t               param_id,
                                                             const uint8_t*         data,
                                                             uint8_t                data_len)
{
  uint32_t       seq_num;
  etcpal_error_t res = rdmnet_controller_send_set_command(handle_.value(), scope_handle.value(), &destination.get(),
                                                          param_id, data, data_len, &seq_num);
  if (res == kEtcPalErrOk)
    return seq_num;
  else
    return res;
}

/// @brief Request a client list from a broker.
///
/// The response will be delivered via the Controller::NotifyHandler::HandleClientListUpdate()
/// callback.
///
/// @param scope_handle Handle to the scope on which to request the client list.
/// @return etcpal::Error::Ok(): Request sent successfully.
/// @return Error codes from rdmnet_controller_request_client_list().
inline etcpal::Error Controller::RequestClientList(ScopeHandle scope_handle)
{
  return rdmnet_controller_request_client_list(handle_.value(), scope_handle.value());
}

/// @brief Request mappings from dynamic UIDs to Responder IDs (RIDs).
///
/// See @ref devices_and_gateways for more information. A RID is a UUID that permanently identifies
/// a virtual RDMnet responder.
///
/// @param scope_handle Handle to the scope on which to request the responder IDs.
/// @param uids List of dynamic UIDs for which to request the corresponding responder ID.
/// @param num_uids Size of the uids array.
/// @return etcpal::Error::Ok(): Request sent successfully.
/// @return Error codes from rdmnet_controller_request_responder_ids(), or #kEtcPalErrInvalid on invalid parameter.
inline etcpal::Error Controller::RequestResponderIds(ScopeHandle scope_handle, const rdm::Uid* uids, size_t num_uids)
{
  if (!uids || (num_uids == 0))
    return kEtcPalErrInvalid;

  std::vector<RdmUid> c_uids;
  c_uids.reserve(num_uids);
  std::transform(uids, uids + num_uids, std::back_inserter(c_uids), [](const rdm::Uid& uid) { return uid.get(); });
  return rdmnet_controller_request_responder_ids(handle_.value(), scope_handle.value(), c_uids.data(), c_uids.size());
}

/// @brief Request mappings from dynamic UIDs to Responder IDs (RIDs).
///
/// See @ref devices_and_gateways for more information. A RID is a UUID that permanently identifies
/// a virtual RDMnet responder.
///
/// @param scope_handle Handle to the scope on which to request the responder IDs.
/// @param uids List of dynamic UIDs for which to request the corresponding responder ID.
/// @return etcpal::Error::Ok(): Request sent successfully.
/// @return Error codes from rdmnet_controller_request_responder_ids().
inline etcpal::Error Controller::RequestResponderIds(ScopeHandle scope_handle, const std::vector<rdm::Uid>& uids)
{
  return RequestResponderIds(scope_handle, uids.data(), uids.size());
}

/// @brief Send an acknowledge (ACK) response to an RDM command received by a controller.
///
/// This function should only be used if a Controller::RdmCommandHandler was supplied when starting
/// this controller.
///
/// @param scope_handle Handle to the scope on which the corresponding command was received.
/// @param received_cmd The command to which this ACK is a response.
/// @param response_data [optional] The response's RDM parameter data, if it has any.
/// @param response_data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// @return etcpal::Error::Ok(): ACK sent successfully.
/// @return Error codes from rdmnet_controller_send_rdm_ack().
inline etcpal::Error Controller::SendRdmAck(ScopeHandle            scope_handle,
                                            const SavedRdmCommand& received_cmd,
                                            const uint8_t*         response_data,
                                            size_t                 response_data_len)
{
  return rdmnet_controller_send_rdm_ack(handle_.value(), scope_handle.value(), &received_cmd.get(), response_data,
                                        response_data_len);
}

/// @brief Send a negative acknowledge (NACK) response to an RDM command received by a controller.
///
/// This function should only be used if a Controller::RdmCommandHandler was supplied when starting
/// this controller.
///
/// @param scope_handle Handle to the scope on which the corresponding command was received.
/// @param received_cmd The command to which this NACK is a response.
/// @param nack_reason The RDM NACK reason to send with the NACK response.
/// @return etcpal::Error::Ok(): NACK sent successfully.
/// @return Error codes from rdmnet_controller_send_rdm_nack().
inline etcpal::Error Controller::SendRdmNack(ScopeHandle            scope_handle,
                                             const SavedRdmCommand& received_cmd,
                                             rdm_nack_reason_t      nack_reason)
{
  return rdmnet_controller_send_rdm_nack(handle_.value(), scope_handle.value(), &received_cmd.get(), nack_reason);
}

/// @brief Send a negative acknowledge (NACK) response to an RDM command received by a controller.
///
/// This function should only be used if a Controller::RdmCommandHandler was supplied when starting
/// this controller.
///
/// @param scope_handle Handle to the scope on which the corresponding command was received.
/// @param received_cmd The command to which this NACK is a response.
/// @param raw_nack_reason The NACK reason (either standard or manufacturer-specific) to send with
///                        the NACK response.
/// @return etcpal::Error::Ok(): NACK sent successfully.
/// @return Error codes from rdmnet_controller_send_rdm_nack().
inline etcpal::Error Controller::SendRdmNack(ScopeHandle            scope_handle,
                                             const SavedRdmCommand& received_cmd,
                                             uint16_t               raw_nack_reason)
{
  return rdmnet_controller_send_rdm_nack(handle_.value(), scope_handle.value(), &received_cmd.get(),
                                         static_cast<rdm_nack_reason_t>(raw_nack_reason));
}

/// @brief Send an asynchronous RDM GET response to update the value of a local parameter.
///
/// This function should only be used if a Controller::RdmCommandHandler was supplied when starting
/// this controller.
///
/// @param scope_handle Handle to the scope on which to send the RDM update.
/// @param param_id The RDM parameter ID that has been updated.
/// @param data [optional] The updated parameter data, if any.
/// @param data_len [optional] The length of the parameter data, if any.
/// @return etcpal::Error::Ok(): RDM update sent successfully.
/// @return Error codes from rdmnet_controller_send_rdm_update().
inline etcpal::Error Controller::SendRdmUpdate(ScopeHandle    scope_handle,
                                               uint16_t       param_id,
                                               const uint8_t* data,
                                               size_t         data_len)
{
  return rdmnet_controller_send_rdm_update(handle_.value(), scope_handle.value(), 0, param_id, data, data_len);
}

/// @brief Send an acknowledge (ACK) response to an LLRP RDM command received by a controller.
///
/// This function should only be used if a Controller::RdmCommandHandler was supplied when starting
/// this controller.
///
/// @param received_cmd The command to which this ACK is a response.
/// @param response_data [optional] The response's RDM parameter data, if it has any.
/// @param response_data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// @return etcpal::Error::Ok(): LLRP ACK sent successfully.
/// @return Error codes from rdmnet_controller_send_llrp_ack().
inline etcpal::Error Controller::SendLlrpAck(const llrp::SavedRdmCommand& received_cmd,
                                             const uint8_t*               response_data,
                                             uint8_t                      response_data_len)
{
  return rdmnet_controller_send_llrp_ack(handle_.value(), &received_cmd.get(), response_data, response_data_len);
}

/// @brief Send a negative acknowledge (NACK) response to an LLRP RDM command received by a controller.
///
/// This function should only be used if a Controller::RdmCommandHandler was supplied when starting
/// this controller.
///
/// @param received_cmd The command to which this NACK is a response.
/// @param nack_reason The RDM NACK reason to send with the NACK response.
/// @return etcpal::Error::Ok(): NACK sent successfully.
/// @return Error codes from rdmnet_controller_send_llrp_nack().
inline etcpal::Error Controller::SendLlrpNack(const llrp::SavedRdmCommand& received_cmd, rdm_nack_reason_t nack_reason)
{
  return rdmnet_controller_send_llrp_nack(handle_.value(), &received_cmd.get(), nack_reason);
}

/// @brief Send a negative acknowledge (NACK) response to an LLRP RDM command received by a controller.
///
/// This function should only be used if a Controller::RdmCommandHandler was supplied when starting
/// this controller.
///
/// @param received_cmd The command to which this NACK is a response.
/// @param raw_nack_reason The NACK reason (either standard or manufacturer-specific) to send with
///                        the NACK response.
/// @return etcpal::Error::Ok(): NACK sent successfully.
/// @return Error codes from rdmnet_controller_send_llrp_nack().
inline etcpal::Error Controller::SendLlrpNack(const llrp::SavedRdmCommand& received_cmd, uint16_t raw_nack_reason)
{
  return rdmnet_controller_send_llrp_nack(handle_.value(), &received_cmd.get(),
                                          static_cast<rdm_nack_reason_t>(raw_nack_reason));
}

/// @brief Retrieve the handle of a controller instance.
inline Controller::Handle Controller::handle() const
{
  return handle_;
}

/// @brief Retrieve the RDM data that this controller was configured with on startup.
/// @return The data, or an invalid RdmData instance if it was not provided.
inline const Controller::RdmData& Controller::rdm_data() const
{
  return my_rdm_data_;
}

/// @brief Retrieve the Controller::NotifyHandler reference that this controller was configured with.
inline Controller::NotifyHandler* Controller::notify_handler() const
{
  return notify_;
}

/// @brief Retrieve the Controller::RdmCommandHandler reference that this controller was configured with.
/// @return A pointer to the handler, or nullptr if it was not provided.
inline Controller::RdmCommandHandler* Controller::rdm_command_handler() const
{
  return rdm_cmd_handler_;
}

/// @brief Retrieve the scope configuration associated with a given scope handle.
/// @return The scope configuration on success.
/// @return #kEtcPalErrNotInit: Module not initialized.
/// @return #kEtcPalErrNotFound: Controller not started, or scope handle not found.
inline etcpal::Expected<Scope> Controller::scope(ScopeHandle scope_handle) const
{
  std::string    scope_id(E133_SCOPE_STRING_PADDED_LENGTH, 0);
  EtcPalSockAddr static_broker_addr;
  etcpal_error_t res =
      rdmnet_controller_get_scope(handle_.value(), scope_handle.value(), &scope_id[0], &static_broker_addr);

  if (res == kEtcPalErrOk)
    return Scope(scope_id, static_broker_addr);
  else
    return res;
}

/// @brief Update the data used to identify this controller to other controllers.
inline void Controller::UpdateRdmData(const RdmData& new_data)
{
  // TODO implement
  ETCPAL_UNUSED_ARG(new_data);
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_CONTROLLER_H_
