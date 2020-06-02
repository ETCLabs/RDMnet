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
#include "etcpal/netint.h"
#include "etcpal/pack.h"
#include "etcpal/socket.h"
#include "etcpal/thread.h"
#include "etcpal/timer.h"
#include "rdmnet/core.h"
#include "rdmnet/core/util.h"
#include "rdmnet/version.h"
#include "rdm/defs.h"

extern "C" {
void llrpcb_target_discovered(llrp_manager_t /*handle*/, const DiscoveredLlrpTarget* target, void* context)
{
  LLRPManager* mgr = static_cast<LLRPManager*>(context);
  if (mgr && target)
    mgr->TargetDiscovered(*target);
}

void llrpcb_discovery_finished(llrp_manager_t /*handle*/, void* context)
{
  LLRPManager* mgr = static_cast<LLRPManager*>(context);
  if (mgr)
    mgr->DiscoveryFinished();
}

void llrpcb_rdm_resp_received(llrp_manager_t /*handle*/, const LlrpRemoteRdmResponse* resp, void* context)
{
  LLRPManager* mgr = static_cast<LLRPManager*>(context);
  if (mgr && resp)
    mgr->RdmRespReceived(*resp);
}
}

bool LLRPManager::Startup(const etcpal::Uuid& my_cid, const EtcPalLogParams* log_params)
{
  printf("ETC Example LLRP Manager version %s initializing...\n", RDMNET_VERSION_STRING);

  cid_ = my_cid;
  rdmnet_core_init(log_params, nullptr);

  size_t num_interfaces = etcpal_netint_get_num_interfaces();
  if (num_interfaces > 0)
  {
    LlrpManagerConfig config;
    config.cid = cid_.get();
    config.manu_id = 0x6574;
    config.callbacks.target_discovered = llrpcb_target_discovered;
    config.callbacks.discovery_finished = llrpcb_discovery_finished;
    config.callbacks.rdm_resp_received = llrpcb_rdm_resp_received;
    config.callback_context = this;

    const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();
    for (const EtcPalNetintInfo* netint = netint_list; netint < netint_list + num_interfaces; ++netint)
    {
      config.netint.ip_type = netint->addr.type;
      config.netint.index = netint->index;
      llrp_manager_t handle;
      etcpal::Error res = rdmnet_llrp_manager_create(&config, &handle);
      if (res)
      {
        managers_.insert(std::make_pair(handle, *netint));
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

void LLRPManager::Shutdown()
{
  for (const auto& netint : managers_)
  {
    rdmnet_llrp_manager_destroy(netint.first);
  }
  rdmnet_core_deinit();
}

LLRPManager::ParseResult LLRPManager::ParseCommandLineArgs(const std::vector<std::string>& args)
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

void LLRPManager::PrintUsage(const std::string& app_name)
{
  printf("Usage: %s [OPTION]...\n", app_name.c_str());
  printf("With no options, the app will start normally and wait for user input.\n");
  printf("\n");
  printf("Options:\n");
  printf("  --help     Display this help and exit.\n");
  printf("  --version  Output version information and exit.\n");
}

void LLRPManager::PrintVersion()
{
  printf("ETC Example LLRP Manager\n");
  printf("Version %s\n\n", RDMNET_VERSION_STRING);
  printf("%s\n", RDMNET_VERSION_COPYRIGHT);
  printf("License: Apache License v2.0 <http://www.apache.org/licenses/LICENSE-2.0>\n");
  printf("Unless required by applicable law or agreed to in writing, this software is\n");
  printf("provided \"AS IS\", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express\n");
  printf("or implied.\n");
}

void LLRPManager::PrintCommandList()
{
  printf("LLRP Manager Commands:\n");
  printf("    ?: Print commands\n");
  printf("    d <netint_handle>: Perform LLRP discovery on network interface indicated by\n");
  printf("        netint_handle\n");
  printf("    pt: Print discovered LLRP Targets\n");
  printf("    pi: Print network interfaces\n");
  printf("    i <target_handle>: Get DEVICE_INFO from Target <target_handle>\n");
  printf("    l <target_handle>: Get DEVICE_LABEL from Target <target_handle>\n");
  printf("    gi <target_handle>: Get LIST_INTERFACES from Target <target_handle>\n");
  printf("    sf <target_handle>: Set FACTORY_DEFAULTS on Target <target_handle>\n");
  printf("    si <target_handle>: Toggle IDENTIFY_DEVICE on/off on Target <target_handle>\n");
  printf("    sl <target_handle> <label>: Set DEVICE_LABEL to <label> on Target\n");
  printf("        <target_handle>\n");
  printf("    sr <target_handle>: Set RESET_DEVICE on Target <target_handle>\n");
  printf("    sz <target_handle> <interface_id>: Set IPV4_ZEROCONF_MODE\n");
  printf("        with Set INTERFACE_APPLY_CONFIGURATION on Target <target_handle>\n");
  printf("    m <target_handle>: Get MANUFACTURER_LABEL from Target <target_handle>\n");
  printf("    c <target_handle>: Get DEVICE_MODEL_DESCRIPTION from Target <target_handle>\n");
  printf("    s <target_handle> <scope_slot>: Get COMPONENT_SCOPE for Scope Slot\n");
  printf("        <scope_slot> from Target <target_handle>\n");
  printf("    ss <target_handle> <scope_slot> <scope> [ip:port]: Set COMPONENT_SCOPE to\n");
  printf("        <scope> for Scope Slot <scope_slot> on Target <target_handle> with\n");
  printf("        optional static Broker address ip:port\n");
  printf("    q: Quit\n");
}

bool LLRPManager::ParseCommand(const std::string& line)
{
  bool res = true;

  if (!line.empty())
  {
    switch (*(line.begin()))
    {
      case 'd':
        try
        {
          llrp_manager_t manager_handle = std::stoi(line.substr(2));
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
      case 'g':
        if (line.length() >= 2)
        {
          switch (line[1])
          {
          case 'i':
             try
             {
               int target_handle = std::stoi(line.substr(3));
               GetInterfaceList(target_handle);
             }
             catch (std::exception)
             {
               printf("Command syntax: gi <target_handle>\n");
             }
             break;
          }
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
                    if (static_config.ip().IsValid())
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
            case 'f':
               try
               {
                 int target_handle = std::stoi(line.substr(3));
                 FactoryDefaults(target_handle);
               }
               catch (std::exception)
               {
                 printf("Command syntax: sf <target_handle>\n");
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
            case 'r':
               try
               {
                 int target_handle = std::stoi(line.substr(3));
                 ResetDevice(target_handle);
               }
               catch (std::exception)
               {
                 printf("Command syntax: sr <target_handle>\n");
               }
               break;
            case 'z':
               try
               {
                 std::string args = line.substr(2);
                 size_t sp_pos = args.find_first_of(' ');
                 int target_handle = std::stoi(args);
                 unsigned interface_id = static_cast<unsigned>(std::stoi(args.substr(sp_pos)));
                 SetZeroconf(target_handle, interface_id);
               }
               catch (std::exception)
               {
                 printf("Command syntax: sz <target_handle>\n");
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

void LLRPManager::Discover(llrp_manager_t manager_handle)
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
  etcpal::Error res = rdmnet_llrp_start_discovery(mgr_pair->first, 0);
  if (res)
  {
    while (discovery_active_)
    {
      etcpal_thread_sleep(100);
    }
    printf("LLRP Discovery finished.\n");
  }
  else
  {
    printf("Error starting LLRP Discovery: '%s'\n", res.ToCString());
  }
}

void LLRPManager::PrintTargets()
{
  printf("Handle %-13s %-36s %-15s %s\n", "UID", "CID", "Type", "Hardware ID");
  for (const auto& target : targets_)
  {
    printf("%-6d %04x:%08x %s %-15s %s\n", target.first, target.second.prot_info.uid.manu,
           target.second.prot_info.uid.id, etcpal::Uuid(target.second.prot_info.cid).ToString().c_str(),
           LLRPComponentTypeToString(target.second.prot_info.component_type),
           etcpal::MacAddr(target.second.prot_info.hardware_address).ToString().c_str());
  }
}

void LLRPManager::PrintNetints()
{
  printf("Handle %-30s %-17s Name\n", "Address", "MAC");
  for (const auto& sock_pair : managers_)
  {
    const EtcPalNetintInfo& info = sock_pair.second;
    printf("%-6d %-30s %s %s\n", sock_pair.first, etcpal::IpAddr(info.addr).ToString().c_str(),
           etcpal::MacAddr(info.mac).ToString().c_str(), info.friendly_name);
  }
}

void LLRPManager::GetDeviceInfo(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCGetCommand;
      cmd_data.param_id = E120_DEVICE_INFO;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        if (resp_data.datalen == 19)
        {
          const uint8_t* cur_ptr = resp_data.data;
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
        else
        {
          printf("Device info response malformed.\n");
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
    printf("Error sending DEVICE_INFO command.\n");
  }
}

void LLRPManager::GetDeviceLabel(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCGetCommand;
      cmd_data.param_id = E120_DEVICE_LABEL;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        std::string dev_label;
        dev_label.assign(reinterpret_cast<char*>(resp_data.data), resp_data.datalen);
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

void LLRPManager::GetManufacturerLabel(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCGetCommand;
      cmd_data.param_id = E120_MANUFACTURER_LABEL;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        std::string manu_label;
        manu_label.assign(reinterpret_cast<char*>(resp_data.data), resp_data.datalen);
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

void LLRPManager::GetDeviceModelDescription(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCGetCommand;
      cmd_data.param_id = E120_DEVICE_MODEL_DESCRIPTION;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        std::string dev_model_desc;
        dev_model_desc.assign(reinterpret_cast<char*>(resp_data.data), resp_data.datalen);
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

void LLRPManager::GetComponentScope(int target_handle, int scope_slot)
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
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCGetCommand;
      cmd_data.param_id = E133_COMPONENT_SCOPE;
      cmd_data.datalen = 2;
      etcpal_pack_u16b(cmd_data.data, static_cast<uint16_t>(scope_slot));

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        if (resp_data.datalen >= (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2))
        {
          const uint8_t* cur_ptr = resp_data.data;

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
        else
        {
          printf("Malformed COMPONENT_SCOPE response.\n");
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

void LLRPManager::IdentifyDevice(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCSetCommand;
      cmd_data.param_id = E120_IDENTIFY_DEVICE;
      cmd_data.datalen = 1;
      cmd_data.data[0] = target->second.identifying ? 0 : 1;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
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

void LLRPManager::SetDeviceLabel(int target_handle, const std::string& label)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCSetCommand;
      cmd_data.param_id = E120_DEVICE_LABEL;
      cmd_data.datalen = (uint8_t)label.length();
      rdmnet_safe_strncpy((char*)cmd_data.data, label.c_str(), RDM_MAX_PDL);

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
        printf("Set device label successfully.\n");
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

void LLRPManager::ResetDevice(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCSetCommand;
      cmd_data.param_id = E120_RESET_DEVICE;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        printf("Reset device successfully.\n");
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending RESET_DEVICE command.\n");
  }
}

void LLRPManager::GetInterfaceList(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCGetCommand;
      cmd_data.param_id = E137_2_LIST_INTERFACES;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        if (resp_data.datalen <= 0xe6)
        {
          const uint8_t* cur_ptr = resp_data.data;
          printf("List interfaces:\n");

          while ((cur_ptr - resp_data.data) < resp_data.datalen)
          {
        	  printf("  0x%08x", etcpal_unpack_u32b(cur_ptr));
        	  cur_ptr += 4;
        	  printf(" 0x%04x\n", etcpal_unpack_u16b(cur_ptr));
        	  cur_ptr += 2;
          }
        }
        else
        {
          printf("List interfaces response malformed.\n");
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
    printf("Error sending LIST_INTERFACES command.\n");
  }
}

void LLRPManager::SetZeroconf(int target_handle, unsigned interface_id)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCSetCommand;
      cmd_data.param_id = E137_2_IPV4_ZEROCONF_MODE;
      cmd_data.datalen = 5;

      uint8_t* cur_ptr = cmd_data.data;
      etcpal_pack_u32b(cur_ptr, static_cast<uint32_t>(interface_id));

      cmd_data.data[4] = 1;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        printf("Zeroconf successfully.\n");
      }

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCSetCommand;
      cmd_data.param_id = E137_2_INTERFACE_APPLY_CONFIGURATION;
      cmd_data.datalen = 4;

      cur_ptr = cmd_data.data;
      etcpal_pack_u32b(cur_ptr, static_cast<uint32_t>(interface_id));

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        printf("Interface apply configuration successfully.\n");
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending IPV4_ZEROCONF_MODE command.\n");
  }
}


void LLRPManager::FactoryDefaults(int target_handle)
{
  auto mgr_pair = managers_.find(active_manager_);
  if (mgr_pair != managers_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCSetCommand;
      cmd_data.param_id = E120_FACTORY_DEFAULTS;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        printf("Factory defaults successfully.\n");
      }
    }
    else
    {
      printf("Target handle %d not found\n", target_handle);
    }
  }
  else
  {
    printf("Error sending FACTORY_DEFAULTS command.\n");
  }
}

void LLRPManager::SetComponentScope(int target_handle, int scope_slot, const std::string& scope_utf8,
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
      RdmCommand cmd_data;
      RdmResponse resp_data;

#define COMPONENT_SCOPE_PDL (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2)

      cmd_data.dest_uid = target->second.prot_info.uid;
      cmd_data.subdevice = 0;
      cmd_data.command_class = kRdmCCSetCommand;
      cmd_data.param_id = E133_COMPONENT_SCOPE;
      cmd_data.datalen = COMPONENT_SCOPE_PDL;
      memset(cmd_data.data, 0, COMPONENT_SCOPE_PDL);

      uint8_t* cur_ptr = cmd_data.data;
      etcpal_pack_u16b(cur_ptr, static_cast<uint16_t>(scope_slot));
      cur_ptr += 2;
      RDMNET_MSVC_NO_DEP_WRN strncpy((char*)cur_ptr, scope_utf8.c_str(), E133_SCOPE_STRING_PADDED_LENGTH - 1);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
      if (static_config.ip().IsV4())
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
        etcpal_pack_u32b(cur_ptr, static_config.ip().v4_data());
        cur_ptr += 4 + 16;
        etcpal_pack_u16b(cur_ptr, static_config.port());
      }
      else if (static_config.ip().IsV6())
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
        cur_ptr += 4;
        memcpy(cur_ptr, static_config.ip().v6_data(), 16);
        cur_ptr += 16;
        etcpal_pack_u16b(cur_ptr, static_config.port());
      }
      else
      {
        *cur_ptr = E133_NO_STATIC_CONFIG;
      }

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
        printf("Set scope successfully.\n");
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

void LLRPManager::TargetDiscovered(const DiscoveredLlrpTarget& target)
{
  if (discovery_active_)
  {
    int next_target_handle = targets_.empty() ? 0 : targets_.rbegin()->first + 1;
    printf("Adding LLRP Target, UID %04x:%08x, with handle %d\n", target.uid.manu, target.uid.id, next_target_handle);

    LLRPTargetInfo new_target_info;
    new_target_info.prot_info = target;
    targets_[next_target_handle] = new_target_info;
  }
}

void LLRPManager::DiscoveryFinished()
{
  discovery_active_ = false;
}

void LLRPManager::RdmRespReceived(const LlrpRemoteRdmResponse& resp)
{
  if (pending_command_response_ && resp.src_cid == pending_resp_cid_ && resp.seq_num == pending_resp_seq_num_)
  {
    resp_received_ = resp.rdm;
    pending_command_response_ = false;
  }
}

bool LLRPManager::SendRDMAndGetResponse(llrp_manager_t manager, const EtcPalUuid& target_cid,
                                        const RdmCommand& cmd_data, RdmResponse& resp_data)
{
  LlrpLocalRdmCommand cmd;
  cmd.rdm = cmd_data;
  cmd.dest_cid = target_cid;

  pending_command_response_ = true;
  pending_resp_cid_ = cmd.dest_cid;
  etcpal::Error res = rdmnet_llrp_send_rdm_command(manager, &cmd, &pending_resp_seq_num_);
  if (res)
  {
    EtcPalTimer resp_timer;
    etcpal_timer_start(&resp_timer, LLRP_TIMEOUT_MS);
    while (pending_command_response_ && !etcpal_timer_is_expired(&resp_timer))
    {
      etcpal_thread_sleep(100);
    }

    if (!pending_command_response_)
    {
      // We got a response.
      if (resp_received_.command_class == cmd_data.command_class + 1 && resp_received_.param_id == cmd_data.param_id)
      {
        if (resp_received_.resp_type == E120_RESPONSE_TYPE_ACK)
        {
          resp_data = resp_received_;
          return true;
        }
        else if (resp_received_.resp_type == E120_RESPONSE_TYPE_NACK_REASON)
        {
          printf("Received RDM NACK with reason %d\n", etcpal_unpack_u16b(resp_received_.data));
        }
        else
        {
          printf("Received LLRP RDM response with illegal response type %d\n", resp_received_.resp_type);
        }
      }
      else
      {
        printf("Received unexpected RDM response.\n");
      }
    }
    else
    {
      printf("Timed out waiting for RDM response.\n");
      pending_command_response_ = false;
    }
  }
  else
  {
    printf("Error sending RDM command: '%s'\n", res.ToCString());
    pending_command_response_ = false;
  }

  return false;
}

const char* LLRPManager::LLRPComponentTypeToString(llrp_component_t type)
{
  switch (type)
  {
    case kLlrpCompBroker:
      return "Broker";
    case kLlrpCompRptController:
      return "RPT Controller";
    case kLlrpCompRptDevice:
      return "RPT Device";
    case kLlrpCompNonRdmnet:
      return "LLRP Only";
    default:
      return "Unknown";
  }
}
