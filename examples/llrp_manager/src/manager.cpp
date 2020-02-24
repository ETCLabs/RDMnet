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

#include "manager.h"

#include <cstring>
#include <cstdio>
#include <memory>
#include <stdexcept>

#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/thread.h"
#include "etcpal/cpp/timer.h"
#include "etcpal/netint.h"
#include "etcpal/pack.h"
#include "etcpal/socket.h"
#include "rdmnet/version.h"

bool LlrpManagerExample::Startup(const etcpal::Uuid& my_cid, const etcpal::Logger& logger)
{
  printf("ETC Example LLRP Manager version %s initializing...\n", RDMNET_VERSION_STRING);

  auto init_result = rdmnet::Init(logger);
  if (!init_result)
  {
    printf("Failed to initialize the RDMnet library: '%s'\n", init_result.ToCString());
    return false;
  }

  size_t num_interfaces = etcpal_netint_get_num_interfaces();
  if (num_interfaces > 0)
  {
    const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();
    for (const EtcPalNetintInfo* netint = netint_list; netint < netint_list + num_interfaces; ++netint)
    {
      llrp::Manager manager;
      auto res = manager.Startup(*this, 0x6574, netint->index, netint->addr.type, my_cid);
      if (res)
      {
        managers_.insert(std::make_pair(manager.handle(), ManagerInfo{std::move(manager), *netint}));
      }
      else
      {
        printf("Warning: couldn't create LLRP Manager on network interface %s (error: '%s').\n",
               etcpal::IpAddr(netint->addr).ToString().c_str(), res.ToCString());
      }
    }

    if (!managers_.empty())
      return true;
  }

  printf("Error: Couldn't set up any network interfaces for LLRP Manager functionality.\n");
  return false;
}

void LlrpManagerExample::Shutdown()
{
  for (auto& manager : managers_)
  {
    manager.second.manager.Shutdown();
  }
  managers_.clear();
  rdmnet::Deinit();
}

LlrpManagerExample::ParseResult LlrpManagerExample::ParseCommandLineArgs(const std::vector<std::string>& args)
{
  auto iter = args.begin();
  if (iter == args.end())
    return ParseResult::kParseErr;  // Nothing in arg list, error
  if (++iter == args.end())
    return ParseResult::kRun;  // No arguments, run the app

  // We have an argument, these are the only valid ones right now.
  if (*iter == "--version" || *iter == "-v")
  {
    return ParseResult::kPrintVersion;
  }
  else if (*iter == "--help" || *iter == "-?" || *iter == "-h")
  {
    return ParseResult::kPrintHelp;
  }
  else
  {
    return ParseResult::kParseErr;
  }
}

void LlrpManagerExample::PrintUsage(const std::string& app_name)
{
  printf("Usage: %s [OPTION]...\n", app_name.c_str());
  printf("With no options, the app will start normally and wait for user input.\n");
  printf("\n");
  printf("Options:\n");
  printf("  --help     Display this help and exit.\n");
  printf("  --version  Output version information and exit.\n");
}

void LlrpManagerExample::PrintVersion()
{
  printf("ETC Example LLRP Manager\n");
  printf("Version %s\n\n", RDMNET_VERSION_STRING);
  printf("%s\n", RDMNET_VERSION_COPYRIGHT);
  printf("License: Apache License v2.0 <http://www.apache.org/licenses/LICENSE-2.0>\n");
  printf("Unless required by applicable law or agreed to in writing, this software is\n");
  printf("provided \"AS IS\", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express\n");
  printf("or implied.\n");
}

void LlrpManagerExample::PrintCommandList()
{
  printf("LLRP Manager Commands:\n");
  printf("    ?: Print commands\n");
  printf("    d <netint_handle>: Perform LLRP discovery on network interface indicated by\n");
  printf("        netint_handle\n");
  printf("    pt: Print discovered LLRP Targets\n");
  printf("    pi: Print network interfaces\n");
  printf("    i <target_handle>: Get DEVICE_INFO from Target <target_handle>\n");
  printf("    l <target_handle>: Get DEVICE_LABEL from Target <target_handle>\n");
  printf("    si <target_handle>: Toggle IDENTIFY_DEVICE on/off on Target <target_handle>\n");
  printf("    sl <target_handle> <label>: Set DEVICE_LABEL to <label> on Target\n");
  printf("        <target_handle>\n");
  printf("    m <target_handle>: Get MANUFACTURER_LABEL from Target <target_handle>\n");
  printf("    c <target_handle>: Get DEVICE_MODEL_DESCRIPTION from Target <target_handle>\n");
  printf("    s <target_handle> <scope_slot>: Get COMPONENT_SCOPE for Scope Slot\n");
  printf("        <scope_slot> from Target <target_handle>\n");
  printf("    ss <target_handle> <scope_slot> <scope> [ip:port]: Set COMPONENT_SCOPE to\n");
  printf("        <scope> for Scope Slot <scope_slot> on Target <target_handle> with\n");
  printf("        optional static Broker address ip:port\n");
  printf("    q: Quit\n");
}

bool LlrpManagerExample::ParseCommand(const std::string& line)
{
  bool res = true;

  if (!line.empty())
  {
    switch (*(line.begin()))
    {
      case 'd':
        try
        {
          llrp::ManagerHandle manager_handle = std::stoi(line.substr(2));
          Discover(manager_handle);
        }
        catch (std::exception)
        {
          printf("Command syntax: d <netint_handle>\n");
        }
        break;
      case 'p':
        if (line.length() >= 2)
        {
          if (line[1] == 't')
            PrintTargets();
          else if (line[1] == 'i')
            PrintNetints();
          else
            printf("Unrecognized command.\n");
        }
        else
        {
          printf("Unrecognized command.\n");
        }
        break;
      case 'i':
        try
        {
          int target_handle = std::stoi(line.substr(2));
          GetDeviceInfo(target_handle);
        }
        catch (std::exception)
        {
          printf("Command syntax: i <target_handle>\n");
        }
        break;
      case 'l':
        try
        {
          int target_handle = std::stoi(line.substr(2));
          GetDeviceLabel(target_handle);
        }
        catch (std::exception)
        {
          printf("Command syntax: l <target_handle>\n");
        }
        break;
      case 'm':
        try
        {
          int target_handle = std::stoi(line.substr(2));
          GetManufacturerLabel(target_handle);
        }
        catch (std::exception)
        {
          printf("Command syntax: m <target_handle>\n");
        }
        break;
      case 'c':
        try
        {
          int target_handle = std::stoi(line.substr(2));
          GetDeviceModelDescription(target_handle);
        }
        catch (std::exception)
        {
          printf("Command syntax: c <target_handle>\n");
        }
        break;
      case 's':
        if (line.length() >= 2)
        {
          switch (line[1])
          {
            case 's':
              try
              {
                std::string args = line.substr(3);
                size_t first_sp_pos = args.find_first_of(' ');
                size_t second_sp_pos = args.find_first_of(' ', first_sp_pos + 1);
                size_t third_sp_pos = args.find_first_of(' ', second_sp_pos + 1);

                int target_handle = std::stoi(args);
                int scope_slot = std::stoi(args.substr(first_sp_pos + 1, second_sp_pos - (first_sp_pos + 1)));

                etcpal::SockAddr static_config;
                if (third_sp_pos != std::string::npos)
                {
                  std::string ip_port = args.substr(third_sp_pos + 1);
                  size_t colon_pos = ip_port.find_first_of(':');
                  if (colon_pos != std::string::npos)
                  {
                    std::string ip_str = ip_port.substr(0, colon_pos);
                    static_config.SetAddress(etcpal::IpAddr::FromString(ip_str));
                    if (static_config.IsValid())
                    {
                      static_config.SetPort(static_cast<uint16_t>(std::stoi(ip_port.substr(colon_pos + 1))));
                    }
                    else
                    {
                      throw std::invalid_argument("Invalid static IP address.");
                    }
                  }
                  else
                  {
                    throw std::invalid_argument("Invalid static IP/port combo.");
                  }
                }

                // Get and convert the scope
                std::string scope = args.substr(
                    second_sp_pos + 1,
                    (third_sp_pos == std::string::npos ? third_sp_pos : third_sp_pos - (second_sp_pos + 1)));
                SetComponentScope(target_handle, scope_slot, scope, static_config);
              }
              catch (const std::exception& e)
              {
                printf("Error occurred while parsing arguments: %s\n", e.what());
                printf("Command syntax: ss <target_handle> <scope_slot> <scope> [ip:port]\n");
              }
              break;
            case 'i':
              try
              {
                int target_handle = std::stoi(line.substr(3));
                IdentifyDevice(target_handle);
              }
              catch (std::exception)
              {
                printf("Command syntax: sl <target_handle> <label>\n");
              }
              break;
            case 'l':
              try
              {
                std::string args = line.substr(3);
                size_t sp_pos = args.find_first_of(' ');

                int target_handle = std::stoi(args);
                std::string label = args.substr(sp_pos + 1);
                SetDeviceLabel(target_handle, label.c_str());
              }
              catch (std::exception)
              {
                printf("Command syntax: sl <target_handle> <label>\n");
              }
              break;
            case ' ':
              try
              {
                std::string args = line.substr(2);
                size_t sp_pos = args.find_first_of(' ');
                int target_handle = std::stoi(args);
                int scope_slot = std::stoi(args.substr(sp_pos));
                GetComponentScope(target_handle, scope_slot);
              }
              catch (std::exception)
              {
                printf("Command syntax: s <target_handle> <scope_slot>\n");
              }
              break;
            default:
              printf("Unrecognized command\n");
              break;
          }
        }
        else
        {
          printf("Command syntax: s <target_handle> <scope_slot>\n");
        }
        break;
      case 'q':
        res = false;
        break;
      case '?':
        PrintCommandList();
        break;
      default:
        printf("Unrecognized command.\n");
        break;
    }
  }
  return res;
}

void LlrpManagerExample::Discover(llrp_manager_t manager_handle)
{
  auto mgr_pair = managers_.find(manager_handle);
  if (mgr_pair == managers_.end())
  {
    printf("Network interface handle not found.\n");
    return;
  }

  targets_.clear();

  active_manager_ = manager_handle;
  discovery_active_ = true;

  printf("Starting LLRP discovery...\n");
  auto res = mgr_pair->second.manager.StartDiscovery();
  if (res)
  {
    while (discovery_active_)
    {
      etcpal::Thread::Sleep(100);
    }
    printf("LLRP Discovery finished.\n");
  }
  else
  {
    printf("Error starting LLRP Discovery: '%s'\n", res.ToCString());
  }
}

void LlrpManagerExample::PrintTargets()
{
  printf("Handle %-13s %-36s %-15s %s\n", "UID", "CID", "Type", "Hardware ID");
  for (const auto& target : targets_)
  {
    printf("%-6d %04x:%08x %s %-15s %s\n", target.first, target.second.prot_info.uid.manu,
           target.second.prot_info.uid.id, etcpal::Uuid(target.second.prot_info.cid).ToString().c_str(),
           llrp_component_type_to_string(target.second.prot_info.component_type),
           etcpal::MacAddr(target.second.prot_info.hardware_address).ToString().c_str());
  }
}

void LlrpManagerExample::PrintNetints()
{
  printf("Handle %-30s %-17s Name\n", "Address", "MAC");
  for (const auto& sock_pair : managers_)
  {
    const EtcPalNetintInfo& info = sock_pair.second.netint_info;
    printf("%-6d %-30s %s %s\n", sock_pair.first, etcpal::IpAddr(info.addr).ToString().c_str(),
           etcpal::MacAddr(info.mac).ToString().c_str(), info.friendly_name);
  }
}

void LlrpManagerExample::GetDeviceInfo(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      auto response_data = GetDataFromTarget(mgr_pair->second.manager, target->second.prot_info, E120_DEVICE_INFO);
      if (response_data.size() == 19)
      {
        const uint8_t* cur_ptr = response_data.data();
        printf("Device info:\n");
        printf("  RDM Protocol Version: %d.%d\n", cur_ptr[0], cur_ptr[1]);
        cur_ptr += 2;
        printf("  Device Model ID: %d (0x%04x)\n", etcpal_unpack_u16b(cur_ptr), etcpal_unpack_u16b(cur_ptr));
        cur_ptr += 2;
        printf("  Product Category:\n");
        printf("    Coarse: %d (0x%02x)\n", *cur_ptr, *cur_ptr);
        ++cur_ptr;
        printf("    Fine: %d (0x%02x)\n", *cur_ptr, *cur_ptr);
        ++cur_ptr;
        printf("  Software Version ID: %d (0x%08x)\n", etcpal_unpack_u32b(cur_ptr), etcpal_unpack_u32b(cur_ptr));
        cur_ptr += 4;
        printf("  DMX512 Footprint: %d\n", etcpal_unpack_u16b(cur_ptr));
        cur_ptr += 2;
        printf("  DMX512 Personality:\n");
        printf("    Current: %d\n", *cur_ptr++);
        printf("    Total: %d\n", *cur_ptr++);
        uint16_t dmx_start_addr = etcpal_unpack_u16b(cur_ptr);
        if (dmx_start_addr == 0xffff)
          printf("  DMX512 Start Address: N/A\n");
        else
          printf("  DMX512 Start Address: %d\n", dmx_start_addr);
        cur_ptr += 2;
        printf("  Subdevice Count: %d\n", etcpal_unpack_u16b(cur_ptr));
        cur_ptr += 2;
        printf("  Sensor Count: %d\n", *cur_ptr);
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending DEVICE_INFO command.\n");
  }
}

void LlrpManagerExample::GetDeviceLabel(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      auto response_data = GetDataFromTarget(mgr_pair->second.manager, target->second.prot_info, E120_DEVICE_LABEL);
      if (!response_data.empty())
      {
        std::string dev_label(reinterpret_cast<char*>(response_data.data()), response_data.size());
        printf("Device label: %s\n", dev_label.c_str());
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending DEVICE_LABEL command.\n");
  }
}

void LlrpManagerExample::GetManufacturerLabel(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      auto response_data =
          GetDataFromTarget(mgr_pair->second.manager, target->second.prot_info, E120_MANUFACTURER_LABEL);
      if (!response_data.empty())
      {
        std::string manu_label(reinterpret_cast<char*>(response_data.data()), response_data.size());
        printf("Manufacturer label: %s\n", manu_label.c_str());
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending MANUFACTURER_LABEL command.\n");
  }
}

void LlrpManagerExample::GetDeviceModelDescription(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      auto response_data =
          GetDataFromTarget(mgr_pair->second.manager, target->second.prot_info, E120_DEVICE_MODEL_DESCRIPTION);
      if (!response_data.empty())
      {
        std::string dev_model_desc(reinterpret_cast<char*>(response_data.data()), response_data.size());
        printf("Device model description: %s\n", dev_model_desc.c_str());
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending DEVICE_MODEL_DESCRIPTION command.\n");
  }
}

void LlrpManagerExample::GetComponentScope(int target_handle, int scope_slot)
{
  if (scope_slot < 1 || scope_slot > 65535)
  {
    printf("Invalid scope slot.\n");
    return;
  }

  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      uint8_t scope_slot_buf[2];
      etcpal_pack_u16b(scope_slot_buf, static_cast<uint16_t>(scope_slot));

      auto response_data = GetDataFromTarget(mgr_pair->second.manager, target->second.prot_info, E133_COMPONENT_SCOPE,
                                             scope_slot_buf, 2);

      if (response_data.size() >= (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2))
      {
        const uint8_t* cur_ptr = response_data.data();

        uint16_t slot = etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;

        char scope_string[E133_SCOPE_STRING_PADDED_LENGTH] = {};
        memcpy(scope_string, cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH - 1);
        cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

        uint8_t static_config_type = *cur_ptr++;
        etcpal::SockAddr sockaddr;

        printf("Scope for slot %d: %s\n", slot, scope_string);
        switch (static_config_type)
        {
          case E133_STATIC_CONFIG_IPV4:
            sockaddr.SetAddress(etcpal_unpack_u32b(cur_ptr));
            cur_ptr += 4 + 16;
            sockaddr.SetPort(etcpal_unpack_u16b(cur_ptr));
            printf("Static Broker IPv4 for slot %d: %s\n", slot, sockaddr.ToString().c_str());
            break;
          case E133_STATIC_CONFIG_IPV6:
            cur_ptr += 4;
            sockaddr.SetAddress(cur_ptr);
            cur_ptr += 16;
            sockaddr.SetPort(etcpal_unpack_u16b(cur_ptr));
            printf("Static Broker IPv6 for slot %d: %s\n", slot, sockaddr.ToString().c_str());
            break;
          case E133_NO_STATIC_CONFIG:
          default:
            printf("No static Broker config.\n");
            break;
        }
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending COMPONENT_SCOPE command.\n");
  }
}

void LlrpManagerExample::IdentifyDevice(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      uint8_t identifying = (target->second.identifying ? 0 : 1);
      if (SetDataOnTarget(mgr_pair->second.manager, target->second.prot_info, E120_IDENTIFY_DEVICE, &identifying, 1))
      {
        target->second.identifying = !target->second.identifying;
        printf("Target is %sidentifying\n", target->second.identifying ? "" : "not ");
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
}

void LlrpManagerExample::SetDeviceLabel(int target_handle, const std::string& label)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      uint8_t label_truncated_size = (label.size() > 32 ? 32 : label.size());
      if (SetDataOnTarget(mgr_pair->second.manager, target->second.prot_info, E120_DEVICE_LABEL,
                          reinterpret_cast<const uint8_t*>(label.c_str()), label_truncated_size))
      {
        printf("Set device label successfully.\n");
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending COMPONENT_SCOPE command.\n");
  }
}

#define COMPONENT_SCOPE_PDL (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2)

void LlrpManagerExample::SetComponentScope(int target_handle, int scope_slot, const std::string& scope_utf8,
                                           const etcpal::SockAddr& static_config)
{
  if (scope_slot < 1 || scope_slot > 65535)
  {
    printf("Invalid scope slot.\n");
    return;
  }

  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      uint8_t data[COMPONENT_SCOPE_PDL] = {0};

      uint8_t* cur_ptr = data;
      etcpal_pack_u16b(cur_ptr, static_cast<uint16_t>(scope_slot));
      cur_ptr += 2;
      rdmnet_safe_strncpy((char*)cur_ptr, scope_utf8.c_str(), E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
      if (static_config.IsV4())
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
        etcpal_pack_u32b(cur_ptr, static_config.v4_data());
        cur_ptr += 4 + 16;
        etcpal_pack_u16b(cur_ptr, static_config.port());
      }
      else if (static_config.IsV6())
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
        cur_ptr += 4;
        memcpy(cur_ptr, static_config.v6_data(), 16);
        cur_ptr += 16;
        etcpal_pack_u16b(cur_ptr, static_config.port());
      }
      else
      {
        *cur_ptr = E133_NO_STATIC_CONFIG;
      }

      if (SetDataOnTarget(mgr_pair->second.manager, target->second.prot_info, E133_COMPONENT_SCOPE, data,
                          COMPONENT_SCOPE_PDL))
      {
        printf("Set scope successfully.\n");
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending COMPONENT_SCOPE command.\n");
  }
}

void LlrpManagerExample::HandleLlrpTargetDiscovered(llrp::ManagerHandle handle, const llrp::DiscoveredTarget& target)
{
  if (discovery_active_)
  {
    int next_target_handle = targets_.empty() ? 0 : targets_.rbegin()->first + 1;
    printf("Adding LLRP Target, UID %04x:%08x, with handle %d\n", target.uid.manu, target.uid.id, next_target_handle);

    TargetInfo new_target_info;
    new_target_info.prot_info = target;
    targets_[next_target_handle] = new_target_info;
  }
}

void LlrpManagerExample::HandleLlrpDiscoveryFinished(llrp::ManagerHandle handle)
{
  discovery_active_ = false;
}

void LlrpManagerExample::HandleLlrpRdmResponseReceived(llrp::ManagerHandle handle, const llrp::RdmResponse& resp)
{
  if (handle == active_manager_ && active_response_handler_)
    active_response_handler_(resp);
}

std::vector<uint8_t> LlrpManagerExample::GetDataFromTarget(llrp::Manager& manager, const llrp::DiscoveredTarget& target,
                                                           uint16_t param_id, const uint8_t* data, uint8_t data_len)
{
  std::vector<uint8_t> to_return;

  llrp::SavedRdmResponse response;
  active_response_handler_ = [&](const llrp::RdmResponse& resp) { response = resp.Save(); };

  auto seq_num = manager.SendGetCommand(target.address(), param_id, data, data_len);
  if (seq_num)
  {
    etcpal::Timer resp_timer(LLRP_TIMEOUT_MS);
    while (!resp_timer.IsExpired())
    {
      if (response.IsValid())
        break;
      etcpal::Thread::Sleep(100);
    }

    if (response.IsValid())
    {
      if (response.seq_num() == *seq_num && response.IsGetResponse() && response.param_id() == param_id)
      {
        // We got a response.
        if (response.IsAck())
          to_return.assign(response.data(), response.data() + response.data_len());
        else if (response.IsNack())
          printf("Received RDM NACK with reason '%s'\n", response.NackReason()->ToCString());
        else
          printf("Received LLRP RDM response with illegal response type %d\n", response.response_type());
      }
      else
      {
        printf("Received unexpected RDM response.\n");
      }
    }
    else
    {
      printf("Timed out waiting for RDM response.\n");
    }
  }
  else
  {
    printf("Error sending RDM command: '%s'\n", seq_num.error().ToCString());
  }

  return to_return;
}
