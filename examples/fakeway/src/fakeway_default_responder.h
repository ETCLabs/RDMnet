/******************************************************************************
 * Copyright 2020 ETC Inc.
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
#include "etcpal/cpp/rwlock.h"
#include "etcpal/cpp/thread.h"
#include "rdmnet/version.h"
#include "rdmnet/cpp/device.h"

constexpr size_t kComponentScopeDataLength = (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2);

enum class RdmnetConfigChange
{
  kNoChange,
  kScopeConfigChanged,
  kSearchDomainChanged
};

class FakewayDefaultResponder
{
public:
  static const uint8_t        kDeviceInfo[];
  static constexpr const char kDefaultDeviceLabel[] = "My ETC RDMnet Gateway";
  static constexpr const char kSoftwareVersionLabel[] = RDMNET_VERSION_STRING;
  static constexpr const char kManufacturerLabel[] = "ETC";
  static constexpr const char kDeviceModelDescription[] = "Example RDMnet Gateway";

  FakewayDefaultResponder(const rdmnet::Scope& scope_config, const std::string& search_domain);
  virtual ~FakewayDefaultResponder();

  const rdmnet::Scope& scope_config() const { return scope_config_; }
  const std::string&   search_domain() const { return search_domain_; }
  uint8_t*             response_buf() { return response_buf_; }

  bool SupportsPid(uint16_t pid) const { return supported_pid_list_.find(pid) != supported_pid_list_.end(); }
  rdmnet::RdmResponseAction Set(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction Get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len);

  void IdentifyThread();

private:
  std::set<uint16_t> supported_pid_list_;

  // Lock around property data
  etcpal::RwLock prop_lock_;
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Property data

  // Identify
  etcpal::Thread identify_thread_;
  bool           identifying_{false};

  // Device Label
  std::string device_label_{kDefaultDeviceLabel};

  // Component Scope
  rdmnet::Scope scope_config_{};

  // Search Domain
  std::string search_domain_;

  // Data buffer
  uint8_t response_buf_[E133_DOMAIN_STRING_PADDED_LENGTH];

  //////////////////////////////////////////////////////////////////////////////////////////////////

  /* SET COMMANDS */
  rdmnet::RdmResponseAction SetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction SetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction SetComponentScope(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction SetSearchDomain(const uint8_t* param_data, uint8_t param_data_len);

  /* GET COMMANDS */
  rdmnet::RdmResponseAction GetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction GetDeviceInfo(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction GetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction GetSupportedParameters(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction GetManufacturerLabel(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction GetDeviceModelDescription(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction GetSoftwareVersionLabel(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction GetComponentScope(const uint8_t* param_data, uint8_t param_data_len);
  rdmnet::RdmResponseAction GetSearchDomain(const uint8_t* param_data, uint8_t param_data_len);
};

#endif /* FAKEWAY_DEFAULT_RESPONDER_H_ */
