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

#pragma once

#include <string>
#include <vector>
#include <map>
#include "lwpa/lock.h"
#include "rdm/responder.h"
#include "rdmnet/defs.h"
#include "rdmnet/version.h"

constexpr size_t kRdmDeviceLabelMaxLen = 32;

struct RdmParamData
{
  uint8_t data[RDM_MAX_PDL];
  uint8_t datalen;
};

class ControllerDefaultResponder
{
public:
  ControllerDefaultResponder() { lwpa_rwlock_create(&prop_lock_); }
  virtual ~ControllerDefaultResponder() { lwpa_rwlock_destroy(&prop_lock_); }

  bool Get(uint16_t pid, const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
           uint16_t &nack_reason);

  bool GetIdentifyDevice(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                         uint16_t &nack_reason) const;
  bool GetDeviceLabel(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                      uint16_t &nack_reason) const;
  bool GetComponentScope(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                         uint16_t &nack_reason) const;
  bool GetSearchDomain(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                       uint16_t &nack_reason) const;
  bool GetTCPCommsStatus(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                         uint16_t &nack_reason) const;
  bool GetSupportedParameters(const uint8_t *param_data, uint8_t param_data_len,
                              std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason) const;
  bool GetDeviceInfo(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                     uint16_t &nack_reason) const;
  bool GetManufacturerLabel(const uint8_t *param_data, uint8_t param_data_len,
                            std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason) const;
  bool GetDeviceModelDescription(const uint8_t *param_data, uint8_t param_data_len,
                                 std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason) const;
  bool GetSoftwareVersionLabel(const uint8_t *param_data, uint8_t param_data_len,
                               std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason) const;

  void UpdateSearchDomain(const std::string &new_search_domain);
  void AddScope(const std::string &new_scope);
  void RemoveScope(const std::string &scope_to_remove);
  void IncrementTcpUnhealthyCounter(const std::string &scope);
  void ResetTcpUnhealthyCounter(const std::string &scope);

private:
  bool GetComponentScope(uint16_t slot, std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason);

  mutable lwpa_rwlock_t prop_lock_;

  // Property data
  const bool identifying_{false};
  const std::string device_label_{"ETC Example RDMnet Controller"};
  std::map<std::string, uint16_t> scopes_;
  std::string search_domain_{E133_DEFAULT_DOMAIN};
  static const std::vector<uint16_t> supported_parameters_;
  static const std::vector<uint8_t> device_info_;
  const std::string manufacturer_label_{"ETC"};
  const std::string device_model_description_{"ETC Example RDMnet Controller"};
  const std::string software_version_labe_{RDMNET_VERSION_STRING};
};
