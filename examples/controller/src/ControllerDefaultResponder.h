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

#pragma once

#include <string>
#include <vector>
#include <map>
#include "etcpal/cpp/lock.h"
#include "rdm/responder.h"
#include "rdmnet/defs.h"
#include "rdmnet/version.h"
#include "ControllerUtils.h"

struct RdmParamData
{
  uint8_t data[RDM_MAX_PDL];
  uint8_t datalen;
};

struct ControllerScopeData
{
  ControllerScopeData(StaticBrokerConfig sb) : static_broker(sb) {}

  uint16_t unhealthy_tcp_events{0};
  StaticBrokerConfig static_broker;
  bool connected{false};
  etcpal::SockAddr current_broker;
};

constexpr char kMyDeviceLabel[] = "ETC Example RDMnet Controller";
constexpr char kMyManufacturerLabel[] = "ETC";
constexpr char kMyDeviceModelDescription[] = "ETC Example RDMnet Controller";
constexpr char kMySoftwareVersionLabel[] = RDMNET_VERSION_STRING;

class ControllerDefaultResponder
{
public:
  virtual ~ControllerDefaultResponder() = default;

  bool Get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
           uint16_t& nack_reason);

  bool GetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                         uint16_t& nack_reason) const;
  bool GetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                      uint16_t& nack_reason) const;
  bool GetComponentScope(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                         uint16_t& nack_reason) const;
  bool GetComponentScope(uint16_t slot, std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;
  bool GetSearchDomain(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                       uint16_t& nack_reason) const;
  bool GetTCPCommsStatus(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                         uint16_t& nack_reason) const;
  bool GetSupportedParameters(const uint8_t* param_data, uint8_t param_data_len,
                              std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;
  bool GetDeviceInfo(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                     uint16_t& nack_reason) const;
  bool GetManufacturerLabel(const uint8_t* param_data, uint8_t param_data_len,
                            std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;
  bool GetDeviceModelDescription(const uint8_t* param_data, uint8_t param_data_len,
                                 std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;
  bool GetSoftwareVersionLabel(const uint8_t* param_data, uint8_t param_data_len,
                               std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;

  void UpdateSearchDomain(const std::string& new_search_domain);
  void AddScope(const std::string& new_scope, StaticBrokerConfig static_broker = StaticBrokerConfig());
  void RemoveScope(const std::string& scope_to_remove);
  void UpdateScopeConnectionStatus(const std::string& scope, bool connected,
                                   const etcpal::SockAddr& broker_addr = etcpal::SockAddr());
  void IncrementTcpUnhealthyCounter(const std::string& scope);
  void ResetTcpUnhealthyCounter(const std::string& scope);

private:
  mutable etcpal::RwLock prop_lock_;

  // Property data
  const bool identifying_{false};
  const std::string device_label_{kMyDeviceLabel};
  std::map<std::string, ControllerScopeData> scopes_;
  std::string search_domain_{E133_DEFAULT_DOMAIN};
  static const std::vector<uint16_t> supported_parameters_;
  static const std::vector<uint8_t> device_info_;
  const std::string manufacturer_label_{kMyManufacturerLabel};
  const std::string device_model_description_{kMyDeviceModelDescription};
  const std::string software_version_label_{kMySoftwareVersionLabel};
};
