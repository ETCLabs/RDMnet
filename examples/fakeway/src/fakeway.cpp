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

// Fakeway.cpp, implementation of the Fakeway class.

#include "fakeway.h"

#include <iostream>
#include "etcpal/cpp/error.h"
#include "etcpal/pack.h"
#include "rdm/responder.h"

bool PhysicalEndpoint::QueueMessageForResponder(const RdmUid& uid, std::unique_ptr<RdmnetRemoteRdmCommand> cmd)
{
  etcpal::MutexGuard responder_guard(responder_lock_);

  auto responder_pair = responders_.find(uid);
  if (responder_pair != responders_.end())
  {
    responder_pair->second.push_back(std::move(cmd));
    return true;
  }
  return false;
}

void PhysicalEndpoint::GotResponse(const RdmUid& uid, const RdmnetRemoteRdmCommand* cmd)
{
  // Cleanup the command from our queue
  etcpal::MutexGuard responder_guard(responder_lock_);

  auto responder_pair = responders_.find(uid);
  if (responder_pair != responders_.end())
  {
    for (auto cmd_ptr = responder_pair->second.begin(); cmd_ptr != responder_pair->second.end(); ++cmd_ptr)
    {
      if (cmd_ptr->get() == cmd)
      {
        responder_pair->second.erase(cmd_ptr);
        break;
      }
    }
  }
}

bool PhysicalEndpoint::AddResponder(const RdmUid& uid)
{
  etcpal::MutexGuard responder_guard(responder_lock_);
  auto insert_result = responders_.insert(std::make_pair(uid, std::vector<std::unique_ptr<RdmnetRemoteRdmCommand>>()));
  return insert_result.second;
}

bool PhysicalEndpoint::RemoveResponder(const RdmUid& uid,
                                       std::vector<std::unique_ptr<RdmnetRemoteRdmCommand>>& orphaned_msgs)
{
  etcpal::MutexGuard responder_guard(responder_lock_);

  auto responder_pair = responders_.find(uid);
  if (responder_pair != responders_.end())
  {
    orphaned_msgs.swap(responder_pair->second);
    responders_.erase(responder_pair);
    return true;
  }
  return false;
}

void Fakeway::PrintVersion()
{
  std::cout << "ETC Example RDMnet Gateway Emulator (\"Fakeway\")\n";
  std::cout << "Version " << RDMNET_VERSION_STRING << "\n\n";
  std::cout << RDMNET_VERSION_COPYRIGHT << "\n";
  std::cout << "License: Apache License v2.0 <http://www.apache.org/licenses/LICENSE-2.0>\n";
  std::cout << "Unless required by applicable law or agreed to in writing, this software is\n";
  std::cout << "provided \"AS IS\", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express\n";
  std::cout << "or implied.\n";
}

bool Fakeway::Startup(const RdmnetScopeConfig& scope_config)
{
  def_resp_ = std::make_unique<FakewayDefaultResponder>(scope_config, E133_DEFAULT_DOMAIN);

  log_.Info("Using libGadget version %s", GadgetInterface::DllVersion().c_str());
  if (!gadget_.Startup(*this, log_))
    return false;

  // A typical hardware-locked device would use etcpal::Uuid::V3() to generate a CID that is
  // the same every time. But this example device is not locked to hardware, so a V4 UUID makes
  // more sense.
  auto my_cid = etcpal::Uuid::V4();
  auto res = rdmnet_->Startup(my_cid, scope_config, this, &log_);
  if (!res)
  {
    gadget_.Shutdown();
    log_.Critical("Fatal: couldn't start RDMnet library due to error: '%s'", res.ToCString());
    return false;
  }

  return true;
}

void Fakeway::Shutdown()
{
  configuration_change_ = true;
  rdmnet_->Shutdown();
  gadget_.Shutdown();

  physical_endpoints_.clear();
  physical_endpoint_rev_lookup_.clear();
}

void Fakeway::Connected(const RdmnetClientConnectedInfo& info)
{
  connected_to_broker_ = true;
  if (log_.CanLog(ETCPAL_LOG_INFO))
  {
    log_.Info("Connected to broker for scope %s at address %s", def_resp_->scope_config().scope,
              etcpal::SockAddr(info.broker_addr).ToString().c_str());
  }
}

void Fakeway::ConnectFailed(const RdmnetClientConnectFailedInfo& info)
{
  connected_to_broker_ = false;
  log_.Info("Connect failed to broker for scope %s.%s", def_resp_->scope_config().scope,
            info.will_retry ? " Retrying..." : "");
}

void Fakeway::Disconnected(const RdmnetClientDisconnectedInfo& info)
{
  connected_to_broker_ = false;
  log_.Info("Disconnected from broker for scope %s.%s", def_resp_->scope_config().scope,
            info.will_retry ? " Retrying..." : "");
}

void Fakeway::RdmCommandReceived(const RdmnetRemoteRdmCommand& cmd)
{
  if (cmd.dest_endpoint == E133_NULL_ENDPOINT)
  {
    RdmnetConfigChange change = RdmnetConfigChange::kNoChange;
    ProcessDefRespRdmCommand(cmd, change);
    if (change == RdmnetConfigChange::kScopeConfigChanged)
    {
      rdmnet_->ChangeScope(def_resp_->scope_config(), kRdmnetDisconnectRptReconfigure);
    }
    else if (change == RdmnetConfigChange::kSearchDomainChanged)
    {
      rdmnet_->ChangeSearchDomain(def_resp_->search_domain(), kRdmnetDisconnectRptReconfigure);
    }
  }
  else
  {
    etcpal::ReadGuard endpoint_read(endpoint_lock_);
    auto endpoint_pair = physical_endpoints_.find(cmd.dest_endpoint);
    if (endpoint_pair != physical_endpoints_.end())
    {
      PhysicalEndpoint* endpoint = endpoint_pair->second.get();
      auto saved_cmd = std::make_unique<RemoteRdmCommand>(cmd);
      assert(saved_cmd);

      auto raw_saved_cmd = saved_cmd.get();
      const RdmCommand& rdm = cmd.rdm;
      if (endpoint->QueueMessageForResponder(rdm.dest_uid, std::move(saved_cmd)))
      {
        RDM_CmdC to_send(static_cast<uint8_t>(rdm.command_class), rdm.param_id, rdm.subdevice, rdm.datalen, rdm.data,
                         rdm.dest_uid.manu, rdm.dest_uid.id);
        gadget_.SendRdmCommand(endpoint->gadget_id(), endpoint->port_num(), to_send, raw_saved_cmd);
      }
      else
      {
        SendRptStatus(cmd, kRptStatusUnknownRdmUid);
      }
    }
    else
    {
      SendRptStatus(cmd, kRptStatusUnknownEndpoint);
    }
  }
}

void Fakeway::LlrpRdmCommandReceived(const LlrpRemoteRdmCommand& cmd)
{
  RdmnetConfigChange change = RdmnetConfigChange::kNoChange;
  ProcessDefRespRdmCommand(cmd, change);
  if (change == RdmnetConfigChange::kScopeConfigChanged)
  {
    rdmnet_->ChangeScope(def_resp_->scope_config(), kRdmnetDisconnectLlrpReconfigure);
  }
  else if (change == RdmnetConfigChange::kSearchDomainChanged)
  {
    rdmnet_->ChangeSearchDomain(def_resp_->search_domain(), kRdmnetDisconnectLlrpReconfigure);
  }
}

void Fakeway::ProcessDefRespRdmCommand(const RdmnetRemoteRdmCommand& cmd, RdmnetConfigChange& config_change)
{
  const RdmCommand& rdm = cmd.rdm;
  if (rdm.command_class != kRdmCCGetCommand && rdm.command_class != kRdmCCSetCommand)
  {
    SendRptStatus(cmd, kRptStatusInvalidCommandClass);
    log_.Warning("Device received RDM command with invalid command class %d", rdm.command_class);
  }
  else if (!def_resp_->SupportsPid(rdm.param_id))
  {
    SendRptNack(cmd, E120_NR_UNKNOWN_PID);
    log_.Debug("Sending NACK to Controller %04x:%08x for unknown PID 0x%04x", cmd.source_uid.manu, cmd.source_uid.id,
               rdm.param_id);
  }
  else
  {
    std::vector<RdmResponse> resp_list;
    uint16_t nack_reason;
    if (ProcessDefRespRdmCommand(cmd.rdm, resp_list, nack_reason, config_change))
    {
      SendRptResponse(cmd, resp_list);
      log_.Debug("ACK'ing SET_COMMAND for PID 0x%04x from Controller %04x:%08x", rdm.param_id, cmd.source_uid.manu,
                 cmd.source_uid.id);
    }
    else
    {
      SendRptNack(cmd, nack_reason);
      log_.Debug("Sending NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
                 cmd.source_uid.manu, cmd.source_uid.id, rdm.param_id, nack_reason);
    }
  }
}

void Fakeway::ProcessDefRespRdmCommand(const LlrpRemoteRdmCommand& cmd, RdmnetConfigChange& config_change)
{
  const RdmCommand& rdm = cmd.rdm;
  if (rdm.command_class != kRdmCCGetCommand && rdm.command_class != kRdmCCSetCommand)
  {
    SendLlrpNack(cmd, E120_NR_UNSUPPORTED_COMMAND_CLASS);
    log_.Warning("Device received RDM command with invalid command class %d", rdm.command_class);
  }
  else if (!def_resp_->SupportsPid(rdm.param_id))
  {
    SendLlrpNack(cmd, E120_NR_UNKNOWN_PID);
    log_.Debug("Sending NACK to Controller %04x:%08x for unknown PID 0x%04x", rdm.source_uid.manu, rdm.source_uid.id,
               rdm.param_id);
  }
  else
  {
    std::vector<RdmResponse> resp_list;
    uint16_t nack_reason;
    if (ProcessDefRespRdmCommand(cmd.rdm, resp_list, nack_reason, config_change))
    {
      SendLlrpResponse(cmd, resp_list[0]);
      log_.Debug("ACK'ing SET_COMMAND for PID 0x%04x from Controller %04x:%08x", rdm.param_id, rdm.source_uid.manu,
                 rdm.source_uid.id);
    }
    else
    {
      SendLlrpNack(cmd, nack_reason);
      log_.Debug("Sending NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
                 rdm.source_uid.manu, rdm.source_uid.id, rdm.param_id, nack_reason);
    }
  }
}

bool Fakeway::ProcessDefRespRdmCommand(const RdmCommand& cmd, std::vector<RdmResponse>& resp_list,
                                       uint16_t& nack_reason, RdmnetConfigChange& config_change)
{
  bool res = false;
  switch (cmd.command_class)
  {
    case kRdmCCSetCommand:
      if (def_resp_->Set(cmd.param_id, cmd.data, cmd.datalen, nack_reason, config_change))
      {
        RdmResponse resp_data;
        resp_data.source_uid = cmd.dest_uid;
        resp_data.dest_uid = cmd.source_uid;
        resp_data.transaction_num = cmd.transaction_num;
        resp_data.resp_type = kRdmResponseTypeAck;
        resp_data.msg_count = 0;
        resp_data.subdevice = 0;
        resp_data.command_class = kRdmCCSetCommandResponse;
        resp_data.param_id = cmd.param_id;
        resp_data.datalen = 0;

        resp_list.push_back(resp_data);
        res = true;
      }
      break;
    case kRdmCCGetCommand:
    {
      FakewayDefaultResponder::ParamDataList resp_data_list;

      if (def_resp_->Get(cmd.param_id, cmd.data, cmd.datalen, resp_data_list, nack_reason) && !resp_data_list.empty())
      {
        RdmResponse resp_data;

        resp_data.source_uid = cmd.dest_uid;
        resp_data.dest_uid = cmd.source_uid;
        resp_data.transaction_num = cmd.transaction_num;
        resp_data.resp_type = resp_data_list.size() > 1 ? kRdmResponseTypeAckOverflow : kRdmResponseTypeAck;
        resp_data.msg_count = 0;
        resp_data.subdevice = 0;
        resp_data.command_class = kRdmCCGetCommandResponse;
        resp_data.param_id = cmd.param_id;

        for (size_t i = 0; i < resp_data_list.size(); ++i)
        {
          memcpy(resp_data.data, resp_data_list[i].data, resp_data_list[i].datalen);
          resp_data.datalen = resp_data_list[i].datalen;

          if (i == resp_data_list.size() - 1)
          {
            resp_data.resp_type = kRdmResponseTypeAck;
          }
          resp_list.push_back(resp_data);
        }
        res = true;
      }
      break;
    }
    default:
      break;
  }
  return res;
}

void Fakeway::SendRptStatus(const RdmnetRemoteRdmCommand& received_cmd, rpt_status_code_t status_code)
{
  RdmnetLocalRptStatus status;
  rdmnet_create_status_from_command(&received_cmd, status_code, &status);

  if (!rdmnet_->SendStatus(status))
    log_.Error("Error sending RPT Status message to Broker.");
}

void Fakeway::SendRptNack(const RdmnetRemoteRdmCommand& received_cmd, uint16_t nack_reason)
{
  RdmResponse resp_data;
  RDM_CREATE_NACK_FROM_COMMAND(&resp_data, &received_cmd.rdm, nack_reason);

  std::vector<RdmResponse> resp_list(1, resp_data);
  SendRptResponse(received_cmd, resp_list);
}

void Fakeway::SendRptResponse(const RdmnetRemoteRdmCommand& received_cmd, std::vector<RdmResponse>& resp_list)
{
  RdmnetLocalRdmResponse resp_to_send;
  rdmnet_create_response_from_command(&received_cmd, resp_list.data(), resp_list.size(), &resp_to_send);

  if (!rdmnet_->SendRdmResponse(resp_to_send))
    log_.Error("Error sending RPT Notification message to Broker.");
}

void Fakeway::SendUnsolicitedRptResponse(uint16_t from_endpoint, std::vector<RdmResponse>& resp_list)
{
  RdmnetLocalRdmResponse resp_to_send;
  rdmnet_create_unsolicited_response(from_endpoint, resp_list.data(), resp_list.size(), &resp_to_send);

  if (!rdmnet_->SendRdmResponse(resp_to_send))
    log_.Error("Error sending RPT Notification message to Broker.");
}

void Fakeway::SendLlrpNack(const LlrpRemoteRdmCommand& received_cmd, uint16_t nack_reason)
{
  RdmResponse resp_data;
  RDM_CREATE_NACK_FROM_COMMAND(&resp_data, &received_cmd.rdm, nack_reason);
  SendLlrpResponse(received_cmd, resp_data);
}

void Fakeway::SendLlrpResponse(const LlrpRemoteRdmCommand& received_cmd, const RdmResponse& resp)
{
  LlrpLocalRdmResponse resp_to_send;
  rdmnet_create_llrp_response_from_command(&received_cmd, &resp, &resp_to_send);

  if (!rdmnet_->SendLlrpResponse(resp_to_send))
    log_.Error("Error sending RPT Notification message to Broker.");
}

/**************************************************************************************************/

void Fakeway::HandleGadgetConnected(unsigned int gadget_id, unsigned int num_ports)
{
  {  // Write lock scope
    etcpal::WriteGuard endpoint_write(endpoint_lock_);

    std::vector<uint16_t> new_endpoints;
    new_endpoints.reserve(num_ports);
    for (uint8_t i = 0; i < num_ports; ++i)
    {
      uint16_t this_endpoint_id = next_endpoint_id_++;
      physical_endpoints_.insert(
          std::make_pair(this_endpoint_id, std::make_unique<PhysicalEndpoint>(this_endpoint_id, gadget_id, i + 1)));
      new_endpoints.push_back(this_endpoint_id);
    }
    physical_endpoint_rev_lookup_.insert(std::make_pair(gadget_id, new_endpoints));
    def_resp_->AddEndpoints(new_endpoints);
  }

  if (connected())
  {
    FakewayDefaultResponder::ParamDataList param_data;
    uint16_t nack_reason;
    if (def_resp_->Get(E137_7_ENDPOINT_LIST_CHANGE, nullptr, 0, param_data, nack_reason) && param_data.size() == 1)
    {
      RdmResponse resp_data;
      // src_uid gets filled in by the library
      resp_data.dest_uid = kRdmnetControllerBroadcastUid;
      resp_data.transaction_num = 0;
      resp_data.resp_type = kRdmResponseTypeAck;
      resp_data.msg_count = 0;
      resp_data.subdevice = 0;
      resp_data.command_class = kRdmCCGetCommandResponse;
      resp_data.param_id = E137_7_ENDPOINT_LIST_CHANGE;
      memcpy(resp_data.data, param_data[0].data, param_data[0].datalen);
      resp_data.datalen = param_data[0].datalen;

      std::vector<RdmResponse> resp_list(1, resp_data);
      SendUnsolicitedRptResponse(E133_NULL_ENDPOINT, resp_list);
      log_.Info("Local RDM Device connected. Sending ENDPOINT_LIST_CHANGE to all Controllers...");
    }
  }
}

void Fakeway::HandleGadgetDisconnected(unsigned int gadget_id)
{
  {  // Write lock scope
    etcpal::WriteGuard endpoint_write(endpoint_lock_);

    auto endpoints = physical_endpoint_rev_lookup_.find(gadget_id);
    if (endpoints != physical_endpoint_rev_lookup_.end())
    {
      for (auto endpoint : endpoints->second)
        physical_endpoints_.erase(endpoint);
      def_resp_->RemoveEndpoints(endpoints->second);
      physical_endpoint_rev_lookup_.erase(endpoints);
    }
  }

  if (connected())
  {
    FakewayDefaultResponder::ParamDataList param_data;
    uint16_t nack_reason;
    if (def_resp_->Get(E137_7_ENDPOINT_LIST_CHANGE, nullptr, 0, param_data, nack_reason) && param_data.size() == 1)
    {
      RdmResponse resp_data;
      // source_uid gets filled in by the library
      resp_data.dest_uid = kRdmnetControllerBroadcastUid;
      resp_data.transaction_num = 0;
      resp_data.resp_type = kRdmResponseTypeAck;
      resp_data.msg_count = 0;
      resp_data.subdevice = 0;
      resp_data.command_class = kRdmCCGetCommandResponse;
      resp_data.param_id = E137_7_ENDPOINT_LIST_CHANGE;
      memcpy(resp_data.data, param_data[0].data, param_data[0].datalen);
      resp_data.datalen = param_data[0].datalen;

      std::vector<RdmResponse> resp_list(1, resp_data);
      SendUnsolicitedRptResponse(E133_NULL_ENDPOINT, resp_list);
      log_.Info("Local RDM Device removed. Sending ENDPOINT_LIST_CHANGE to all Controllers...");
    }
  }
}

void Fakeway::HandleNewRdmResponderDiscovered(unsigned int gadget_id, unsigned int port_number,
                                              const RdmDeviceInfo& info)
{
  etcpal::ReadGuard endpoint_read(endpoint_lock_);

  auto dev_pair = physical_endpoint_rev_lookup_.find(gadget_id);
  if (dev_pair != physical_endpoint_rev_lookup_.end() && port_number <= dev_pair->second.size())
  {
    auto endpt_pair = physical_endpoints_.find(dev_pair->second[port_number - 1]);
    if (endpt_pair != physical_endpoints_.end())
    {
      PhysicalEndpoint* endpt = endpt_pair->second.get();
      RdmUid responder{info.manufacturer_id, info.device_id};
      if (endpt->AddResponder(responder))
      {
        def_resp_->AddResponderOnEndpoint(endpt_pair->first, responder);

        if (connected())
        {
          FakewayDefaultResponder::ParamDataList param_data;
          uint16_t nack_reason;
          std::array<uint8_t, 2> endpt_buf;
          etcpal_pack_u16b(endpt_buf.data(), endpt_pair->first);
          if (def_resp_->Get(E137_7_ENDPOINT_RESPONDER_LIST_CHANGE, endpt_buf.data(), 2, param_data, nack_reason) &&
              param_data.size() == 1)
          {
            RdmResponse resp_data;
            // source_uid gets filled in by the library
            resp_data.dest_uid = kRdmBroadcastUid;
            resp_data.transaction_num = 0;
            resp_data.resp_type = kRdmResponseTypeAck;
            resp_data.msg_count = 0;
            resp_data.subdevice = 0;
            resp_data.command_class = kRdmCCGetCommandResponse;
            resp_data.param_id = E137_7_ENDPOINT_RESPONDER_LIST_CHANGE;
            memcpy(resp_data.data, param_data[0].data, param_data[0].datalen);
            resp_data.datalen = param_data[0].datalen;

            std::vector<RdmResponse> resp_list(1, resp_data);
            SendUnsolicitedRptResponse(E133_NULL_ENDPOINT, resp_list);
            log_.Info("RDM Responder discovered. Sending ENDPOINT_RESPONDER_LIST_CHANGE to all Controllers...");
          }
        }
      }
    }
  }
}

void Fakeway::HandleRdmResponse(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& cmd,
                                const void* cookie)
{
  etcpal::ReadGuard endpoint_read(endpoint_lock_);

  auto dev_pair = physical_endpoint_rev_lookup_.find(gadget_id);
  if (dev_pair != physical_endpoint_rev_lookup_.end() && port_number <= dev_pair->second.size())
  {
    auto endpt_pair = physical_endpoints_.find(dev_pair->second[port_number - 1]);
    if (endpt_pair != physical_endpoints_.end())
    {
      const RdmnetRemoteRdmCommand* received_cmd = static_cast<const RdmnetRemoteRdmCommand*>(cookie);
      RdmUid resp_src_uid{cmd.getManufacturerId(), cmd.getDeviceId()};

      RdmResponse resp_data;
      resp_data.source_uid = resp_src_uid;
      resp_data.dest_uid = received_cmd ? received_cmd->source_uid : kRdmBroadcastUid;
      resp_data.transaction_num = cmd.getTransactionNum();
      resp_data.resp_type = static_cast<rdm_response_type_t>(cmd.getResponseType());
      resp_data.msg_count = 0;
      resp_data.subdevice = cmd.getSubdevice();
      resp_data.command_class = static_cast<rdm_command_class_t>(cmd.getCommand());
      resp_data.param_id = cmd.getParameter();
      resp_data.datalen = cmd.getLength();
      memcpy(resp_data.data, cmd.getBuffer(), resp_data.datalen);

      std::vector<RdmResponse> resp_list(1, resp_data);
      if (!received_cmd)
        SendUnsolicitedRptResponse(endpt_pair->first, resp_list);
      else
        SendRptResponse(*received_cmd, resp_list);

      if (received_cmd)
      {
        endpt_pair->second->GotResponse(resp_src_uid, received_cmd);
      }
    }
  }
}

void Fakeway::HandleRdmTimeout(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& orig_cmd,
                               const void* cookie)
{
  const RdmnetRemoteRdmCommand* received_cmd = static_cast<const RdmnetRemoteRdmCommand*>(cookie);
  if (received_cmd)
  {
    etcpal::ReadGuard endpoint_read(endpoint_lock_);

    auto dev_pair = physical_endpoint_rev_lookup_.find(gadget_id);
    if (dev_pair != physical_endpoint_rev_lookup_.end() && port_number <= dev_pair->second.size())
    {
      auto endpt_pair = physical_endpoints_.find(dev_pair->second[port_number - 1]);
      if (endpt_pair != physical_endpoints_.end())
      {
        SendRptStatus(*received_cmd, kRptStatusRdmTimeout);
        RdmUid resp_src_uid{orig_cmd.getManufacturerId(), orig_cmd.getDeviceId()};
        endpt_pair->second->GotResponse(resp_src_uid, received_cmd);
      }
    }
  }
}

void Fakeway::HandleRdmResponderLost(unsigned int gadget_id, unsigned int port_number, uid id)
{
  etcpal::ReadGuard endpoint_read(endpoint_lock_);

  auto dev_pair = physical_endpoint_rev_lookup_.find(gadget_id);
  if (dev_pair != physical_endpoint_rev_lookup_.end() && port_number <= dev_pair->second.size())
  {
    auto endpt_pair = physical_endpoints_.find(dev_pair->second[port_number - 1]);
    if (endpt_pair != physical_endpoints_.end())
    {
      PhysicalEndpoint* endpt = endpt_pair->second.get();
      std::vector<std::unique_ptr<RemoteRdmCommand>> orphaned_msgs;
      RdmUid uid_lost{id.manu, id.id};
      if (endpt->RemoveResponder(uid_lost, orphaned_msgs) && connected())
      {
        // Send RDM timeout responses for each queued command for this device.
        for (const auto& msg : orphaned_msgs)
        {
          if (msg)
            SendRptStatus(*msg, kRptStatusRdmTimeout);
        }
        orphaned_msgs.clear();

        def_resp_->RemoveResponderOnEndpoint(endpt_pair->first, uid_lost);

        FakewayDefaultResponder::ParamDataList param_data;
        uint16_t nack_reason;
        std::array<uint8_t, 2> endpt_buf;
        etcpal_pack_u16b(endpt_buf.data(), endpt_pair->first);
        if (def_resp_->Get(E137_7_ENDPOINT_RESPONDER_LIST_CHANGE, endpt_buf.data(), 2, param_data, nack_reason) &&
            param_data.size() == 1)
        {
          // Now send the responder list change message.
          RdmResponse resp_data;
          // src_uid gets filled in by the library
          resp_data.dest_uid = kRdmBroadcastUid;
          resp_data.transaction_num = 0;
          resp_data.resp_type = kRdmResponseTypeAck;
          resp_data.msg_count = 0;
          resp_data.subdevice = 0;
          resp_data.command_class = kRdmCCGetCommandResponse;
          resp_data.param_id = E137_7_ENDPOINT_RESPONDER_LIST_CHANGE;
          memcpy(resp_data.data, param_data[0].data, param_data[0].datalen);
          resp_data.datalen = param_data[0].datalen;

          std::vector<RdmResponse> resp_list(1, resp_data);
          SendUnsolicitedRptResponse(E133_NULL_ENDPOINT, resp_list);
          log_.Info("RDM Responder lost. Sending ENDPOINT_RESPONDER_LIST_CHANGE to all Controllers...");
        }
      }
    }
  }
}

void Fakeway::HandleGadgetLogMsg(const char* str)
{
  log_.Info(str);
}
