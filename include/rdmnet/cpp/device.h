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

/// \file rdmnet/cpp/device.h
/// \brief C++ wrapper for the RDMnet Device API

#ifndef RDMNET_CPP_DEVICE_H_
#define RDMNET_CPP_DEVICE_H_

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include "etcpal/common.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/log.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/device.h"
#include "rdmnet/cpp/common.h"
#include "rdmnet/cpp/client.h"
#include "rdmnet/cpp/message.h"

namespace rdmnet
{
/// \defgroup rdmnet_device_cpp Device API
/// \ingroup rdmnet_cpp_api
/// \brief Implementation of RDMnet device functionality; see \ref using_device.
///
/// RDMnet devices are clients which exclusively receive and respond to RDM commands. Devices
/// operate on only one scope at a time. This API provides classes tailored to the usage concerns
/// of an RDMnet device.
///
/// See \ref using_device for a detailed description of how to use this API.
///
/// @{

/// A handle type used by the RDMnet library to identify device instances.
using DeviceHandle = rdmnet_device_t;
/// An invalid DeviceHandle value.
constexpr DeviceHandle kInvalidDeviceHandle = RDMNET_DEVICE_INVALID;

/// @}

class Device;

/// \ingroup rdmnet_device_cpp
/// \brief A base class for a class that receives notification callbacks from a device.
///
/// See \ref using_device for details of how to use this API.
class DeviceNotifyHandler
{
public:
  /// \brief A device has successfully connected to a broker.
  /// \param handle Handle to device instance which has connected.
  /// \param info More information about the successful connection.
  virtual void HandleConnectedToBroker(DeviceHandle handle, const ClientConnectedInfo& info) = 0;

  /// \brief A connection attempt failed between a device and a broker.
  /// \param handle Handle to device instance which has failed to connect.
  /// \param info More information about the failed connection.
  virtual void HandleBrokerConnectFailed(DeviceHandle handle, const ClientConnectFailedInfo& info) = 0;

  /// \brief A device which was previously connected to a broker has disconnected.
  /// \param handle Handle to device instance which has disconnected.
  /// \param info More information about the disconnect event.
  virtual void HandleDisconnectedFromBroker(DeviceHandle handle, const ClientDisconnectedInfo& info) = 0;

  /// \brief An RDM command has been received addressed to a device.
  /// \param handle Handle to device instance which has received the RDM command.
  /// \param cmd The RDM command data.
  /// \return The action to take in response to this RDM command.
  virtual RdmResponseAction HandleRdmCommand(DeviceHandle handle, const RdmCommand& cmd) = 0;

  /// \brief An RDM command has been received over LLRP, addressed to a device.
  /// \param handle Handle to device instance which has received the RDM command.
  /// \param cmd The RDM command data.
  /// \return The action to take in response to this LLRP RDM command.
  virtual RdmResponseAction HandleLlrpRdmCommand(DeviceHandle handle, const llrp::RdmCommand& cmd) = 0;

  /// \brief The dynamic UID assignment status for a set of virtual responders has been received.
  ///
  /// This callback need only be implemented if adding virtual responders with dynamic UIDs. See
  /// \ref devices_and_gateways and \ref using_device for more information.
  ///
  /// Note that the list may indicate failed assignments for some or all responders, with a status
  /// code.
  ///
  /// \param handle Handle to device instance which has received the dynamic UID assignments.
  /// \param list The list of dynamic UID assignments.
  virtual void HandleDynamicUidStatus(DeviceHandle handle, const DynamicUidAssignmentList& list)
  {
    ETCPAL_UNUSED_ARG(handle);
    ETCPAL_UNUSED_ARG(list);
  }
};

/// \ingroup rdmnet_device_cpp
/// \brief Configuration information for a virtual endpoint on a device.
///
/// Can be implicitly converted from a simple endpoint number to create an endpoint configuration
/// with no initial responders, e.g.:
/// \code
/// rdmnet::VirtualEndpointConfig endpoint_config = 1;
/// \endcode
///
/// Or use the constructors to create an endpoint with responders:
/// \code
/// std::vector<etcpal::Uuid> dynamic_responders;
/// dynamic_responders.push_back(responder_id_1);
/// dynamic_responders.push_back(responder_id_2);
/// rdmnet::VirtualEndpointConfig endpoint_config(2, dynamic_responders);
/// \endcode
///
/// See \ref devices_and_gateways for more information about endpoints.
class VirtualEndpointConfig
{
public:
  VirtualEndpointConfig(uint16_t id, const etcpal::Uuid* dynamic_responders = nullptr,
                        size_t num_dynamic_responders = 0);
  VirtualEndpointConfig(uint16_t id, const std::vector<etcpal::Uuid>& dynamic_responders);
  VirtualEndpointConfig(uint16_t id, const rdm::Uid* static_responders, size_t num_static_responders,
                        const etcpal::Uuid* dynamic_responders = nullptr, size_t num_dynamic_responders = 0);
  VirtualEndpointConfig(uint16_t id, const std::vector<rdm::Uid>& static_responders,
                        const std::vector<etcpal::Uuid>& dynamic_responders = std::vector<etcpal::Uuid>{});

  constexpr const RdmnetVirtualEndpointConfig& get() const noexcept;

private:
  void UpdateConfig();

  std::vector<EtcPalUuid> dynamic_responders_;
  std::vector<RdmUid> static_responders_;
  RdmnetVirtualEndpointConfig config_;
};

/// \brief Create a virtual endpoint configuration with an optional set of virtual responders with dynamic UIDs.
/// \param id Endpoint ID - must be between 1 and 63,999 inclusive.
/// \param dynamic_responders Array of responder IDs identifying the initial virtual responders present on the endpoint.
/// \param num_dynamic_responders Size of responders array.
inline VirtualEndpointConfig::VirtualEndpointConfig(uint16_t id, const etcpal::Uuid* dynamic_responders,
                                                    size_t num_dynamic_responders)
    : config_{RDMNET_VIRTUAL_ENDPOINT_INIT_VALUES(id)}
{
  if (dynamic_responders && num_dynamic_responders)
  {
    std::transform(dynamic_responders, dynamic_responders + num_dynamic_responders,
                   std::back_inserter(dynamic_responders_), [](const etcpal::Uuid& rid) { return rid.get(); });
  }
  UpdateConfig();
}

/// \brief Create a virtual endpoint configuration with a set of virtual responders with dynamic UIDs.
/// \param id Endpoint ID - must be between 1 and 63,999 inclusive.
/// \param dynamic_responders Responder IDs identifying the initial virtual responders present on the endpoint.
inline VirtualEndpointConfig::VirtualEndpointConfig(uint16_t id, const std::vector<etcpal::Uuid>& dynamic_responders)
    : config_{RDMNET_VIRTUAL_ENDPOINT_INIT_VALUES(id)}
{
  if (!dynamic_responders.empty())
  {
    std::transform(dynamic_responders.begin(), dynamic_responders.end(), std::back_inserter(dynamic_responders_),
                   [](const etcpal::Uuid& rid) { return rid.get(); });
  }
  UpdateConfig();
}

/// \brief Create a virtual endpoint configuration with a set of virtual responders.
/// \param id Endpoint ID - must be between 1 and 63,999 inclusive.
/// \param static_responders Array of UIDs identifying the initial virtual responders with static
///                          UIDs present on the endpoint.
/// \param num_static_responders Size of static_responders array.
/// \param dynamic_responders (optional) Array of responder IDs identifying the initial virtual
///                           responders with dynamic UIDs present on the endpoint.
/// \param num_dynamic_responders (optional) Size of dynamic_responders array.
inline VirtualEndpointConfig::VirtualEndpointConfig(uint16_t id, const rdm::Uid* static_responders,
                                                    size_t num_static_responders,
                                                    const etcpal::Uuid* dynamic_responders,
                                                    size_t num_dynamic_responders)
    : config_{RDMNET_VIRTUAL_ENDPOINT_INIT_VALUES(id)}
{
  if (static_responders && num_static_responders)
  {
    std::transform(static_responders, static_responders + num_static_responders, std::back_inserter(static_responders_),
                   [](const rdm::Uid& uid) { return uid.get(); });
  }
  if (dynamic_responders && num_dynamic_responders)
  {
    std::transform(dynamic_responders, dynamic_responders + num_dynamic_responders,
                   std::back_inserter(dynamic_responders_), [](const etcpal::Uuid& rid) { return rid.get(); });
  }
  UpdateConfig();
}

/// \brief Create a virtual endpoint configuration with a set of virtual responders.
/// \param id Endpoint ID - must be between 1 and 63,999 inclusive.
/// \param static_responders UIDs identifying the initial virtual responders with static UIDs
///                          present on the endpoint.
/// \param dynamic_responders (optional) Responder IDs identifying the initial virtual responders
///                           with dynamic UIDs present on the end
inline VirtualEndpointConfig::VirtualEndpointConfig(uint16_t id, const std::vector<rdm::Uid>& static_responders,
                                                    const std::vector<etcpal::Uuid>& dynamic_responders)
    : config_{RDMNET_VIRTUAL_ENDPOINT_INIT_VALUES(id)}
{
  if (!static_responders.empty())
  {
    std::transform(static_responders.begin(), static_responders.end(), std::back_inserter(static_responders_),
                   [](const rdm::Uid& uid) { return uid.get(); });
  }
  if (!dynamic_responders.empty())
  {
    std::transform(dynamic_responders.begin(), dynamic_responders.end(), std::back_inserter(dynamic_responders_),
                   [](const etcpal::Uuid& rid) { return rid.get(); });
  }
  UpdateConfig();
}

/// \brief Get a const reference to the underlying C type.
constexpr const RdmnetVirtualEndpointConfig& VirtualEndpointConfig::get() const noexcept
{
  return config_;
}

// Sets the values of the encapsulated config correctly.
inline void VirtualEndpointConfig::UpdateConfig()
{
  if (!static_responders_.empty())
  {
    config_.static_responders = static_responders_.data();
    config_.num_static_responders = static_responders_.size();
  }
  if (!dynamic_responders_.empty())
  {
    config_.dynamic_responders = dynamic_responders_.data();
    config_.num_dynamic_responders = dynamic_responders_.size();
  }
}

/// \ingroup rdmnet_device_cpp
/// \brief Configuration information for a physical endpoint on a device.
///
/// Can be implicitly converted from a simple endpoint number to create an endpoint configuration
/// with no initial responders, e.g.:
/// \code
/// rdmnet::PhysicalEndpointConfig endpoint_config = 1;
/// \endcode
///
/// Or use the constructors to create an endpoint with responders:
/// \code
/// std::vector<rdm::Uid> physical_responders;
/// physical_responders.push_back(uid_1);
/// physical_responders.push_back(uid_2);
/// rdmnet::PhysicalEndpointConfig endpoint_config(2, physical_responders);
/// \endcode
///
/// \details See \ref devices_and_gateways for more information about endpoints.
class PhysicalEndpointConfig
{
public:
  PhysicalEndpointConfig(uint16_t id, const rdm::Uid* responders = nullptr, size_t num_responders = 0);
  PhysicalEndpointConfig(uint16_t id, const std::vector<rdm::Uid>& responders);

  constexpr const RdmnetPhysicalEndpointConfig& get() const noexcept;

private:
  void UpdateConfig();

  std::vector<RdmUid> responders_;
  RdmnetPhysicalEndpointConfig config_;
};

/// \brief Create a physical endpoint configuration with an optional set of RDM responders.
/// \param id Endpoint ID - must be between 1 and 63,999 inclusive.
/// \param responders Array of UIDs identifying the initial physical RDM responders present on the endpoint.
/// \param num_responders Size of responders array.
inline PhysicalEndpointConfig::PhysicalEndpointConfig(uint16_t id, const rdm::Uid* responders, size_t num_responders)
    : config_{id, nullptr, 0}
{
  if (responders && num_responders)
  {
    std::transform(responders, responders + num_responders, std::back_inserter(responders_),
                   [](const rdm::Uid& resp) { return resp.get(); });
    UpdateConfig();
  }
}

/// \brief Create a physical endpoint configuration with a set of RDM responders.
/// \param id Endpoint ID - must be between 1 and 63,999 inclusive.
/// \param responders UIDs identifying the initial physical RDM responders present on the endpoint.
inline PhysicalEndpointConfig::PhysicalEndpointConfig(uint16_t id, const std::vector<rdm::Uid>& responders)
    : config_{id, nullptr, 0}
{
  if (!responders.empty())
  {
    std::transform(responders.begin(), responders.end(), std::back_inserter(responders_),
                   [](const rdm::Uid& resp) { return resp.get(); });
    UpdateConfig();
  }
}

/// \brief Get a const reference to the underlying C type.
constexpr const RdmnetPhysicalEndpointConfig& PhysicalEndpointConfig::get() const noexcept
{
  return config_;
}

// Sets the values of the encapsulated config correctly.
inline void PhysicalEndpointConfig::UpdateConfig()
{
  config_.responders = responders_.data();
  config_.num_responders = responders_.size();
}

/// A set of configuration settings that a device needs to initialize.
struct DeviceSettings
{
  etcpal::Uuid cid;           ///< The device's Component Identifier (CID).
  rdm::Uid uid;               ///< The device's RDM UID. For a dynamic UID, use rdm::Uid::DynamicUidRequest().
  std::string search_domain;  ///< The device's search domain for discovering brokers.

  /// A data buffer to be used to respond synchronously to RDM commands. See
  /// \ref handling_rdm_commands for more information.
  const uint8_t* response_buf{nullptr};

  /// Array of configurations for virtual endpoints that are present on the device at startup.
  std::vector<VirtualEndpointConfig> virtual_endpoints;
  /// Array of configurations for physical endpoints that are present on the device at startup.
  std::vector<PhysicalEndpointConfig> physical_endpoints;
  /// (optional) A set of network interfaces to use for the LLRP target associated with this
  /// device. If empty, the set passed to rdmnet::Init() will be used, or all network interfaces on
  /// the system if that was not provided.
  std::vector<RdmnetMcastNetintId> llrp_netints;

  /// Create an empty, invalid data structure by default.
  DeviceSettings() = default;
  DeviceSettings(const etcpal::Uuid& new_cid, const rdm::Uid& new_uid);
  DeviceSettings(const etcpal::Uuid& new_cid, uint16_t manufacturer_id);

  bool IsValid() const;
};

/// \brief Create a DeviceSettings instance by passing the required members explicitly.
///
/// This version takes the fully-formed RDM UID that the device will use. Optional members can be
/// modified directly in the struct.
inline DeviceSettings::DeviceSettings(const etcpal::Uuid& new_cid, const rdm::Uid& new_uid) : cid(new_cid), uid(new_uid)
{
}

/// \brief Create a DeviceSettings instance by passing the required members explicitly.
///
/// This version just takes the device's ESTA manufacturer ID and uses it to generate an RDMnet
/// dynamic UID request. Optional members can be modified directly in the struct or through use of
/// the setter methods.
inline DeviceSettings::DeviceSettings(const etcpal::Uuid& cid_in, uint16_t manufacturer_id)
    : cid(cid_in), uid(rdm::Uid::DynamicUidRequest(manufacturer_id))
{
}

/// Determine whether a DeviceSettings instance contains valid data for RDMnet operation.
inline bool DeviceSettings::IsValid() const
{
  return (!cid.IsNull() && (uid.IsStatic() || uid.IsDynamicUidRequest()));
}

/// \ingroup rdmnet_device_cpp
/// \brief An instance of RDMnet device functionality.
///
/// See \ref using_device for details of how to use this API.
class Device
{
public:
  Device() = default;
  Device(const Device& other) = delete;
  Device& operator=(const Device& other) = delete;

  etcpal::Error StartupWithDefaultScope(DeviceNotifyHandler& notify_handler, const DeviceSettings& settings,
                                        const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Error Startup(DeviceNotifyHandler& notify_handler, const DeviceSettings& settings, const char* scope_id_str,
                        const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Error Startup(DeviceNotifyHandler& notify_handler, const DeviceSettings& settings, const Scope& scope_config);
  void Shutdown(rdmnet_disconnect_reason_t disconnect_reason = kRdmnetDisconnectShutdown);

  etcpal::Error ChangeScope(const char* new_scope_id_str, rdmnet_disconnect_reason_t disconnect_reason,
                            const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Error ChangeScope(const Scope& new_scope_config, rdmnet_disconnect_reason_t disconnect_reason);

  etcpal::Error SendRdmAck(const SavedRdmCommand& received_cmd, const uint8_t* response_data = nullptr,
                           size_t response_data_len = 0);
  etcpal::Error SendRdmNack(const SavedRdmCommand& received_cmd, rdm_nack_reason_t nack_reason);
  etcpal::Error SendRdmNack(const SavedRdmCommand& received_cmd, uint16_t raw_nack_reason);
  etcpal::Error SendRdmUpdate(uint16_t param_id, const uint8_t* data = nullptr, size_t data_len = 0);
  etcpal::Error SendRdmUpdate(uint16_t endpoint, const rdm::Uid& source_uid, uint16_t param_id,
                              const uint8_t* data = nullptr, size_t data_len = 0);
  etcpal::Error SendRptStatus(const SavedRdmCommand& received_cmd, rpt_status_code_t status_code,
                              const char* status_string = nullptr);

  etcpal::Error SendLlrpAck(const llrp::SavedRdmCommand& received_cmd, const uint8_t* response_data = nullptr,
                            uint8_t response_data_len = 0);
  etcpal::Error SendLlrpNack(const llrp::SavedRdmCommand& received_cmd, rdm_nack_reason_t nack_reason);
  etcpal::Error SendLlrpNack(const llrp::SavedRdmCommand& received_cmd, uint16_t raw_nack_reason);

  etcpal::Error AddVirtualEndpoint(const VirtualEndpointConfig& endpoint_config);
  etcpal::Error AddVirtualEndpoints(const std::vector<VirtualEndpointConfig>& endpoint_configs);
  etcpal::Error AddPhysicalEndpoint(const PhysicalEndpointConfig& physical_config);
  etcpal::Error AddPhysicalEndpoints(const std::vector<PhysicalEndpointConfig>& endpoint_configs);
  etcpal::Error RemoveEndpoint(uint16_t endpoint_id);
  etcpal::Error RemoveEndpoints(const std::vector<uint16_t>& endpoint_ids);

  etcpal::Error AddVirtualResponder(uint16_t endpoint_id, const etcpal::Uuid& responder_id);
  etcpal::Error AddVirtualResponder(uint16_t endpoint_id, const rdm::Uid& responder_static_uid);
  etcpal::Error AddVirtualResponders(uint16_t endpoint_id, const std::vector<etcpal::Uuid>& responder_ids);
  etcpal::Error AddVirtualResponders(uint16_t endpoint_id, const std::vector<rdm::Uid>& responder_static_uids);
  etcpal::Error AddPhysicalResponder(uint16_t endpoint_id, const rdm::Uid& responder_uid);
  etcpal::Error AddPhysicalResponders(uint16_t endpoint_id, const std::vector<rdm::Uid>& responder_uids);
  etcpal::Error RemoveVirtualResponder(uint16_t endpoint_id, const etcpal::Uuid& responder_id);
  etcpal::Error RemoveVirtualResponder(uint16_t endpoint_id, const rdm::Uid& responder_static_uid);
  etcpal::Error RemoveVirtualResponders(uint16_t endpoint_id, const std::vector<etcpal::Uuid>& responder_ids);
  etcpal::Error RemoveVirtualResponders(uint16_t endpoint_id, const std::vector<rdm::Uid>& responder_static_uids);
  etcpal::Error RemovePhysicalResponder(uint16_t endpoint_id, const rdm::Uid& responder_uid);
  etcpal::Error RemovePhysicalResponders(uint16_t endpoint_id, const std::vector<rdm::Uid>& responder_uids);

  constexpr DeviceHandle handle() const;
  etcpal::Expected<Scope> scope() const;

private:
  DeviceHandle handle_{kInvalidDeviceHandle};
  DeviceNotifyHandler* notify_{nullptr};
};

/// \brief Allocate resources and start up this device with the given configuration on the default
///        RDMnet scope.
///
/// Will immediately attempt to discover and connect to a broker for the default scope (or just
/// connect if a static broker address is given); the status of these attempts will be communicated
/// via the associated DeviceNotifyHandler.
///
/// \param notify_handler A class instance to handle callback notifications from this device.
/// \param settings Configuration settings used by this device.
/// \param static_broker_addr [optional] A static IP address and port at which to connect to a
///                           broker for the default scope.
/// \return etcpal::Error::Ok(): Device started successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return Errors forwarded from rdmnet_device_create().
inline etcpal::Error Device::StartupWithDefaultScope(DeviceNotifyHandler& notify_handler,
                                                     const DeviceSettings& settings,
                                                     const etcpal::SockAddr& static_broker_addr)
{
  ETCPAL_UNUSED_ARG(notify_handler);
  ETCPAL_UNUSED_ARG(settings);
  ETCPAL_UNUSED_ARG(static_broker_addr);
  return kEtcPalErrNotImpl;
}

/// \brief Allocate resources and start up this device with the given configuration on the given
///        RDMnet scope.
///
/// Will immediately attempt to discover and connect to a broker for the given scope (or just
/// connect if a static broker address is given); the status of these attempts will be communicated
/// via the associated DeviceNotifyHandler.
///
/// \param notify_handler A class instance to handle callback notifications from this device.
/// \param settings Configuration settings used by this device.
/// \param scope_id_str The scope ID string.
/// \param static_broker_addr [optional] A static IP address and port at which to connect to a
///                           broker for this scope.
/// \return etcpal::Error::Ok(): Device started successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return Errors forwarded from rdmnet_device_create().
inline etcpal::Error Device::Startup(DeviceNotifyHandler& notify_handler, const DeviceSettings& settings,
                                     const char* scope_id_str, const etcpal::SockAddr& static_broker_addr)
{
  ETCPAL_UNUSED_ARG(notify_handler);
  ETCPAL_UNUSED_ARG(settings);
  ETCPAL_UNUSED_ARG(scope_id_str);
  ETCPAL_UNUSED_ARG(static_broker_addr);
  return kEtcPalErrNotImpl;
}

/// \brief Allocate resources and start up this device with the given configuration on the given
///        RDMnet scope.
///
/// Will immediately attempt to discover and connect to a broker for the given scope (or just
/// connect if a static broker address is given); the status of these attempts will be communicated
/// via the associated DeviceNotifyHandler.
///
/// \param notify_handler A class instance to handle callback notifications from this device.
/// \param settings Configuration settings used by this device.
/// \param scope_config Configuration information for the device's RDMnet scope.
/// \return etcpal::Error::Ok(): Device started successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return Errors forwarded from rdmnet_device_create().
inline etcpal::Error Device::Startup(DeviceNotifyHandler& notify_handler, const DeviceSettings& settings,
                                     const Scope& scope_config)
{
  ETCPAL_UNUSED_ARG(notify_handler);
  ETCPAL_UNUSED_ARG(settings);
  ETCPAL_UNUSED_ARG(scope_config);
  return kEtcPalErrNotImpl;
}

/// \brief Shut down this device and deallocate resources.
///
/// Will disconnect any scope to which this device is currently connected, sending the disconnect
/// reason provided in the disconnect_reason parameter.
///
/// \param disconnect_reason Reason code for disconnecting from the current scope.
inline void Device::Shutdown(rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(disconnect_reason);
}

/// \brief Change the device's RDMnet scope.
///
/// Will disconnect from the current scope, sending the disconnect reason provided in the
/// disconnect_reason parameter, and then attempt to discover and connect to a broker for the new
/// scope. The status of the connection attempt will be communicated via the associated
/// DeviceNotifyHandler.
///
/// \param new_scope_id_str The ID string for the new scope.
/// \param disconnect_reason Reason code for disconnecting from the current scope.
/// \param static_broker_addr [optional] A static IP address and port at which to connect to the
///                           broker for the new scope.
/// \return etcpal::Error::Ok(): Scope changed successfully.
/// \return Errors forwarded from rdmnet_device_change_scope().
inline etcpal::Error Device::ChangeScope(const char* new_scope_id_str, rdmnet_disconnect_reason_t disconnect_reason,
                                         const etcpal::SockAddr& static_broker_addr)
{
  ETCPAL_UNUSED_ARG(new_scope_id_str);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  ETCPAL_UNUSED_ARG(static_broker_addr);
  return kEtcPalErrNotImpl;
}

/// \brief Change the device's RDMnet scope.
///
/// Will disconnect from the current scope, sending the disconnect reason provided in the
/// disconnect_reason parameter, and then attempt to discover and connect to a broker for the new
/// scope. The status of the connection attempt will be communicated via the associated
/// DeviceNotifyHandler.
///
/// \param new_scope_config Configuration information for the new scope.
/// \param disconnect_reason Reason code for disconnecting from the current scope.
/// \return etcpal::Error::Ok(): Scope changed successfully.
/// \return Errors forwarded from rdmnet_device_change_scope().
inline etcpal::Error Device::ChangeScope(const Scope& new_scope_config, rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(new_scope_config);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
}

/// \brief Send an acknowledge (ACK) response to an RDM command received by a device.
/// \param received_cmd The command to which this ACK is a response.
/// \param response_data [optional] The response's RDM parameter data, if it has any.
/// \param response_data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// \return etcpal::Error::Ok(): ACK sent successfully.
/// \return Error codes from rdmnet_device_send_rdm_ack().
inline etcpal::Error Device::SendRdmAck(const SavedRdmCommand& received_cmd, const uint8_t* response_data,
                                        size_t response_data_len)
{
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(response_data);
  ETCPAL_UNUSED_ARG(response_data_len);
  return kEtcPalErrNotImpl;
}

/// \brief Send a negative acknowledge (NACK) response to an RDM command received by a device.
/// \param received_cmd The command to which this NACK is a response.
/// \param nack_reason The RDM NACK reason to send with the NACK response.
/// \return etcpal::Error::Ok(): NACK sent successfully.
/// \return Error codes from rdmnet_device_send_rdm_nack().
inline etcpal::Error Device::SendRdmNack(const SavedRdmCommand& received_cmd, rdm_nack_reason_t nack_reason)
{
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(nack_reason);
  return kEtcPalErrNotImpl;
}

/// \brief Send a negative acknowledge (NACK) response to an RDM command received by a device.
/// \param received_cmd The command to which this NACK is a response.
/// \param raw_nack_reason The NACK reason (either standard or manufacturer-specific) to send with
///                        the NACK response.
/// \return etcpal::Error::Ok(): NACK sent successfully.
/// \return Error codes from rdmnet_device_send_rdm_nack().
inline etcpal::Error Device::SendRdmNack(const SavedRdmCommand& received_cmd, uint16_t raw_nack_reason)
{
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(raw_nack_reason);
  return kEtcPalErrNotImpl;
}

/// \brief Send an asynchronous RDM GET response to update the value of a local parameter.
///
/// This overload is for updating a parameter on the device's default responder - see
/// \ref devices_and_gateways for more information.
///
/// \param param_id The RDM parameter ID that has been updated.
/// \param data [optional] The updated parameter data, if any.
/// \param data_len [optional] The length of the parameter data, if any.
/// \return etcpal::Error::Ok(): RDM update sent successfully.
/// \return Error codes from rdmnet_device_send_rdm_update().
inline etcpal::Error Device::SendRdmUpdate(uint16_t param_id, const uint8_t* data, size_t data_len)
{
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/// \brief Send an asynchronous RDM GET response to update the value of a parameter on a sub-responder.
///
/// This overload is for updating a parameter on a physical or virtual responder associated with
/// one of a device's endpoints. In particular, this is the one for a gateway to use when it
/// collects a new queued message from a responder. See \ref devices_and_gateways for more
/// information.
///
/// \param endpoint The endpoint of the responder that has an updated parameter.
/// \param source_uid The UID of the responder that has an updated parameter.
/// \param param_id The RDM parameter ID that has been updated.
/// \param data [optional] The updated parameter data, if any.
/// \param data_len [optional] The length of the parameter data, if any.
/// \return etcpal::Error::Ok(): RDM update sent successfully.
/// \return Error codes from rdmnet_device_send_rdm_update_from_responder().
inline etcpal::Error Device::SendRdmUpdate(uint16_t endpoint, const rdm::Uid& source_uid, uint16_t param_id,
                                           const uint8_t* data, size_t data_len)
{
  ETCPAL_UNUSED_ARG(endpoint);
  ETCPAL_UNUSED_ARG(source_uid);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/// \brief Send an RPT status message from a device.
///
/// All RPT status messages are handled internally except those associated with RDMnet gateways. If
/// not implementing an RDMnet gateway, this method should not be used. See
/// \ref handling_rdm_commands for more information.
///
/// \param received_cmd The command to which this RPT status is a response.
/// \param status_code A code indicating the result of the command.
/// \param status_string [optional] A string with more information about the status condition.
/// \return etcpal::Error::Ok(): RPT status sent successfully.
/// \return Error codes from rdmnet_device_send_status().
inline etcpal::Error Device::SendRptStatus(const SavedRdmCommand& received_cmd, rpt_status_code_t status_code,
                                           const char* status_string)
{
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(status_code);
  ETCPAL_UNUSED_ARG(status_string);
  return kEtcPalErrNotImpl;
}

/// \brief Send an acknowledge (ACK) response to an RDM command received by a device over LLRP.
/// \param received_cmd The command to which this ACK is a response.
/// \param response_data [optional] The response's RDM parameter data, if it has any.
/// \param response_data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// \return etcpal::Error::Ok(): ACK sent successfully.
/// \return Error codes from rdmnet_device_send_llrp_ack().
inline etcpal::Error Device::SendLlrpAck(const llrp::SavedRdmCommand& received_cmd, const uint8_t* response_data,
                                         uint8_t response_data_len)
{
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(response_data);
  ETCPAL_UNUSED_ARG(response_data_len);
  return kEtcPalErrNotImpl;
}

/// \brief Send a negative acknowledge (NACK) response to an RDM command received by a device over LLRP.
/// \param received_cmd The command to which this NACK is a response.
/// \param nack_reason The RDM NACK reason to send with the NACK response.
/// \return etcpal::Error::Ok(): NACK sent successfully.
/// \return Error codes from rdmnet_device_send_llrp_nack().
inline etcpal::Error Device::SendLlrpNack(const llrp::SavedRdmCommand& received_cmd, rdm_nack_reason_t nack_reason)
{
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(nack_reason);
  return kEtcPalErrNotImpl;
}

/// \brief Send a negative acknowledge (NACK) response to an RDM command received by a device over LLRP.
/// \param received_cmd The command to which this NACK is a response.
/// \param raw_nack_reason The NACK reason (either standard or manufacturer-specific) to send with
///                        the NACK response.
/// \return etcpal::Error::Ok(): NACK sent successfully.
/// \return Error codes from rdmnet_device_send_llrp_nack().
inline etcpal::Error Device::SendLlrpNack(const llrp::SavedRdmCommand& received_cmd, uint16_t raw_nack_reason)
{
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(raw_nack_reason);
  return kEtcPalErrNotImpl;
}

/// \brief Add a virtual endpoint to a device.
/// \details See \ref devices_and_gateways for more information about endpoints.
/// \param endpoint_config Configuration information for the new virtual endpoint.
/// \return etcpal::Error::Ok(): Endpoint added successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started - call Device::Startup() first.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddVirtualEndpoint(const VirtualEndpointConfig& endpoint_config)
{
  ETCPAL_UNUSED_ARG(endpoint_config);
  return kEtcPalErrNotImpl;
}

/// \brief Add multiple virtual endpoints to a device.
/// \details See \ref devices_and_gateways for more information about endpoints.
/// \param endpoint_configs Configuration information for the new virtual endpoints.
/// \return etcpal::Error::Ok(): Endpoints added successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started - call Device::Startup() first.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddVirtualEndpoints(const std::vector<VirtualEndpointConfig>& endpoint_configs)
{
  ETCPAL_UNUSED_ARG(endpoint_configs);
  return kEtcPalErrNotImpl;
}

/// \brief Add a physical endpoint to a device.
/// \details See \ref devices_and_gateways for more information about endpoints.
/// \param endpoint_config Configuration information for the new physical endpoint.
/// \return etcpal::Error::Ok(): Endpoint added successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started - call Device::Startup() first.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddPhysicalEndpoint(const PhysicalEndpointConfig& endpoint_config)
{
  ETCPAL_UNUSED_ARG(endpoint_config);
  return kEtcPalErrNotImpl;
}

/// \brief Add multiple physical endpoints to a device.
/// \details See \ref devices_and_gateways for more information about endpoints.
/// \param endpoint_configs Configuration information for the new physical endpoints.
/// \return etcpal::Error::Ok(): Endpoints added successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started - call Device::Startup() first.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddPhysicalEndpoints(const std::vector<PhysicalEndpointConfig>& endpoint_configs)
{
  ETCPAL_UNUSED_ARG(endpoint_configs);
  return kEtcPalErrNotImpl;
}

/// \brief Remove an endpoint from a device.
/// \details See \ref devices_and_gateways for more information about endpoints.
/// \param endpoint_id ID of the endpoint to remove.
/// \return etcpal::Error::Ok(): Endpoint removed successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started or endpoint_id was not previously added.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::RemoveEndpoint(uint16_t endpoint_id)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  return kEtcPalErrNotImpl;
}

/// \brief Remove multiple endpoints from a device.
/// \details See \ref devices_and_gateways for more information about endpoints.
/// \param endpoint_ids IDs of the endpoints to remove.
/// \return etcpal::Error::Ok(): Endpoints removed successfully.
/// \return #kEtcPalErrInvalid: Invalid argument.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, or one or more endpoint IDs was not previously
///         added.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::RemoveEndpoints(const std::vector<uint16_t>& endpoint_ids)
{
  ETCPAL_UNUSED_ARG(endpoint_ids);
  return kEtcPalErrNotImpl;
}

/// \brief Add a responder with a dynamic UID to a virtual endpoint.
///
/// This function can only be used on virtual endpoints. A dynamic UID for the responder will be
/// requested from the broker and the assigned UID (or error code) will be delivered to
/// DeviceNotifyHandler::HandleDynamicUidStatus(). Save this UID for comparison when handling RDM
/// commands addressed to the dynamic responder. Add the endpoint first with
/// Device::AddVirtualEndpoint(). See \ref devices_and_gateways for more information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to add the responder.
/// \param responder_id Responder ID (permanent UUID representing the responder) to add.
/// \return etcpal::Error::Ok(): Responder added sucessfully (pending dynamic UID assignment).
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, or endpoint_id was not previously added.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddVirtualResponder(uint16_t endpoint_id, const etcpal::Uuid& responder_id)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_id);
  return kEtcPalErrNotImpl;
}

/// \brief Add a responder with a static UID to a virtual endpoint.
///
/// Add the endpoint first with Device::AddVirtualEndpoint(). See \ref devices_and_gateways for
/// more information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to add the responder.
/// \param responder_static_uid Responder UID (permanent static RDM UID representing the responder) to add.
/// \return etcpal::Error::Ok(): Responder added sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, or endpoint_id was not previously added.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddVirtualResponder(uint16_t endpoint_id, const rdm::Uid& responder_static_uid)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_static_uid);
  return kEtcPalErrNotImpl;
}

/// \brief Add multiple responders with dynamic UIDs to a virtual endpoint.
///
/// This function can only be used on virtual endpoints. Dynamic UIDs for the responders will be
/// requested from the broker and the assigned UIDs (or error codes) will be delivered to
/// DeviceNotifyHandler::HandleDynamicUidStatus(). Save these UIDs for comparison when handling RDM
/// commands addressed to the dynamic responders. Add the endpoint first with
/// Device::AddVirtualEndpoint(). See \ref devices_and_gateways for more information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to add the responders.
/// \param responder_ids Responder IDs (permanent UUIDs representing the responder) to add.
/// \return etcpal::Error::Ok(): Responders added sucessfully (pending dynamic UID assignment).
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, or endpoint_id was not previously added.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddVirtualResponders(uint16_t endpoint_id, const std::vector<etcpal::Uuid>& responder_ids)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_ids);
  return kEtcPalErrNotImpl;
}

/// \brief Add multiple responders with static UIDs to a virtual endpoint.
///
/// Add the endpoint first with Device::AddVirtualEndpoint(). See \ref devices_and_gateways for
/// more information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to add the responder.
/// \param responder_static_uids Responder UIDs (permanent static RDM UIDs representing the responder) to add.
/// \return etcpal::Error::Ok(): Responders added sucessfully (pending dynamic UID assignment).
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, or endpoint_id was not previously added.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddVirtualResponders(uint16_t endpoint_id,
                                                  const std::vector<rdm::Uid>& responder_static_uids)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_static_uids);
  return kEtcPalErrNotImpl;
}

/// \brief Add a responder to a physical endpoint.
///
/// Add the endpoint first with Device::AddPhysicalEndpoint(). See \ref devices_and_gateways for
/// more information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to add the responder.
/// \param responder_uid Responder UID to add.
/// \return etcpal::Error::Ok(): Responder added sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a virtual endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, or endpoint_id was not previously added.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddPhysicalResponder(uint16_t endpoint_id, const rdm::Uid& responder_uid)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_uid);
  return kEtcPalErrNotImpl;
}

/// \brief Add multiple responders to a physical endpoint.
///
/// Add the endpoint first with Device::AddPhysicalEndpoint(). See \ref devices_and_gateways for
/// more information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to add the responders.
/// \param responder_uids Responder UIDs to add.
/// \return etcpal::Error::Ok(): Responders added sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a virtual endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, or endpoint_id was not previously added.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::AddPhysicalResponders(uint16_t endpoint_id, const std::vector<rdm::Uid>& responder_uids)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_uids);
  return kEtcPalErrNotImpl;
}

/// \brief Remove a responder with a dynamic UID from a virtual endpoint.
///
/// This function can only be used on virtual endpoints. See \ref devices_and_gateways for more
/// information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to remove the responder.
/// \param responder_id Responder ID to remove.
/// \return etcpal::Error::Ok(): Responder removed sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, endpoint_id was not previously added, or
///         responder_id was not previously added to the endpoint.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::RemoveVirtualResponder(uint16_t endpoint_id, const etcpal::Uuid& responder_id)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_id);
  return kEtcPalErrNotImpl;
}

/// \brief Remove a responder with a static UID from a virtual endpoint.
///
/// This function can only be used on virtual endpoints. See \ref devices_and_gateways for more
/// information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to remove the responder.
/// \param responder_static_uid RDM UID of responder to remove.
/// \return etcpal::Error::Ok(): Responder removed sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, endpoint_id was not previously added, or
///         responder_static_uid was not previously added to the endpoint.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::RemoveVirtualResponder(uint16_t endpoint_id, const rdm::Uid& responder_static_uid)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_static_uid);
  return kEtcPalErrNotImpl;
}

/// \brief Remove multiple responder with dynamic UIDs from a virtual endpoint.
///
/// This function can only be used on virtual endpoints. See \ref devices_and_gateways for more
/// information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to remove the responders.
/// \param responder_ids Responder IDs to remove.
/// \return etcpal::Error::Ok(): Responders removed sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, endpoint_id was not previously added, or one
///         or more responder IDs were not previously added to the endpoint.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::RemoveVirtualResponders(uint16_t endpoint_id,
                                                     const std::vector<etcpal::Uuid>& responder_ids)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_ids);
  return kEtcPalErrNotImpl;
}

/// \brief Remove multiple responders with static UIDs from a virtual endpoint.
///
/// This function can only be used on virtual endpoints. See \ref devices_and_gateways for more
/// information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to remove the responders.
/// \param responder_static_uids RDM UIDs of the responders to remove.
/// \return etcpal::Error::Ok(): Responders removed sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a physical endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, endpoint_id was not previously added, or one
///         or more responder UIDs were not previously added to the endpoint.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::RemoveVirtualResponders(uint16_t endpoint_id,
                                                     const std::vector<rdm::Uid>& responder_static_uids)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_static_uids);
  return kEtcPalErrNotImpl;
}

/// \brief Remove a responder from a physical endpoint.
///
/// This function can only be used on physical endpoints. See \ref devices_and_gateways for more
/// information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to remove the responder.
/// \param responder_uid RDM UID of responder to remove.
/// \return etcpal::Error::Ok(): Responder removed sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a virtual endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, endpoint_id was not previously added, or
///         responder_uid was not previously added to the endpoint.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::RemovePhysicalResponder(uint16_t endpoint_id, const rdm::Uid& responder_uid)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_uid);
  return kEtcPalErrNotImpl;
}

/// \brief Remove multiple responders from a physical endpoint.
///
/// This function can only be used on physical endpoints. See \ref devices_and_gateways for more
/// information on endpoints.
///
/// \param endpoint_id ID for the endpoint on which to remove the responders.
/// \param responder_uids RDM UIDs of responders to remove.
/// \return etcpal::Error::Ok(): Responders removed sucessfully.
/// \return #kEtcPalErrInvalid: Invalid argument, or the endpoint is a virtual endpoint.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started, endpoint_id was not previously added, or one
///         or more responder UIDs were not previously added to the endpoint.
/// \return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error Device::RemovePhysicalResponders(uint16_t endpoint_id, const std::vector<rdm::Uid>& responder_uids)
{
  ETCPAL_UNUSED_ARG(endpoint_id);
  ETCPAL_UNUSED_ARG(responder_uids);
  return kEtcPalErrNotImpl;
}

/// \brief Retrieve the handle of a device instance.
constexpr DeviceHandle Device::handle() const
{
  return handle_;
}

/// \brief Retrieve the scope configuration associated with a device instance.
/// \return The scope configuration on success.
/// \return #kEtcPalErrNotInit: Module not initialized.
/// \return #kEtcPalErrNotFound: Device not started.
inline etcpal::Expected<Scope> Device::scope() const
{
  return kEtcPalErrNotImpl;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_DEVICE_H_
