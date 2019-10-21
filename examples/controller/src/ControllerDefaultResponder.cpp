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
#include "rdmnet/core/util.h"
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
// static etcpal_error_t default_responder_supported_params(PidHandlerData* data)
//{
//  return kEtcPalErrNotImpl;
//}

static etcpal_error_t default_responder_parameter_description(PidHandlerData* data)
{
  // if (!rdmresp_validate_pid_handler_data(data, true)) result = kEtcPalErrInvalid; // TODO: Caller should do this

  etcpal_error_t result = kEtcPalErrNotImpl;

  RdmPdParameterDescription description;
  rdmpd_nack_reason_t nack_reason;

  uint16_t requested_pid;
  result = rdmpd_unpack_get_parameter_description(data->pd_in, &requested_pid);

  if (result == kEtcPalErrOk)
  {
    ControllerDefaultResponder* responder = static_cast<ControllerDefaultResponder*>(data->context);
    assert(responder);

    result = responder->ProcessGetParameterDescription(requested_pid, description, data->response_type, nack_reason);
  }
  else if (result == kEtcPalErrProtocol)
  {
    result = kEtcPalErrOk;
    data->response_type = kRdmRespRtNackReason;
    nack_reason = kRdmPdNrFormatError;
  }

  if (result == kEtcPalErrOk)
  {
    if (data->response_type == kRdmRespRtNackReason)
    {
      result = rdmpd_pack_nack_reason(nack_reason, data->pd_out);
    }
    else if (data->response_type != kRdmRespRtNoSend)  // Assuming no ACK timer
    {
      result = rdmpd_pack_get_resp_parameter_description(&description, data->pd_out);
    }
  }

  return result;
}

static etcpal_error_t default_responder_device_model_description(PidHandlerData* data)
{
  etcpal_error_t result = kEtcPalErrNotImpl;

  ControllerDefaultResponder* responder = static_cast<ControllerDefaultResponder*>(data->context);
  assert(responder);

  RdmPdString description;
  rdmpd_nack_reason_t nack_reason;
  result = responder->ProcessGetDeviceModelDescription(description, data->response_type, nack_reason);

  if (result == kEtcPalErrOk)
  {
    if (data->response_type == kRdmRespRtNackReason)
    {
      result = rdmpd_pack_nack_reason(nack_reason, data->pd_out);
    }
    else if (data->response_type != kRdmRespRtNoSend)  // Assuming no ACK timer
    {
      result = rdmpd_pack_get_resp_device_model_description(&description, data->pd_out);
    }
  }

  return result;
}

// static etcpal_error_t default_responder_manufacturer_label(PidHandlerData* data)
//{
//  return kEtcPalErrNotImpl;
//}

static etcpal_error_t default_responder_device_label(PidHandlerData* data)
{
  etcpal_error_t result = kEtcPalErrNotImpl;

  ControllerDefaultResponder* responder = static_cast<ControllerDefaultResponder*>(data->context);
  assert(responder);

  RdmPdString label;
  rdmpd_nack_reason_t nack_reason;

  if (data->cmd_class == kRdmCCGetCommand)
  {
    result = responder->ProcessGetDeviceLabel(label, data->response_type, nack_reason);
  }
  else  // kRdmCCSetCommand
  {
    result = rdmpd_unpack_set_device_label(data->pd_in, &label);

    if (result == kEtcPalErrOk)
    {
      result = responder->ProcessSetDeviceLabel(label, data->response_type, nack_reason);
    }
    else if (result == kEtcPalErrProtocol)
    {
      result = kEtcPalErrOk;
      data->response_type = kRdmRespRtNackReason;
      nack_reason = kRdmPdNrFormatError;
    }
  }

  if (result == kEtcPalErrOk)
  {
    if (data->response_type == kRdmRespRtNackReason)
    {
      result = rdmpd_pack_nack_reason(nack_reason, data->pd_out);
    }
    else if ((data->response_type != kRdmRespRtNoSend) &&  // Assuming no ACK timer
             (data->cmd_class == kRdmCCGetCommand))
    {
      result = rdmpd_pack_get_resp_device_label(&label, data->pd_out);
    }
  }

  return result;
}

static etcpal_error_t default_responder_software_version_label(PidHandlerData* data)
{
  etcpal_error_t result = kEtcPalErrNotImpl;

  ControllerDefaultResponder* responder = static_cast<ControllerDefaultResponder*>(data->context);
  assert(responder);

  RdmPdString label;
  rdmpd_nack_reason_t nack_reason;
  result = responder->ProcessGetSoftwareVersionLabel(label, data->response_type, nack_reason);

  if (result == kEtcPalErrOk)
  {
    if (data->response_type == kRdmRespRtNackReason)
    {
      result = rdmpd_pack_nack_reason(nack_reason, data->pd_out);
    }
    else if (data->response_type != kRdmRespRtNoSend)  // Assuming no ACK timer
    {
      result = rdmpd_pack_get_resp_software_version_label(&label, data->pd_out);
    }
  }

  return result;
}

static etcpal_error_t default_responder_identify_device(PidHandlerData* data)
{
  etcpal_error_t result = kEtcPalErrNotImpl;

  ControllerDefaultResponder* responder = static_cast<ControllerDefaultResponder*>(data->context);
  assert(responder);

  bool identify;
  rdmpd_nack_reason_t nack_reason;

  if (data->cmd_class == kRdmCCGetCommand)
  {
    result = responder->ProcessGetIdentifyDevice(identify, data->response_type, nack_reason);
  }
  else  // kRdmCCSetCommand
  {
    result = rdmpd_unpack_set_identify_device(data->pd_in, &identify);

    if (result == kEtcPalErrOk)
    {
      result = responder->ProcessSetIdentifyDevice(identify, data->response_type, nack_reason);
    }
    else if (result == kEtcPalErrProtocol)
    {
      result = kEtcPalErrOk;
      data->response_type = kRdmRespRtNackReason;
      nack_reason = kRdmPdNrFormatError;
    }
  }

  if (result == kEtcPalErrOk)
  {
    if (data->response_type == kRdmRespRtNackReason)
    {
      result = rdmpd_pack_nack_reason(nack_reason, data->pd_out);
    }
    else if ((data->response_type != kRdmRespRtNoSend) &&  // Assuming no ACK timer
             (data->cmd_class == kRdmCCGetCommand))
    {
      result = rdmpd_pack_get_resp_identify_device(&identify, data->pd_out);
    }
  }

  return result;
}

static etcpal_error_t default_responder_component_scope(PidHandlerData* data)
{
  etcpal_error_t result = kEtcPalErrNotImpl;

  ControllerDefaultResponder* responder = static_cast<ControllerDefaultResponder*>(data->context);
  assert(responder);

  uint16_t requested_slot;
  RdmPdComponentScope scope;
  rdmpd_nack_reason_t nack_reason;

  if (data->cmd_class == kRdmCCGetCommand)
  {
    result = rdmpd_unpack_get_component_scope(data->pd_in, &requested_slot);

    if (result == kEtcPalErrOk)
    {
      result = responder->ProcessGetComponentScope(requested_slot, scope, data->response_type, nack_reason);
    }
    else if (result == kEtcPalErrProtocol)
    {
      result = kEtcPalErrOk;
      data->response_type = kRdmRespRtNackReason;
      nack_reason = kRdmPdNrFormatError;
    }
  }
  else  // kRdmCCSetCommand
  {
    result = rdmpd_unpack_set_component_scope(data->pd_in, &scope);

    if (result == kEtcPalErrOk)
    {
      result = responder->ProcessSetComponentScope(scope, data->response_type, nack_reason);
    }
    else if (result == kEtcPalErrProtocol)
    {
      result = kEtcPalErrOk;
      data->response_type = kRdmRespRtNackReason;
      nack_reason = kRdmPdNrFormatError;
    }
  }

  if (result == kEtcPalErrOk)
  {
    if (data->response_type == kRdmRespRtNackReason)
    {
      result = rdmpd_pack_nack_reason(nack_reason, data->pd_out);
    }
    else if ((data->response_type != kRdmRespRtNoSend) &&  // Assuming no ACK timer
             (data->cmd_class == kRdmCCGetCommand))
    {
      result = rdmpd_pack_get_resp_component_scope(&scope, data->pd_out);
    }
  }

  return result;
}

static etcpal_error_t default_responder_search_domain(PidHandlerData* data)
{
  etcpal_error_t result = kEtcPalErrNotImpl;

  ControllerDefaultResponder* responder = static_cast<ControllerDefaultResponder*>(data->context);
  assert(responder);

  RdmPdSearchDomain search_domain;
  rdmpd_nack_reason_t nack_reason;

  if (data->cmd_class == kRdmCCGetCommand)
  {
    result = responder->ProcessGetSearchDomain(search_domain, data->response_type, nack_reason);
  }
  else  // kRdmCCSetCommand
  {
    result = rdmpd_unpack_set_search_domain(data->pd_in, &search_domain);

    if (result == kEtcPalErrOk)
    {
      result = responder->ProcessSetSearchDomain(search_domain, data->response_type, nack_reason);
    }
    else if (result == kEtcPalErrProtocol)
    {
      result = kEtcPalErrOk;
      data->response_type = kRdmRespRtNackReason;
      nack_reason = kRdmPdNrFormatError;
    }
  }

  if (result == kEtcPalErrOk)
  {
    if (data->response_type == kRdmRespRtNackReason)
    {
      result = rdmpd_pack_nack_reason(nack_reason, data->pd_out);
    }
    else if ((data->response_type != kRdmRespRtNoSend) &&  // Assuming no ACK timer
             (data->cmd_class == kRdmCCGetCommand))
    {
      result = rdmpd_pack_get_resp_search_domain(&search_domain, data->pd_out);
    }
  }

  return result;
}

static etcpal_error_t default_responder_tcp_comms_status(PidHandlerData* data)
{
  etcpal_error_t result = kEtcPalErrNotImpl;

  ControllerDefaultResponder* responder = static_cast<ControllerDefaultResponder*>(data->context);
  assert(responder);

  RdmPdTcpCommsEntry entry;
  rdmpd_nack_reason_t nack_reason;

  if (data->cmd_class == kRdmCCGetCommand)
  {
    result = responder->ProcessGetTcpCommsStatus(data->overflow_index, entry, data->response_type, nack_reason);
  }
  else  // kRdmCCSetCommand
  {
    RdmPdScopeString scope;
    result = rdmpd_unpack_set_tcp_comms_status(data->pd_in, &scope);

    if (result == kEtcPalErrOk)
    {
      result = responder->ProcessSetTcpCommsStatus(scope, data->response_type, nack_reason);
    }
    else if (result == kEtcPalErrProtocol)
    {
      result = kEtcPalErrOk;
      data->response_type = kRdmRespRtNackReason;
      nack_reason = kRdmPdNrFormatError;
    }
  }

  if (result == kEtcPalErrOk)
  {
    if (data->response_type == kRdmRespRtNackReason)
    {
      result = rdmpd_pack_nack_reason(nack_reason, data->pd_out);
    }
    else if ((data->response_type != kRdmRespRtNoSend) &&  // Assuming no ACK timer
             (data->cmd_class == kRdmCCGetCommand))
    {
      result = rdmpd_pack_get_resp_tcp_comms_status(&entry, data->pd_out);
    }
  }

  return result;
}

static uint8_t default_responder_get_message_count()
{
  return 0;
}

static void default_responder_get_next_queued_message(GetNextQueuedMessageData* data)
{
  // Does nothing
}
}  // extern "C"

void ControllerDefaultResponder::InitResponder()
{
  RdmPidHandlerEntry handler_array[CONTROLLER_HANDLER_ARRAY_SIZE] = {
      //{E120_SUPPORTED_PARAMETERS, default_responder_supported_params, RDM_PS_ALL | RDM_PS_GET},
      {E120_PARAMETER_DESCRIPTION, default_responder_parameter_description, RDM_PS_ROOT | RDM_PS_GET},
      {E120_DEVICE_MODEL_DESCRIPTION, default_responder_device_model_description,
       RDM_PS_ALL | RDM_PS_GET | RDM_PS_SHOW_SUPPORTED},
      //{E120_MANUFACTURER_LABEL, default_responder_manufacturer_label, RDM_PS_ALL | RDM_PS_GET |
      //RDM_PS_SHOW_SUPPORTED},
      {E120_DEVICE_LABEL, default_responder_device_label, RDM_PS_ALL | RDM_PS_GET_SET | RDM_PS_SHOW_SUPPORTED},
      {E120_SOFTWARE_VERSION_LABEL, default_responder_software_version_label, RDM_PS_ROOT | RDM_PS_GET},
      {E120_IDENTIFY_DEVICE, default_responder_identify_device, RDM_PS_ALL | RDM_PS_GET_SET},
      {E133_COMPONENT_SCOPE, default_responder_component_scope, RDM_PS_ROOT | RDM_PS_GET_SET | RDM_PS_SHOW_SUPPORTED},
      {E133_SEARCH_DOMAIN, default_responder_search_domain, RDM_PS_ROOT | RDM_PS_GET_SET | RDM_PS_SHOW_SUPPORTED},
      {E133_TCP_COMMS_STATUS, default_responder_tcp_comms_status,
       RDM_PS_ROOT | RDM_PS_GET_SET | RDM_PS_SHOW_SUPPORTED}};

  rdm_responder_state_.port_number = 0;
  rdm_responder_state_.number_of_subdevices = 0;
  rdm_responder_state_.responder_type = kRdmRespTypeController;
  rdm_responder_state_.callback_context = this;
  memcpy(handler_array_, handler_array, CONTROLLER_HANDLER_ARRAY_SIZE * sizeof(RdmPidHandlerEntry));
  rdm_responder_state_.handler_array = handler_array_;
  rdm_responder_state_.handler_array_size = CONTROLLER_HANDLER_ARRAY_SIZE;
  rdm_responder_state_.get_message_count = default_responder_get_message_count;
  rdm_responder_state_.get_next_queued_message = default_responder_get_next_queued_message;

  rdmresp_sort_handler_array(handler_array_, CONTROLLER_HANDLER_ARRAY_SIZE);
  assert(rdmresp_validate_state(&rdm_responder_state_));
}

etcpal_error_t ControllerDefaultResponder::ProcessCommand(const std::string& scope, const RdmCommand& cmd,
                                                          RdmResponse& resp, rdmresp_response_type_t& presp_type)
{
  etcpal_error_t result;
  ScopeEntry entry;

  if (scopes_.Find(scope, entry))
  {
    rdm_responder_state_.uid = entry.my_uid;
    result = rdmresp_process_command(&rdm_responder_state_, &cmd, &resp, &presp_type);
  }
  else  // Not found
  {
    presp_type = kRdmRespRtNackReason;
    rdmresp_create_nack_from_command(&resp, &cmd, E120_NR_HARDWARE_FAULT);

    result = kEtcPalErrNotFound;  // Scope handle not found
  }

  return result;
}

etcpal_error_t ControllerDefaultResponder::ProcessGetRdmPdString(RdmPdString& string, const char* source,
                                                                 rdmresp_response_type_t& response_type,
                                                                 rdmpd_nack_reason_t& nack_reason)
{
  etcpal::ReadGuard prop_read(prop_lock_);

  assert(source);
  assert(strlen(source) < RDMPD_STRING_MAX_LENGTH);

  rdmnet_safe_strncpy(string.string, source, RDMPD_STRING_MAX_LENGTH);
  response_type = kRdmRespRtAck;

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessGetParameterDescription(uint16_t pid,
                                                                          RdmPdParameterDescription& description,
                                                                          rdmresp_response_type_t& response_type,
                                                                          rdmpd_nack_reason_t& nack_reason)
{
  // etcpal::ReadGuard prop_read(prop_lock_);

  assert(response_type);
  assert(nack_reason);

  response_type = kRdmRespRtNackReason;
  nack_reason = kRdmPdNrDataOutOfRange;  // No manufacturer-specific PIDs apply currently

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessGetDeviceModelDescription(RdmPdString& description,
                                                                            rdmresp_response_type_t& response_type,
                                                                            rdmpd_nack_reason_t& nack_reason)
{
  return ProcessGetRdmPdString(description, device_model_description_.c_str(), response_type, nack_reason);
}

etcpal_error_t ControllerDefaultResponder::ProcessGetDeviceLabel(RdmPdString& label,
                                                                 rdmresp_response_type_t& response_type,
                                                                 rdmpd_nack_reason_t& nack_reason)
{
  return ProcessGetRdmPdString(label, device_label_.c_str(), response_type, nack_reason);
}

etcpal_error_t ControllerDefaultResponder::ProcessSetDeviceLabel(const RdmPdString& label,
                                                                 rdmresp_response_type_t& response_type,
                                                                 rdmpd_nack_reason_t& nack_reason)
{
  etcpal::WriteGuard prop_write(prop_lock_);

  device_label_.assign(label.string, RDMPD_STRING_MAX_LENGTH);
  response_type = kRdmRespRtAck;

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessGetSoftwareVersionLabel(RdmPdString& label,
                                                                          rdmresp_response_type_t& response_type,
                                                                          rdmpd_nack_reason_t& nack_reason)
{
  return ProcessGetRdmPdString(label, software_version_label_.c_str(), response_type, nack_reason);
}

etcpal_error_t ControllerDefaultResponder::ProcessGetIdentifyDevice(bool& identify_state,
                                                                    rdmresp_response_type_t& response_type,
                                                                    rdmpd_nack_reason_t& nack_reason)
{
  etcpal::ReadGuard prop_read(prop_lock_);

  identify_state = identifying_;
  response_type = kRdmRespRtAck;

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessSetIdentifyDevice(bool identify,
                                                                    rdmresp_response_type_t& response_type,
                                                                    rdmpd_nack_reason_t& nack_reason)
{
  etcpal::WriteGuard prop_write(prop_lock_);

  identifying_ = identify;
  response_type = kRdmRespRtAck;

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessGetComponentScope(uint16_t slot, RdmPdComponentScope& component_scope,
                                                                    rdmresp_response_type_t& response_type,
                                                                    rdmpd_nack_reason_t& nack_reason)
{
  etcpal::ReadGuard prop_read(prop_lock_);
  bool got_scope = false;

  if (slot != 0)
  {
    ScopeEntry entry;
    if (scopes_.Find(slot, entry))
    {
      component_scope.scope_slot = slot;
      rdmnet_safe_strncpy(component_scope.scope_string.string, entry.scope_string.c_str(), RDMPD_MAX_SCOPE_STR_LEN);
      component_scope.static_broker_addr = entry.static_broker.addr;
      got_scope = true;
    }
  }

  if (got_scope)
  {
    response_type = kRdmRespRtAck;
  }
  else
  {
    response_type = kRdmRespRtNackReason;
    nack_reason = kRdmPdNrDataOutOfRange;
  }

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessSetComponentScope(const RdmPdComponentScope& component_scope,
                                                                    rdmresp_response_type_t& response_type,
                                                                    rdmpd_nack_reason_t& nack_reason)
{
  etcpal::WriteGuard prop_write(prop_lock_);

  if (component_scope.scope_slot == 0)
  {
    response_type = kRdmRespRtNackReason;
    nack_reason = kRdmPdNrDataOutOfRange;
  }
  else
  {
    // TO DO: Find a way to notify that scope has changed (if it has) (along with other property sets)
    if (strlen(component_scope.scope_string.string) == 0)
    {
      scopes_.Remove(component_scope.scope_slot);
    }
    else
    {
      ScopeEntry entry;
      memset(&entry.current_broker, 0, sizeof(EtcPalSockaddr));
      entry.current_broker.ip.type = kEtcPalIpTypeInvalid;
      memset(&entry.static_broker, 0, sizeof(StaticBrokerConfig));
      entry.static_broker.addr.ip.type = kEtcPalIpTypeInvalid;
      memset(&entry.my_uid, 0, sizeof(RdmUid));

      scopes_.Find(component_scope.scope_slot, entry);  // Same code should follow regardless of if found.
      entry.scope_string.assign(component_scope.scope_string.string, RDMPD_MAX_SCOPE_STR_LEN);
      entry.scope_slot = component_scope.scope_slot;
      bool old_static_broker_valid = entry.static_broker.valid;
      entry.static_broker.valid = (component_scope.static_broker_addr.ip.type != kEtcPalIpTypeInvalid) &&
                                  (component_scope.static_broker_addr.port != 0);

      if (entry.static_broker.valid)
      {
        // The next connection should be static.
        entry.current_broker = entry.static_broker.addr = component_scope.static_broker_addr;
      }
      else if (old_static_broker_valid && !entry.static_broker.valid)
      {
        // Close the static connection (which is current). Both current and static should be invalidated.
        memset(&entry.current_broker, 0, sizeof(EtcPalSockaddr));
        entry.current_broker.ip.type = kEtcPalIpTypeInvalid;
        memset(&entry.static_broker, 0, sizeof(StaticBrokerConfig));
        entry.static_broker.addr.ip.type = kEtcPalIpTypeInvalid;
      }

      scopes_.Set(entry);
    }

    response_type = kRdmRespRtAck;
  }

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessGetSearchDomain(RdmPdSearchDomain& search_domain,
                                                                  rdmresp_response_type_t& response_type,
                                                                  rdmpd_nack_reason_t& nack_reason)
{
  etcpal::ReadGuard prop_read(prop_lock_);

  rdmnet_safe_strncpy(search_domain.string, search_domain_.c_str(), RDMPD_MAX_SEARCH_DOMAIN_STR_LEN);
  response_type = kRdmRespRtAck;

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessSetSearchDomain(const RdmPdSearchDomain& search_domain,
                                                                  rdmresp_response_type_t& response_type,
                                                                  rdmpd_nack_reason_t& nack_reason)
{
  etcpal::WriteGuard prop_write(prop_lock_);

  search_domain_.assign(search_domain.string, RDMPD_MAX_SEARCH_DOMAIN_STR_LEN);
  response_type = kRdmRespRtAck;

  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessGetTcpCommsStatus(size_t overflow_index, RdmPdTcpCommsEntry& entry,
                                                                    rdmresp_response_type_t& response_type,
                                                                    rdmpd_nack_reason_t& nack_reason)
{
  etcpal::ReadGuard prop_read(prop_lock_);

  auto iter = scopes_.Begin();

  if ((iter != scopes_.End()) && (overflow_index <= scopes_.Size()))
  {
    std::advance(iter, overflow_index - 1);

    entry.broker_addr = iter->second.current_broker;
    rdmnet_safe_strncpy(entry.scope_string.string, iter->second.scope_string.c_str(), RDMPD_MAX_SCOPE_STR_LEN);
    entry.unhealthy_tcp_events = iter->second.unhealthy_tcp_events;

    response_type = kRdmRespRtAck;
  }
  else
  {
    response_type = kRdmRespRtNackReason;
    nack_reason = kRdmPdNrDataOutOfRange;
  }
  return kEtcPalErrOk;
}

etcpal_error_t ControllerDefaultResponder::ProcessSetTcpCommsStatus(const RdmPdScopeString& scope,
                                                                    rdmresp_response_type_t& response_type,
                                                                    rdmpd_nack_reason_t& nack_reason)
{
  etcpal::WriteGuard prop_write(prop_lock_);

  ScopeEntry entry;

  if (scopes_.Find(scope.string, entry))
  {
    entry.unhealthy_tcp_events = 0;
    scopes_.Set(entry);

    response_type = kRdmRespRtAck;
  }
  else
  {
    response_type = kRdmRespRtNackReason;
    nack_reason = kRdmPdNrDataOutOfRange;
  }

  return kEtcPalErrOk;
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
    ScopeEntry entry;

    if (scopes_.Find(slot, entry))
    {
      RdmParamData resp_data;

      // Build the parameter data of the COMPONENT_SCOPE response.

      // Scope slot
      uint8_t* cur_ptr = resp_data.data;
      etcpal_pack_16b(cur_ptr, slot);
      cur_ptr += 2;

      // Scope string
      const std::string& scope_str = entry.scope_string;
      strncpy((char*)cur_ptr, scope_str.c_str(), E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      // Static configuration
      if (entry.static_broker.valid)
      {
        const EtcPalSockaddr& saddr = entry.static_broker.addr;
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
  //etcpal::ReadGuard prop_read(prop_lock_);

  //for (const auto& scope_pair : scopes_)
  //{
  //  RdmParamData resp_data;
  //  uint8_t* cur_ptr = resp_data.data;

  //  const std::string& scope_str = scope_pair.first;
  //  memset(cur_ptr, 0, E133_SCOPE_STRING_PADDED_LENGTH);
  //  memcpy(cur_ptr, scope_str.data(), std::min<size_t>(scope_str.length(), E133_SCOPE_STRING_PADDED_LENGTH));
  //  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

  //  const ControllerScopeData& scope_data = scope_pair.second;
  //  if (!scope_data.connected)
  //  {
  //    etcpal_pack_32b(cur_ptr, 0);
  //    cur_ptr += 4;
  //    memset(cur_ptr, 0, ETCPAL_IPV6_BYTES);
  //    cur_ptr += ETCPAL_IPV6_BYTES;
  //    etcpal_pack_16b(cur_ptr, 0);
  //    cur_ptr += 2;
  //  }
  //  else
  //  {
  //    if (ETCPAL_IP_IS_V4(&scope_data.current_broker.ip))
  //    {
  //      etcpal_pack_32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&scope_data.current_broker.ip));
  //      cur_ptr += 4;
  //      memset(cur_ptr, 0, ETCPAL_IPV6_BYTES);
  //      cur_ptr += ETCPAL_IPV6_BYTES;
  //    }
  //    else  // IPv6
  //    {
  //      etcpal_pack_32b(cur_ptr, 0);
  //      cur_ptr += 4;
  //      memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&scope_data.current_broker.ip), ETCPAL_IPV6_BYTES);
  //      cur_ptr += ETCPAL_IPV6_BYTES;
  //    }
  //    etcpal_pack_16b(cur_ptr, scope_data.current_broker.port);
  //    cur_ptr += 2;
  //  }
  //  etcpal_pack_16b(cur_ptr, scope_data.unhealthy_tcp_events);
  //  cur_ptr += 2;
  //  resp_data.datalen = (uint8_t)(cur_ptr - resp_data.data);
  //  resp_data_list.push_back(resp_data);
  //}
  //return true;
  return false; // This function is on its way out
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

  ScopeEntry entry;
  entry.static_broker = static_broker;
  entry.scope_string = new_scope;
  scopes_.Set(entry);
}

void ControllerDefaultResponder::RemoveScope(const std::string& scope_to_remove)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  scopes_.Remove(scope_to_remove);
}

void ControllerDefaultResponder::UpdateScopeConnectionStatus(const std::string& scope, bool connected,
                                                             const EtcPalSockaddr& broker_addr,
                                                             const RdmUid& controller_uid)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  ScopeEntry entry;
  if (scopes_.Find(scope, entry))
  {
    entry.connected = connected;
    if (connected)
    {
      entry.current_broker = broker_addr;
      entry.my_uid = controller_uid;
    }

    scopes_.Set(entry);
  }
}

void ControllerDefaultResponder::IncrementTcpUnhealthyCounter(const std::string& scope)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  ScopeEntry entry;
  if (scopes_.Find(scope, entry))
  {
    ++entry.unhealthy_tcp_events;
    scopes_.Set(entry);
  }
}

void ControllerDefaultResponder::ResetTcpUnhealthyCounter(const std::string& scope)
{
  etcpal::WriteGuard prop_write(prop_lock_);
  ScopeEntry entry;
  if (scopes_.Find(scope, entry))
  {
    entry.unhealthy_tcp_events = 0;
    scopes_.Set(entry);
  }
}

bool ControllerDefaultResponder::ScopeMap::Find(const std::string& scope, ScopeEntry& entry) const
{
  auto iter = string_map_.find(scope);
  if (iter != string_map_.end())
  {
    entry = iter->second;
    return true;
  }
  return false;
}

bool ControllerDefaultResponder::ScopeMap::Find(uint16_t slot, ScopeEntry& entry) const
{
  auto iter = slot_map_.find(slot);
  if (iter != slot_map_.end())
  {
    entry = iter->second;
    return true;
  }
  return false;
}

bool ControllerDefaultResponder::ScopeMap::Remove(const std::string& scope)
{
  auto string_map_iter = string_map_.find(scope);
  if (string_map_iter != string_map_.end())
  {
    uint16_t slot = string_map_iter->second.get().scope_slot;
    auto slot_map_iter = slot_map_.find(slot);
    if (slot_map_iter != slot_map_.end())
    {
      string_map_.erase(string_map_iter);
      slot_map_.erase(slot_map_iter);
      return true;
    }
  }
  return false;
}

bool ControllerDefaultResponder::ScopeMap::Remove(uint16_t slot)
{
  auto slot_map_iter = slot_map_.find(slot);
  if (slot_map_iter != slot_map_.end())
  {
    std::string scope = slot_map_iter->second.scope_string;
    auto string_map_iter = string_map_.find(scope);
    if (string_map_iter != string_map_.end())
    {
      string_map_.erase(string_map_iter);
      slot_map_.erase(slot_map_iter);
      return true;
    }
  }
  return false;
}

void ControllerDefaultResponder::ScopeMap::Set(const ScopeEntry& entry)
{
  auto existing_entry = slot_map_.find(entry.scope_slot);
  if (existing_entry != slot_map_.end())
  {
    if (existing_entry->second.scope_string != entry.scope_string)
    {
      // This string_map_ reference is no longer accurate.
      string_map_.erase(string_map_.find(existing_entry->second.scope_string));
    }
  }

  slot_map_[entry.scope_slot] = entry;

  // Point reference to slot_map_ entry.
  auto string_map_iter = string_map_.find(entry.scope_string);
  if (string_map_iter == string_map_.end())
  {
    string_map_.insert(
        std::make_pair(entry.scope_string, std::reference_wrapper<ScopeEntry>(slot_map_[entry.scope_slot])));
  }
  else
  {
    string_map_iter->second = std::reference_wrapper<ScopeEntry>(slot_map_[entry.scope_slot]);
  }
}

std::map<uint16_t, ControllerDefaultResponder::ScopeEntry>::const_iterator ControllerDefaultResponder::ScopeMap::Begin()
    const
{
  return slot_map_.begin();
}

std::map<uint16_t, ControllerDefaultResponder::ScopeEntry>::const_iterator ControllerDefaultResponder::ScopeMap::End()
    const
{
  return slot_map_.end();
}

size_t ControllerDefaultResponder::ScopeMap::Size() const
{
  return slot_map_.size();
}
