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

#ifndef FAKEWAY_DEFAULT_RESPONDER_H_
#define FAKEWAY_DEFAULT_RESPONDER_H_

#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/lock.h"
#include "etcpal/cpp/thread.h"
#include "rdmnet/version.h"
#include "rdmnet/client.h"

#define DEFAULT_DEVICE_LABEL "My ETC RDMnet Gateway"
#define SOFTWARE_VERSION_LABEL RDMNET_VERSION_STRING
#define MANUFACTURER_LABEL "ETC"
#define DEVICE_MODEL_DESCRIPTION "Example RDMnet Gateway"

enum class RdmnetConfigChange
{
  kNoChange,
  kScopeConfigChanged,
  kSearchDomainChanged
};

class FakewayDefaultResponder
{
public:
  static const size_t MAX_RESPONSES_IN_ACK_OVERFLOW = 2;

  struct RdmParamData
  {
    uint8_t datalen;
    uint8_t data[RDM_MAX_PDL];
  };
  using ParamDataList = std::vector<RdmParamData>;

  static const uint8_t kDeviceInfo[];

  FakewayDefaultResponder(const RdmnetScopeConfig& scope_config, const std::string& search_domain);
  virtual ~FakewayDefaultResponder();

  // Update the information served by the Default Responder
  void IncrementTcpUnhealthyCounter();
  void ResetTcpUnhealthyCounter();
  void UpdateConnectionStatus(bool connected, const etcpal::SockAddr& broker_addr = etcpal::SockAddr{});
  void AddEndpoints(const std::vector<uint16_t>& new_endpoints);
  void RemoveEndpoints(const std::vector<uint16_t>& endpoints_to_remove);
  void AddResponderOnEndpoint(uint16_t endpoint, const RdmUid& responder);
  void RemoveResponderOnEndpoint(uint16_t endpoint, const RdmUid& responder);

  RdmnetScopeConfig scope_config() const { return scope_config_; }
  std::string search_domain() const { return search_domain_; }

  bool SupportsPid(uint16_t pid) const { return supported_pid_list_.find(pid) != supported_pid_list_.end(); }
  bool HandlePidWithPhysicalEndpoint(uint16_t pid) const
  {
    return pids_handled_with_phys_endpt_.find(pid) != pids_handled_with_phys_endpt_.end();
  }
  bool Set(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len, uint16_t& nack_reason,
           RdmnetConfigChange& config_change);
  bool Get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
           uint16_t& nack_reason);

  void IdentifyThread();

private:
  std::set<uint16_t> supported_pid_list_;
  std::set<uint16_t> pids_handled_with_phys_endpt_;

  struct EndpointInfo
  {
    std::set<RdmUid> responders;
    uint32_t responder_change_number{0};
  };

  // Lock around property data
  etcpal::RwLock prop_lock_;
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Property data

  // Identify
  etcpal::Thread identify_thread_;
  bool identifying_{false};

  // Device Label
  std::string device_label_{DEFAULT_DEVICE_LABEL};

  // Component Scope
  RdmnetScopeConfig scope_config_{};

  // Search Domain
  std::string search_domain_;

  // TCP Comms Status
  bool connected_{false};
  etcpal::SockAddr cur_broker_addr_;
  uint16_t tcp_unhealthy_count_{0};

  // Endpoint List
  uint32_t endpoint_change_number_{0};
  std::map<uint16_t, EndpointInfo> endpoints_;

  //////////////////////////////////////////////////////////////////////////////////////////////////

  /* SET COMMANDS */
  bool SetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len, uint16_t& nack_reason,
                         RdmnetConfigChange& config_change);
  bool SetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len, uint16_t& nack_reason,
                      RdmnetConfigChange& config_change);
  bool SetComponentScope(const uint8_t* param_data, uint8_t param_data_len, uint16_t& nack_reason,
                         RdmnetConfigChange& config_change);
  bool SetSearchDomain(const uint8_t* param_data, uint8_t param_data_len, uint16_t& nack_reason,
                       RdmnetConfigChange& config_change);
  bool SetTcpCommsStatus(const uint8_t* param_data, uint8_t param_data_len, uint16_t& nack_reason,
                         RdmnetConfigChange& config_change);

  /* GET COMMANDS */
  bool GetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                         uint16_t& nack_reason);
  bool GetDeviceInfo(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                     uint16_t& nack_reason);
  bool GetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                      uint16_t& nack_reason);
  bool GetComponentScope(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                         uint16_t& nack_reason);
  bool GetSearchDomain(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                       uint16_t& nack_reason);
  bool GetTcpCommsStatus(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                         uint16_t& nack_reason);
  bool GetSupportedParameters(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                              uint16_t& nack_reason);
  bool GetManufacturerLabel(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                            uint16_t& nack_reason);
  bool GetDeviceModelDescription(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                                 uint16_t& nack_reason);
  bool GetSoftwareVersionLabel(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                               uint16_t& nack_reason);
  bool GetEndpointList(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                       uint16_t& nack_reason);
  bool GetEndpointListChange(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                             uint16_t& nack_reason);
  bool GetEndpointResponders(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                             uint16_t& nack_reason);
  bool GetEndpointResponderListChange(const uint8_t* param_data, uint8_t param_data_len, ParamDataList& resp_data_list,
                                      uint16_t& nack_reason);
};

#endif /* FAKEWAY_DEFAULT_RESPONDER_H_ */
