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

// The generic Fakeway logic.  This spawns up its own threads for socket
// listening, reading, etc.

#ifndef FAKEWAY_H_
#define FAKEWAY_H_

#include <string>
#include <vector>
#include <memory>
#include <cassert>

#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/lock.h"
#include "etcpal/cpp/log.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/cpp/device.h"

#include "fakeway_default_responder.h"
#include "gadget_interface.h"

class PhysicalEndpoint
{
public:
  PhysicalEndpoint(uint16_t id, unsigned int gadget_id, unsigned int gadget_port_num)
      : id_(id), gadget_id_(gadget_id), gadget_port_num_(gadget_port_num)
  {
  }
  virtual ~PhysicalEndpoint() = default;

  bool QueueMessageForResponder(const rdm::Uid& uid, std::unique_ptr<rdmnet::SavedRdmCommand> cmd);
  void GotResponse(const rdm::Uid& uid, const rdmnet::SavedRdmCommand* cmd);
  bool AddResponder(const rdm::Uid& uid);
  bool RemoveResponder(const rdm::Uid& uid, std::vector<std::unique_ptr<rdmnet::SavedRdmCommand>>& orphaned_msgs);

  unsigned int gadget_id() const { return gadget_id_; }
  unsigned int port_num() const { return gadget_port_num_; }

protected:
  uint16_t id_;
  unsigned int gadget_id_;
  unsigned int gadget_port_num_;
  etcpal::Mutex responder_lock_;
  std::map<rdm::Uid, std::vector<std::unique_ptr<rdmnet::SavedRdmCommand>>> responders_;
};

class Fakeway : public GadgetNotify, public rdmnet::DeviceNotifyHandler
{
public:
  static void PrintVersion();

  // Starts up the RDMnet Device logic with the scope configuration provided. Also initializes the
  // USB library and begins listening for Gadgets.
  bool Startup(const rdmnet::Scope& scope_config, etcpal::Logger& logger, const etcpal::Uuid& cid);
  // Shuts down RDMnet and USB/RDM functionality.
  void Shutdown();

  // Is this Fakeway connected to a Broker?
  bool connected() const { return connected_to_broker_; }

  FakewayDefaultResponder* def_resp() const { return def_resp_.get(); }

protected:
  void HandleConnectedToBroker(rdmnet::DeviceHandle handle, const rdmnet::ClientConnectedInfo& info) override;
  void HandleBrokerConnectFailed(rdmnet::DeviceHandle handle, const rdmnet::ClientConnectFailedInfo& info) override;
  void HandleDisconnectedFromBroker(rdmnet::DeviceHandle handle, const rdmnet::ClientDisconnectedInfo& info) override;
  rdmnet::RdmResponseAction HandleRdmCommand(rdmnet::DeviceHandle handle, const rdmnet::RdmCommand& cmd) override;
  rdmnet::RdmResponseAction HandleLlrpRdmCommand(rdmnet::DeviceHandle handle,
                                                 const rdmnet::llrp::RdmCommand& cmd) override;

  rdmnet::RdmResponseAction ProcessDefRespRdmCommand(const rdm::CommandHeader& cmd, const uint8_t* data,
                                                     uint8_t data_len);

  ////////////////////////////////////////////////////////////////////////////
  // GadgetNotify interface

  void HandleGadgetConnected(unsigned int gadget_id, unsigned int num_ports) override;
  void HandleGadgetDisconnected(unsigned int gadget_id) override;
  void HandleNewRdmResponderDiscovered(unsigned int gadget_id, unsigned int port_number,
                                       const RdmDeviceInfo& info) override;
  void HandleRdmResponderLost(unsigned int gadget_id, unsigned int port_number, uid id) override;
  void HandleRdmResponse(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& response,
                         const void* cookie) override;
  void HandleRdmTimeout(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& orig_cmd,
                        const void* cookie) override;
  void HandleGadgetLogMsg(const char* str) override;

private:
  bool configuration_change_{false};
  etcpal::Logger* log_{nullptr};
  std::unique_ptr<FakewayDefaultResponder> def_resp_;
  rdmnet::Device rdmnet_;

  etcpal::SockAddr resolved_broker_addr_;

  bool connected_to_broker_{false};

  // Keep track of Physical Endpoints
  uint16_t next_endpoint_id_{1};
  std::map<uint16_t, std::unique_ptr<PhysicalEndpoint>> physical_endpoints_;
  std::map<unsigned int, std::vector<uint16_t>> physical_endpoint_rev_lookup_;
  etcpal::RwLock endpoint_lock_;

  // Local RDM
  GadgetInterface gadget_;
};

#endif  // FAKEWAY_H_
