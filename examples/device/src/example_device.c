/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "example_device.h"

#include <stdio.h>
#include "lwpa/int.h"
#include "lwpa/pack.h"
#include "rdm/uid.h"
#include "rdm/defs.h"
#include "rdm/responder.h"
#include "rdm/controller.h"
#include "rdmnet/version.h"
#include "default_responder.h"

/***************************** Private macros ********************************/

#define rdm_uid_matches_mine(uidptr) (rdm_uid_equal(uidptr, &device_state.my_uid) || rdm_uid_is_broadcast(uidptr))

/**************************** Private variables ******************************/

static struct device_state
{
  bool configuration_change;

  rdmnet_device_t device_handle;
  RdmnetScopeConfig cur_scope_config;
  char cur_search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];

  bool connected;

  const LwpaLogParams *lparams;
} device_state;

/*********************** Private function prototypes *************************/

/* RDM command handling */
static void device_handle_rpt_command(const RemoteRdmCommand *cmd, rdmnet_data_changed_t *data_changed);
static void device_handle_llrp_command(const LlrpRemoteRdmCommand *cmd, rdmnet_data_changed_t *data_changed);
static bool device_handle_rdm_command(const RdmCommand *rdm_cmd, RdmResponse *resp_list, size_t *resp_list_size,
                                      uint16_t *nack_reason, rdmnet_data_changed_t *data_changed);
static void device_send_rpt_status(rpt_status_code_t status_code, const RemoteRdmCommand *received_cmd);
static void device_send_rpt_nack(uint16_t nack_reason, const RemoteRdmCommand *received_cmd);
static void device_send_rpt_response(RdmResponse *resp_list, size_t num_responses,
                                     const RemoteRdmCommand *received_cmd);
static void device_send_llrp_nack(uint16_t nack_reason, const LlrpRemoteRdmCommand *received_cmd);
static void device_send_llrp_response(RdmResponse *resp, const LlrpRemoteRdmCommand *received_cmd);

/* Device callbacks */
static void device_connected(rdmnet_device_t handle, const RdmnetClientConnectedInfo *info, void *context);
static void device_connect_failed(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo *info, void *context);
static void device_disconnected(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo *info, void *context);
static void device_rdm_cmd_received(rdmnet_device_t handle, const RemoteRdmCommand *cmd, void *context);
static void device_llrp_rdm_cmd_received(rdmnet_device_t handle, const LlrpRemoteRdmCommand *cmd, void *context);

/*************************** Function definitions ****************************/

void device_print_version()
{
  printf("ETC Prototype RDMnet Device\n");
  printf("Version %s\n\n", RDMNET_VERSION_STRING);
  printf("%s\n", RDMNET_VERSION_COPYRIGHT);
  printf("License: Apache License v2.0 <http://www.apache.org/licenses/LICENSE-2.0>\n");
  printf("Unless required by applicable law or agreed to in writing, this software is\n");
  printf("provided \"AS IS\", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express\n");
  printf("or implied.\n");
}

lwpa_error_t device_init(const RdmnetScopeConfig *scope_config, const LwpaLogParams *lparams)
{
  if (!scope_config)
    return kLwpaErrInvalid;

  device_state.lparams = lparams;

  lwpa_log(lparams, LWPA_LOG_INFO, "ETC Prototype RDMnet Device Version " RDMNET_VERSION_STRING);

  rdmnet_safe_strncpy(device_state.cur_search_domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
  device_state.cur_scope_config = *scope_config;
  default_responder_init(scope_config, device_state.cur_search_domain);

  lwpa_error_t res = rdmnet_device_init(lparams);
  if (res != kLwpaErrOk)
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "RDMnet initialization failed with error: '%s'", lwpa_strerror(res));
    return res;
  }

  RdmnetDeviceConfig config;
  RDMNET_DEVICE_CONFIG_INIT(&config, 0x6574);
  // A typical hardware-locked device would use lwpa_generate_v3_uuid() to generate a CID that is
  // the same every time. But this example device is not locked to hardware, so a V4 UUID makes
  // more sense.
  lwpa_generate_v4_uuid(&config.cid);
  config.scope_config = *scope_config;
  config.callbacks.connected = device_connected;
  config.callbacks.connect_failed = device_connect_failed;
  config.callbacks.disconnected = device_disconnected;
  config.callbacks.rdm_command_received = device_rdm_cmd_received;
  config.callbacks.llrp_rdm_command_received = device_llrp_rdm_cmd_received;
  config.callback_context = NULL;

  res = rdmnet_device_create(&config, &device_state.device_handle);
  if (res != kLwpaErrOk)
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Device initialization failed with error: '%s'", lwpa_strerror(res));
    rdmnet_core_deinit();
  }
  return res;
}

void device_deinit()
{
  rdmnet_device_destroy(device_state.device_handle);
  rdmnet_core_deinit();
  default_responder_deinit();
}

void device_connected(rdmnet_device_t handle, const RdmnetClientConnectedInfo *info, void *context)
{
  (void)handle;
  (void)context;

  default_responder_update_connection_status(true, &info->broker_addr);
  lwpa_log(device_state.lparams, LWPA_LOG_INFO, "Device connected to Broker on scope '%s'.",
           device_state.cur_scope_config.scope);
}

void device_connect_failed(rdmnet_device_t handle, const RdmnetClientConnectFailedInfo *info, void *context)
{
  (void)handle;
  (void)info;
  (void)context;
}

void device_disconnected(rdmnet_device_t handle, const RdmnetClientDisconnectedInfo *info, void *context)
{
  (void)handle;
  (void)context;
  (void)info;

  default_responder_update_connection_status(false, NULL);
  lwpa_log(device_state.lparams, LWPA_LOG_INFO, "Device disconnected from Broker on scope '%s'.",
           device_state.cur_scope_config.scope);
}

void device_rdm_cmd_received(rdmnet_device_t handle, const RemoteRdmCommand *cmd, void *context)
{
  (void)handle;
  (void)context;

  rdmnet_data_changed_t data_changed = kNoRdmnetDataChanged;
  device_handle_rpt_command(cmd, &data_changed);
  if (data_changed == kRdmnetScopeConfigChanged)
  {
    default_responder_get_scope_config(&device_state.cur_scope_config);
    rdmnet_device_change_scope(handle, &device_state.cur_scope_config, kRdmnetDisconnectRptReconfigure);
  }
  else if (data_changed == kRdmnetSearchDomainChanged)
  {
    default_responder_get_search_domain(device_state.cur_search_domain);
    rdmnet_device_change_search_domain(handle, device_state.cur_search_domain, kRdmnetDisconnectRptReconfigure);
  }
}

void device_llrp_rdm_cmd_received(rdmnet_device_t handle, const LlrpRemoteRdmCommand *cmd, void *context)
{
  (void)handle;
  (void)context;
  (void)cmd;

  rdmnet_data_changed_t data_changed = kNoRdmnetDataChanged;
  device_handle_llrp_command(cmd, &data_changed);
  if (data_changed == kRdmnetScopeConfigChanged)
  {
    default_responder_get_scope_config(&device_state.cur_scope_config);
    rdmnet_device_change_scope(handle, &device_state.cur_scope_config, kRdmnetDisconnectLlrpReconfigure);
  }
  else if (data_changed == kRdmnetSearchDomainChanged)
  {
    default_responder_get_search_domain(device_state.cur_search_domain);
    rdmnet_device_change_search_domain(handle, device_state.cur_search_domain, kRdmnetDisconnectLlrpReconfigure);
  }
}

void device_handle_rpt_command(const RemoteRdmCommand *cmd, rdmnet_data_changed_t *data_changed)
{
  const RdmCommand *rdm_cmd = &cmd->rdm;
  if (rdm_cmd->command_class != kRdmCCGetCommand && rdm_cmd->command_class != kRdmCCSetCommand)
  {
    device_send_rpt_status(VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS, cmd);
    lwpa_log(device_state.lparams, LWPA_LOG_WARNING, "Device received RDM command with invalid command class 0x%02x",
             rdm_cmd->command_class);
  }
  else if (!default_responder_supports_pid(rdm_cmd->param_id))
  {
    device_send_rpt_nack(E120_NR_UNKNOWN_PID, cmd);
    lwpa_log(device_state.lparams, LWPA_LOG_DEBUG, "Sending NACK to Controller %04x:%08x for unknown PID 0x%04x",
             cmd->source_uid.manu, cmd->source_uid.id, rdm_cmd->param_id);
  }
  else
  {
    RdmResponse resp_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
    size_t resp_list_size;
    uint16_t nack_reason;
    if (device_handle_rdm_command(&cmd->rdm, resp_list, &resp_list_size, &nack_reason, data_changed))
    {
      device_send_rpt_response(resp_list, resp_list_size, cmd);
      lwpa_log(device_state.lparams, LWPA_LOG_DEBUG, "ACK'ing %s for PID 0x%04x from Controller %04x:%08x",
               rdm_cmd->command_class == kRdmCCSetCommand ? "SET_COMMAND" : "GET_COMMAND", rdm_cmd->param_id,
               cmd->source_uid.manu, cmd->source_uid.id);
    }
    else
    {
      device_send_rpt_nack(nack_reason, cmd);
      lwpa_log(device_state.lparams, LWPA_LOG_DEBUG,
               "Sending %s NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
               rdm_cmd->command_class == kRdmCCSetCommand ? "SET_COMMAND" : "GET_COMMAND", cmd->source_uid.manu,
               cmd->source_uid.id, rdm_cmd->param_id, nack_reason);
    }
  }
}

void device_handle_llrp_command(const LlrpRemoteRdmCommand *cmd, rdmnet_data_changed_t *data_changed)
{
  const RdmCommand *rdm_cmd = &cmd->rdm;
  if (rdm_cmd->command_class != kRdmCCGetCommand && rdm_cmd->command_class != kRdmCCSetCommand)
  {
    device_send_llrp_nack(E120_NR_UNSUPPORTED_COMMAND_CLASS, cmd);
    lwpa_log(device_state.lparams, LWPA_LOG_WARNING,
             "Device received LLRP RDM command with invalid command class 0x%02x", rdm_cmd->command_class);
  }
  else if (!default_responder_supports_pid(rdm_cmd->param_id))
  {
    device_send_llrp_nack(E120_NR_UNKNOWN_PID, cmd);
    lwpa_log(device_state.lparams, LWPA_LOG_DEBUG, "Sending NACK to LLRP Manager %04x:%08x for unknown PID 0x%04x",
             cmd->rdm.source_uid.manu, cmd->rdm.source_uid.id, rdm_cmd->param_id);
  }
  else
  {
    RdmResponse resp_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
    size_t resp_list_size;
    uint16_t nack_reason;
    if (device_handle_rdm_command(&cmd->rdm, resp_list, &resp_list_size, &nack_reason, data_changed))
    {
      if (resp_list_size > 1)
      {
        device_send_llrp_nack(E137_7_NR_ACTION_NOT_SUPPORTED, cmd);
        lwpa_log(
            device_state.lparams, LWPA_LOG_DEBUG,
            "Sending NACK to LLRP Manager %04x:%08x for supported PID 0x%04x because response would cause ACK_OVERFLOW",
            cmd->rdm.source_uid.manu, cmd->rdm.source_uid.id, rdm_cmd->param_id);
      }
      else
      {
        device_send_llrp_response(resp_list, cmd);
        lwpa_log(device_state.lparams, LWPA_LOG_DEBUG, "ACK'ing %s for PID 0x%04x from LLRP Manager %04x:%08x",
                 rdm_cmd->command_class == kRdmCCSetCommand ? "SET_COMMAND" : "GET_COMMAND", rdm_cmd->param_id,
                 cmd->rdm.source_uid.manu, cmd->rdm.source_uid.id);
      }
    }
    else
    {
      device_send_llrp_nack(nack_reason, cmd);
      lwpa_log(device_state.lparams, LWPA_LOG_DEBUG,
               "Sending %s NACK to LLRP Manager %04x:%08x for supported PID 0x%04x with reason 0x%04x",
               rdm_cmd->command_class == kRdmCCSetCommand ? "SET_COMMAND" : "GET_COMMAND", cmd->rdm.source_uid.manu,
               cmd->rdm.source_uid.id, rdm_cmd->param_id, nack_reason);
    }
  }
}

bool device_handle_rdm_command(const RdmCommand *rdm_cmd, RdmResponse *resp_list, size_t *resp_list_size,
                               uint16_t *nack_reason, rdmnet_data_changed_t *data_changed)
{
  bool res = false;
  switch (rdm_cmd->command_class)
  {
    case kRdmCCSetCommand:
    {
      if (default_responder_set(rdm_cmd->param_id, rdm_cmd->data, rdm_cmd->datalen, nack_reason, data_changed))
      {
        resp_list->source_uid = rdm_cmd->dest_uid;
        resp_list->dest_uid = kBroadcastUid;
        resp_list->transaction_num = rdm_cmd->transaction_num;
        resp_list->resp_type = kRdmResponseTypeAck;
        resp_list->msg_count = 0;
        resp_list->subdevice = 0;
        resp_list->command_class = kRdmCCSetCommandResponse;
        resp_list->param_id = rdm_cmd->param_id;
        resp_list->datalen = 0;

        *resp_list_size = 1;
        res = true;
      }
      break;
    }
    case kRdmCCGetCommand:
    {
      param_data_list_t resp_data_list;
      if (default_responder_get(rdm_cmd->param_id, rdm_cmd->data, rdm_cmd->datalen, resp_data_list, resp_list_size,
                                nack_reason))
      {
        for (size_t i = 0; i < *resp_list_size; ++i)
        {
          resp_list[i].source_uid = rdm_cmd->dest_uid;
          resp_list[i].dest_uid = rdm_cmd->source_uid;
          resp_list[i].transaction_num = rdm_cmd->transaction_num;
          resp_list[i].resp_type = (i == *resp_list_size - 1) ? kRdmResponseTypeAck : kRdmResponseTypeAckOverflow;
          resp_list[i].msg_count = 0;
          resp_list[i].subdevice = 0;
          resp_list[i].command_class = kRdmCCGetCommandResponse;
          resp_list[i].param_id = rdm_cmd->param_id;

          memcpy(resp_list[i].data, resp_data_list[i].data, resp_data_list[i].datalen);
          resp_list[i].datalen = resp_data_list[i].datalen;
          res = true;
        }
      }
      break;
    }
    default:
      break;
  }
  return res;
}

void device_send_rpt_status(rpt_status_code_t status_code, const RemoteRdmCommand *received_cmd)
{
  LocalRptStatus status;
  RDMNET_CREATE_STATUS_FROM_COMMAND(&status, received_cmd, status_code);

  lwpa_error_t send_res = rdmnet_device_send_status(device_state.device_handle, &status);
  if (send_res != kLwpaErrOk)
  {
    lwpa_log(device_state.lparams, LWPA_LOG_ERR, "Error sending RPT Status message to Broker: '%s'.",
             lwpa_strerror(send_res));
  }
}

void device_send_rpt_nack(uint16_t nack_reason, const RemoteRdmCommand *received_cmd)
{
  RdmResponse resp;
  RDM_CREATE_NACK_FROM_COMMAND(&resp, &received_cmd->rdm, nack_reason);
  device_send_rpt_response(&resp, 1, received_cmd);
}

void device_send_rpt_response(RdmResponse *resp_list, size_t num_responses, const RemoteRdmCommand *received_cmd)
{
  LocalRdmResponse resp_to_send;
  RDMNET_CREATE_RESPONSE_FROM_COMMAND(&resp_to_send, received_cmd, resp_list, num_responses);

  lwpa_error_t send_res = rdmnet_device_send_rdm_response(device_state.device_handle, &resp_to_send);
  if (send_res != kLwpaErrOk)
  {
    lwpa_log(device_state.lparams, LWPA_LOG_ERR, "Error sending RPT RDM response: '%s.", lwpa_strerror(send_res));
  }
}

void device_send_llrp_nack(uint16_t nack_reason, const LlrpRemoteRdmCommand *received_cmd)
{
  RdmResponse resp;
  RDM_CREATE_NACK_FROM_COMMAND(&resp, &received_cmd->rdm, nack_reason);
  device_send_llrp_response(&resp, received_cmd);
}

void device_send_llrp_response(RdmResponse *resp, const LlrpRemoteRdmCommand *received_cmd)
{
  LlrpLocalRdmResponse resp_to_send;
  LLRP_CREATE_RESPONSE_FROM_COMMAND(&resp_to_send, received_cmd, resp);

  lwpa_error_t send_res = rdmnet_device_send_llrp_response(device_state.device_handle, &resp_to_send);
  if (send_res != kLwpaErrOk)
  {
    lwpa_log(device_state.lparams, LWPA_LOG_ERR, "Error sending LLRP RDM response: '%s.", lwpa_strerror(send_res));
  }
}
