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
/// @{

using DeviceHandle = rdmnet_device_t;

/// @}

/// \brief A base class for a class that receives notification callbacks from a controller.
/// \ingroup rdmnet_device_cpp
class DeviceNotifyHandler
{
public:
  virtual void HandleConnectedToBroker(const RdmnetClientConnectedInfo& info) = 0;
  virtual void HandleBrokerConnectFailed(const RdmnetClientConnectFailedInfo& info) = 0;
  virtual void HandleDisconnectedFromBroker(const RdmnetClientDisconnectedInfo& info) = 0;
  virtual void HandleRdmCommand(const RemoteRdmCommand& cmd) = 0;
  virtual void HandleLlrpRdmCommand(const LlrpRemoteRdmCommand& cmd) = 0;
};

/// \brief An instance of RDMnet Device functionality.
/// \ingroup rdmnet_device_cpp
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
  const etcpal::Uuid& cid() const;

  void SetMcastNetints(const std::vector<RdmnetMcastNetintId>& mcast_netints);
  void SetUid(const RdmUid& uid);
  void SetSearchDomain(const std::string& search_domain);

private:
  DeviceNotifyHandler* notify_{nullptr};
  DeviceHandle handle_{};
  etcpal::Uuid cid_;
  RdmUid uid_{};
  std::string search_domain_;
};

};  // namespace rdmnet

#endif  // RDMNET_CPP_DEVICE_H_
