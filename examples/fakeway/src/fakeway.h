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
#include "etcpal/cpp/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/device.h"

#include "fakeway_log.h"
#include "fakeway_default_responder.h"
#include "gadget_interface.h"
#include "rdmnet_lib_wrapper.h"

class PhysicalEndpoint
{
public:
  static const std::set<uint16_t> kPhysicalEndpointPIDs;

  PhysicalEndpoint(uint16_t id, unsigned int gadget_id, unsigned int gadget_port_num)
      : id_(id), gadget_id_(gadget_id), gadget_port_num_(gadget_port_num)
  {
  }
  virtual ~PhysicalEndpoint() = default;

  bool QueueMessageForResponder(const RdmUid& uid, std::unique_ptr<RemoteRdmCommand> cmd);
  void GotResponse(const RdmUid& uid, const RemoteRdmCommand* cmd);
  bool AddResponder(const RdmUid& uid);
  bool RemoveResponder(const RdmUid& uid, std::vector<std::unique_ptr<RemoteRdmCommand>>& orphaned_msgs);

  unsigned int gadget_id() const { return gadget_id_; }
  unsigned int port_num() const { return gadget_port_num_; }

protected:
  uint16_t id_;
  unsigned int gadget_id_;
  unsigned int gadget_port_num_;
  etcpal::Mutex responder_lock_;
  std::map<RdmUid, std::vector<std::unique_ptr<RemoteRdmCommand>>> responders_;
};

class Fakeway : public GadgetNotify, public RdmnetLibNotify
{
public:
  Fakeway(std::unique_ptr<RdmnetLibInterface> rdmnet = std::make_unique<RdmnetLibWrapper>())
      : log_("fakeway.log"), rdmnet_(std::move(rdmnet))
  {
  }

  static void PrintVersion();

  // Starts up the RDMnet Device logic with the scope configuration provided. Also initializes the
  // USB library and begins listening for Gadgets.
  bool Startup(const RdmnetScopeConfig& scope_config);
  // Shuts down RDMnet and USB/RDM functionality.
  void Shutdown();

  // Is this Fakeway connected to a Broker?
  bool connected() const { return connected_to_broker_; }

  FakewayLog* log() { return &log_; }
  FakewayDefaultResponder* def_resp() const { return def_resp_.get(); }

protected:
  void Connected(const RdmnetClientConnectedInfo& info) override;
  void ConnectFailed(const RdmnetClientConnectFailedInfo& info) override;
  void Disconnected(const RdmnetClientDisconnectedInfo& info) override;
  void RdmCommandReceived(const RemoteRdmCommand& cmd) override;
  void LlrpRdmCommandReceived(const LlrpRemoteRdmCommand& cmd) override;

  void ProcessDefRespRdmCommand(const RemoteRdmCommand& cmd, RdmnetConfigChange& config_change);
  void ProcessDefRespRdmCommand(const LlrpRemoteRdmCommand& cmd, RdmnetConfigChange& config_change);
  bool ProcessDefRespRdmCommand(const RdmCommand& cmd, std::vector<RdmResponse>& resp_list, uint16_t& nack_reason,
                                RdmnetConfigChange& config_change);

  // Send responses
  void SendRptStatus(const RemoteRdmCommand& received_cmd, rpt_status_code_t status_code);
  void SendRptNack(const RemoteRdmCommand& received_cmd, uint16_t nack_reason);
  void SendRptResponse(const RemoteRdmCommand& received_cmd, std::vector<RdmResponse>& resp_list);
  void SendUnsolicitedRptResponse(uint16_t from_endpoint, std::vector<RdmResponse>& resp_list);

  void SendLlrpNack(const LlrpRemoteRdmCommand& received_cmd, uint16_t nack_reason);
  void SendLlrpResponse(const LlrpRemoteRdmCommand& received_cmd, const RdmResponse& resp);

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
  FakewayLog log_;
  std::unique_ptr<FakewayDefaultResponder> def_resp_;
  std::unique_ptr<RdmnetLibInterface> rdmnet_;

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
