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

#include "rdmnet/device.h"
#include "rdmnet/cpp/client.h"

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

using DeviceHandle = rdmnet_device_t;

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
  /// \param device Device instance which has connected.
  /// \param info More information about the successful connection.
  virtual void HandleConnectedToBroker(Device& device, const RdmnetClientConnectedInfo& info) = 0;

  /// \brief A connection attempt failed between a device and a broker.
  /// \param device Device instance which has failed to connect.
  /// \param info More information about the failed connection.
  virtual void HandleBrokerConnectFailed(Device& device, const RdmnetClientConnectFailedInfo& info) = 0;

  /// \brief A device which was previously connected to a broker has disconnected.
  /// \param device Device instance which has disconnected.
  /// \param info More information about the disconnect event.
  virtual void HandleDisconnectedFromBroker(Device& device, const RdmnetClientDisconnectedInfo& info) = 0;

  /// \brief An RDM command has been received addressed to a device.
  /// \param device Device instance which has received the RDM command.
  /// \param cmd The RDM command data.
  virtual void HandleRdmCommand(Device& device, const RdmnetRemoteRdmCommand& cmd) = 0;

  /// \brief An RDM command has been received over LLRP, addressed to a device.
  /// \param device Device instance which has received the RDM command.
  /// \param cmd The RDM command data.
  virtual void HandleLlrpRdmCommand(Device& device, const LlrpRemoteRdmCommand& cmd) = 0;
};

// A set of configuration data that a device needs to initialize.
struct DeviceData
{
  etcpal::Uuid cid;           ///< The device's Component Identifier (CID).
  rdm::Uid uid;               ///< The device's RDM UID. For a dynamic UID, use ::rdm::Uid::DynamicUidRequest().
  std::string search_domain;  ///< The device's search domain for discovering brokers.

  /// Create an empty, invalid data structure by default.
  DeviceData() = default;
  DeviceData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in,
             const std::string& search_domain_in = E133_DEFAULT_DOMAIN);

  bool IsValid() const;
};

/// Create a DeviceData instance by passing all members explicitly. Search domain is optional and
/// has a default value.
inline bool DeviceData::DeviceData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in,
                                   const std::string& search_domain_in = E133_DEFAULT_DOMAIN)
    : cid(cid_in), uid(uid_in), search_domain(search_domain_in)
{
}

/// Determine whether a DeviceData instance contains valid data for RDMnet operation.
inline bool DeviceData::IsValid() const
{
  return (!cid.IsNull() && (uid.IsStatic() || uid.IsDynamicUidRequest()) && !search_domain.empty());
}

/// \ingroup rdmnet_device_cpp
/// \brief An instance of RDMnet device functionality.
///
/// See \ref using_device for details of how to use this API.
class Device
{
public:
  etcpal::Error StartupWithDefaultScope(DeviceNotifyHandler& notify_handler, const DeviceData& data,
                                         const etcpal::Sockaddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Error Startup(DeviceNotifyHandler& notify_handler, const DeviceData& data, const std::string& scope_str,
                         const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Error Startup(DeviceNotifyHandler& notify_handler, const DeviceData& data, const Scope& scope_config);
  void Shutdown();

  etcpal::Error SendRdmResponse(const RdmnetLocalRdmResponse& resp);
  etcpal::Error SendLlrpResponse(const LlrpLocalRdmResponse& resp);
  etcpal::Error ChangeScope(const Scope& new_scope_config, rdmnet_disconnect_reason_t disconnect_reason);
  etcpal::Error ChangeScope(const std::string& new_scope_string, rdmnet_disconnect_reason_t disconnect_reason,
                             const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});

  DeviceHandle handle() const;
  const DeviceData& data() const;
  Scope scope() const;

  static etcpal::Error Init(
      const EtcPalLogParams* log_params = nullptr,
      const std::vector<RdmnetMcastNetintId>& mcast_netints = std::vector<RdmnetMcastNetintId>{});
  static etcpal::Error Init(const etcpal::Logger& logger, const std::vector<RdmnetMcastNetintId>& mcast_netints =
                                                               std::vector<RdmnetMcastNetintId>{});
  static void Deinit();

private:
  DeviceHandle handle_{RDMNET_DEVICE_INVALID};
  DeviceData my_data_;
  DeviceNotifyHandler* notify_{nullptr};
};

};  // namespace rdmnet

#endif  // RDMNET_CPP_DEVICE_H_
