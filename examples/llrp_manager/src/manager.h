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

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/log.h"
#include "rdmnet/cpp/llrp_manager.h"

namespace llrp = rdmnet::llrp;

struct TargetInfo
{
  llrp::DiscoveredTarget prot_info;
  bool identifying{false};
};

struct ManagerInfo
{
  llrp::Manager manager;
  EtcPalNetintInfo netint_info;
};

class LlrpManagerExample : public llrp::Manager::NotifyHandler
{
public:
  bool Startup(const etcpal::Uuid& my_cid, const etcpal::Logger& logger);
  void Shutdown();

  enum class ParseResult
  {
    kRun,
    kParseErr,
    kPrintHelp,
    kPrintVersion
  };
  ParseResult ParseCommandLineArgs(const std::vector<std::string>& args);
  void PrintUsage(const std::string& app_name);
  void PrintVersion();

  void PrintCommandList();
  bool ParseCommand(const std::string& line);

  void Discover(llrp::Manager::Handle handle);
  void PrintTargets();
  void PrintNetints();
  void GetDeviceInfo(int target_handle);
  void GetDeviceLabel(int target_handle);
  void GetManufacturerLabel(int target_handle);
  void GetDeviceModelDescription(int target_handle);
  void GetComponentScope(int target_handle, int scope_slot);

  void IdentifyDevice(int target_handle);
  void SetDeviceLabel(int target_handle, const std::string& label);
  void SetComponentScope(int target_handle, int scope_slot, const std::string& scope_utf8,
                         const etcpal::SockAddr& static_config);
  void SetFactoryDefaults(int target_handle);
  void SetResetDevice(int target_handle);

  void HandleLlrpTargetDiscovered(llrp::Manager::Handle handle, const llrp::DiscoveredTarget& target) override;
  void HandleLlrpDiscoveryFinished(llrp::Manager::Handle handle) override;
  void HandleLlrpRdmResponse(llrp::Manager::Handle handle, const llrp::RdmResponse& resp) override;

private:
  using RdmResponseHandler = std::function<void(const llrp::RdmResponse&)>;

  std::vector<uint8_t> GetDataFromTarget(llrp::Manager& manager, const llrp::DiscoveredTarget& target,
                                         uint16_t param_id, const uint8_t* data = nullptr, uint8_t data_len = 0);
  bool SetDataOnTarget(llrp::Manager& manager, const llrp::DiscoveredTarget& target, uint16_t param_id,
                       const uint8_t* data = nullptr, uint8_t data_len = 0);

  std::map<llrp::Manager::Handle, ManagerInfo> managers_;

  std::map<int, TargetInfo> targets_;
  llrp::Manager::Handle active_manager_{llrp::Manager::kInvalidHandle};
  RdmResponseHandler active_response_handler_;

  bool discovery_active_{false};
};
