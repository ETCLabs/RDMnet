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

#include <chrono>
#include "etcpal/common.h"
#include "etcpal/cpp/thread.h"
#include "etcpal/pack.h"
#include "rdmnet/defs.h"
#include "rdm/defs.h"
#include "rdmnet/version.h"

using namespace std::chrono_literals;

/**************************** Private constants ******************************/

// clang-format off
const uint8_t FakewayDefaultResponder::kDeviceInfo[] = {
    0x01, 0x00, // RDM Protocol version
    0xe1, 0x34, // Device Model ID
    0x71, 0x01, // Product Category

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

constexpr size_t kDeviceLabelMaxLength = 32;

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
}

FakewayDefaultResponder::~FakewayDefaultResponder()
{
  if (identifying_)
  {
    identifying_ = false;
    identify_thread_.Join();
  }
}

rdmnet::ResponseAction FakewayDefaultResponder::Set(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len)
{
  bool res = false;

  etcpal::WriteGuard prop_write(prop_lock_);
  switch (pid)
  {
    case E120_IDENTIFY_DEVICE:
      return SetIdentifyDevice(param_data, param_data_len);
    case E120_DEVICE_LABEL:
      return SetDeviceLabel(param_data, param_data_len);
    case E133_COMPONENT_SCOPE:
      return SetComponentScope(param_data, param_data_len);
    case E133_SEARCH_DOMAIN:
      return SetSearchDomain(param_data, param_data_len);
    case E120_DEVICE_INFO:
    case E120_SUPPORTED_PARAMETERS:
    case E120_MANUFACTURER_LABEL:
    case E120_DEVICE_MODEL_DESCRIPTION:
    case E120_SOFTWARE_VERSION_LABEL:
      return rdmnet::ResponseAction::SendNack(kRdmNRUnsupportedCommandClass);
    default:
      return rdmnet::ResponseAction::SendNack(kRdmNRUnknownPid);
  }
}

rdmnet::ResponseAction FakewayDefaultResponder::Get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len)
{
  bool res = false;

  etcpal::ReadGuard prop_read(prop_lock_);
  switch (pid)
  {
    case E120_IDENTIFY_DEVICE:
      return GetIdentifyDevice(param_data, param_data_len);
    case E120_DEVICE_INFO:
      return GetDeviceInfo(param_data, param_data_len);
    case E120_DEVICE_LABEL:
      return GetDeviceLabel(param_data, param_data_len);
    case E120_SUPPORTED_PARAMETERS:
      return GetSupportedParameters(param_data, param_data_len);
    case E120_MANUFACTURER_LABEL:
      return GetManufacturerLabel(param_data, param_data_len);
    case E120_DEVICE_MODEL_DESCRIPTION:
      return GetDeviceModelDescription(param_data, param_data_len);
    case E120_SOFTWARE_VERSION_LABEL:
      return GetSoftwareVersionLabel(param_data, param_data_len);
    default:
      return rdmnet::ResponseAction::SendNack(kRdmNRUnknownPid);
  }
}

void FakewayDefaultResponder::IdentifyThread()
{
  while (identifying_)
  {
    Beep(440, 1000);
    etcpal::Thread::Sleep(1s);
  }
}

rdmnet::ResponseAction FakewayDefaultResponder::SetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len)
{
  if (param_data_len >= 1)
  {
    bool new_identify_setting = (*param_data != 0);
    if (new_identify_setting && !identifying_)
    {
      identify_thread_.SetName("Identify Thread").Start(&FakewayDefaultResponder::IdentifyThread, this);
    }
    identifying_ = new_identify_setting;
    return rdmnet::ResponseAction::SendAck();
  }
  else
  {
    return rdmnet::ResponseAction::SendNack(kRdmNRFormatError);
  }
}

rdmnet::ResponseAction FakewayDefaultResponder::SetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len)
{
  if (param_data_len >= 1)
  {
    device_label_.assign(reinterpret_cast<const char*>(param_data),
                         (param_data_len > kDeviceLabelMaxLength ? kDeviceLabelMaxLength : param_data_len));
    return rdmnet::ResponseAction::SendAck();
  }
  else
  {
    return rdmnet::ResponseAction::SendNack(kRdmNRFormatError);
  }
}

rdmnet::ResponseAction FakewayDefaultResponder::SetComponentScope(const uint8_t* param_data, uint8_t param_data_len)
{
  if (param_data_len == kComponentScopeDataLength && param_data[1 + E133_SCOPE_STRING_PADDED_LENGTH] == '\0')
  {
    if (etcpal_unpack_u16b(param_data) == 1)
    {
      const uint8_t* cur_ptr = param_data + 2;

      scope_config_.SetIdString(reinterpret_cast<const char*>(cur_ptr));
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      etcpal::SockAddr static_broker;
      switch (*cur_ptr++)
      {
        case E133_STATIC_CONFIG_IPV4:
          static_broker.SetAddress(etcpal_unpack_u32b(cur_ptr));
          cur_ptr += 4 + 16;
          static_broker.SetPort(etcpal_unpack_u16b(cur_ptr));
          break;
        case E133_STATIC_CONFIG_IPV6:
          cur_ptr += 4;
          static_broker.SetAddress(cur_ptr);
          cur_ptr += 16;
          static_broker.SetPort(etcpal_unpack_u16b(cur_ptr));
          break;
        case E133_NO_STATIC_CONFIG:
        default:
          break;
      }

      scope_config_.SetStaticBrokerAddr(static_broker);
      return rdmnet::ResponseAction::SendAck();
    }
    else
    {
      return rdmnet::ResponseAction::SendNack(kRdmNRDataOutOfRange);
    }
  }
  else
  {
    return rdmnet::ResponseAction::SendNack(kRdmNRFormatError);
  }
}

rdmnet::ResponseAction FakewayDefaultResponder::SetSearchDomain(const uint8_t* param_data, uint8_t param_data_len)
{
  if (param_data_len > 0 && param_data_len < E133_DOMAIN_STRING_PADDED_LENGTH)
  {
    search_domain_.assign(reinterpret_cast<const char*>(param_data), param_data_len);
    return rdmnet::ResponseAction::SendAck();
  }
  else
  {
    return rdmnet::ResponseAction::SendNack(kRdmNRFormatError);
  }
}

rdmnet::ResponseAction FakewayDefaultResponder::GetIdentifyDevice(const uint8_t* /*param_data*/,
                                                                  uint8_t /*param_data_len*/)
{
  response_buf_[0] = identifying_ ? 1 : 0;
  return rdmnet::ResponseAction::SendAck(1);
}

rdmnet::ResponseAction FakewayDefaultResponder::GetDeviceInfo(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/)
{
  memcpy(response_buf_, kDeviceInfo, sizeof kDeviceInfo);
  return rdmnet::ResponseAction::SendAck(sizeof kDeviceInfo);
}

rdmnet::ResponseAction FakewayDefaultResponder::GetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len)
{
  (void)param_data;
  (void)param_data_len;

  size_t response_length =
      (device_label_.length() > kDeviceLabelMaxLength ? kDeviceLabelMaxLength : device_label_.length());
  memcpy(response_buf_, device_label_.c_str(), response_length);
  return rdmnet::ResponseAction::SendAck(response_length);
}

rdmnet::ResponseAction FakewayDefaultResponder::GetSupportedParameters(const uint8_t* /*param_data*/,
                                                                       uint8_t /*param_data_len*/)
{
  uint8_t* cur_ptr = response_buf_;

  for (auto& pid : supported_pid_list_)
  {
    etcpal_pack_u16b(cur_ptr, pid);
    cur_ptr += 2;
  }
  return rdmnet::ResponseAction::SendAck(cur_ptr - response_buf_);
}

rdmnet::ResponseAction FakewayDefaultResponder::GetManufacturerLabel(const uint8_t* /*param_data*/,
                                                                     uint8_t /*param_data_len*/)
{
  ETCPAL_MSVC_NO_DEP_WRN strcpy(reinterpret_cast<char*>(response_buf_), kManufacturerLabel);
  return rdmnet::ResponseAction::SendAck(sizeof(kManufacturerLabel) - 1);
}

rdmnet::ResponseAction FakewayDefaultResponder::GetDeviceModelDescription(const uint8_t* /*param_data*/,
                                                                          uint8_t /*param_data_len*/)
{
  ETCPAL_MSVC_NO_DEP_WRN strcpy(reinterpret_cast<char*>(response_buf_), kDeviceModelDescription);
  return rdmnet::ResponseAction::SendAck(sizeof(kDeviceModelDescription) - 1);
}

rdmnet::ResponseAction FakewayDefaultResponder::GetSoftwareVersionLabel(const uint8_t* /*param_data*/,
                                                                        uint8_t /*param_data_len*/)
{
  ETCPAL_MSVC_NO_DEP_WRN strcpy(reinterpret_cast<char*>(response_buf_), kSoftwareVersionLabel);
  return rdmnet::ResponseAction::SendAck(sizeof(kSoftwareVersionLabel) - 1);
}
