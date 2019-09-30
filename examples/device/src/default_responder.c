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

#include "default_responder.h"

#include <string.h>
#include <stdio.h>
#include "etcpal/pack.h"
#include "etcpal/thread.h"
#include "etcpal/lock.h"
#include "rdm/defs.h"
#include "rdmnet/defs.h"
#include "rdmnet/version.h"

/**************************** Private constants ******************************/

#define NUM_SUPPORTED_PIDS 15
static const uint16_t kSupportedPIDList[NUM_SUPPORTED_PIDS] = {
    E120_IDENTIFY_DEVICE,    E120_SUPPORTED_PARAMETERS,     E120_DEVICE_INFO,
    E120_MANUFACTURER_LABEL, E120_DEVICE_MODEL_DESCRIPTION, E120_SOFTWARE_VERSION_LABEL,
    E120_DEVICE_LABEL,       E133_COMPONENT_SCOPE,          E133_SEARCH_DOMAIN,
    E133_TCP_COMMS_STATUS,   E137_7_ENDPOINT_RESPONDERS,    E137_7_ENDPOINT_LIST,
};

/* clang-format off */
static const uint8_t kDeviceInfo[] = {
    0x01, 0x00, /* RDM Protocol version */
    0xe1, 0x33, /* Device Model ID */
    0xe1, 0x33, /* Product Category */

    /* Software Version ID */
    RDMNET_VERSION_MAJOR, RDMNET_VERSION_MINOR,
    RDMNET_VERSION_PATCH, (RDMNET_VERSION_BUILD > 0xff ? 0xff : RDMNET_VERSION_BUILD),

    0x00, 0x00, /* DMX512 Footprint */
    0x00, 0x00, /* DMX512 Personality */
    0xff, 0xff, /* DMX512 Start Address */
    0x00, 0x00, /* Sub-device count */
    0x00 /* Sensor count */
};
/* clang-format on */

#define DEVICE_LABEL_MAX_LEN 32
#define DEFAULT_DEVICE_LABEL "My ETC RDMnet Device"
#define SOFTWARE_VERSION_LABEL RDMNET_VERSION_STRING
#define MANUFACTURER_LABEL "ETC"
#define DEVICE_MODEL_DESCRIPTION "Prototype RDMnet Device"

#define DEVICE_HANDLER_ARRAY_SIZE 8

/**************************** Private variables ******************************/

static struct DeviceResponderState
{
  uint32_t endpoint_list_change_number;
  etcpal_thread_t identify_thread;
  bool identifying;
  char device_label[DEVICE_LABEL_MAX_LEN + 1];
  RdmnetScopeConfig scope_config;
  char search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];
  uint16_t tcp_unhealthy_counter;
  bool connected;
  EtcPalSockaddr cur_broker_addr;

  RdmResponderState rdm_responder_state;
  RdmPidHandlerEntry handler_array[DEVICE_HANDLER_ARRAY_SIZE];
} device_responder_state;

static etcpal_rwlock_t state_lock;

/*********************** Private function prototypes *************************/

/* SET COMMANDS */
bool set_identify_device(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                         rdmnet_data_changed_t* data_changed);
bool set_device_label(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                      rdmnet_data_changed_t* data_changed);
bool set_component_scope(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                         rdmnet_data_changed_t* data_changed);
bool set_search_domain(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                       rdmnet_data_changed_t* data_changed);
bool set_tcp_comms_status(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                          rdmnet_data_changed_t* data_changed);

/* GET COMMANDS */
bool get_identify_device(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                         size_t* num_responses, uint16_t* nack_reason);
bool get_device_label(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                      size_t* num_responses, uint16_t* nack_reason);
bool get_component_scope(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                         size_t* num_responses, uint16_t* nack_reason);
bool get_search_domain(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                       size_t* num_responses, uint16_t* nack_reason);
bool get_tcp_comms_status(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                          size_t* num_responses, uint16_t* nack_reason);
bool get_supported_parameters(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                              size_t* num_responses, uint16_t* nack_reason);
bool get_device_info(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                     size_t* num_responses, uint16_t* nack_reason);
bool get_manufacturer_label(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                            size_t* num_responses, uint16_t* nack_reason);
bool get_device_model_description(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                                  size_t* num_responses, uint16_t* nack_reason);
bool get_software_version_label(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                                size_t* num_responses, uint16_t* nack_reason);
bool get_endpoint_list(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                       size_t* num_responses, uint16_t* nack_reason);
bool get_endpoint_responders(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                             size_t* num_responses, uint16_t* nack_reason);

/* RESPONDER HANDLERS */
//resp_process_result_t default_responder_supported_params(PidHandlerData* data);
resp_process_result_t default_responder_parameter_description(PidHandlerData* data);
resp_process_result_t default_responder_device_model_description(PidHandlerData* data);
//resp_process_result_t default_responder_manufacturer_label(PidHandlerData* data);
resp_process_result_t default_responder_device_label(PidHandlerData* data);
resp_process_result_t default_responder_software_version_label(PidHandlerData* data);
resp_process_result_t default_responder_identify_device(PidHandlerData* data);
resp_process_result_t default_responder_component_scope(PidHandlerData* data);
resp_process_result_t default_responder_search_domain(PidHandlerData* data);
resp_process_result_t default_responder_tcp_comms_status(PidHandlerData* data);
uint8_t default_responder_get_message_count();
void default_responder_get_next_queue_message(GetNextQueueMessageData* data);

/*************************** Function definitions ****************************/

void default_responder_init(const RdmnetScopeConfig* scope_config, const char* search_domain)
{
  etcpal_rwlock_create(&state_lock);

  rdmnet_safe_strncpy(device_responder_state.device_label, DEFAULT_DEVICE_LABEL, DEVICE_LABEL_MAX_LEN);
  rdmnet_safe_strncpy(device_responder_state.search_domain, search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  device_responder_state.scope_config = *scope_config;

  RdmPidHandlerEntry handler_array[DEVICE_HANDLER_ARRAY_SIZE] = {
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

  device_responder_state.rdm_responder_state.port_number = 0;
  device_responder_state.rdm_responder_state.number_of_subdevices = 0;
  device_responder_state.rdm_responder_state.responder_type = kRespTypeDevice;
  device_responder_state.rdm_responder_state.callback_context = NULL;
  memcpy(device_responder_state.handler_array, handler_array, DEVICE_HANDLER_ARRAY_SIZE * sizeof(RdmPidHandlerEntry));
  device_responder_state.rdm_responder_state.handler_array = device_responder_state.handler_array;
  device_responder_state.rdm_responder_state.handler_array_size = DEVICE_HANDLER_ARRAY_SIZE;
  device_responder_state.rdm_responder_state.get_message_count = default_responder_get_message_count;
  device_responder_state.rdm_responder_state.get_next_queue_message = default_responder_get_next_queue_message;

  rdmresp_sort_handler_array(device_responder_state.handler_array, DEVICE_HANDLER_ARRAY_SIZE);
  assert(rdmresp_validate_state(&device_responder_state.rdm_responder_state));
}

void default_responder_deinit()
{
  if (device_responder_state.identifying)
  {
    device_responder_state.identifying = false;
    etcpal_thread_join(&device_responder_state.identify_thread);
  }
  memset(&device_responder_state, 0, sizeof(device_responder_state));
  etcpal_rwlock_destroy(&state_lock);
}

void default_responder_get_scope_config(RdmnetScopeConfig* scope_config)
{
  if (etcpal_rwlock_readlock(&state_lock))
  {
    *scope_config = device_responder_state.scope_config;
    etcpal_rwlock_readunlock(&state_lock);
  }
}

void default_responder_get_search_domain(char* search_domain)
{
  if (etcpal_rwlock_readlock(&state_lock))
  {
    rdmnet_safe_strncpy(search_domain, device_responder_state.search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
    etcpal_rwlock_readunlock(&state_lock);
  }
}

void default_responder_update_connection_status(bool connected, const EtcPalSockaddr* broker_addr,
                                                const RdmUid* responder_uid)
{
  if (etcpal_rwlock_readlock(&state_lock))
  {
    device_responder_state.connected = connected;
    if (device_responder_state.connected)
      device_responder_state.cur_broker_addr = *broker_addr;
    device_responder_state.rdm_responder_state.uid = *responder_uid;
    etcpal_rwlock_readunlock(&state_lock);
  }
}

void default_responder_incr_unhealthy_count()
{
  if (etcpal_rwlock_writelock(&state_lock))
  {
    ++device_responder_state.tcp_unhealthy_counter;
    etcpal_rwlock_writeunlock(&state_lock);
  }
}

void default_responder_set_tcp_status(EtcPalSockaddr* broker_addr)
{
  if (etcpal_rwlock_writelock(&state_lock))
  {
    device_responder_state.cur_broker_addr = *broker_addr;
    etcpal_rwlock_writeunlock(&state_lock);
  }
}

bool default_responder_supports_pid(uint16_t pid)
{
  size_t i;
  for (i = 0; i < NUM_SUPPORTED_PIDS; ++i)
  {
    if (kSupportedPIDList[i] == pid)
      return true;
  }
  return false;
}

bool default_responder_set(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                           rdmnet_data_changed_t* data_changed)
{
  bool res = false;
  if (etcpal_rwlock_writelock(&state_lock))
  {
    switch (pid)
    {
      case E120_IDENTIFY_DEVICE:
        res = set_identify_device(param_data, param_data_len, nack_reason, data_changed);
        break;
      case E120_DEVICE_LABEL:
        res = set_device_label(param_data, param_data_len, nack_reason, data_changed);
        break;
      case E133_COMPONENT_SCOPE:
        res = set_component_scope(param_data, param_data_len, nack_reason, data_changed);
        break;
      case E133_SEARCH_DOMAIN:
        res = set_search_domain(param_data, param_data_len, nack_reason, data_changed);
        break;
      case E133_TCP_COMMS_STATUS:
        res = set_tcp_comms_status(param_data, param_data_len, nack_reason, data_changed);
        break;
      case E120_SUPPORTED_PARAMETERS:
      case E120_MANUFACTURER_LABEL:
      case E120_DEVICE_MODEL_DESCRIPTION:
      case E120_SOFTWARE_VERSION_LABEL:
      case E137_7_ENDPOINT_LIST:
      case E137_7_ENDPOINT_RESPONDERS:
        *nack_reason = E120_NR_UNSUPPORTED_COMMAND_CLASS;
        *data_changed = kNoRdmnetDataChanged;
        break;
      default:
        *nack_reason = E120_NR_UNKNOWN_PID;
        *data_changed = kNoRdmnetDataChanged;
        break;
    }
    etcpal_rwlock_writeunlock(&state_lock);
  }
  return res;
}

bool default_responder_get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len,
                           param_data_list_t resp_data_list, size_t* num_responses, uint16_t* nack_reason)
{
  bool res = false;
  if (etcpal_rwlock_readlock(&state_lock))
  {
    switch (pid)
    {
      case E120_IDENTIFY_DEVICE:
        res = get_identify_device(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E120_DEVICE_LABEL:
        res = get_device_label(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E133_COMPONENT_SCOPE:
        res = get_component_scope(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E133_SEARCH_DOMAIN:
        res = get_search_domain(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E133_TCP_COMMS_STATUS:
        res = get_tcp_comms_status(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E120_SUPPORTED_PARAMETERS:
        res = get_supported_parameters(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E120_DEVICE_INFO:
        res = get_device_info(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E120_MANUFACTURER_LABEL:
        res = get_manufacturer_label(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E120_DEVICE_MODEL_DESCRIPTION:
        res = get_device_model_description(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E120_SOFTWARE_VERSION_LABEL:
        res = get_software_version_label(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E137_7_ENDPOINT_LIST:
        res = get_endpoint_list(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      case E137_7_ENDPOINT_RESPONDERS:
        res = get_endpoint_responders(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
        break;
      default:
        *nack_reason = E120_NR_UNKNOWN_PID;
        break;
    }
    etcpal_rwlock_readunlock(&state_lock);
  }
  return res;
}

resp_process_result_t default_responder_process_command(const RdmCommand* command, RdmResponse* response)
{
  return rdmresp_process_command(&device_responder_state.rdm_responder_state, command, response);
}

void identify_thread(void* arg)
{
  (void)arg;

  while (device_responder_state.identifying)
  {
    printf("I AM IDENTIFYING!!!\n");
    etcpal_thread_sleep(1000);
  }
}

bool set_identify_device(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                         rdmnet_data_changed_t* data_changed)
{
  if (param_data_len >= 1)
  {
    bool new_identify_setting = (bool)(*param_data);
    if (new_identify_setting && !device_responder_state.identifying)
    {
      EtcPalThreadParams ithread_params;

      ithread_params.thread_priority = ETCPAL_THREAD_DEFAULT_PRIORITY;
      ithread_params.stack_size = ETCPAL_THREAD_DEFAULT_STACK;
      ithread_params.thread_name = "Identify Thread";
      ithread_params.platform_data = NULL;

      etcpal_thread_create(&device_responder_state.identify_thread, &ithread_params, identify_thread, NULL);
    }
    device_responder_state.identifying = new_identify_setting;
    *data_changed = kNoRdmnetDataChanged;
    return true;
  }
  else
  {
    *nack_reason = E120_NR_FORMAT_ERROR;
    return false;
  }
}

bool set_device_label(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                      rdmnet_data_changed_t* data_changed)
{
  if (param_data_len >= 1)
  {
    if (param_data_len > DEVICE_LABEL_MAX_LEN)
      param_data_len = DEVICE_LABEL_MAX_LEN;
    memcpy(device_responder_state.device_label, param_data, param_data_len);
    device_responder_state.device_label[param_data_len] = '\0';
    *data_changed = kNoRdmnetDataChanged;
    return true;
  }
  else
  {
    *nack_reason = E120_NR_FORMAT_ERROR;
    return false;
  }
}

bool set_component_scope(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                         rdmnet_data_changed_t* data_changed)
{
  if (param_data_len == (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2))
  {
    if (etcpal_upack_16b(param_data) == 1)
    {
      const uint8_t* cur_ptr = param_data + 2;
      char new_scope[E133_SCOPE_STRING_PADDED_LENGTH];
      bool have_new_static_broker = false;
      EtcPalSockaddr new_static_broker = {0};

      strncpy(new_scope, (const char*)cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH);
      new_scope[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      switch (*cur_ptr++)
      {
        case E133_STATIC_CONFIG_IPV4:
          ETCPAL_IP_SET_V4_ADDRESS(&new_static_broker.ip, etcpal_upack_32b(cur_ptr));
          cur_ptr += 4 + 16;
          new_static_broker.port = etcpal_upack_16b(cur_ptr);
          have_new_static_broker = true;
          break;
        case E133_STATIC_CONFIG_IPV6:
          cur_ptr += 4;
          ETCPAL_IP_SET_V6_ADDRESS(&new_static_broker.ip, cur_ptr);
          cur_ptr += 16;
          new_static_broker.port = etcpal_upack_16b(cur_ptr);
          have_new_static_broker = true;
          break;
        case E133_NO_STATIC_CONFIG:
        default:
          break;
      }

      RdmnetScopeConfig* existing_scope_config = &device_responder_state.scope_config;
      if (strncmp((char*)&param_data[2], existing_scope_config->scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0 &&
          ((!have_new_static_broker && !existing_scope_config->has_static_broker_addr) ||
           (etcpal_ip_equal(&new_static_broker.ip, &existing_scope_config->static_broker_addr.ip) &&
            new_static_broker.port == existing_scope_config->static_broker_addr.port)))
      {
        /* Same settings as current */
        *data_changed = kNoRdmnetDataChanged;
      }
      else
      {
        rdmnet_safe_strncpy(existing_scope_config->scope, new_scope, E133_SCOPE_STRING_PADDED_LENGTH);
        existing_scope_config->has_static_broker_addr = have_new_static_broker;
        existing_scope_config->static_broker_addr = new_static_broker;
        *data_changed = kRdmnetScopeConfigChanged;
      }
      return true;
    }
    else
    {
      *nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }
  }
  else
  {
    *nack_reason = E120_NR_FORMAT_ERROR;
  }
  return false;
}

bool set_search_domain(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                       rdmnet_data_changed_t* data_changed)
{
  if (param_data_len <= E133_DOMAIN_STRING_PADDED_LENGTH)
  {
    if (param_data_len > 0 || strcmp("", (char*)param_data) != 0)
    {
      if (strncmp(device_responder_state.search_domain, (char*)param_data, E133_DOMAIN_STRING_PADDED_LENGTH) == 0)
      {
        /* Same domain as current */
        *data_changed = kNoRdmnetDataChanged;
      }
      else
      {
        strncpy(device_responder_state.search_domain, (char*)param_data, E133_DOMAIN_STRING_PADDED_LENGTH);
        *data_changed = kRdmnetSearchDomainChanged;
      }
    }
    else
    {
      *nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }
    return true;
  }
  else
  {
    *nack_reason = E120_NR_FORMAT_ERROR;
  }

  return false;
}

bool set_tcp_comms_status(const uint8_t* param_data, uint8_t param_data_len, uint16_t* nack_reason,
                          rdmnet_data_changed_t* data_changed)
{
  *data_changed = kNoRdmnetDataChanged;

  if (param_data_len == E133_SCOPE_STRING_PADDED_LENGTH)
  {
    if (strncmp((char*)param_data, device_responder_state.scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0)
    {
      /* Same scope as current */
      device_responder_state.tcp_unhealthy_counter = 0;
      return true;
    }
    else
    {
      *nack_reason = E133_NR_UNKNOWN_SCOPE;
    }
  }
  else
  {
    *nack_reason = E120_NR_FORMAT_ERROR;
  }

  return false;
}

bool get_identify_device(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                         size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  resp_data_list[0].data[0] = device_responder_state.identifying ? 1 : 0;
  resp_data_list[0].datalen = 1;
  *num_responses = 1;
  return true;
}

bool get_device_label(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                      size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strncpy((char*)resp_data_list[0].data, device_responder_state.device_label, DEVICE_LABEL_MAX_LEN);
  resp_data_list[0].datalen = (uint8_t)strnlen(device_responder_state.device_label, DEVICE_LABEL_MAX_LEN);
  *num_responses = 1;
  return true;
}

bool get_component_scope(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                         size_t* num_responses, uint16_t* nack_reason)
{
  if (param_data_len >= 2)
  {
    if (etcpal_upack_16b(param_data) == 1)
    {
      const RdmnetScopeConfig* scope_config = &device_responder_state.scope_config;

      // Pack the scope
      uint8_t* cur_ptr = resp_data_list[0].data;
      etcpal_pack_16b(cur_ptr, 1);
      cur_ptr += 2;
      rdmnet_safe_strncpy((char*)cur_ptr, scope_config->scope, E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      // Pack the static config data
      if (scope_config->has_static_broker_addr)
      {
        if (ETCPAL_IP_IS_V4(&scope_config->static_broker_addr.ip))
        {
          *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
          etcpal_pack_32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&scope_config->static_broker_addr.ip));
          cur_ptr += 4 + 16;
          etcpal_pack_16b(cur_ptr, scope_config->static_broker_addr.port);
          cur_ptr += 2;
        }
        else  // V6
        {
          *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
          cur_ptr += 4;
          memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&scope_config->static_broker_addr.ip), 16);
          cur_ptr += 16;
          etcpal_pack_16b(cur_ptr, scope_config->static_broker_addr.port);
          cur_ptr += 2;
        }
      }
      else
      {
        *cur_ptr++ = E133_NO_STATIC_CONFIG;
        memset(cur_ptr, 0, 4 + 16 + 2);
        cur_ptr += 4 + 16 + 2;
      }
      resp_data_list[0].datalen = (uint8_t)(cur_ptr - resp_data_list[0].data);
      *num_responses = 1;
      return true;
    }
    else
    {
      *nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }
  }
  else
  {
    *nack_reason = E120_NR_FORMAT_ERROR;
  }
  return false;
}

bool get_search_domain(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                       size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;
  strncpy((char*)resp_data_list[0].data, device_responder_state.search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  resp_data_list[0].datalen = (uint8_t)strlen(device_responder_state.search_domain);
  *num_responses = 1;
  return true;
}

bool get_tcp_comms_status(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                          size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;
  uint8_t* cur_ptr = resp_data_list[0].data;

  memcpy(cur_ptr, device_responder_state.scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  if (device_responder_state.connected)
  {
    if (ETCPAL_IP_IS_V4(&device_responder_state.cur_broker_addr.ip))
    {
      etcpal_pack_32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&device_responder_state.cur_broker_addr.ip));
      cur_ptr += 4;
      memset(cur_ptr, 0, ETCPAL_IPV6_BYTES);
      cur_ptr += ETCPAL_IPV6_BYTES;
    }
    else
    {
      etcpal_pack_32b(cur_ptr, 0);
      cur_ptr += 4;
      memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&device_responder_state.cur_broker_addr.ip), ETCPAL_IPV6_BYTES);
      cur_ptr += ETCPAL_IPV6_BYTES;
    }
    etcpal_pack_16b(cur_ptr, device_responder_state.cur_broker_addr.port);
    cur_ptr += 2;
  }
  else
  {
    memset(cur_ptr, 0, 22);
    cur_ptr += 22;
  }
  etcpal_pack_16b(cur_ptr, device_responder_state.tcp_unhealthy_counter);
  cur_ptr += 2;
  resp_data_list[0].datalen = (uint8_t)(cur_ptr - resp_data_list[0].data);
  *num_responses = 1;
  return true;
}

bool get_supported_parameters(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                              size_t* num_responses, uint16_t* nack_reason)
{
  size_t list_index = 0;
  uint8_t* cur_ptr = resp_data_list[0].data;
  size_t i;

  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  for (i = 0; i < NUM_SUPPORTED_PIDS; ++i)
  {
    etcpal_pack_16b(cur_ptr, kSupportedPIDList[i]);
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

bool get_device_info(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                     size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;
  memcpy(resp_data_list[0].data, kDeviceInfo, sizeof kDeviceInfo);
  resp_data_list[0].datalen = sizeof kDeviceInfo;
  *num_responses = 1;
  return true;
}

bool get_manufacturer_label(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                            size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char*)resp_data_list[0].data, MANUFACTURER_LABEL);
  resp_data_list[0].datalen = sizeof(MANUFACTURER_LABEL) - 1;
  *num_responses = 1;
  return true;
}

bool get_device_model_description(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                                  size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char*)resp_data_list[0].data, DEVICE_MODEL_DESCRIPTION);
  resp_data_list[0].datalen = sizeof(DEVICE_MODEL_DESCRIPTION) - 1;
  *num_responses = 1;
  return true;
}

bool get_software_version_label(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                                size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char*)resp_data_list[0].data, SOFTWARE_VERSION_LABEL);
  resp_data_list[0].datalen = sizeof(SOFTWARE_VERSION_LABEL) - 1;
  *num_responses = 1;
  return true;
}

bool get_endpoint_list(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                       size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  uint8_t* cur_ptr = resp_data_list[0].data;

  /* Hardcoded: no endpoints other than NULL_ENDPOINT. NULL_ENDPOINT is not
   * reported in this response. */
  resp_data_list[0].datalen = 4;
  etcpal_pack_32b(cur_ptr, device_responder_state.endpoint_list_change_number);
  *num_responses = 1;
  return true;
}

bool get_endpoint_responders(const uint8_t* param_data, uint8_t param_data_len, param_data_list_t resp_data_list,
                             size_t* num_responses, uint16_t* nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)resp_data_list;
  (void)num_responses;

  if (param_data_len >= 2)
  {
    /* We have no valid endpoints for this message */
    *nack_reason = E137_7_NR_ENDPOINT_NUMBER_INVALID;
  }
  else
    *nack_reason = E120_NR_FORMAT_ERROR;
  return false;
}

//resp_process_result_t default_responder_supported_params(PidHandlerData* data)
//{
//  return kRespNoSend;
//}

resp_process_result_t default_responder_parameter_description(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

resp_process_result_t default_responder_device_model_description(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

//resp_process_result_t default_responder_manufacturer_label(PidHandlerData* data)
//{
//  return kRespNoSend;
//}

resp_process_result_t default_responder_device_label(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

resp_process_result_t default_responder_software_version_label(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

resp_process_result_t default_responder_identify_device(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

resp_process_result_t default_responder_component_scope(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

resp_process_result_t default_responder_search_domain(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

resp_process_result_t default_responder_tcp_comms_status(PidHandlerData* data)
{
  return kRespNoSend;  // TODO: Not yet implemented
}

uint8_t default_responder_get_message_count()
{
  return 0;  // TODO: Not yet implemented
}

void default_responder_get_next_queue_message(GetNextQueueMessageData* data)
{
  // TODO: Not yet implemented
}
