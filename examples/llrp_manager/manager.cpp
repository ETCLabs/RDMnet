/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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

#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <cstring>
#include <cwchar>
#include <cstdio>
#include <string>
#include <iostream>
#include <map>

#include "rdmnet/llrp.h"
#include "lwpa/netint.h"
#include "lwpa/socket.h"
#include "lwpa/log.h"
#include "lwpa/pack.h"
#include "lwpa/timer.h"
#include "rdm/defs.h"
#include "rdm/controller.h"
#include "rdmnet/defs.h"
#include "rdmnet/common/util.h"

struct LLRPNetint
{
  LwpaNetintInfo info;
  llrp_socket_t sock;
};

struct LLRPTargetInfo
{
  LlrpTarget prot_info;
  bool identifying{false};
};

class LLRPManager
{
public:
  LLRPManager(const LwpaUuid &my_cid, const RdmUid &my_uid);
  virtual ~LLRPManager();

  void PrintCommandList();
  bool ParseCommand(const std::wstring &line);

  void Discover(int netint_handle);
  void PrintTargets();
  void PrintNetints();
  void GetDeviceInfo(int target_handle);
  void GetDeviceLabel(int target_handle);
  void GetManufacturerLabel(int target_handle);
  void GetDeviceModelDescription(int target_handle);
  void GetComponentScope(int target_handle, int scope_slot);

  void IdentifyDevice(int target_handle);
  void SetDeviceLabel(int target_handle, const std::string &label);
  void SetComponentScope(int target_handle, int scope_slot, const std::string &scope_utf8,
                         const LwpaSockaddr &static_config);

protected:
  bool SendRDMAndGetResponse(llrp_socket_t sock, const LwpaUuid &target_cid, const RdmCommand &cmd_data,
                             RdmResponse &resp_data);
  static const char *LLRPComponentTypeToString(llrp_component_t type);

private:
  std::map<int, LLRPNetint> llrp_sockets_;
  LwpaUuid cid_;
  RdmUid uid_;
  uint8_t rdm_trans_num_;

  std::map<int, LLRPTargetInfo> targets_;
  int active_interface_;
};

LLRPManager::LLRPManager(const LwpaUuid &my_cid, const RdmUid &my_uid)
    : cid_(my_cid), uid_(my_uid), rdm_trans_num_(0), active_interface_(-1)
{
  llrp_init();

  int netint_handle = 0;
  size_t num_interfaces = lwpa_netint_get_num_interfaces();
  if (num_interfaces > 0)
  {
    size_t i;
    LwpaNetintInfo *netints = new LwpaNetintInfo[num_interfaces];

    num_interfaces = lwpa_netint_get_interfaces(netints, num_interfaces);
    for (i = 0; i < num_interfaces; ++i)
    {
      LwpaNetintInfo *netint = &netints[i];
      llrp_socket_t handle = llrp_create_manager_socket(&netint->addr, &cid_);
      if (handle != LLRP_SOCKET_INVALID)
      {
        LLRPNetint llrp_netint;
        llrp_netint.info = *netint;
        llrp_netint.sock = handle;
        llrp_sockets_.insert(std::make_pair(netint_handle++, llrp_netint));
      }
      else
      {
        char addr_str[LWPA_INET6_ADDRSTRLEN];
        lwpa_inet_ntop(&netint->addr, addr_str, LWPA_INET6_ADDRSTRLEN);
        printf("Warning: couldn't create LLRP Manager socket on network interface %s.\n", addr_str);
      }
    }
    free(netints);
  }
}

LLRPManager::~LLRPManager()
{
  for (const auto &sock : llrp_sockets_)
  {
    llrp_close_socket(sock.second.sock);
  }
  llrp_deinit();
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

bool LLRPManager::ParseCommand(const std::wstring &line)
{
  bool res = true;

  if (!line.empty())
  {
    switch (*(line.begin()))
    {
      case 'd':
        try
        {
          int netint_handle = std::stoi(line.substr(2));
          Discover(netint_handle);
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
                std::wstring args = line.substr(3);
                size_t first_sp_pos = args.find_first_of(' ');
                size_t second_sp_pos = args.find_first_of(' ', first_sp_pos + 1);
                size_t third_sp_pos = args.find_first_of(' ', second_sp_pos + 1);

                int target_handle = std::stoi(args);
                int scope_slot = std::stoi(args.substr(first_sp_pos + 1, second_sp_pos - (first_sp_pos + 1)));
                LwpaSockaddr static_config;
                lwpaip_set_invalid(&static_config.ip);
                if (third_sp_pos != std::string::npos)
                {
                  std::wstring ip_port = args.substr(third_sp_pos + 1);
                  size_t colon_pos = ip_port.find_first_of(':');
                  if (colon_pos != std::string::npos)
                  {
                    char ip_utf8[LWPA_INET6_ADDRSTRLEN];
                    if (WideCharToMultiByte(CP_UTF8, 0, ip_port.substr(0, colon_pos).c_str(), -1, ip_utf8,
                                            LWPA_INET6_ADDRSTRLEN, NULL, NULL) > 0)
                    {
                      if ((LWPA_OK == lwpa_inet_pton(kLwpaIpTypeV4, ip_utf8, &static_config.ip)) ||
                          (LWPA_OK == lwpa_inet_pton(kLwpaIpTypeV6, ip_utf8, &static_config.ip)))
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
                  else
                  {
                    throw std::invalid_argument("Invalid static IP/port combo.");
                  }
                }

                // Get and convert the scope
                std::wstring scope = args.substr(
                    second_sp_pos + 1,
                    (third_sp_pos == std::string::npos ? third_sp_pos : third_sp_pos - (second_sp_pos + 1)));
                char scope_utf8[E133_SCOPE_STRING_PADDED_LENGTH];
                if (WideCharToMultiByte(CP_UTF8, 0, scope.c_str(), -1, scope_utf8, E133_SCOPE_STRING_PADDED_LENGTH,
                                        NULL, NULL) > 0)
                {
                  SetComponentScope(target_handle, scope_slot, scope_utf8, static_config);
                }
                else
                {
                  throw std::invalid_argument("Invalid scope.");
                }
              }
              catch (const std::exception &e)
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
                std::wstring args = line.substr(3);
                size_t sp_pos = args.find_first_of(' ');

                int target_handle = std::stoi(args);
                std::wstring label = args.substr(sp_pos + 1);
                // Yes, yes, the device label is supposed to be ASCII. This is easier on unicode Windows for now.
                char label_utf8[32];
                if (WideCharToMultiByte(CP_UTF8, 0, label.c_str(), -1, label_utf8, 32, NULL, NULL) > 0)
                {
                  SetDeviceLabel(target_handle, label_utf8);
                }
                else
                {
                  printf("Invalid Device Label.\n");
                }
              }
              catch (std::exception)
              {
                printf("Command syntax: sl <target_handle> <label>\n");
              }
              break;
            case ' ':
              try
              {
                std::wstring args = line.substr(2);
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

void LLRPManager::Discover(int netint_handle)
{
  auto sock_pair = llrp_sockets_.find(netint_handle);
  if (sock_pair == llrp_sockets_.end())
  {
    printf("Network interface handle not found.\n");
    return;
  }

  targets_.clear();
  int target_handle = 0;

  active_interface_ = netint_handle;

  LlrpPoll poll;
  llrp_start_discovery(sock_pair->second.sock, 0);

  memset(&poll, 0, sizeof(LlrpPoll));
  poll.handle = sock_pair->second.sock;
  printf("Starting LLRP discovery...\n");
  int update_status;
  do
  {
    update_status = llrp_update(&poll, 1, LWPA_WAIT_FOREVER);

    if (update_status < 0)
    {
      // Error handling
    }
    else if (poll.err != LWPA_OK)
    {
      // Error handling
    }
    else if (llrp_data_is_disc_target(&poll.data))
    {
      RdmUid target_uid = llrp_data_disc_target(&poll.data)->target_uid;

      printf("Adding LLRP Target, UID %04x:%08x, with handle %d\n", target_uid.manu, target_uid.id, target_handle);

      LLRPTargetInfo new_target_info;
      new_target_info.prot_info = *(llrp_data_disc_target(&poll.data));
      targets_[target_handle++] = new_target_info;
    }
    else if (llrp_data_is_disc_finished(&poll.data))
    {
      printf("LLRP Discovery finished.\n");
      break;
    }

  } while (update_status >= 0 || update_status == LWPA_TIMEDOUT);
}

void LLRPManager::PrintTargets()
{
  printf("Handle %-13s %-36s %-15s\n", "UID", "CID", "Type");
  for (const auto &target : targets_)
  {
    char cid_str[LWPA_UUID_STRING_BYTES];
    lwpa_uuid_to_string(cid_str, &target.second.prot_info.target_cid);
    printf("%-6d %04x:%08x %s %s\n", target.first, target.second.prot_info.target_uid.manu,
           target.second.prot_info.target_uid.id, cid_str,
           LLRPComponentTypeToString(target.second.prot_info.component_type));
  }
}

void LLRPManager::PrintNetints()
{
  printf("Handle %-15s %-17s Name\n", "Address", "MAC");
  for (const auto &sock_pair : llrp_sockets_)
  {
    char addr_str[LWPA_INET6_ADDRSTRLEN];
    const LwpaNetintInfo &info = sock_pair.second.info;
    lwpa_inet_ntop(&info.addr, addr_str, LWPA_INET6_ADDRSTRLEN);
    printf("%-6d %-15s %02x:%02x:%02x:%02x:%02x:%02x %s\n", sock_pair.first, addr_str, info.mac[0], info.mac[1],
           info.mac[2], info.mac[3], info.mac[4], info.mac[5], info.name);
  }
}

void LLRPManager::GetDeviceInfo(int target_handle)
{
  auto netint_pair = llrp_sockets_.find(active_interface_);
  if (netint_pair != llrp_sockets_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.src_uid = uid_;
      cmd_data.dest_uid = target->second.prot_info.target_uid;
      cmd_data.transaction_num = rdm_trans_num_++;
      cmd_data.port_id = 0;
      cmd_data.subdevice = 0;
      cmd_data.command_class = E120_GET_COMMAND;
      cmd_data.param_id = E120_DEVICE_INFO;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(netint_pair->second.sock, target->second.prot_info.target_cid, cmd_data, resp_data))
      {
        if (resp_data.datalen == 19)
        {
          const uint8_t *cur_ptr = resp_data.data;
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
  auto netint_pair = llrp_sockets_.find(active_interface_);
  if (netint_pair != llrp_sockets_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.src_uid = uid_;
      cmd_data.dest_uid = target->second.prot_info.target_uid;
      cmd_data.transaction_num = rdm_trans_num_++;
      cmd_data.port_id = 0;
      cmd_data.subdevice = 0;
      cmd_data.command_class = E120_GET_COMMAND;
      cmd_data.param_id = E120_DEVICE_LABEL;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(netint_pair->second.sock, target->second.prot_info.target_cid, cmd_data, resp_data))
      {
        std::string dev_label;
        dev_label.assign(reinterpret_cast<char *>(resp_data.data), resp_data.datalen);
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
  auto netint_pair = llrp_sockets_.find(active_interface_);
  if (netint_pair != llrp_sockets_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.src_uid = uid_;
      cmd_data.dest_uid = target->second.prot_info.target_uid;
      cmd_data.transaction_num = rdm_trans_num_++;
      cmd_data.port_id = 0;
      cmd_data.subdevice = 0;
      cmd_data.command_class = E120_GET_COMMAND;
      cmd_data.param_id = E120_MANUFACTURER_LABEL;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(netint_pair->second.sock, target->second.prot_info.target_cid, cmd_data, resp_data))
      {
        std::string manu_label;
        manu_label.assign(reinterpret_cast<char *>(resp_data.data), resp_data.datalen);
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
  auto netint_pair = llrp_sockets_.find(active_interface_);
  if (netint_pair != llrp_sockets_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.src_uid = uid_;
      cmd_data.dest_uid = target->second.prot_info.target_uid;
      cmd_data.transaction_num = rdm_trans_num_++;
      cmd_data.port_id = 0;
      cmd_data.subdevice = 0;
      cmd_data.command_class = E120_GET_COMMAND;
      cmd_data.param_id = E120_DEVICE_MODEL_DESCRIPTION;
      cmd_data.datalen = 0;

      if (SendRDMAndGetResponse(netint_pair->second.sock, target->second.prot_info.target_cid, cmd_data, resp_data))
      {
        std::string dev_model_desc;
        dev_model_desc.assign(reinterpret_cast<char *>(resp_data.data), resp_data.datalen);
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

  auto netint_pair = llrp_sockets_.find(active_interface_);
  if (netint_pair != llrp_sockets_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.src_uid = uid_;
      cmd_data.dest_uid = target->second.prot_info.target_uid;
      cmd_data.transaction_num = rdm_trans_num_++;
      cmd_data.port_id = 0;
      cmd_data.subdevice = 0;
      cmd_data.command_class = E120_GET_COMMAND;
      cmd_data.param_id = E133_COMPONENT_SCOPE;
      cmd_data.datalen = 2;
      lwpa_pack_16b(cmd_data.data, scope_slot);

      if (SendRDMAndGetResponse(netint_pair->second.sock, target->second.prot_info.target_cid, cmd_data, resp_data))
      {
        if (resp_data.datalen >= (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2))
        {
          const uint8_t *cur_ptr = resp_data.data;

          uint16_t slot = lwpa_upack_16b(cur_ptr);
          cur_ptr += 2;

          std::string scope;
          char scope_string[E133_SCOPE_STRING_PADDED_LENGTH] = {};
          memcpy(scope_string, cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH - 1);
          cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

          uint8_t static_config_type = *cur_ptr++;
          LwpaIpAddr ip;
          char ip_string[LWPA_INET6_ADDRSTRLEN] = {};
          uint8_t port = 0;

          printf("Scope for slot %d: %s\n", slot, scope_string);
          switch (static_config_type)
          {
            case E133_STATIC_CONFIG_IPV4:
              lwpaip_set_v4_address(&ip, lwpa_upack_32b(cur_ptr));
              lwpa_inet_ntop(&ip, ip_string, LWPA_INET6_ADDRSTRLEN);
              cur_ptr += 4 + 16;
              port = lwpa_upack_16b(cur_ptr);
              printf("Static Broker IPv4 for slot %d: %s:%d\n", slot, ip_string, port);
              break;
            case E133_STATIC_CONFIG_IPV6:
              cur_ptr += 4;
              lwpaip_set_v6_address(&ip, cur_ptr);
              lwpa_inet_ntop(&ip, ip_string, LWPA_INET6_ADDRSTRLEN);
              cur_ptr += 16;
              port = lwpa_upack_16b(cur_ptr);
              printf("Static Broker IPv6 for slot %d: [%s]:%d\n", slot, ip_string, port);
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
  auto netint_pair = llrp_sockets_.find(active_interface_);
  if (netint_pair != llrp_sockets_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.src_uid = uid_;
      cmd_data.dest_uid = target->second.prot_info.target_uid;
      cmd_data.transaction_num = rdm_trans_num_++;
      cmd_data.port_id = 0;
      cmd_data.subdevice = 0;
      cmd_data.command_class = E120_SET_COMMAND;
      cmd_data.param_id = E120_IDENTIFY_DEVICE;
      cmd_data.datalen = 1;
      cmd_data.data[0] = target->second.identifying ? 0 : 1;

      if (SendRDMAndGetResponse(netint_pair->second.sock, target->second.prot_info.target_cid, cmd_data, resp_data))
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

void LLRPManager::SetDeviceLabel(int target_handle, const std::string &label)
{
  auto netint_pair = llrp_sockets_.find(active_interface_);
  if (netint_pair != llrp_sockets_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

      cmd_data.src_uid = uid_;
      cmd_data.dest_uid = target->second.prot_info.target_uid;
      cmd_data.transaction_num = rdm_trans_num_++;
      cmd_data.port_id = 0;
      cmd_data.subdevice = 0;
      cmd_data.command_class = E120_SET_COMMAND;
      cmd_data.param_id = E120_DEVICE_LABEL;
      cmd_data.datalen = (uint8_t)label.length();
      RDMNET_MSVC_NO_DEP_WRN strncpy((char *)cmd_data.data, label.c_str(), RDM_MAX_PDL);

      if (SendRDMAndGetResponse(netint_pair->second.sock, target->second.prot_info.target_cid, cmd_data, resp_data))
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

void LLRPManager::SetComponentScope(int target_handle, int scope_slot, const std::string &scope_utf8,
                                    const LwpaSockaddr &static_config)
{
  if (scope_slot < 1 || scope_slot > 65535)
  {
    printf("Invalid scope slot.\n");
    return;
  }

  auto netint_pair = llrp_sockets_.find(active_interface_);
  if (netint_pair != llrp_sockets_.end())
  {
    auto target = targets_.find(target_handle);
    if (target != targets_.end())
    {
      RdmCommand cmd_data;
      RdmResponse resp_data;

#define COMPONENT_SCOPE_PDL (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2)

      cmd_data.src_uid = uid_;
      cmd_data.dest_uid = target->second.prot_info.target_uid;
      cmd_data.transaction_num = rdm_trans_num_++;
      cmd_data.port_id = 0;
      cmd_data.subdevice = 0;
      cmd_data.command_class = E120_SET_COMMAND;
      cmd_data.param_id = E133_COMPONENT_SCOPE;
      cmd_data.datalen = COMPONENT_SCOPE_PDL;
      memset(cmd_data.data, 0, COMPONENT_SCOPE_PDL);

      uint8_t *cur_ptr = cmd_data.data;
      lwpa_pack_16b(cur_ptr, scope_slot);
      cur_ptr += 2;
      RDMNET_MSVC_NO_DEP_WRN strncpy((char *)cur_ptr, scope_utf8.c_str(), E133_SCOPE_STRING_PADDED_LENGTH - 1);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
      if (lwpaip_is_v4(&static_config.ip))
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
        lwpa_pack_32b(cur_ptr, lwpaip_v4_address(&static_config.ip));
        cur_ptr += 4 + 16;
        lwpa_pack_16b(cur_ptr, static_config.port);
      }
      else if (lwpaip_is_v6(&static_config.ip))
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
        cur_ptr += 4;
        memcpy(cur_ptr, lwpaip_v6_address(&static_config.ip), 16);
        cur_ptr += 16;
        lwpa_pack_16b(cur_ptr, static_config.port);
      }
      else
      {
        *cur_ptr = E133_NO_STATIC_CONFIG;
      }

      if (SendRDMAndGetResponse(netint_pair->second.sock, target->second.prot_info.target_cid, cmd_data, resp_data))
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

bool LLRPManager::SendRDMAndGetResponse(llrp_socket_t sock, const LwpaUuid &target_cid, const RdmCommand &cmd_data,
                                        RdmResponse &resp_data)
{
  RdmBuffer cmd;
  if (LWPA_OK == rdmctl_create_command(&cmd_data, &cmd))
  {
    uint32_t trans_num;
    LwpaTimer resp_timer;
    bool resp_timeout = true;

    llrp_send_rdm_command(sock, &target_cid, &cmd, &trans_num);
    lwpa_timer_start(&resp_timer, LLRP_TIMEOUT_MS);
    while (!lwpa_timer_isexpired(&resp_timer))
    {
      LlrpPoll poll;
      poll.handle = sock;
      int update_res = llrp_update(&poll, 1, lwpa_timer_remaining(&resp_timer));
      if (update_res >= 1 && poll.err == LWPA_OK && llrp_data_is_rdm(&poll.data))
      {
        LlrpRdmMessage *llrp_rdm = llrp_data_rdm(&poll.data);
        if (llrp_rdm->transaction_num == trans_num)
        {
          if (LWPA_OK == rdmctl_unpack_response(&llrp_rdm->msg, &resp_data))
          {
            if (resp_data.command_class == cmd_data.command_class + 1 && resp_data.param_id == cmd_data.param_id)
            {
              if (resp_data.resp_type == E120_RESPONSE_TYPE_ACK)
              {
                return true;
              }
              else if (resp_data.resp_type == E120_RESPONSE_TYPE_NACK_REASON)
              {
                resp_timeout = false;
                printf("Received RDM NACK with reason %d\n", lwpa_upack_16b(resp_data.data));
              }
              else
              {
                resp_timeout = false;
                printf("Received LLRP RDM response with illegal response type %d\n", resp_data.resp_type);
              }
            }
            else
            {
              printf("Received unexpected RDM response.\n");
            }
          }
          else
          {
            printf("Received malformed RDM response.\n");
          }
        }
      }
      else if (update_res != LWPA_TIMEDOUT)
      {
        resp_timeout = false;
        printf("Error receiving response to RDM command.\n");
        break;
      }
    }
    if (resp_timeout)
    {
      printf("Timed out waiting for RDM response.\n");
    }
  }
  else
  {
    printf("Error building RDM command.\n");
  }

  return false;
}

const char *LLRPManager::LLRPComponentTypeToString(llrp_component_t type)
{
  switch (type)
  {
    case kLLRPCompBroker:
      return "Broker";
    case kLLRPCompRPTController:
      return "RPT Controller";
    case kLLRPCompRPTDevice:
      return "RPT Device";
    default:
      return "Unknown";
  }
}

int wmain(int /*argc*/, wchar_t * /*argv*/[])
{
  LwpaUuid manager_cid;
  RdmUid manager_uid;

  UUID uuid;
  UuidCreate(&uuid);
  memcpy(manager_cid.data, &uuid, LWPA_UUID_BYTES);

  manager_uid.manu = 0xe574;
  /* Slight hack - using the last 32 bits of the CID as the UID. */
  manager_uid.id = lwpa_upack_32b(&manager_cid.data[12]);

  LLRPManager mgr(manager_cid, manager_uid);
  printf("Discovered network interfaces:\n");
  mgr.PrintNetints();
  mgr.PrintCommandList();

  std::wstring input;
  while (true)
  {
    std::getline(std::wcin, input);
    if (!mgr.ParseCommand(input))
      break;
  }
  return 0;
}
