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
#include "etcpal/common.h"
#include "etcpal/pack.h"
#include "etcpal/thread.h"
#include "rdm/defs.h"
#include "rdmnet/defs.h"
#include "rdmnet/version.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/**************************** Private constants ******************************/

static const uint16_t kSupportedPIDList[NUM_SUPPORTED_PIDS] = {
    E120_IDENTIFY_DEVICE,    E120_SUPPORTED_PARAMETERS,     E120_DEVICE_INFO,
    E120_MANUFACTURER_LABEL, E120_DEVICE_MODEL_DESCRIPTION, E120_SOFTWARE_VERSION_LABEL,
    E120_DEVICE_LABEL,       E133_COMPONENT_SCOPE,          E133_SEARCH_DOMAIN,
};

static const uint8_t kDeviceInfo[] = {
    0x01, 0x00, /* RDM Protocol version */
    0xe1, 0x33, /* Device Model ID */
    0x71, 0x01, /* Product Category */

    /* Software Version ID */
    RDMNET_VERSION_MAJOR, RDMNET_VERSION_MINOR, RDMNET_VERSION_PATCH,
    (RDMNET_VERSION_BUILD > 0xff ? 0xff : RDMNET_VERSION_BUILD),

    0x00, 0x00, /* DMX512 Footprint */
    0x00, 0x00, /* DMX512 Personality */
    0xff, 0xff, /* DMX512 Start Address */
    0x00, 0x00, /* Sub-device count */
    0x00        /* Sensor count */
};

#define DEVICE_LABEL_MAX_LEN 32
#define DEFAULT_DEVICE_LABEL "My ETC RDMnet Device"
#define SOFTWARE_VERSION_LABEL RDMNET_VERSION_STRING
#define MANUFACTURER_LABEL "ETC"
#define DEVICE_MODEL_DESCRIPTION "Prototype RDMnet Device"

/**************************** Private variables ******************************/

static struct DefaultResponderPropertyData
{
  etcpal_thread_t identify_thread;
  bool identifying;
  char device_label[DEVICE_LABEL_MAX_LEN + 1];
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  EtcPalSockAddr static_broker_addr;
  char search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];
} prop_data;

/*********************** Private function prototypes *************************/

/* SET COMMANDS */
void set_identify_device(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);
void set_device_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);
void set_component_scope(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);
void set_search_domain(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response);

/* GET COMMANDS */
void get_identify_device(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                         uint8_t* response_buf);
void get_device_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                      uint8_t* response_buf);
void get_component_scope(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                         uint8_t* response_buf);
void get_search_domain(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
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

/*************************** Function definitions ****************************/

void default_responder_init(const char* scope, const EtcPalSockAddr* static_broker_addr)
{
  strcpy(prop_data.device_label, DEFAULT_DEVICE_LABEL);
  strcpy(prop_data.search_domain, E133_DEFAULT_DOMAIN);
  strcpy(prop_data.scope, scope);
  prop_data.static_broker_addr = *static_broker_addr;
}

void default_responder_deinit()
{
  if (prop_data.identifying)
  {
    prop_data.identifying = false;
    etcpal_thread_join(&prop_data.identify_thread);
  }
  memset(&prop_data, 0, sizeof(prop_data));
}

const char* default_responder_get_scope(void)
{
  return prop_data.scope;
}

void default_responder_get_static_broker_addr(EtcPalSockAddr* addr)
{
  if (addr)
    *addr = prop_data.static_broker_addr;
}

const char* default_responder_get_search_domain(void)
{
  return prop_data.search_domain;
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
    case E120_SUPPORTED_PARAMETERS:
    case E120_MANUFACTURER_LABEL:
    case E120_DEVICE_MODEL_DESCRIPTION:
    case E120_SOFTWARE_VERSION_LABEL:
      RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnsupportedCommandClass);
      break;
    default:
      RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnknownPid);
      break;
  }
}

void default_responder_get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len,
                           RdmnetSyncRdmResponse* response, uint8_t* response_buf)
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
    default:
      RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnknownPid);
      break;
  }
}

void identify_thread(void* arg)
{
  ETCPAL_UNUSED_ARG(arg);

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
      EtcPalThreadParams ithread_params = ETCPAL_THREAD_PARAMS_INIT;
      ithread_params.thread_name = "Identify Thread";
      etcpal_thread_create(&prop_data.identify_thread, &ithread_params, identify_thread, NULL);
    }
    prop_data.identifying = new_identify_setting;
    RDMNET_SYNC_SEND_RDM_ACK(response, 0);
  }
  else
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
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
    RDMNET_SYNC_SEND_RDM_ACK(response, 0);
  }
  else
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
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
      EtcPalSockAddr new_static_broker = ETCPAL_IP_INVALID_INIT;

      strncpy(new_scope, (const char*)cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH);
      new_scope[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      switch (*cur_ptr++)
      {
        case E133_STATIC_CONFIG_IPV4:
          ETCPAL_IP_SET_V4_ADDRESS(&new_static_broker.ip, etcpal_unpack_u32b(cur_ptr));
          cur_ptr += 4 + 16;
          new_static_broker.port = etcpal_unpack_u16b(cur_ptr);
          break;
        case E133_STATIC_CONFIG_IPV6:
          cur_ptr += 4;
          ETCPAL_IP_SET_V6_ADDRESS(&new_static_broker.ip, cur_ptr);
          cur_ptr += 16;
          new_static_broker.port = etcpal_unpack_u16b(cur_ptr);
          break;
        case E133_NO_STATIC_CONFIG:
        default:
          break;
      }

      strcpy(prop_data.scope, new_scope);
      prop_data.static_broker_addr = new_static_broker;
      RDMNET_SYNC_SEND_RDM_ACK(response, 0);
    }
    else
    {
      RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRDataOutOfRange);
    }
  }
  else
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
  }
}

void set_search_domain(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response)
{
  if (param_data_len <= E133_DOMAIN_STRING_PADDED_LENGTH)
  {
    if (param_data_len > 0 || strcmp("", (char*)param_data) != 0)
    {
      strncpy(prop_data.search_domain, (char*)param_data, E133_DOMAIN_STRING_PADDED_LENGTH);
      RDMNET_SYNC_SEND_RDM_ACK(response, 0);
    }
    else
    {
      RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRDataOutOfRange);
    }
  }
  else
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
  }
}

void get_identify_device(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                         uint8_t* response_buf)
{
  ETCPAL_UNUSED_ARG(param_data);
  ETCPAL_UNUSED_ARG(param_data_len);

  response_buf[0] = prop_data.identifying ? 1 : 0;
  RDMNET_SYNC_SEND_RDM_ACK(response, 1);
}

void get_device_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                      uint8_t* response_buf)
{
  ETCPAL_UNUSED_ARG(param_data);
  ETCPAL_UNUSED_ARG(param_data_len);

  strncpy((char*)response_buf, prop_data.device_label, DEVICE_LABEL_MAX_LEN);
  RDMNET_SYNC_SEND_RDM_ACK(response, (uint8_t)strnlen(prop_data.device_label, DEVICE_LABEL_MAX_LEN));
}

void get_component_scope(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                         uint8_t* response_buf)
{
  if (param_data_len >= 2)
  {
    if (etcpal_unpack_u16b(param_data) == 1)
    {
      // Pack the scope
      uint8_t* cur_ptr = response_buf;
      etcpal_pack_u16b(cur_ptr, 1);
      cur_ptr += 2;
      strncpy((char*)cur_ptr, prop_data.scope, E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH - 1;
      *cur_ptr++ = 0;

      // Pack the static config data
      if (!ETCPAL_IP_IS_INVALID(&prop_data.static_broker_addr.ip))
      {
        if (ETCPAL_IP_IS_V4(&prop_data.static_broker_addr.ip))
        {
          *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
          etcpal_pack_u32b(cur_ptr, ETCPAL_IP_V4_ADDRESS(&prop_data.static_broker_addr.ip));
          cur_ptr += 4 + 16;
          etcpal_pack_u16b(cur_ptr, prop_data.static_broker_addr.port);
          cur_ptr += 2;
        }
        else  // V6
        {
          *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
          cur_ptr += 4;
          memcpy(cur_ptr, ETCPAL_IP_V6_ADDRESS(&prop_data.static_broker_addr.ip), 16);
          cur_ptr += 16;
          etcpal_pack_u16b(cur_ptr, prop_data.static_broker_addr.port);
          cur_ptr += 2;
        }
      }
      else
      {
        *cur_ptr++ = E133_NO_STATIC_CONFIG;
        memset(cur_ptr, 0, 4 + 16 + 2);
        cur_ptr += 4 + 16 + 2;
      }
      RDMNET_SYNC_SEND_RDM_ACK(response, (uint8_t)(cur_ptr - response_buf));
    }
    else
    {
      RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRDataOutOfRange);
    }
  }
  else
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRFormatError);
  }
}

void get_search_domain(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                       uint8_t* response_buf)
{
  ETCPAL_UNUSED_ARG(param_data);
  ETCPAL_UNUSED_ARG(param_data_len);

  strncpy((char*)response_buf, prop_data.search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  RDMNET_SYNC_SEND_RDM_ACK(response, (uint8_t)strlen(prop_data.search_domain));
}

void get_supported_parameters(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                              uint8_t* response_buf)
{
  uint8_t* cur_ptr = response_buf;

  ETCPAL_UNUSED_ARG(param_data);
  ETCPAL_UNUSED_ARG(param_data_len);

  for (size_t i = 0; i < NUM_SUPPORTED_PIDS; ++i)
  {
    etcpal_pack_u16b(cur_ptr, kSupportedPIDList[i]);
    cur_ptr += 2;
  }

  RDMNET_SYNC_SEND_RDM_ACK(response, (uint8_t)(cur_ptr - response_buf));
}

void get_device_info(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                     uint8_t* response_buf)
{
  ETCPAL_UNUSED_ARG(param_data);
  ETCPAL_UNUSED_ARG(param_data_len);

  memcpy(response_buf, kDeviceInfo, sizeof kDeviceInfo);
  RDMNET_SYNC_SEND_RDM_ACK(response, sizeof kDeviceInfo);
}

void get_manufacturer_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                            uint8_t* response_buf)
{
  ETCPAL_UNUSED_ARG(param_data);
  ETCPAL_UNUSED_ARG(param_data_len);

  strcpy((char*)response_buf, MANUFACTURER_LABEL);
  RDMNET_SYNC_SEND_RDM_ACK(response, sizeof(MANUFACTURER_LABEL) - 1);
}

void get_device_model_description(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                                  uint8_t* response_buf)
{
  ETCPAL_UNUSED_ARG(param_data);
  ETCPAL_UNUSED_ARG(param_data_len);

  strcpy((char*)response_buf, DEVICE_MODEL_DESCRIPTION);
  RDMNET_SYNC_SEND_RDM_ACK(response, sizeof(DEVICE_MODEL_DESCRIPTION) - 1);
}

void get_software_version_label(const uint8_t* param_data, uint8_t param_data_len, RdmnetSyncRdmResponse* response,
                                uint8_t* response_buf)
{
  ETCPAL_UNUSED_ARG(param_data);
  ETCPAL_UNUSED_ARG(param_data_len);

  strcpy((char*)response_buf, SOFTWARE_VERSION_LABEL);
  RDMNET_SYNC_SEND_RDM_ACK(response, sizeof(SOFTWARE_VERSION_LABEL) - 1);
}
