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

bool PhysicalEndpoint::QueueMessageForResponder(const rdm::Uid& uid, std::unique_ptr<rdmnet::SavedRdmCommand> cmd)
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

void PhysicalEndpoint::GotResponse(const rdm::Uid& uid, const rdmnet::SavedRdmCommand* cmd)
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

bool PhysicalEndpoint::AddResponder(const rdm::Uid& uid)
{
  etcpal::MutexGuard responder_guard(responder_lock_);
  auto insert_result = responders_.insert(std::make_pair(uid, std::vector<std::unique_ptr<rdmnet::SavedRdmCommand>>()));
  return insert_result.second;
}

bool PhysicalEndpoint::RemoveResponder(const rdm::Uid& uid,
                                       std::vector<std::unique_ptr<rdmnet::SavedRdmCommand>>& orphaned_msgs)
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

bool Fakeway::Startup(const rdmnet::Scope& scope_config, etcpal::Logger& logger, const etcpal::Uuid& cid)
{
  def_resp_ = std::make_unique<FakewayDefaultResponder>(scope_config, E133_DEFAULT_DOMAIN);

  log_ = &logger;
  log_->Info("Using libGadget version %s", GadgetInterface::DllVersion().c_str());
  if (!gadget_.Startup(*this, logger))
    return false;

  auto my_settings = rdmnet::Device::Settings(cid, rdm::Uid::DynamicUidRequest(0x6574));
  auto res = rdmnet_.Startup(*this, my_settings, scope_config);
  if (!res)
  {
    gadget_.Shutdown();
    log_->Critical("Fatal: couldn't start RDMnet library due to error: '%s'", res.ToCString());
    return false;
  }

  return true;
}

void Fakeway::Shutdown()
{
  configuration_change_ = true;
  rdmnet_.Shutdown();
  gadget_.Shutdown();

  physical_endpoints_.clear();
  physical_endpoint_rev_lookup_.clear();
}

void Fakeway::HandleConnectedToBroker(rdmnet::Device::Handle /*handle*/, const rdmnet::ClientConnectedInfo& info)
{
  connected_to_broker_ = true;
  if (log_->CanLog(ETCPAL_LOG_INFO))
  {
    log_->Info("Connected to broker for scope %s at address %s", def_resp_->scope_config().id_string().c_str(),
               info.broker_addr().ToString().c_str());
  }
}

void Fakeway::HandleBrokerConnectFailed(rdmnet::Device::Handle /*handle*/, const rdmnet::ClientConnectFailedInfo& info)
{
  connected_to_broker_ = false;
  log_->Info("Connect failed to broker for scope %s.%s", def_resp_->scope_config().id_string().c_str(),
             info.will_retry() ? " Retrying..." : "");
}

void Fakeway::HandleDisconnectedFromBroker(rdmnet::Device::Handle /*handle*/,
                                           const rdmnet::ClientDisconnectedInfo& info)
{
  connected_to_broker_ = false;
  log_->Info("Disconnected from broker for scope %s.%s", def_resp_->scope_config().id_string().c_str(),
             info.will_retry() ? " Retrying..." : "");
}

rdmnet::RdmResponseAction Fakeway::HandleRdmCommand(rdmnet::Device::Handle /*handle*/, const rdmnet::RdmCommand& cmd)
{
  if (cmd.IsToDefaultResponder())
  {
    return ProcessDefRespRdmCommand(cmd.rdm_header(), cmd.data(), cmd.data_len());
  }
  else
  {
    etcpal::ReadGuard endpoint_read(endpoint_lock_);
    auto endpoint_pair = physical_endpoints_.find(cmd.dest_endpoint());
    if (endpoint_pair != physical_endpoints_.end())
    {
      PhysicalEndpoint* endpoint = endpoint_pair->second.get();
      auto saved_cmd = std::make_unique<rdmnet::SavedRdmCommand>(cmd.Save());
      assert(saved_cmd);

      auto raw_saved_cmd = saved_cmd.get();
      if (endpoint->QueueMessageForResponder(cmd.rdm_dest_uid(), std::move(saved_cmd)))
      {
        RDM_CmdC to_send(static_cast<uint8_t>(cmd.command_class()), cmd.param_id(), cmd.subdevice(), cmd.data_len(),
                         cmd.data(), cmd.rdm_dest_uid().manufacturer_id(), cmd.rdm_dest_uid().device_id());
        gadget_.SendRdmCommand(endpoint->gadget_id(), endpoint->port_num(), to_send, raw_saved_cmd);
      }
      else
      {
        rdmnet_.SendRptStatus(cmd.Save(), kRptStatusUnknownRdmUid);
      }
    }
    else
    {
      rdmnet_.SendRptStatus(cmd.Save(), kRptStatusUnknownEndpoint);
    }
  }
  return rdmnet::RdmResponseAction::DeferResponse();
}

rdmnet::RdmResponseAction Fakeway::HandleLlrpRdmCommand(rdmnet::Device::Handle /*handle*/,
                                                        const rdmnet::llrp::RdmCommand& cmd)
{
  return ProcessDefRespRdmCommand(cmd.rdm_header(), cmd.data(), cmd.data_len());
}

rdmnet::RdmResponseAction Fakeway::ProcessDefRespRdmCommand(const rdm::CommandHeader& cmd_header, const uint8_t* data,
                                                            uint8_t data_len)
{
  if (!def_resp_->SupportsPid(cmd_header.param_id()))
  {
    log_->Debug("Sending NACK to Controller %04x:%08x for unknown PID 0x%04x",
                cmd_header.source_uid().manufacturer_id(), cmd_header.source_uid().device_id(), cmd_header.param_id());
    return rdmnet::RdmResponseAction::SendNack(kRdmNRUnknownPid);
  }
  switch (cmd_header.command_class())
  {
    case kRdmCCSetCommand:
      return def_resp_->Set(cmd_header.param_id(), data, data_len);
    case kRdmCCGetCommand:
      return def_resp_->Get(cmd_header.param_id(), data, data_len);
    default:
      return rdmnet::RdmResponseAction::SendNack(kRdmNRUnsupportedCommandClass);
  }
}

/**************************************************************************************************/

void Fakeway::HandleGadgetConnected(unsigned int gadget_id, unsigned int num_ports)
{
  etcpal::WriteGuard endpoint_write(endpoint_lock_);

  std::vector<rdmnet::PhysicalEndpointConfig> new_endpoint_configs;
  std::vector<uint16_t> new_endpoints;

  for (uint8_t i = 0; i < num_ports; ++i)
  {
    uint16_t this_endpoint_id = next_endpoint_id_++;
    physical_endpoints_.insert(
        std::make_pair(this_endpoint_id, std::make_unique<PhysicalEndpoint>(this_endpoint_id, gadget_id, i + 1)));
    new_endpoints.push_back(this_endpoint_id);
    new_endpoint_configs.push_back(this_endpoint_id);
  }

  physical_endpoint_rev_lookup_.insert(std::make_pair(gadget_id, new_endpoints));

  rdmnet_.AddPhysicalEndpoints(new_endpoint_configs);
}

void Fakeway::HandleGadgetDisconnected(unsigned int gadget_id)
{
  etcpal::WriteGuard endpoint_write(endpoint_lock_);

  auto endpoints = physical_endpoint_rev_lookup_.find(gadget_id);
  if (endpoints != physical_endpoint_rev_lookup_.end())
  {
    for (auto endpoint : endpoints->second)
      physical_endpoints_.erase(endpoint);
    rdmnet_.RemoveEndpoints(endpoints->second);
    physical_endpoint_rev_lookup_.erase(endpoints);
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
      rdm::Uid responder(info.manufacturer_id, info.device_id);
      if (endpt->AddResponder(responder))
      {
        rdmnet_.AddPhysicalResponder(endpt_pair->first, responder);
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
      const rdmnet::SavedRdmCommand* received_cmd = static_cast<const rdmnet::SavedRdmCommand*>(cookie);
      rdm::Uid resp_src_uid(cmd.getManufacturerId(), cmd.getDeviceId());

      if (!received_cmd && cmd.getCommand() == E120_GET_COMMAND_RESPONSE)
        rdmnet_.SendRdmUpdate(endpt_pair->first, resp_src_uid, cmd.getParameter(),
                              reinterpret_cast<const uint8_t*>(cmd.getBuffer()), cmd.getLength());
      else if (cmd.getResponseType() == E120_RESPONSE_TYPE_ACK)
        rdmnet_.SendRdmAck(*received_cmd, reinterpret_cast<const uint8_t*>(cmd.getBuffer()), cmd.getLength());
      else if (cmd.getResponseType() == E120_RESPONSE_TYPE_NACK_REASON)
        rdmnet_.SendRdmNack(*received_cmd, etcpal_unpack_u16b(reinterpret_cast<const uint8_t*>(cmd.getBuffer())));
      else
        rdmnet_.SendRptStatus(*received_cmd, kRptStatusInvalidRdmResponse);

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
  const rdmnet::SavedRdmCommand* received_cmd = static_cast<const rdmnet::SavedRdmCommand*>(cookie);
  if (received_cmd)
  {
    etcpal::ReadGuard endpoint_read(endpoint_lock_);

    auto dev_pair = physical_endpoint_rev_lookup_.find(gadget_id);
    if (dev_pair != physical_endpoint_rev_lookup_.end() && port_number <= dev_pair->second.size())
    {
      auto endpt_pair = physical_endpoints_.find(dev_pair->second[port_number - 1]);
      if (endpt_pair != physical_endpoints_.end())
      {
        rdmnet_.SendRptStatus(*received_cmd, kRptStatusRdmTimeout);
        rdm::Uid resp_src_uid(orig_cmd.getManufacturerId(), orig_cmd.getDeviceId());
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
      std::vector<std::unique_ptr<rdmnet::SavedRdmCommand>> orphaned_msgs;
      rdm::Uid uid_lost(id.manu, id.id);
      if (endpt->RemoveResponder(uid_lost, orphaned_msgs))
      {
        // Send RDM timeout responses for each queued command for this device.
        for (const auto& msg : orphaned_msgs)
        {
          if (msg)
            rdmnet_.SendRptStatus(*msg, kRptStatusRdmTimeout);
        }
        orphaned_msgs.clear();
        rdmnet_.RemovePhysicalResponder(endpt_pair->first, uid_lost);
      }
    }
  }
}

void Fakeway::HandleGadgetLogMsg(const char* str)
{
  log_->Info(str);
}
