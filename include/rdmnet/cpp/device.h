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
/// \defgroup rdmnet_device_cpp Device C++ Language API
/// \ingroup rdmnet_device
/// \brief C++ Language version of the Device API
///
/// See \ref using_device for details of how to use this API.
///
/// @{

using DeviceHandle = rdmnet_device_t;

/// @}

class Device;

/// \ingroup rdmnet_device_cpp
/// \brief A base class for a class that receives notification callbacks from a controller.
///
/// See \ref using_device for details of how to use this API.
class DeviceNotifyHandler
{
public:
  virtual void HandleConnectedToBroker(Device& device, const RdmnetClientConnectedInfo& info) = 0;
  virtual void HandleBrokerConnectFailed(Device& device, const RdmnetClientConnectFailedInfo& info) = 0;
  virtual void HandleDisconnectedFromBroker(Device& device, const RdmnetClientDisconnectedInfo& info) = 0;
  virtual void HandleRdmCommand(Device& device, const RemoteRdmCommand& cmd) = 0;
  virtual void HandleLlrpRdmCommand(Device& device, const LlrpRemoteRdmCommand& cmd) = 0;
};

struct DeviceData
{
  etcpal::Uuid cid;           ///< The device's Component Identifier (CID).
  rdm::Uid uid;               ///< The device's RDM UID. For a dynamic UID, use ::rdm::Uid::DynamicUidRequest().
  std::string search_domain;  ///< The device's search domain for discovering brokers.

  DeviceData() = default;
  DeviceData(const etcpal::Uuid& cid_in, const rdm::Uid& uid_in, const std::string& search_domain_in);
  static DeviceData Default(uint16_t manufacturer_id);
};

/// \ingroup rdmnet_device_cpp
/// \brief An instance of RDMnet Device functionality.
///
/// See \ref using_device for details of how to use this API.
class Device
{
public:
  etcpal::Result StartupWithDefaultScope(const etcpal::Uuid& cid, DeviceNotifyHandler& notify_handler,
                                         const etcpal::Sockaddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Result Startup(const etcpal::Uuid& cid, DeviceNotifyHandler& notify_handler, const std::string& scope_str,
                         const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  void Shutdown();

  etcpal::Result SendRdmResponse(const LocalRdmResponse& resp);
  etcpal::Result SendLlrpResponse(const LlrpLocalRdmResponse& resp);

  DeviceHandle handle() const;
  const DeviceData& data() const;
  Scope scope() const;

  static etcpal::Result Init(
      const EtcPalLogParams* log_params = nullptr,
      const std::vector<RdmnetMcastNetintId>& mcast_netints = std::vector<RdmnetMcastNetintId>{});
  static etcpal::Result Init(const etcpal::Logger& logger, const std::vector<RdmnetMcastNetintId>& mcast_netints =
                                                               std::vector<RdmnetMcastNetintId>{});
  static void Deinit();

private:
  DeviceHandle handle_{RDMNET_DEVICE_INVALID};
  DeviceData my_data_;
  DeviceNotifyHandler* notify_{nullptr};
};

};  // namespace rdmnet

#endif  // RDMNET_CPP_DEVICE_H_
