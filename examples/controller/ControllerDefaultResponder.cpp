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

#include "rdm/defs.h"
#include "rdmnet/defs.h"
#include "ControllerDefaultResponder.h"

bool ControllerDefaultResponder::Get(uint16_t pid, const uint8_t *param_data, uint8_t param_data_len,
                                     std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
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
    case E137_7_ENDPOINT_LIST:
      return GetEndpointList(param_data, param_data_len, resp_data_list, nack_reason);
    case E137_7_ENDPOINT_RESPONDERS:
      return GetEndpointResponders(param_data, param_data_len, resp_data_list, nack_reason);
    default:
      nack_reason = E120_NR_UNKNOWN_PID;
      return false;
  }
}

bool ControllerDefaultResponder::GetIdentifyDevice(const uint8_t *param_data, uint8_t param_data_len,
                                                   std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    resp_data_list[0].data[0] = prop_data_.identifying ? 1 : 0;
    lwpa_rwlock_readunlock(&prop_lock);
  }
  resp_data_list[0].datalen = 1;
  *num_responses = 1;
  return true;
}

bool ControllerDefaultResponder::GetDeviceLabel(const uint8_t *param_data, uint8_t param_data_len,
                                                std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    strncpy((char *)resp_data_list[0].data, prop_data_.device_label, DEVICE_LABEL_MAX_LEN);
    resp_data_list[0].datalen = (uint8_t)strnlen(prop_data_.device_label, DEVICE_LABEL_MAX_LEN);
    lwpa_rwlock_readunlock(&prop_lock);
  }
  *num_responses = 1;
  return true;
}

bool ControllerDefaultResponder::GetComponentScope(const uint8_t *param_data, uint8_t param_data_len,
                                                   std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  if (param_data_len >= 2)
  {
    return GetComponentScope(lwpa_upack_16b(param_data), resp_data_list, num_responses, nack_reason);
  }
  else
  {
    &nack_reason = E120_NR_FORMAT_ERROR;
  }
  return false;
}

bool ControllerDefaultResponder::GetComponentScope(uint16_t slot, std::vector<RdmParamData> &resp_data_list,
                                                   uint16_t &nack_reason)
{
  if (slot == 0)
  {
    if (nack_reason != NULL)
    {
      &nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }
  }
  else if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    auto connectionIter = broker_connections_.find(slot - 1);

    if (connectionIter == broker_connections_.end())
    {
      // Return the next highest Scope Slot that is not empty.
      connectionIter = broker_connections_.upper_bound(slot - 1);

      if (connectionIter != broker_connections_.end())
      {
        slot = connectionIter->first + 1;
      }
      else
      {
        slot = 0;
      }
    }

    if (slot != 0)
    {
      uint8_t *cur_ptr = resp_data_list[0].data;
      lwpa_pack_16b(cur_ptr, slot);
      cur_ptr += 2;

      std::string scope = connectionIter->second->scope();

      strncpy((char *)cur_ptr, scope.data(), E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      LwpaSockaddr saddr = connectionIter->second->staticSockAddr();
      if (lwpaip_is_v4(&saddr.ip))
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
        lwpa_pack_32b(cur_ptr, lwpaip_v4_address(&saddr.ip));
        cur_ptr += 4;
        /* Skip the IPv6 field */
        cur_ptr += 16;
        lwpa_pack_16b(cur_ptr, saddr.port);
        cur_ptr += 2;
      }
      else if (lwpaip_is_v6(&saddr.ip))
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
        /* Skip the IPv4 field */
        cur_ptr += 4;
        memcpy(cur_ptr, lwpaip_v6_address(&saddr.ip), LWPA_IPV6_BYTES);
        cur_ptr += LWPA_IPV6_BYTES;
        lwpa_pack_16b(cur_ptr, saddr.port);
        cur_ptr += 2;
      }
      else
      {
        *cur_ptr++ = E133_NO_STATIC_CONFIG;
        /* Skip the IPv4, IPv6 and port fields */
        cur_ptr += 4 + 16 + 2;
      }
      resp_data_list[0].datalen = static_cast<uint8_t>(cur_ptr - resp_data_list[0].data);
      *num_responses = 1;

      lwpa_rwlock_readunlock(&prop_lock);
      return true;
    }
    else if (nack_reason != NULL)
    {
      &nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }

    lwpa_rwlock_readunlock(&prop_lock);
  }

  return false;
}

bool ControllerDefaultResponder::GetSearchDomain(const uint8_t *param_data, uint8_t param_data_len,
                                                 std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    strncpy((char *)resp_data_list[0].data, prop_data_.search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
    resp_data_list[0].datalen = (uint8_t)strlen(prop_data_.search_domain);

    lwpa_rwlock_readunlock(&prop_lock);
  }
  *num_responses = 1;
  return true;
}

bool ControllerDefaultResponder::GetTCPCommsStatus(const uint8_t *param_data, uint8_t param_data_len,
                                                   std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  int currentListIndex = 0;

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    for (const auto &connection : broker_connections_)
    {
      uint8_t *cur_ptr = resp_data_list[currentListIndex].data;
      LwpaSockaddr saddr = connection.second->currentSockAddr();
      std::string scope = connection.second->scope();

      memset(cur_ptr, 0, E133_SCOPE_STRING_PADDED_LENGTH);
      memcpy(cur_ptr, scope.data(), min(scope.length(), E133_SCOPE_STRING_PADDED_LENGTH));
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      if (lwpaip_is_invalid(&saddr.ip) || !connection.second->connected())
      {
        lwpa_pack_32b(cur_ptr, 0);
        cur_ptr += 4;
        memset(cur_ptr, 0, LWPA_IPV6_BYTES);
        cur_ptr += LWPA_IPV6_BYTES;
      }
      else if (lwpaip_is_v4(&saddr.ip))
      {
        lwpa_pack_32b(cur_ptr, lwpaip_v4_address(&saddr.ip));
        cur_ptr += 4;
        memset(cur_ptr, 0, LWPA_IPV6_BYTES);
        cur_ptr += LWPA_IPV6_BYTES;
      }
      else  // IPv6
      {
        lwpa_pack_32b(cur_ptr, 0);
        cur_ptr += 4;
        memcpy(cur_ptr, lwpaip_v6_address(&saddr.ip), LWPA_IPV6_BYTES);
        cur_ptr += LWPA_IPV6_BYTES;
      }
      lwpa_pack_16b(cur_ptr, saddr.port);
      cur_ptr += 2;
      lwpa_pack_16b(cur_ptr, prop_data_.tcp_unhealthy_counter);
      cur_ptr += 2;
      resp_data_list[currentListIndex].datalen = (uint8_t)(cur_ptr - resp_data_list[currentListIndex].data);

      ++currentListIndex;

      if (currentListIndex == MAX_RESPONSES_IN_ACK_OVERFLOW)
      {
        break;
      }
    }

    lwpa_rwlock_readunlock(&prop_lock);
  }

  *num_responses = currentListIndex;
  return true;
}

bool ControllerDefaultResponder::GetSupportedParameters(const uint8_t *param_data, uint8_t param_data_len,
                                                        std::vector<RdmParamData> &resp_data_list,
                                                        uint16_t &nack_reason)
{
  size_t list_index = 0;
  uint8_t *cur_ptr = resp_data_list[0].data;
  size_t i;

  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  for (i = 0; i < NUM_SUPPORTED_PIDS; ++i)
  {
    lwpa_pack_16b(cur_ptr, kSupportedPIDList[i]);
    cur_ptr += 2;
    if ((cur_ptr - resp_data_list[list_index].data) >= RDM_MAX_PDL - 1)
    {
      resp_data_list[list_index].datalen = (uint8_t)(cur_ptr - resp_data_list[list_index].data);
      cur_ptr = resp_data_list[++list_index].data;
    }
  }
  resp_data_list[list_index].datalen = (uint8_t)(cur_ptr - resp_data_list[list_index].data);
  *num_responses = list_index + 1;
  return true;
}

bool ControllerDefaultResponder::GetDeviceInfo(const uint8_t *param_data, uint8_t param_data_len,
                                               std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;
  memcpy(resp_data_list[0].data, kDeviceInfo, sizeof kDeviceInfo);
  resp_data_list[0].datalen = sizeof kDeviceInfo;
  *num_responses = 1;
  return true;
}

bool ControllerDefaultResponder::GetManufacturerLabel(const uint8_t *param_data, uint8_t param_data_len,
                                                      std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char *)resp_data_list[0].data, MANUFACTURER_LABEL);
  resp_data_list[0].datalen = sizeof(MANUFACTURER_LABEL) - 1;
  *num_responses = 1;
  return true;
}

bool ControllerDefaultResponder::GetDeviceModelDescription(const uint8_t *param_data, uint8_t param_data_len,
                                                           std::vector<RdmParamData> &resp_data_list,
                                                           uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char *)resp_data_list[0].data, DEVICE_MODEL_DESCRIPTION);
  resp_data_list[0].datalen = sizeof(DEVICE_MODEL_DESCRIPTION) - 1;
  *num_responses = 1;
  return true;
}

bool ControllerDefaultResponder::GetSoftwareVersionLabel(const uint8_t *param_data, uint8_t param_data_len,
                                                         std::vector<RdmParamData> &resp_data_list,
                                                         uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char *)resp_data_list[0].data, SOFTWARE_VERSION_LABEL);
  resp_data_list[0].datalen = sizeof(SOFTWARE_VERSION_LABEL) - 1;
  *num_responses = 1;
  return true;
}

bool ControllerDefaultResponder::GetEndpointList(const uint8_t *param_data, uint8_t param_data_len,
                                                 std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  uint8_t *cur_ptr = resp_data_list[0].data;

  /* Hardcoded: no endpoints other than NULL_ENDPOINT. NULL_ENDPOINT is not
   * reported in this response. */
  resp_data_list[0].datalen = 4;
  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    lwpa_pack_32b(cur_ptr, prop_data_.endpoint_list_change_number);
    lwpa_rwlock_readunlock(&prop_lock);
  }
  *num_responses = 1;
  return true;
}

bool ControllerDefaultResponder::GetEndpointResponders(const uint8_t *param_data, uint8_t param_data_len,
                                                       std::vector<RdmParamData> &resp_data_list, uint16_t &nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)resp_data_list;
  (void)num_responses;

  if (param_data_len >= 2)
  {
    /* We have no valid endpoints for this message */
    &nack_reason = E137_7_NR_ENDPOINT_NUMBER_INVALID;
  }
  else
    &nack_reason = E120_NR_FORMAT_ERROR;
  return false;
}
