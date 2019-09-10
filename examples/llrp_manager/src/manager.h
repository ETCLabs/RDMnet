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

#include <map>
#include <string>
#include <vector>

#include "etcpal/log.h"
#include "etcpal/uuid.h"
#include "rdm/message.h"
#include "rdmnet/core/llrp_manager.h"

struct LLRPTargetInfo
{
  DiscoveredLlrpTarget prot_info;
  bool identifying{false};
};

class LLRPManager
{
public:
  bool Startup(const EtcPalUuid &my_cid, const EtcPalLogParams *log_params = nullptr);
  void Shutdown();

  enum class ParseResult
  {
    kRun,
    kParseErr,
    kPrintHelp,
    kPrintVersion
  };
  ParseResult ParseCommandLineArgs(const std::vector<std::string> &args);
  void PrintUsage(const std::string &app_name);
  void PrintVersion();

  void PrintCommandList();
  bool ParseCommand(const std::string &line);

  void Discover(llrp_manager_t manager_handle);
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
                         const EtcPalSockaddr &static_config);

  void TargetDiscovered(const DiscoveredLlrpTarget &target);
  void DiscoveryFinished();
  void RdmRespReceived(const LlrpRemoteRdmResponse &resp);

private:
  bool SendRDMAndGetResponse(llrp_manager_t manager, const EtcPalUuid &target_cid, const RdmCommand &cmd_data,
                             RdmResponse &resp_data);
  static const char *LLRPComponentTypeToString(llrp_component_t type);

  std::map<llrp_manager_t, EtcPalNetintInfo> managers_;
  EtcPalUuid cid_{};

  std::map<int, LLRPTargetInfo> targets_;
  llrp_manager_t active_manager_{LLRP_MANAGER_INVALID};

  bool discovery_active_{false};

  bool pending_command_response_{false};
  EtcPalUuid pending_resp_cid_{};
  uint32_t pending_resp_seq_num_{0};
  RdmResponse resp_received_{};
};
