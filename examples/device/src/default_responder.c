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

/**************************** Private variables ******************************/

static struct DefaultResponderPropertyData
{
  uint32_t endpoint_list_change_number;
  etcpal_thread_t identify_thread;
  bool identifying;
  char device_label[DEVICE_LABEL_MAX_LEN + 1];
  RdmnetScopeConfig scope_config;
  char search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];
  uint16_t tcp_unhealthy_counter;
  bool connected;
  EtcPalSockAddr cur_broker_addr;
} prop_data;

static etcpal_rwlock_t prop_lock;

/*********************** Private function prototypes *************************/

/* SET COMMANDS */
void set_identify_device(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);
void set_device_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);
void set_component_scope(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);
void set_search_domain(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);
void set_tcp_comms_status(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);

/* GET COMMANDS */
void get_identify_device(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                         uint8_t* response_buf);
void get_device_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                      uint8_t* response_buf);
void get_component_scope(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                         uint8_t* response_buf);
void get_search_domain(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                       uint8_t* response_buf);
void get_tcp_comms_status(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                          uint8_t* response_buf);
void get_supported_parameters(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                              uint8_t* response_buf);
void get_device_info(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                     uint8_t* response_buf);
void get_manufacturer_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                            uint8_t* response_buf);
void get_device_model_description(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                                  uint8_t* response_buf);
void get_software_version_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                                uint8_t* response_buf);
void get_endpoint_list(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                       uint8_t* response_buf);
void get_endpoint_responders(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                             uint8_t* response_buf);

/*************************** Function definitions ****************************/

void default_responder_init(const RdmnetScopeConfig* scope_config, const char* search_domain)
{
  etcpal_rwlock_create(&prop_lock);

  rdmnet_safe_strncpy(prop_data.device_label, DEFAULT_DEVICE_LABEL, DEVICE_LABEL_MAX_LEN);
  rdmnet_safe_strncpy(prop_data.search_domain, search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  prop_data.scope_config = *scope_config;
}

void default_responder_deinit()
{
  if (prop_data.identifying)
  {
    prop_data.identifying = false;
    etcpal_thread_join(&prop_data.identify_thread);
  }
  memset(&prop_data, 0, sizeof(prop_data));
  etcpal_rwlock_destroy(&prop_lock);
}

void default_responder_get_scope_config(RdmnetScopeConfig* scope_config)
{
  if (etcpal_rwlock_readlock(&prop_lock))
  {
    *scope_config = prop_data.scope_config;
    etcpal_rwlock_readunlock(&prop_lock);
  }
}

void default_responder_get_search_domain(char* search_domain)
{
  if (etcpal_rwlock_readlock(&prop_lock))
  {
    rdmnet_safe_strncpy(search_domain, prop_data.search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
    etcpal_rwlock_readunlock(&prop_lock);
  }
}

void default_responder_update_connection_status(bool connected, const EtcPalSockAddr* broker_addr)
{
  if (etcpal_rwlock_readlock(&prop_lock))
  {
    prop_data.connected = connected;
    if (prop_data.connected)
      prop_data.cur_broker_addr = *broker_addr;
    etcpal_rwlock_readunlock(&prop_lock);
  }
}

void default_responder_incr_unhealthy_count()
{
  if (etcpal_rwlock_writelock(&prop_lock))
  {
    ++prop_data.tcp_unhealthy_counter;
    etcpal_rwlock_writeunlock(&prop_lock);
  }
}

void default_responder_set_tcp_status(EtcPalSockAddr* broker_addr)
{
  if (etcpal_rwlock_writelock(&prop_lock))
  {
    prop_data.cur_broker_addr = *broker_addr;
    etcpal_rwlock_writeunlock(&prop_lock);
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

void default_responder_set(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len,
                           RdmnetSyncRdmResponse* response)
{
  bool res = false;
  if (etcpal_rwlock_writelock(&prop_lock))
  {
    switch (pid)
    {
      case E120_IDENTIFY_DEVICE:
        set_identify_device(param_data, param_data_len, response);
        break;
      case E120_DEVICE_LABEL:
        set_device_label(param_data, param_data_len, response);
        break;
      case E133_COMPONENT_SCOPE:
        set_component_scope(param_data, param_data_len, response);
        break;
      case E133_SEARCH_DOMAIN:
        set_search_domain(param_data, param_data_len, response);
        break;
      case E133_TCP_COMMS_STATUS:
        set_tcp_comms_status(param_data, param_data_len, response);
        break;
      case E120_SUPPORTED_PARAMETERS:
      case E120_MANUFACTURER_LABEL:
      case E120_DEVICE_MODEL_DESCRIPTION:
      case E120_SOFTWARE_VERSION_LABEL:
      case E137_7_ENDPOINT_LIST:
      case E137_7_ENDPOINT_RESPONDERS:
        response->ack = false;
        response->nack_reason = kRdmNRUnsupportedCommandClass;
        break;
      default:
        response->ack = false;
        response->nack_reason = kRdmNRUnknownPid;
        break;
    }
    etcpal_rwlock_writeunlock(&prop_lock);
  }
  return res;
}

void default_responder_get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len,
                           RdmnetSyncRdmResponse* response, uint8_t* response_buf)
{
  bool res = false;
  if (etcpal_rwlock_readlock(&prop_lock))
  {
    switch (pid)
    {
      case E120_IDENTIFY_DEVICE:
        get_identify_device(param_data, param_data_len, response, response_buf);
        break;
      case E120_DEVICE_LABEL:
        get_device_label(param_data, param_data_len, response, response_buf);
        break;
      case E133_COMPONENT_SCOPE:
        get_component_scope(param_data, param_data_len, response, response_buf);
        break;
      case E133_SEARCH_DOMAIN:
        get_search_domain(param_data, param_data_len, response, response_buf);
        break;
      case E133_TCP_COMMS_STATUS:
        get_tcp_comms_status(param_data, param_data_len, response, response_buf);
        break;
      case E120_SUPPORTED_PARAMETERS:
        get_supported_parameters(param_data, param_data_len, response, response_buf);
        break;
      case E120_DEVICE_INFO:
        get_device_info(param_data, param_data_len, response, response_buf);
        break;
      case E120_MANUFACTURER_LABEL:
        get_manufacturer_label(param_data, param_data_len, response, response_buf);
        break;
      case E120_DEVICE_MODEL_DESCRIPTION:
        get_device_model_description(param_data, param_data_len, response, response_buf);
        break;
      case E120_SOFTWARE_VERSION_LABEL:
        get_software_version_label(param_data, param_data_len, response, response_buf);
        break;
      case E137_7_ENDPOINT_LIST:
        get_endpoint_list(param_data, param_data_len, response, response_buf);
        break;
      case E137_7_ENDPOINT_RESPONDERS:
        get_endpoint_responders(param_data, param_data_len, response, response_buf);
        break;
      default:
        response->ack = false;
        response->nack_reason = kRdmNRUnknownPid;
        break;
    }
    etcpal_rwlock_readunlock(&prop_lock);
  }
  return res;
}

void identify_thread(void* arg)
{
  (void)arg;

  while (prop_data.identifying)
  {
    printf("I AM IDENTIFYING!!!\n");
    etcpal_thread_sleep(1000);
  }
}

void set_identify_device(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response)
{
  if (param_data_len >= 1)
  {
    bool new_identify_setting = (bool)(*param_data);
    if (new_identify_setting && !prop_data.identifying)
    {
      EtcPalThreadParams ithread_params;

      ithread_params.priority = ETCPAL_THREAD_DEFAULT_PRIORITY;
      ithread_params.stack_size = ETCPAL_THREAD_DEFAULT_STACK;
      ithread_params.thread_name = "Identify Thread";
      ithread_params.platform_data = NULL;

      etcpal_thread_create(&prop_data.identify_thread, &ithread_params, identify_thread, NULL);
    }
    prop_data.identifying = new_identify_setting;
    response->ack = true;
  }
  else
  {
    response->ack = false;
    response->nack_reason = kRdmNRFormatError;
    return false;
  }
}

void set_device_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response)
{
  if (param_data_len >= 1)
  {
    if (param_data_len > DEVICE_LABEL_MAX_LEN)
      param_data_len = DEVICE_LABEL_MAX_LEN;
    memcpy(prop_data.device_label, param_data, param_data_len);
    prop_data.device_label[param_data_len] = '\0';
    response->ack = true;
  }
  else
  {
    response->ack = false;
    response->nack_reason = kRdmNRFormatError;
    return false;
  }
}

void set_component_scope(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response)
{
  if (param_data_len == (2 + E133_SCOPE_STRING_PADDED_LENGTH + 1 + 4 + 16 + 2))
  {
    if (etcpal_unpack_u16b(param_data) == 1)
    {
      const uint8_t* cur_ptr = param_data + 2;
      char new_scope[E133_SCOPE_STRING_PADDED_LENGTH];
      bool have_new_static_broker = false;
      EtcPalSockAddr new_static_broker = {0};

      strncpy(new_scope, (const char*)cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH);
      new_scope[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      switch (*cur_ptr++)
      {
        case E133_STATIC_CONFIG_IPV4:
          ETCPAL_IP_SET_V4_ADDRESS(&new_static_broker.ip, etcpal_unpack_u32b(cur_ptr));
          cur_ptr += 4 + 16;
          new_static_broker.port = etcpal_unpack_u16b(cur_ptr);
          have_new_static_broker = true;
          break;
        case E133_STATIC_CONFIG_IPV6:
          cur_ptr += 4;
          ETCPAL_IP_SET_V6_ADDRESS(&new_static_broker.ip, cur_ptr);
          cur_ptr += 16;
          new_static_broker.port = etcpal_unpack_u16b(cur_ptr);
          have_new_static_broker = true;
          break;
        case E133_NO_STATIC_CONFIG:
        default:
          break;
      }

      RdmnetScopeConfig* existing_scope_config = &prop_data.scope_config;
      rdmnet_safe_strncpy(existing_scope_config->scope, new_scope, E133_SCOPE_STRING_PADDED_LENGTH);
      existing_scope_config->has_static_broker_addr = have_new_static_broker;
      existing_scope_config->static_broker_addr = new_static_broker;
      response->ack = true;
    }
    else
    {
      response->ack = false;
      response->nack_reason = kRdmNRDataOutOfRange;
    }
  }
  else
  {
    response->ack = false;
    response->nack_reason = kRdmNRFormatError;
  }
}

void set_search_domain(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response)
{
  if (param_data_len <= E133_DOMAIN_STRING_PADDED_LENGTH)
  {
    if (param_data_len > 0 || strcmp("", (char*)param_data) != 0)
    {
      strncpy(prop_data.search_domain, (char*)param_data, E133_DOMAIN_STRING_PADDED_LENGTH);
    }
    else
    {
      response->ack = false;
      response->nack_reason = kRdmNRDataOutOfRange;
    }
  }
  else
  {
    response->ack = false;
    response->nack_reason = kRdmNRFormatError;
  }
}

void set_tcp_comms_status(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response)
{
  if (param_data_len == E133_SCOPE_STRING_PADDED_LENGTH)
  {
    if (strncmp((char*)param_data, prop_data.scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH) == 0)
    {
      // Same scope as current
      prop_data.tcp_unhealthy_counter = 0;
      response->ack = true;
    }
    else
    {
      response->ack = false;
      response->nack_reason = kRdmNRUnknownScope;
    }
  }
  else
  {
    response->ack = false;
    response->nack_reason = kRdmNRFormatError;
  }
}

void get_identify_device(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                         uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  response->ack = true;
  response_buf[0] = prop_data.identifying ? 1 : 0;
  response->response_data_len = 1;
}

void get_device_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                      uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  response->ack = true;
  strncpy((char*)response_buf, prop_data.device_label, DEVICE_LABEL_MAX_LEN);
  response->response_data_len = (uint8_t)strnlen(prop_data.device_label, DEVICE_LABEL_MAX_LEN);
}

void get_component_scope(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                         uint8_t* response_buf)
{
  if (param_data_len >= 2)
  {
    if (etcpal_unpack_u16b(param_data) == 1)
    {
      const RdmnetScopeConfig* scope_config = &prop_data.scope_config;

      // Pack the scope
      uint8_t* cur_ptr = response_buf;
      etcpal_pack_u16b(cur_ptr, 1);
      cur_ptr += 2;
      rdmnet_safe_strncpy((char*)cur_ptr, scope_config->scope, E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      // Pack the static config data
      if (scope_config->has_static_broker_addr)
      {
        if (ETCPAL_IP_IS_V4(&scope_config->static_broker_addr.ip))
        {
          *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
          etcpal_pack_u32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&scope_config->static_broker_addr.ip));
          cur_ptr += 4 + 16;
          etcpal_pack_u16b(cur_ptr, scope_config->static_broker_addr.port);
          cur_ptr += 2;
        }
        else  // V6
        {
          *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
          cur_ptr += 4;
          memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&scope_config->static_broker_addr.ip), 16);
          cur_ptr += 16;
          etcpal_pack_u16b(cur_ptr, scope_config->static_broker_addr.port);
          cur_ptr += 2;
        }
      }
      else
      {
        *cur_ptr++ = E133_NO_STATIC_CONFIG;
        memset(cur_ptr, 0, 4 + 16 + 2);
        cur_ptr += 4 + 16 + 2;
      }
      response->ack = true;
      response->response_data_len = (uint8_t)(cur_ptr - response_buf);
    }
    else
    {
      response->ack = false;
      response->nack_reason = kRdmNRDataOutOfRange;
    }
  }
  else
  {
    response->ack = false;
    response->nack_reason = kRdmNRFormatError;
  }
}

void get_search_domain(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                       uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  response->ack = true;
  strncpy((char*)response_buf, prop_data.search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  response->response_data_len = (uint8_t)strlen(prop_data.search_domain);
}

void get_tcp_comms_status(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                          uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  uint8_t* cur_ptr = response_buf;

  memcpy(cur_ptr, prop_data.scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  if (prop_data.connected)
  {
    if (ETCPAL_IP_IS_V4(&prop_data.cur_broker_addr.ip))
    {
      etcpal_pack_u32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&prop_data.cur_broker_addr.ip));
      cur_ptr += 4;
      memset(cur_ptr, 0, ETCPAL_IPV6_BYTES);
      cur_ptr += ETCPAL_IPV6_BYTES;
    }
    else
    {
      etcpal_pack_u32b(cur_ptr, 0);
      cur_ptr += 4;
      memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&prop_data.cur_broker_addr.ip), ETCPAL_IPV6_BYTES);
      cur_ptr += ETCPAL_IPV6_BYTES;
    }
    etcpal_pack_u16b(cur_ptr, prop_data.cur_broker_addr.port);
    cur_ptr += 2;
  }
  else
  {
    memset(cur_ptr, 0, 22);
    cur_ptr += 22;
  }
  etcpal_pack_u16b(cur_ptr, prop_data.tcp_unhealthy_counter);
  cur_ptr += 2;

  response->ack = true;
  response->response_data_len = (uint8_t)(cur_ptr - response_buf);
}

void get_supported_parameters(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                              uint8_t* response_buf)
{
  size_t list_index = 0;
  uint8_t* cur_ptr = response_buf;
  size_t i;

  (void)param_data;
  (void)param_data_len;

  for (i = 0; i < NUM_SUPPORTED_PIDS; ++i)
  {
    etcpal_pack_u16b(cur_ptr, kSupportedPIDList[i]);
    cur_ptr += 2;
  }

  response->ack = true;
  response->response_data_len = (uint8_t)(cur_ptr - response_buf);
}

void get_device_info(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                     uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  response->ack = true;
  memcpy(response_buf, kDeviceInfo, sizeof kDeviceInfo);
  response->response_data_len = sizeof kDeviceInfo;
}

void get_manufacturer_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                            uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  response->ack = true;
  strcpy((char*)response_buf, MANUFACTURER_LABEL);
  response->response_data_len = sizeof(MANUFACTURER_LABEL) - 1;
}

void get_device_model_description(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                                  uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  response->ack = true;
  strcpy((char*)response_buf, DEVICE_MODEL_DESCRIPTION);
  response->response_data_len = sizeof(DEVICE_MODEL_DESCRIPTION) - 1;
}

void get_software_version_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                                uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  response->ack = true;
  strcpy((char*)response_buf, SOFTWARE_VERSION_LABEL);
  response->response_data_len = sizeof(SOFTWARE_VERSION_LABEL) - 1;
}

void get_endpoint_list(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                       uint8_t* response_buf)
{
  (void)param_data;
  (void)param_data_len;

  // Hardcoded: no endpoints other than NULL_ENDPOINT. NULL_ENDPOINT is not reported in this response.
  response->ack = true;
  response->response_data_len = 4;
  etcpal_pack_u32b(response_buf, prop_data.endpoint_list_change_number);
}

void get_endpoint_responders(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                             uint8_t* response_buf)
{
  (void)param_data;

  if (param_data_len >= 2)
  {
    // We have no valid endpoints for this message
    response->ack = false;
    response->nack_reason = kRdmNREndpointNumberInvalid;
  }
  else
  {
    response->ack = false;
    response->nack_reason = kRdmNRFormatError;
  }
}
