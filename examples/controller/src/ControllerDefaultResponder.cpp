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
#include "ControllerDefaultResponder.h"

#include <algorithm>
#include <iterator>
#include <cassert>
#include "etcpal/pack.h"
#include "rdm/defs.h"
#include "rdmnet/defs.h"
#include "ControllerUtils.h"

static_assert(sizeof(kMyDeviceLabel) <= 33, "Defined Device Label is too long for RDM's requirements.");
static_assert(sizeof(kMyManufacturerLabel) <= 33, "Defined Manufacturer Label is too long for RDM's requirements.");
static_assert(sizeof(kMyDeviceModelDescription) <= 33,
              "Defined Device Model Description is too long for RDM's requirements.");
static_assert(sizeof(kMySoftwareVersionLabel) <= 33,
              "Defined Software Version Label is too long for RDM's requirements.");

/* clang-format off */
const std::vector<uint16_t> ControllerDefaultResponder::supported_parameters_ = {
  E120_IDENTIFY_DEVICE,
  E120_SUPPORTED_PARAMETERS,
  E120_DEVICE_INFO,
  E120_MANUFACTURER_LABEL,
  E120_DEVICE_MODEL_DESCRIPTION,
  E120_SOFTWARE_VERSION_LABEL,
  E120_DEVICE_LABEL,
  E133_COMPONENT_SCOPE,
  E133_SEARCH_DOMAIN,
  E133_TCP_COMMS_STATUS
};

const std::vector<uint8_t> ControllerDefaultResponder::device_info_ = {
  0x01, 0x00, /* RDM Protocol version */
  0xe1, 0x33, /* Device Model ID */
  0xe1, 0x33, /* Product Category */

  /* Software Version ID */
  RDMNET_VERSION_MAJOR, RDMNET_VERSION_MINOR,
  RDMNET_VERSION_PATCH, RDMNET_VERSION_BUILD,

  0x00, 0x00, /* DMX512 Footprint */
  0x00, 0x00, /* DMX512 Personality */
  0xff, 0xff, /* DMX512 Start Address */
  0x00, 0x00, /* Sub-device count */
  0x00 /* Sensor count */
};
/* clang-format on */

extern "C" {

/* RESPONDER HANDLERS */
// static resp_process_result_t default_responder_supported_params(PidHandlerData* data)
//{
//  return kRespNoSend;
//}

static resp_process_result_t default_responder_parameter_description(PidHandlerData* data)
{
  return kRespNoSend; // TODO: Not yet implemented
}

static resp_process_result_t default_responder_device_model_description(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

// static resp_process_result_t default_responder_manufacturer_label(PidHandlerData* data)
//{
//  return kRespNoSend;
//}

static resp_process_result_t default_responder_device_label(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

static resp_process_result_t default_responder_software_version_label(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

static resp_process_result_t default_responder_identify_device(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

static resp_process_result_t default_responder_component_scope(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

static resp_process_result_t default_responder_search_domain(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

static resp_process_result_t default_responder_tcp_comms_status(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

static uint8_t default_responder_get_message_count()
{
  return 0;  // TODO: Not yet implemented
}

static void default_responder_get_next_queue_message(GetNextQueueMessageData* data)
{
  // TODO: Not yet implemented
}
}  // extern "C"

void ControllerDefaultResponder::InitResponder()
{
  RdmPidHandlerEntry handler_array[CONTROLLER_HANDLER_ARRAY_SIZE] = {
      //{E120_SUPPORTED_PARAMETERS, default_responder_supported_params, RDM_PS_ALL | RDM_PS_GET},
      {E120_PARAMETER_DESCRIPTION, default_responder_parameter_description, RDM_PS_ROOT | RDM_PS_GET},
      {E120_DEVICE_MODEL_DESCRIPTION, default_responder_device_model_description,
       RDM_PS_ALL | RDM_PS_GET | RDM_PS_SHOWSUP},
      //{E120_MANUFACTURER_LABEL, default_responder_manufacturer_label, RDM_PS_ALL | RDM_PS_GET | RDM_PS_SHOWSUP},
      {E120_DEVICE_LABEL, default_responder_device_label, RDM_PS_ALL | RDM_PS_GET_SET | RDM_PS_SHOWSUP},
      {E120_SOFTWARE_VERSION_LABEL, default_responder_software_version_label, RDM_PS_ROOT | RDM_PS_GET},
      {E120_IDENTIFY_DEVICE, default_responder_identify_device, RDM_PS_ALL | RDM_PS_GET_SET},
      {E133_COMPONENT_SCOPE, default_responder_component_scope, RDM_PS_ROOT | RDM_PS_GET_SET | RDM_PS_SHOWSUP},
      {E133_SEARCH_DOMAIN, default_responder_search_domain, RDM_PS_ROOT | RDM_PS_GET_SET | RDM_PS_SHOWSUP},
      {E133_TCP_COMMS_STATUS, default_responder_tcp_comms_status, RDM_PS_ROOT | RDM_PS_GET_SET | RDM_PS_SHOWSUP}};

  rdm_responder_state_.port_number = 0;
  rdm_responder_state_.number_of_subdevices = 0;
  rdm_responder_state_.responder_type = kRespTypeController;
  rdm_responder_state_.callback_context = this;
  memcpy(handler_array_, handler_array, CONTROLLER_HANDLER_ARRAY_SIZE * sizeof(RdmPidHandlerEntry));
  rdm_responder_state_.handler_array = handler_array_;
  rdm_responder_state_.handler_array_size = CONTROLLER_HANDLER_ARRAY_SIZE;
  rdm_responder_state_.get_message_count = default_responder_get_message_count;
  rdm_responder_state_.get_next_queue_message = default_responder_get_next_queue_message;

  rdmresp_sort_handler_array(handler_array_, CONTROLLER_HANDLER_ARRAY_SIZE);
  assert(rdmresp_validate_state(&rdm_responder_state_));
}

resp_process_result_t ControllerDefaultResponder::ProcessCommand(const std::string& scope, const RdmCommand& pcmd,
                                                                 RdmResponse& presp)
{
  auto scope_entry = scopes_.find(scope);
  if (scope_entry != scopes_.end())
  {
    rdm_responder_state_.uid = scope_entry->second.my_uid;
    return rdmresp_process_command(&rdm_responder_state_, &pcmd, &presp);
  }

  return kRespNoSend;
}

bool ControllerDefaultResponder::Get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len,
                                     std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason)
{
  switch (pid)
  {
    case E120_IDENTIFY_DEVICE:
      return GetIdentifyDevice(param_data, param_data_len, resp_data_list, nack_reason);
    case E120_DEVICE_LABEL:
      return GetDeviceLabel(param_data, param_data_len, resp_data_list, nack_reason);
    case E133_COMPONENT_SCOPE:
      return GetComponentScope(param_data, param_data_len, resp_data_list, nack_reason);
    case E133_SEARCH_DOMAIN:
      return GetSearchDomain(param_data, param_data_len, resp_data_list, nack_reason);
    case E133_TCP_COMMS_STATUS:
      return GetTCPCommsStatus(param_data, param_data_len, resp_data_list, nack_reason);
    case E120_SUPPORTED_PARAMETERS:
      return GetSupportedParameters(param_data, param_data_len, resp_data_list, nack_reason);
    case E120_DEVICE_INFO:
      return GetDeviceInfo(param_data, param_data_len, resp_data_list, nack_reason);
    case E120_MANUFACTURER_LABEL:
      return GetManufacturerLabel(param_data, param_data_len, resp_data_list, nack_reason);
    case E120_DEVICE_MODEL_DESCRIPTION:
      return GetDeviceModelDescription(param_data, param_data_len, resp_data_list, nack_reason);
    case E120_SOFTWARE_VERSION_LABEL:
      return GetSoftwareVersionLabel(param_data, param_data_len, resp_data_list, nack_reason);
    default:
      nack_reason = E120_NR_UNKNOWN_PID;
      return false;
  }
}

bool ControllerDefaultResponder::GetIdentifyDevice(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                   std::vector<RdmParamData>& resp_data_list,
                                                   uint16_t& /*nack_reason*/) const
{
  RdmParamData resp_data;
  resp_data.data[0] = identifying_ ? 1 : 0;
  resp_data.datalen = 1;
  resp_data_list.push_back(resp_data);
  return true;
}

bool ControllerDefaultResponder::GetDeviceLabel(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                std::vector<RdmParamData>& resp_data_list,
                                                uint16_t& /*nack_reason*/) const
{
  etcpal::ReadGuard prop_read(prop_lock_);

  size_t label_len = std::min(device_label_.length(), kRdmDeviceLabelMaxLength);
  RdmParamData resp_data;
  memcpy(resp_data.data, device_label_.c_str(), label_len);
  resp_data.datalen = static_cast<uint8_t>(label_len);
  resp_data_list.push_back(resp_data);
  return true;
}

bool ControllerDefaultResponder::GetComponentScope(const uint8_t* param_data, uint8_t param_data_len,
                                                   std::vector<RdmParamData>& resp_data_list,
                                                   uint16_t& nack_reason) const
{
  if (param_data_len >= 2)
  {
    return GetComponentScope(etcpal_upack_16b(param_data), resp_data_list, nack_reason);
  }
  else
  {
    nack_reason = E120_NR_FORMAT_ERROR;
    return false;
  }
}

bool ControllerDefaultResponder::GetComponentScope(uint16_t slot, std::vector<RdmParamData>& resp_data_list,
                                                   uint16_t& nack_reason) const
{
  if (slot != 0)
  {
    etcpal::ReadGuard prop_read(prop_lock_);

    if (slot - 1u < scopes_.size())
    {
      auto scopeIter = scopes_.begin();
      if (scopeIter != scopes_.end())
      {
        std::advance(scopeIter, slot - 1);

        RdmParamData resp_data;

        // Build the parameter data of the COMPONENT_SCOPE response.

        // Scope slot
        uint8_t* cur_ptr = resp_data.data;
        etcpal_pack_16b(cur_ptr, slot);
        cur_ptr += 2;

        // Scope string
        const std::string& scope_str = scopeIter->first;
        strncpy((char*)cur_ptr, scope_str.c_str(), E133_SCOPE_STRING_PADDED_LENGTH);
        cur_ptr[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
        cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

        // Static configuration
        if (scopeIter->second.static_broker.valid)
        {
          const EtcPalSockaddr& saddr = scopeIter->second.static_broker.addr;
          if (ETCPAL_IP_IS_V4(&saddr.ip))
          {
            *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
            etcpal_pack_32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&saddr.ip));
            cur_ptr += 4;
            // Skip the IPv6 field
            cur_ptr += 16;
            etcpal_pack_16b(cur_ptr, saddr.port);
            cur_ptr += 2;
          }
          else if (ETCPAL_IP_IS_V6(&saddr.ip))
          {
            *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
            // Skip the IPv4 field
            cur_ptr += 4;
            memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&saddr.ip), ETCPAL_IPV6_BYTES);
            cur_ptr += ETCPAL_IPV6_BYTES;
            etcpal_pack_16b(cur_ptr, saddr.port);
            cur_ptr += 2;
          }
        }
        else
        {
          *cur_ptr++ = E133_NO_STATIC_CONFIG;
          // Skip the IPv4, IPv6 and port fields
          cur_ptr += 4 + 16 + 2;
        }
        resp_data.datalen = static_cast<uint8_t>(cur_ptr - resp_data.data);
        resp_data_list.push_back(resp_data);
        return true;
      }
      else
      {
        nack_reason = E120_NR_DATA_OUT_OF_RANGE;
      }
    }
    else
    {
      nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }
  }
  else
  {
    nack_reason = E120_NR_DATA_OUT_OF_RANGE;
  }
  return false;
}

bool ControllerDefaultResponder::GetSearchDomain(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                 std::vector<RdmParamData>& resp_data_list,
                                                 uint16_t& /*nack_reason*/) const
{
  etcpal::ReadGuard prop_read(prop_lock_);

  RdmParamData resp_data;
  strncpy((char*)resp_data.data, search_domain_.c_str(), E133_DOMAIN_STRING_PADDED_LENGTH);
  resp_data.datalen = static_cast<uint8_t>(search_domain_.length());
  resp_data_list.push_back(resp_data);
  return true;
}

bool ControllerDefaultResponder::GetTCPCommsStatus(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                   std::vector<RdmParamData>& resp_data_list,
                                                   uint16_t& /*nack_reason*/) const
{
  etcpal::ReadGuard prop_read(prop_lock_);

  for (const auto& scope_pair : scopes_)
  {
    RdmParamData resp_data;
    uint8_t* cur_ptr = resp_data.data;

    const std::string& scope_str = scope_pair.first;
    memset(cur_ptr, 0, E133_SCOPE_STRING_PADDED_LENGTH);
    memcpy(cur_ptr, scope_str.data(), std::min<size_t>(scope_str.length(), E133_SCOPE_STRING_PADDED_LENGTH));
    cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

    const ControllerScopeData& scope_data = scope_pair.second;
    if (!scope_data.connected)
    {
      etcpal_pack_32b(cur_ptr, 0);
      cur_ptr += 4;
      memset(cur_ptr, 0, ETCPAL_IPV6_BYTES);
      cur_ptr += ETCPAL_IPV6_BYTES;
      etcpal_pack_16b(cur_ptr, 0);
      cur_ptr += 2;
    }
    else
    {
      if (ETCPAL_IP_IS_V4(&scope_data.current_broker.ip))
      {
        etcpal_pack_32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&scope_data.current_broker.ip));
        cur_ptr += 4;
        memset(cur_ptr, 0, ETCPAL_IPV6_BYTES);
        cur_ptr += ETCPAL_IPV6_BYTES;
      }
      else  // IPv6
      {
        etcpal_pack_32b(cur_ptr, 0);
        cur_ptr += 4;
        memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&scope_data.current_broker.ip), ETCPAL_IPV6_BYTES);
        cur_ptr += ETCPAL_IPV6_BYTES;
      }
      etcpal_pack_16b(cur_ptr, scope_data.current_broker.port);
      cur_ptr += 2;
    }
    etcpal_pack_16b(cur_ptr, scope_data.unhealthy_tcp_events);
    cur_ptr += 2;
    resp_data.datalen = (uint8_t)(cur_ptr - resp_data.data);
    resp_data_list.push_back(resp_data);
  }
  return true;
}

bool ControllerDefaultResponder::GetSupportedParameters(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                        std::vector<RdmParamData>& resp_data_list,
                                                        uint16_t& /*nack_reason*/) const
{
  RdmParamData resp_data;
  uint8_t* cur_ptr = resp_data.data;

  for (uint16_t param : supported_parameters_)
  {
    etcpal_pack_16b(cur_ptr, param);
    cur_ptr += 2;
    if ((cur_ptr - resp_data.data) >= RDM_MAX_PDL - 1)
    {
      resp_data.datalen = (uint8_t)(cur_ptr - resp_data.data);
      resp_data_list.push_back(resp_data);
      cur_ptr = resp_data.data;
    }
  }
  resp_data.datalen = (uint8_t)(cur_ptr - resp_data.data);
  resp_data_list.push_back(resp_data);
  return true;
}

bool ControllerDefaultResponder::GetDeviceInfo(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                               std::vector<RdmParamData>& resp_data_list,
                                               uint16_t& /*nack_reason*/) const
{
  RdmParamData resp_data;
  memcpy(resp_data.data, device_info_.data(), device_info_.size());
  resp_data.datalen = static_cast<uint8_t>(device_info_.size());
  resp_data_list.push_back(resp_data);
  return true;
}

bool ControllerDefaultResponder::GetManufacturerLabel(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                      std::vector<RdmParamData>& resp_data_list,
                                                      uint16_t& /*nack_reason*/) const
{
  RdmParamData resp_data;
  strcpy((char*)resp_data.data, manufacturer_label_.c_str());
  resp_data.datalen = static_cast<uint8_t>(manufacturer_label_.length());
  resp_data_list.push_back(resp_data);
  return true;
}

bool ControllerDefaultResponder::GetDeviceModelDescription(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                           std::vector<RdmParamData>& resp_data_list,
                                                           uint16_t& /*nack_reason*/) const
{
  RdmParamData resp_data;
  strcpy((char*)resp_data.data, device_model_description_.c_str());
  resp_data.datalen = static_cast<uint8_t>(device_model_description_.length());
  resp_data_list.push_back(resp_data);
  return true;
}

bool ControllerDefaultResponder::GetSoftwareVersionLabel(const uint8_t* /*param_data*/, uint8_t /*param_data_len*/,
                                                         std::vector<RdmParamData>& resp_data_list,
                                                         uint16_t& /*nack_reason*/) const
{
  RdmParamData resp_data;
  strcpy((char*)resp_data.data, software_version_label_.c_str());
  resp_data.datalen = static_cast<uint8_t>(software_version_label_.length());
  resp_data_list.push_back(resp_data);
  return true;
}

void ControllerDefaultResponder::UpdateSearchDomain(const std::string& new_search_domain)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  search_domain_ = new_search_domain;
}

void ControllerDefaultResponder::AddScope(const std::string& new_scope, StaticBrokerConfig static_broker)
{
  etcpal::WriteGuard prop_write(prop_lock_);

  scopes_.insert(std::make_pair(new_scope, ControllerScopeData(static_broker)));
}

void ControllerDefaultResponder::RemoveScope(const std::string& scope_to_remove)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  scopes_.erase(scope_to_remove);
}

void ControllerDefaultResponder::UpdateScopeConnectionStatus(const std::string& scope, bool connected,
                                                             const EtcPalSockaddr& broker_addr,
                                                             const RdmUid& controller_uid)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  auto scope_entry = scopes_.find(scope);
  if (scope_entry != scopes_.end())
  {
    scope_entry->second.connected = connected;
    if (connected)
    {
      scope_entry->second.current_broker = broker_addr;
      scope_entry->second.my_uid = controller_uid;
    }
  }
}

void ControllerDefaultResponder::IncrementTcpUnhealthyCounter(const std::string& scope)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  auto scope_entry = scopes_.find(scope);
  if (scope_entry != scopes_.end())
  {
    ++scope_entry->second.unhealthy_tcp_events;
  }
}

void ControllerDefaultResponder::ResetTcpUnhealthyCounter(const std::string& scope)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  auto scope_entry = scopes_.find(scope);
  if (scope_entry != scopes_.end())
  {
    scope_entry->second.unhealthy_tcp_events = 0;
  }
}
