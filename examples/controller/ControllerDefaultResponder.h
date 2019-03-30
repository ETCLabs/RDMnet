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
#include "rdm/responder.h"

constexpr size_t kRdmDeviceLabelMaxLen = 32;

struct RdmParamData
{
  uint8_t datalen;
  uint8_t data[RDM_MAX_PDL];
};

typedef struct DefaultResponderPropertyData
{
} DefaultResponderPropertyData;

class ControllerDefaultResponder
{
public:
  bool Get(uint16_t pid, const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
           uint16_t &nack_reason);

  bool GetIdentifyDevice(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                         uint16_t &nack_reason);
  bool GetDeviceLabel(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                      uint16_t &nack_reason);
  bool GetComponentScope(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                         uint16_t &nack_reason);
  bool GetComponentScope(uint16_t slot, std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason);
  bool GetSearchDomain(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                       uint16_t &nack_reason);
  bool GetTCPCommsStatus(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                         uint16_t &nack_reason);
  bool GetSupportedParameters(const uint8_t *param_data, uint8_t param_data_len,
                              std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason);
  bool GetDeviceInfo(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                     uint16_t &nack_reason);
  bool GetManufacturerLabel(const uint8_t *param_data, uint8_t param_data_len,
                            std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason);
  bool GetDeviceModelDescription(const uint8_t *param_data, uint8_t param_data_len,
                                 std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason);
  bool GetSoftwareVersionLabel(const uint8_t *param_data, uint8_t param_data_len,
                               std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason);
  bool GetEndpointList(const uint8_t *param_data, uint8_t param_data_len, std::vector<RdmParamData> &resp_data_list,
                       uint16_t &nack_reason);
  bool GetEndpointResponders(const uint8_t *param_data, uint8_t param_data_len,
                             std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason);

private:
  // Property data
  uint32_t endpoint_list_change_number;
  bool identifying;
  std::string device_label;
  std::string search_domain;
  uint16_t tcp_unhealthy_counter;
};
