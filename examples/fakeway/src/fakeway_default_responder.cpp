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

#include "fakeway_default_responder.h"

#include "etcpal/pack.h"
#include "rdmnet/defs.h"
#include "rdm/defs.h"
#include "rdmnet/version.h"

/**************************** Private constants ******************************/

// clang-format off
const uint8_t FakewayDefaultResponder::kDeviceInfo[] = {
    0x01, 0x00, // RDM Protocol version
    0xe1, 0x34, // Device Model ID
    0xe1, 0x33, // Product Category

    // Software Version ID
    RDMNET_VERSION_MAJOR, RDMNET_VERSION_MINOR,
    RDMNET_VERSION_PATCH, RDMNET_VERSION_BUILD,

    0x00, 0x00, // DMX512 Footprint
    0x00, 0x00, // DMX512 Personality
    0xff, 0xff, // DMX512 Start Address
    0x00, 0x00, // Sub-device count
    0x00 // Sensor count
};
// clang-format on

#define DEVICE_LABEL_MAX_LEN 32

/*************************** Function definitions ****************************/

FakewayDefaultResponder::FakewayDefaultResponder(const RdmnetScopeConfig& scope_config,
                                                 const std::string& search_domain)
    : scope_config_(scope_config), search_domain_(search_domain)
{
  supported_pid_list_.insert(E120_IDENTIFY_DEVICE);
  supported_pid_list_.insert(E120_SUPPORTED_PARAMETERS);
  supported_pid_list_.insert(E120_DEVICE_INFO);
  supported_pid_list_.insert(E120_MANUFACTURER_LABEL);
  supported_pid_list_.insert(E120_DEVICE_MODEL_DESCRIPTION);
  supported_pid_list_.insert(E120_SOFTWARE_VERSION_LABEL);
  supported_pid_list_.insert(E120_DEVICE_LABEL);
  supported_pid_list_.insert(E133_COMPONENT_SCOPE);
  supported_pid_list_.insert(E133_SEARCH_DOMAIN);
  supported_pid_list_.insert(E133_TCP_COMMS_STATUS);
  supported_pid_list_.insert(E137_7_ENDPOINT_LIST);
  supported_pid_list_.insert(E137_7_ENDPOINT_LIST_CHANGE);
  supported_pid_list_.insert(E137_7_ENDPOINT_RESPONDERS);
  supported_pid_list_.insert(E137_7_ENDPOINT_RESPONDER_LIST_CHANGE);
  supported_pid_list_.insert(E137_7_ENDPOINT_LIST);
  supported_pid_list_.insert(E137_7_ENDPOINT_LIST_CHANGE);
  supported_pid_list_.insert(E137_7_ENDPOINT_RESPONDERS);
  supported_pid_list_.insert(E137_7_ENDPOINT_RESPONDER_LIST_CHANGE);
}

FakewayDefaultResponder::~FakewayDefaultResponder()
{
  if (identifying_)
  {
    identifying_ = false;
    identify_thread_.Join();
  }
}

void FakewayDefaultResponder::IncrementTcpUnhealthyCounter()
{
  etcpal::WriteGuard prop_write(prop_lock_);
  ++tcp_unhealthy_count_;
}

void FakewayDefaultResponder::UpdateConnectionStatus(bool connected, const etcpal::SockAddr& broker_addr)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  connected_ = connected;
  cur_broker_addr_ = broker_addr;
}

void FakewayDefaultResponder::AddEndpoints(const std::vector<uint16_t>& new_endpoints)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  for (const auto& endpoint : new_endpoints)
  {
    endpoints_.insert(std::make_pair(endpoint, EndpointInfo()));
  }
  ++endpoint_change_number_;
}

void FakewayDefaultResponder::RemoveEndpoints(const std::vector<uint16_t>& endpoints_to_remove)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  for (const auto& endpoint : endpoints_to_remove)
  {
    endpoints_.erase(endpoint);
  }
  ++endpoint_change_number_;
}

void FakewayDefaultResponder::AddResponderOnEndpoint(uint16_t endpoint, const RdmUid& responder)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  auto endpoint_pair = endpoints_.find(endpoint);
  if (endpoint_pair != endpoints_.end())
  {
    endpoint_pair->second.responders.insert(responder);
    ++endpoint_pair->second.responder_change_number;
  }
}

void FakewayDefaultResponder::RemoveResponderOnEndpoint(uint16_t endpoint, const RdmUid& responder)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  auto endpoint_pair = endpoints_.find(endpoint);
  if (endpoint_pair != endpoints_.end())
  {
    endpoint_pair->second.responders.erase(responder);
    ++endpoint_pair->second.responder_change_number;
  }
}

bool FakewayDefaultResponder::FakewayDefaultResponder::Set(uint16_t pid, const uint8_t* param_data,
                                                           uint8_t param_data_len, uint16_t& nack_reason,
                                                           RdmnetConfigChange& config_change)
{
  bool res = false;
  config_change = RdmnetConfigChange::kNoChange;

  etcpal::WriteGuard prop_write(prop_lock_);
  switch (pid)
  {
    case E120_IDENTIFY_DEVICE:
      res = SetIdentifyDevice(param_data, param_data_len, nack_reason, config_change);
      break;
    case E120_DEVICE_LABEL:
      res = SetDeviceLabel(param_data, param_data_len, nack_reason, config_change);
      break;
    case E133_COMPONENT_SCOPE:
      res = SetComponentScope(param_data, param_data_len, nack_reason, config_change);
      break;
    case E133_SEARCH_DOMAIN:
      res = SetSearchDomain(param_data, param_data_len, nack_reason, config_change);
      break;
    case E133_TCP_COMMS_STATUS:
      res = SetTcpCommsStatus(param_data, param_data_len, nack_reason, config_change);
      break;
    case E120_DEVICE_INFO:
    case E120_SUPPORTED_PARAMETERS:
    case E120_MANUFACTURER_LABEL:
    case E120_DEVICE_MODEL_DESCRIPTION:
    case E120_SOFTWARE_VERSION_LABEL:
    case E137_7_ENDPOINT_LIST:
    case E137_7_ENDPOINT_RESPONDERS:
      nack_reason = E120_NR_UNSUPPORTED_COMMAND_CLASS;
      break;
    default:
      nack_reason = E120_NR_UNKNOWN_PID;
      break;
  }
  return res;
}

bool FakewayDefaultResponder::Get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len,
                                  ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  bool res = false;

  etcpal::ReadGuard prop_read(prop_lock_);
  switch (pid)
  {
    case E120_IDENTIFY_DEVICE:
      res = GetIdentifyDevice(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E120_DEVICE_INFO:
      res = GetDeviceInfo(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E120_DEVICE_LABEL:
      res = GetDeviceLabel(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E133_COMPONENT_SCOPE:
      res = GetComponentScope(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E133_SEARCH_DOMAIN:
      res = GetSearchDomain(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E133_TCP_COMMS_STATUS:
      res = GetTcpCommsStatus(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E120_SUPPORTED_PARAMETERS:
      res = GetSupportedParameters(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E120_MANUFACTURER_LABEL:
      res = GetManufacturerLabel(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E120_DEVICE_MODEL_DESCRIPTION:
      res = GetDeviceModelDescription(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E120_SOFTWARE_VERSION_LABEL:
      res = GetSoftwareVersionLabel(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E137_7_ENDPOINT_LIST:
      res = GetEndpointList(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E137_7_ENDPOINT_LIST_CHANGE:
      res = GetEndpointListChange(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E137_7_ENDPOINT_RESPONDERS:
      res = GetEndpointResponders(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    case E137_7_ENDPOINT_RESPONDER_LIST_CHANGE:
      res = GetEndpointResponderListChange(param_data, param_data_len, resp_data_list, nack_reason);
      break;
    default:
      nack_reason = E120_NR_UNKNOWN_PID;
      break;
  }
  return res;
}

void FakewayDefaultResponder::IdentifyThread()
{
  while (identifying_)
  {
    Beep(440, 1000);
    etcpal_thread_sleep(1000);
  }
}

bool FakewayDefaultResponder::SetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len,
                                                uint16_t& nack_reason, RdmnetConfigChange& /*config_change*/)
{
  if (param_data_len >= 1)
  {
    bool new_identify_setting = (*param_data != 0);
    if (new_identify_setting && !identifying_)
    {
      identify_thread_.SetName("Identify Thread").Start(&FakewayDefaultResponder::IdentifyThread, this);
    }
    identifying_ = new_identify_setting;
    return true;
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
    return false;
  }
}

bool FakewayDefaultResponder::SetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len, uint16_t& nack_reason,
                                             RdmnetConfigChange& /*config_change*/)
{
  if (param_data_len >= 1)
  {
    if (param_data_len > DEVICE_LABEL_MAX_LEN)
      param_data_len = DEVICE_LABEL_MAX_LEN;
    device_label_.assign(reinterpret_cast<const char*>(param_data), param_data_len);
    return true;
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
    return false;
  }
}

bool FakewayDefaultResponder::SetComponentScope(const uint8_t* param_data, uint8_t param_data_len,
                                                uint16_t& nack_reason, RdmnetConfigChange& config_change)
{
  if (param_data_len == (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2) &&
      param_data[1 + E133_SCOPE_STRING_PADDED_LENGTH] == '\0')
  {
    if (etcpal_unpack_u16b(param_data) == 1)
    {
      const uint8_t* cur_ptr = param_data + 2;
      RdmnetScopeConfig new_scope_config{};

      rdmnet_safe_strncpy(new_scope_config.scope, reinterpret_cast<const char*>(cur_ptr),
                          E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      switch (*cur_ptr++)
      {
        case E133_STATIC_CONFIG_IPV4:
          ETCPAL_IP_SET_V4_ADDRESS(&new_scope_config.static_broker_addr.ip, etcpal_unpack_u32b(cur_ptr));
          cur_ptr += 4 + 16;
          new_scope_config.static_broker_addr.port = etcpal_unpack_u16b(cur_ptr);
          new_scope_config.has_static_broker_addr = true;
          break;
        case E133_STATIC_CONFIG_IPV6:
          cur_ptr += 4;
          ETCPAL_IP_SET_V6_ADDRESS(&new_scope_config.static_broker_addr.ip, cur_ptr);
          cur_ptr += 16;
          new_scope_config.static_broker_addr.port = etcpal_unpack_u16b(cur_ptr);
          new_scope_config.has_static_broker_addr = true;
          break;
        case E133_NO_STATIC_CONFIG:
        default:
          break;
      }

      if (strncmp(new_scope_config.scope, scope_config_.scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0 &&
          ((!new_scope_config.has_static_broker_addr && !scope_config_.has_static_broker_addr) ||
           (new_scope_config.static_broker_addr == scope_config_.static_broker_addr)))
      {
        // Same settings as current
        config_change = RdmnetConfigChange::kNoChange;
      }
      else
      {
        scope_config_ = new_scope_config;
        config_change = RdmnetConfigChange::kScopeConfigChanged;
      }
      return true;
    }
    else
    {
      nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
  }
  return false;
}

bool FakewayDefaultResponder::SetSearchDomain(const uint8_t* param_data, uint8_t param_data_len, uint16_t& nack_reason,
                                              RdmnetConfigChange& config_change)
{
  if (param_data_len > 0 && param_data_len < E133_DOMAIN_STRING_PADDED_LENGTH)
  {
    search_domain_.assign(reinterpret_cast<const char*>(param_data), param_data_len);
    config_change = RdmnetConfigChange::kSearchDomainChanged;
    return true;
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
  }
  return false;
}

bool FakewayDefaultResponder::SetTcpCommsStatus(const uint8_t* param_data, uint8_t param_data_len,
                                                uint16_t& nack_reason, RdmnetConfigChange& /*config_change*/)
{
  if (param_data_len == E133_SCOPE_STRING_PADDED_LENGTH && param_data[E133_SCOPE_STRING_PADDED_LENGTH - 1] == '\0')
  {
    if (0 == strcmp(scope_config_.scope, reinterpret_cast<const char*>(param_data)))
    {
      tcp_unhealthy_count_ = 0;
      return true;
    }
    else
    {
      nack_reason = E133_NR_UNKNOWN_SCOPE;
    }
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
  }
  return false;
}

bool FakewayDefaultResponder::GetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len,
                                                ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  RdmParamData pd;
  pd.data[0] = identifying_ ? 1 : 0;
  pd.datalen = 1;
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetDeviceInfo(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                            ParamDataList& resp_data_list, uint16_t& /*nack_reason*/)
{
  RdmParamData pd;
  memcpy(pd.data, kDeviceInfo, sizeof kDeviceInfo);
  pd.datalen = sizeof kDeviceInfo;
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len,
                                             ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  RdmParamData pd;
  pd.datalen = static_cast<uint8_t>(device_label_.length() > DEVICE_LABEL_MAX_LEN ? DEVICE_LABEL_MAX_LEN
                                                                                  : device_label_.length());
  memcpy(pd.data, device_label_.c_str(), pd.datalen);
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetComponentScope(const uint8_t* param_data, uint8_t param_data_len,
                                                ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  if (param_data_len >= 2)
  {
    if (etcpal_unpack_u16b(param_data) == 1)
    {
      RdmParamData pd;

      // Pack the scope
      uint8_t* cur_ptr = pd.data;
      etcpal_pack_u16b(cur_ptr, 1);
      cur_ptr += 2;
      rdmnet_safe_strncpy((char*)cur_ptr, scope_config_.scope, E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      // Pack the static config data
      if (scope_config_.has_static_broker_addr)
      {
        if (ETCPAL_IP_IS_V4(&scope_config_.static_broker_addr.ip))
        {
          *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
          etcpal_pack_u32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&scope_config_.static_broker_addr.ip));
          cur_ptr += 4 + 16;
          etcpal_pack_u16b(cur_ptr, scope_config_.static_broker_addr.port);
          cur_ptr += 2;
        }
        else  // V6
        {
          *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
          cur_ptr += 4;
          memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&scope_config_.static_broker_addr.ip), 16);
          cur_ptr += 16;
          etcpal_pack_u16b(cur_ptr, scope_config_.static_broker_addr.port);
          cur_ptr += 2;
        }
      }
      else
      {
        *cur_ptr++ = E133_NO_STATIC_CONFIG;
        memset(cur_ptr, 0, 4 + 16 + 2);
        cur_ptr += 4 + 16 + 2;
      }
      pd.datalen = (uint8_t)(cur_ptr - pd.data);
      resp_data_list.push_back(pd);
      return true;
    }
    else
    {
      nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
  }
  return false;
}

bool FakewayDefaultResponder::GetSearchDomain(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                              ParamDataList& resp_data_list, uint16_t& /*nack_reason*/)
{
  RdmParamData pd;
  rdmnet_safe_strncpy(reinterpret_cast<char*>(pd.data), search_domain_.c_str(), E133_DOMAIN_STRING_PADDED_LENGTH);
  pd.datalen = static_cast<uint8_t>(search_domain_.length());
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetTcpCommsStatus(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                ParamDataList& resp_data_list, uint16_t& /*nack_reason*/)
{
  RdmParamData pd;
  uint8_t* cur_ptr = pd.data;

  rdmnet_safe_strncpy(reinterpret_cast<char*>(pd.data), scope_config_.scope, E133_SCOPE_STRING_PADDED_LENGTH);
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  if (cur_broker_addr_.ip().IsV4())
  {
    etcpal_pack_u32b(cur_ptr, cur_broker_addr_.ip().v4_data());
    cur_ptr += 4;
    memset(cur_ptr, 0, ETCPAL_IPV6_BYTES);
    cur_ptr += ETCPAL_IPV6_BYTES;
  }
  else
  {
    etcpal_pack_u32b(cur_ptr, 0);
    cur_ptr += 4;
    memcpy(cur_ptr, cur_broker_addr_.ip().v6_data(), ETCPAL_IPV6_BYTES);
    cur_ptr += ETCPAL_IPV6_BYTES;
  }
  etcpal_pack_u16b(cur_ptr, cur_broker_addr_.port());
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, tcp_unhealthy_count_);
  cur_ptr += 2;
  pd.datalen = (uint8_t)(cur_ptr - pd.data);
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetSupportedParameters(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                     ParamDataList& resp_data_list, uint16_t& /*nack_reason*/)
{
  RdmParamData pd;
  uint8_t* cur_ptr = pd.data;

  for (auto& pid : supported_pid_list_)
  {
    etcpal_pack_u16b(cur_ptr, pid);
    cur_ptr += 2;
    if ((cur_ptr - pd.data) >= RDM_MAX_PDL - 1)
    {
      pd.datalen = (uint8_t)(cur_ptr - pd.data);
      resp_data_list.push_back(pd);
      cur_ptr = pd.data;
    }
  }
  pd.datalen = (uint8_t)(cur_ptr - pd.data);
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetManufacturerLabel(const uint8_t* param_data, uint8_t param_data_len,
                                                   ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  RdmParamData pd;
  RDMNET_MSVC_NO_DEP_WRN strcpy((char*)pd.data, MANUFACTURER_LABEL);
  pd.datalen = sizeof(MANUFACTURER_LABEL) - 1;
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetDeviceModelDescription(const uint8_t* param_data, uint8_t param_data_len,
                                                        ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  RdmParamData pd;
  RDMNET_MSVC_NO_DEP_WRN strcpy((char*)pd.data, DEVICE_MODEL_DESCRIPTION);
  pd.datalen = sizeof(DEVICE_MODEL_DESCRIPTION) - 1;
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetSoftwareVersionLabel(const uint8_t* param_data, uint8_t param_data_len,
                                                      ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  RdmParamData pd;
  RDMNET_MSVC_NO_DEP_WRN strcpy((char*)pd.data, SOFTWARE_VERSION_LABEL);
  pd.datalen = sizeof(SOFTWARE_VERSION_LABEL) - 1;
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetEndpointList(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                              ParamDataList& resp_data_list, uint16_t& /*nack_reason*/)
{
  RdmParamData pd;
  uint8_t* cur_ptr = pd.data;

  etcpal_pack_u32b(cur_ptr, endpoint_change_number_);
  cur_ptr += 4;

  for (const auto& endpoint : endpoints_)
  {
    etcpal_pack_u16b(cur_ptr, endpoint.first);
    cur_ptr += 2;
    *cur_ptr++ = E137_7_ENDPOINT_TYPE_PHYSICAL;
    if ((cur_ptr - pd.data) >= RDM_MAX_PDL - 2)
    {
      pd.datalen = (uint8_t)(cur_ptr - pd.data);
      resp_data_list.push_back(pd);
      cur_ptr = pd.data;
    }
  }
  pd.datalen = (uint8_t)(cur_ptr - pd.data);
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetEndpointListChange(const uint8_t* param_data, uint8_t param_data_len,
                                                    FakewayDefaultResponder::ParamDataList& resp_data_list,
                                                    uint16_t& nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  RdmParamData pd;
  etcpal_pack_u32b(pd.data, endpoint_change_number_);
  pd.datalen = 4;
  resp_data_list.push_back(pd);
  return true;
}

bool FakewayDefaultResponder::GetEndpointResponders(const uint8_t* param_data, uint8_t param_data_len,
                                                    ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  if (param_data_len >= 2)
  {
    uint16_t endpoint_id = etcpal_unpack_u16b(param_data);
    const auto endpt_pair = endpoints_.find(endpoint_id);
    if (endpt_pair != endpoints_.end())
    {
      RdmParamData pd;
      uint8_t* cur_ptr = pd.data;

      etcpal_pack_u16b(cur_ptr, endpoint_id);
      cur_ptr += 2;
      etcpal_pack_u32b(cur_ptr, endpt_pair->second.responder_change_number);
      cur_ptr += 4;
      for (const auto& responder : endpt_pair->second.responders)
      {
        etcpal_pack_u16b(cur_ptr, responder.manu);
        cur_ptr += 2;
        etcpal_pack_u32b(cur_ptr, responder.id);
        cur_ptr += 4;
        if ((cur_ptr - pd.data) >= RDM_MAX_PDL - 5)
        {
          pd.datalen = (uint8_t)(cur_ptr - pd.data);
          resp_data_list.push_back(pd);
          cur_ptr = pd.data;
        }
      }
      pd.datalen = (uint8_t)(cur_ptr - pd.data);
      resp_data_list.push_back(pd);
      return true;
    }
    else
    {
      nack_reason = E137_7_NR_ENDPOINT_NUMBER_INVALID;
      return false;
    }
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
    return false;
  }
}

bool FakewayDefaultResponder::GetEndpointResponderListChange(const uint8_t* param_data, uint8_t param_data_len,
                                                             ParamDataList& resp_data_list, uint16_t& nack_reason)
{
  if (param_data_len >= 2)
  {
    uint16_t endpoint_id = etcpal_unpack_u16b(param_data);
    auto endpt_pair = endpoints_.find(endpoint_id);
    if (endpt_pair != endpoints_.end())
    {
      RdmParamData pd;
      etcpal_pack_u16b(pd.data, endpt_pair->first);
      etcpal_pack_u32b(&pd.data[2], endpt_pair->second.responder_change_number);
      pd.datalen = 6;
      resp_data_list.push_back(pd);
      return true;
    }
    else
    {
      nack_reason = E137_7_NR_ENDPOINT_NUMBER_INVALID;
      return false;
    }
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
    return false;
  }
}
