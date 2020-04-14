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

#include "example_device.h"

#include <stdint.h>
#include <stdio.h>
#include "etcpal/pack.h"
#include "rdm/uid.h"
#include "rdm/defs.h"
#include "rdm/responder.h"
#include "rdm/controller.h"
#include "rdmnet/version.h"
#include "default_responder.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/***************************** Private macros ********************************/

#define rdm_uid_matches_mine(uidptr) (rdm_uid_equal(uidptr, &device_state.my_uid) || rdm_uid_is_broadcast(uidptr))

/**************************** Private variables ******************************/

static struct device_state
{
  rdmnet_device_t device_handle;
  char cur_scope[E133_SCOPE_STRING_PADDED_LENGTH];

  bool connected;

  const EtcPalLogParams* lparams;
} device_state;

static uint8_t rdm_response_buf[RDM_RESPONSE_BUF_LENGTH];

/*********************** Private function prototypes *************************/

/* RDM command handling */
static void device_handle_rdm_command(const RdmCommandHeader* rdm_header, const uint8_t* data, uint8_t data_len,
                                      RdmnetSyncRdmResponse* response);

/* Device callbacks */
static void device_connected(rdmnet_device_t handle, const RdmnetClientConnectedInfo* info, void* context);
static void device_connect_failed(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo* info, void* context);
static void device_disconnected(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo* info, void* context);
static void device_rdm_cmd_received(rdmnet_device_t handle, const RdmnetRdmCommand* cmd,
                                    RdmnetSyncRdmResponse* response, void* context);
static void device_llrp_rdm_cmd_received(rdmnet_device_t handle, const LlrpRdmCommand* cmd,
                                         RdmnetSyncRdmResponse* response, void* context);

/*************************** Function definitions ****************************/

void device_print_version()
{
  printf("ETC Example RDMnet Device\n");
  printf("Version %s\n\n", RDMNET_VERSION_STRING);
  printf("%s\n", RDMNET_VERSION_COPYRIGHT);
  printf("License: Apache License v2.0 <http://www.apache.org/licenses/LICENSE-2.0>\n");
  printf("Unless required by applicable law or agreed to in writing, this software is\n");
  printf("provided \"AS IS\", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express\n");
  printf("or implied.\n");
}

etcpal_error_t device_init(const EtcPalLogParams* lparams, const char* scope, const EtcPalSockAddr* static_broker_addr)
{
  if (!scope)
    return kEtcPalErrInvalid;

  device_state.lparams = lparams;

  etcpal_log(lparams, ETCPAL_LOG_INFO, "ETC Prototype RDMnet Device Version " RDMNET_VERSION_STRING);

  strncpy(device_state.cur_scope, scope, E133_SCOPE_STRING_PADDED_LENGTH);

  default_responder_init(scope, static_broker_addr);

  etcpal_error_t res = rdmnet_init(lparams, NULL);
  if (res != kEtcPalErrOk)
  {
    etcpal_log(lparams, ETCPAL_LOG_CRIT, "RDMnet initialization failed with error: '%s'", etcpal_strerror(res));
    return res;
  }

  RdmnetDeviceConfig config = RDMNET_DEVICE_CONFIG_DEFAULT_INIT(0x6574);

  // Give a buffer for synchronous RDM responses
  config.response_buf = rdm_response_buf;

  // A typical hardware-locked device would use etcpal_generate_v5_uuid() or
  // etcpal_generate_device_uuid() to generate a CID that is the same every time. But this example
  // device is not locked to hardware, so we'll just use the UUID format preferred by the
  // underlying OS, which is different each time it is generated.
  etcpal_generate_v4_uuid(&config.cid);
  RDMNET_CLIENT_SET_STATIC_SCOPE(&config.scope_config, scope, *static_broker_addr);
  rdmnet_device_set_callbacks(&config, device_connected, device_connect_failed, device_disconnected,
                              device_rdm_cmd_received, device_llrp_rdm_cmd_received, NULL, NULL);

  res = rdmnet_device_create(&config, &device_state.device_handle);
  if (res != kEtcPalErrOk)
  {
    etcpal_log(lparams, ETCPAL_LOG_CRIT, "Device initialization failed with error: '%s'", etcpal_strerror(res));
    rdmnet_deinit();
  }
  return res;
}

void device_deinit()
{
  rdmnet_device_destroy(device_state.device_handle, kRdmnetDisconnectShutdown);
  rdmnet_deinit();
  default_responder_deinit();
}

void device_connected(rdmnet_device_t handle, const RdmnetClientConnectedInfo* info, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(info);
  ETCPAL_UNUSED_ARG(context);

  etcpal_log(device_state.lparams, ETCPAL_LOG_INFO, "Device connected to Broker on scope '%s'.",
             default_responder_get_scope());
}

void device_connect_failed(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo* info, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(info);
  ETCPAL_UNUSED_ARG(context);

  if (info->will_retry)
  {
    etcpal_log(device_state.lparams, ETCPAL_LOG_INFO, "Connect failed to broker on scope '%s': %s. Retrying...",
               default_responder_get_scope(), rdmnet_connect_fail_event_to_string(info->event));
  }
  else
  {
    etcpal_log(device_state.lparams, ETCPAL_LOG_CRIT, "Connect to broker on scope '%s' failed FATALLY: %s",
               default_responder_get_scope(), rdmnet_connect_fail_event_to_string(info->event));
  }
  if (info->event == kRdmnetConnectFailSocketFailure || info->event == kRdmnetConnectFailTcpLevel)
  {
    etcpal_log(device_state.lparams, ETCPAL_LOG_INFO, "Socket error: '%s'", etcpal_strerror(info->socket_err));
  }
  if (info->event == kRdmnetConnectFailRejected)
  {
    etcpal_log(device_state.lparams, ETCPAL_LOG_INFO, "Reject reason: '%s'",
               rdmnet_connect_status_to_string(info->rdmnet_reason));
  }
}

void device_disconnected(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo* info, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(info);
  ETCPAL_UNUSED_ARG(context);

  if (info->will_retry)
  {
    etcpal_log(device_state.lparams, ETCPAL_LOG_INFO, "Device disconnected from broker on scope '%s': %s. Retrying...",
               default_responder_get_scope(), rdmnet_disconnect_event_to_string(info->event));
  }
  else
  {
    etcpal_log(device_state.lparams, ETCPAL_LOG_CRIT, "Device disconnected FATALLY from broker on scope '%s': %s.",
               default_responder_get_scope(), rdmnet_disconnect_event_to_string(info->event));
  }
  if (info->event == kRdmnetDisconnectAbruptClose)
  {
    etcpal_log(device_state.lparams, ETCPAL_LOG_INFO, "Socket error: '%s'", etcpal_strerror(info->socket_err));
  }
  if (info->event == kRdmnetDisconnectGracefulRemoteInitiated)
  {
    etcpal_log(device_state.lparams, ETCPAL_LOG_INFO, "Disconnect reason: '%s'",
               rdmnet_disconnect_reason_to_string(info->rdmnet_reason));
  }
}

void device_rdm_cmd_received(rdmnet_device_t handle, const RdmnetRdmCommand* command, RdmnetSyncRdmResponse* response,
                             void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(context);

  device_handle_rdm_command(&command->rdm_header, command->data, command->data_len, response);
}

void device_llrp_rdm_cmd_received(rdmnet_device_t handle, const LlrpRdmCommand* command,
                                  RdmnetSyncRdmResponse* response, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(context);

  device_handle_rdm_command(&command->rdm_header, command->data, command->data_len, response);
}

void device_handle_rdm_command(const RdmCommandHeader* rdm_header, const uint8_t* data, uint8_t data_len,
                               RdmnetSyncRdmResponse* response)
{
  if (!default_responder_supports_pid(rdm_header->param_id))
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRUnknownPid);
    if (etcpal_can_log(device_state.lparams, ETCPAL_LOG_DEBUG))
    {
      char controller_uid_str[RDM_UID_STRING_BYTES];
      rdm_uid_to_string(&rdm_header->source_uid, controller_uid_str);
      etcpal_log(device_state.lparams, ETCPAL_LOG_DEBUG, "Sending NACK to Controller %s for unknown PID 0x%04x",
                 controller_uid_str, rdm_header->param_id);
    }
  }

  switch (rdm_header->command_class)
  {
    case kRdmCCSetCommand:
      default_responder_set(rdm_header->param_id, data, data_len, response);
      break;
    case kRdmCCGetCommand:
      default_responder_get(rdm_header->param_id, data, data_len, response, rdm_response_buf);
      break;
    default:
      etcpal_log(device_state.lparams, ETCPAL_LOG_ERR, "Ignoring command with invalid command class %d",
                 rdm_header->command_class);
      break;
  }
}
