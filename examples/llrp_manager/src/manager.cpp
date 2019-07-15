/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "manager.h"

#include <cstring>
#include <cstdio>
#include <memory>

#include "lwpa/netint.h"
#include "lwpa/pack.h"
#include "lwpa/socket.h"
#include "lwpa/thread.h"
#include "lwpa/timer.h"
#include "rdmnet/core.h"
#include "rdmnet/core/util.h"

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

LLRPManager::LLRPManager(const LwpaUuid& my_cid, const LwpaLogParams* log_params) : cid_(my_cid)
{
  rdmnet_core_init(log_params);

  size_t num_interfaces = lwpa_netint_get_num_interfaces();
  if (num_interfaces > 0)
  {
    LlrpManagerConfig config;
    config.cid = cid_;
    config.manu_id = 0x6574;
    config.callbacks.target_discovered = llrpcb_target_discovered;
    config.callbacks.discovery_finished = llrpcb_discovery_finished;
    config.callbacks.rdm_resp_received = llrpcb_rdm_resp_received;
    config.callback_context = this;

    const LwpaNetintInfo* netint_list = lwpa_netint_get_interfaces();
    for (const LwpaNetintInfo* netint = netint_list; netint < netint_list + num_interfaces; ++netint)
    {
      config.netint.ip_type = netint->addr.type;
      config.netint.index = netint->index;
      llrp_manager_t handle;
      lwpa_error_t res = rdmnet_llrp_manager_create(&config, &handle);
      if (res == kLwpaErrOk)
      {
        managers_.insert(std::make_pair(handle, *netint));
      }
      else
      {
        char addr_str[LWPA_INET6_ADDRSTRLEN];
        lwpa_inet_ntop(&netint->addr, addr_str, LWPA_INET6_ADDRSTRLEN);
        printf("Warning: couldn't create LLRP Manager on network interface %s (error: '%s').\n", addr_str,
               lwpa_strerror(res));
      }
    }
  }
}

LLRPManager::~LLRPManager()
{
  for (const auto& netint : managers_)
  {
    rdmnet_llrp_manager_destroy(netint.first);
  }
  rdmnet_core_deinit();
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
                LwpaSockaddr static_config;
                LWPA_IP_SET_INVALID(&static_config.ip);
                if (third_sp_pos != std::string::npos)
                {
                  std::string ip_port = args.substr(third_sp_pos + 1);
                  size_t colon_pos = ip_port.find_first_of(':');
                  if (colon_pos != std::string::npos)
                  {
                    char ip_utf8[LWPA_INET6_ADDRSTRLEN];
                    if ((kLwpaErrOk == lwpa_inet_pton(kLwpaIpTypeV4, ip_utf8, &static_config.ip)) ||
                        (kLwpaErrOk == lwpa_inet_pton(kLwpaIpTypeV6, ip_utf8, &static_config.ip)))
                    {
                      static_config.port = static_cast<uint16_t>(std::stoi(ip_port.substr(colon_pos + 1)));
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
                char scope_utf8[E133_SCOPE_STRING_PADDED_LENGTH];
                SetComponentScope(target_handle, scope_slot, scope_utf8, static_config);
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
  lwpa_error_t res = rdmnet_llrp_start_discovery(mgr_pair->first, 0);
  if (res == kLwpaErrOk)
  {
    while (discovery_active_)
    {
      lwpa_thread_sleep(100);
    }
    printf("LLRP Discovery finished.\n");
  }
  else
  {
    printf("Error starting LLRP Discovery: '%s'\n", lwpa_strerror(res));
  }
}

void LLRPManager::PrintTargets()
{
  printf("Handle %-13s %-36s %-15s %s\n", "UID", "CID", "Type", "Hardware ID");
  for (const auto& target : targets_)
  {
    char cid_str[LWPA_UUID_STRING_BYTES];
    lwpa_uuid_to_string(cid_str, &target.second.prot_info.cid);

    char mac_str[21];
    const uint8_t* mac_bytes = target.second.prot_info.hardware_address;
    snprintf(mac_str, 21, "%02x:%02x:%02x:%02x:%02x:%02x", mac_bytes[0], mac_bytes[1], mac_bytes[2], mac_bytes[3],
             mac_bytes[4], mac_bytes[5]);

    printf("%-6d %04x:%08x %s %-15s %s\n", target.first, target.second.prot_info.uid.manu,
           target.second.prot_info.uid.id, cid_str, LLRPComponentTypeToString(target.second.prot_info.component_type),
           mac_str);
  }
}

void LLRPManager::PrintNetints()
{
  printf("Handle %-30s %-17s Name\n", "Address", "MAC");
  for (const auto& sock_pair : managers_)
  {
    char addr_str[LWPA_INET6_ADDRSTRLEN];
    const LwpaNetintInfo& info = sock_pair.second;
    lwpa_inet_ntop(&info.addr, addr_str, LWPA_INET6_ADDRSTRLEN);
    printf("%-6d %-30s %02x:%02x:%02x:%02x:%02x:%02x %s\n", sock_pair.first, addr_str, info.mac[0], info.mac[1],
           info.mac[2], info.mac[3], info.mac[4], info.mac[5], info.friendly_name);
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
          printf("  Device Model ID: %d\n", lwpa_upack_16b(cur_ptr));
          cur_ptr += 2;
          printf("  Product Category: %d\n", lwpa_upack_16b(cur_ptr));
          cur_ptr += 2;
          printf("  Software Version ID: %d\n", lwpa_upack_32b(cur_ptr));
          cur_ptr += 4;
          printf("  DMX512 Footprint: %d\n", lwpa_upack_16b(cur_ptr));
          cur_ptr += 2;
          printf("  DMX512 Personality: %d\n", lwpa_upack_16b(cur_ptr));
          cur_ptr += 2;
          printf("  DMX512 Start Address: %d\n", lwpa_upack_16b(cur_ptr));
          cur_ptr += 2;
          printf("  Subdevice Count: %d\n", lwpa_upack_16b(cur_ptr));
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
      lwpa_pack_16b(cmd_data.data, static_cast<uint16_t>(scope_slot));

      if (SendRDMAndGetResponse(mgr_pair->first, target->second.prot_info.cid, cmd_data, resp_data))
      {
        if (resp_data.datalen >= (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2))
        {
          const uint8_t* cur_ptr = resp_data.data;

          uint16_t slot = lwpa_upack_16b(cur_ptr);
          cur_ptr += 2;

          std::string scope;
          char scope_string[E133_SCOPE_STRING_PADDED_LENGTH] = {};
          memcpy(scope_string, cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH - 1);
          cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

          uint8_t static_config_type = *cur_ptr++;
          LwpaIpAddr ip;
          char ip_string[LWPA_INET6_ADDRSTRLEN] = {};
          uint16_t port = 0;

          printf("Scope for slot %d: %s\n", slot, scope_string);
          switch (static_config_type)
          {
            case E133_STATIC_CONFIG_IPV4:
              LWPA_IP_SET_V4_ADDRESS(&ip, lwpa_upack_32b(cur_ptr));
              lwpa_inet_ntop(&ip, ip_string, LWPA_INET6_ADDRSTRLEN);
              cur_ptr += 4 + 16;
              port = lwpa_upack_16b(cur_ptr);
              printf("Static Broker IPv4 for slot %d: %s:%u\n", slot, ip_string, port);
              break;
            case E133_STATIC_CONFIG_IPV6:
              cur_ptr += 4;
              LWPA_IP_SET_V6_ADDRESS(&ip, cur_ptr);
              lwpa_inet_ntop(&ip, ip_string, LWPA_INET6_ADDRSTRLEN);
              cur_ptr += 16;
              port = lwpa_upack_16b(cur_ptr);
              printf("Static Broker IPv6 for slot %d: [%s]:%u\n", slot, ip_string, port);
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

void LLRPManager::SetComponentScope(int target_handle, int scope_slot, const std::string& scope_utf8,
                                    const LwpaSockaddr& static_config)
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
      lwpa_pack_16b(cur_ptr, static_cast<uint16_t>(scope_slot));
      cur_ptr += 2;
      RDMNET_MSVC_NO_DEP_WRN strncpy((char*)cur_ptr, scope_utf8.c_str(), E133_SCOPE_STRING_PADDED_LENGTH - 1);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
      if (LWPA_IP_IS_V4(&static_config.ip))
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
        lwpa_pack_32b(cur_ptr, LWPA_IP_V4_ADDRESS(&static_config.ip));
        cur_ptr += 4 + 16;
        lwpa_pack_16b(cur_ptr, static_config.port);
      }
      else if (LWPA_IP_IS_V6(&static_config.ip))
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
        cur_ptr += 4;
        memcpy(cur_ptr, LWPA_IP_V6_ADDRESS(&static_config.ip), 16);
        cur_ptr += 16;
        lwpa_pack_16b(cur_ptr, static_config.port);
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
  if (pending_command_response_ && LWPA_UUID_CMP(&resp.src_cid, &pending_resp_cid_) == 0 &&
      resp.seq_num == pending_resp_seq_num_)
  {
    resp_received_ = resp.rdm;
    pending_command_response_ = false;
  }
}

bool LLRPManager::SendRDMAndGetResponse(llrp_manager_t manager, const LwpaUuid& target_cid, const RdmCommand& cmd_data,
                                        RdmResponse& resp_data)
{
  LlrpLocalRdmCommand cmd;
  cmd.rdm = cmd_data;
  cmd.dest_cid = target_cid;

  pending_command_response_ = true;
  pending_resp_cid_ = cmd.dest_cid;
  lwpa_error_t res = rdmnet_llrp_send_rdm_command(manager, &cmd, &pending_resp_seq_num_);
  if (res == kLwpaErrOk)
  {
    LwpaTimer resp_timer;
    lwpa_timer_start(&resp_timer, LLRP_TIMEOUT_MS);
    while (pending_command_response_ && !lwpa_timer_is_expired(&resp_timer))
    {
      lwpa_thread_sleep(100);
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
          printf("Received RDM NACK with reason %d\n", lwpa_upack_16b(resp_received_.data));
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
    printf("Error sending RDM command: '%s'\n", lwpa_strerror(res));
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
    default:
      return "Unknown";
  }
}
