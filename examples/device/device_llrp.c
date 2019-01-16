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
#include "device_llrp.h"

#include "lwpa/netint.h"
#include "lwpa/thread.h"
#include "lwpa/pack.h"
#include "lwpa/socket.h"
#include "rdm/uid.h"
#include "rdm/defs.h"
#include "rdm/responder.h"
#include "rdmnet/llrp.h"
#include "device.h"

#define rdm_uid_matches_mine(uidptr) (rdm_uid_equal(uidptr, &llrp_info.uid) || rdm_uid_is_broadcast(uidptr))

static struct llrp_info
{
  LlrpPoll *target_socks;
  size_t num_target_socks;
  lwpa_thread_t update_thread;
  bool llrp_thread_run;
  LwpaUuid cid;
  RdmUid uid;
  const LwpaLogParams *lparams;
} llrp_info;

void llrp_send_nack(llrp_socket_t sock, const LlrpRdmMessage *llrp_msg, const RdmCommand *cmd_data,
                    uint16_t nack_reason)
{
  RdmResponse resp_data;
  RdmBuffer resp;

  resp_data.src_uid = llrp_info.uid;
  resp_data.dest_uid = cmd_data->src_uid;
  resp_data.transaction_num = cmd_data->transaction_num;
  resp_data.resp_type = E120_RESPONSE_TYPE_NACK_REASON;
  resp_data.msg_count = 0;
  resp_data.subdevice = 0;
  resp_data.command_class = cmd_data->command_class + 1;
  resp_data.param_id = cmd_data->param_id;
  resp_data.datalen = 2;
  pack_16b(resp_data.data, nack_reason);

  if (LWPA_OK == rdmresp_create_response(&resp_data, &resp))
  {
    llrp_send_rdm_response(sock, &llrp_msg->source_cid, &resp, llrp_msg->transaction_num);
  }
}

void llrp_handle_rdm_command(llrp_socket_t sock, const LlrpRdmMessage *llrp_msg)
{
  RdmCommand cmd_data;
  if (LWPA_OK != rdmresp_unpack_command(&llrp_msg->msg, &cmd_data))
  {
    lwpa_log(llrp_info.lparams, LWPA_LOG_WARNING, "Device received incorrectly-formatted LLRP RDM command.");
  }
  else if (!rdm_uid_matches_mine(&cmd_data.dest_uid))
  {
    lwpa_log(llrp_info.lparams, LWPA_LOG_WARNING, "Device received LLRP RDM command addressed to unknown UID %04x:%08x",
             cmd_data.dest_uid.manu, cmd_data.dest_uid.id);
  }
  else if (cmd_data.command_class != E120_GET_COMMAND && cmd_data.command_class != E120_SET_COMMAND)
  {
    lwpa_log(llrp_info.lparams, LWPA_LOG_WARNING, "Device received LLRP RDM command with invalid command class %d",
             cmd_data.command_class);
  }
  else if (!default_responder_supports_pid(cmd_data.param_id))
  {
    llrp_send_nack(sock, llrp_msg, &cmd_data, E120_NR_UNKNOWN_PID);
    lwpa_log(llrp_info.lparams, LWPA_LOG_DEBUG, "Sending LLRP NACK to Manager %04x:%08x for unknown PID 0x%04x",
             cmd_data.src_uid.manu, cmd_data.src_uid.id, cmd_data.param_id);
  }
  else
  {
    switch (cmd_data.command_class)
    {
      case E120_SET_COMMAND:
      {
        uint16_t nack_reason;
        if (device_llrp_set(&cmd_data, &nack_reason))
        {
          RdmResponse resp_data;
          RdmBuffer resp;

          resp_data.src_uid = llrp_info.uid;
          resp_data.dest_uid = cmd_data.src_uid;
          resp_data.transaction_num = cmd_data.transaction_num;
          resp_data.resp_type = E120_RESPONSE_TYPE_ACK;
          resp_data.msg_count = 0;
          resp_data.subdevice = 0;
          resp_data.command_class = E120_SET_COMMAND_RESPONSE;
          resp_data.param_id = cmd_data.param_id;
          resp_data.datalen = 0;

          if (LWPA_OK == rdmresp_create_response(&resp_data, &resp))
          {
            llrp_send_rdm_response(sock, &llrp_msg->source_cid, &resp, llrp_msg->transaction_num);
            lwpa_log(llrp_info.lparams, LWPA_LOG_DEBUG,
                     "ACK'ing LLRP SET_COMMAND for PID 0x%04x from Controller %04x:%08x", cmd_data.param_id,
                     cmd_data.src_uid.manu, cmd_data.src_uid.id);
          }
        }
        else
        {
          llrp_send_nack(sock, llrp_msg, &cmd_data, nack_reason);
          lwpa_log(llrp_info.lparams, LWPA_LOG_DEBUG,
                   "Sending LLRP SET_COMMAND NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
                   cmd_data.src_uid.manu, cmd_data.src_uid.id, cmd_data.param_id, nack_reason);
        }
        break;
      }
      case E120_GET_COMMAND:
      {
        static param_data_list_t resp_data_list;
        RdmBuffer resp;
        size_t num_responses;
        uint16_t nack_reason;
        bool get_success = default_responder_get(cmd_data.param_id, cmd_data.data, cmd_data.datalen, resp_data_list,
                                                 &num_responses, &nack_reason);

        /* E1.33 sect. 5.7.2: ACK_OVERFLOW is not allowed in LLRP. */
        if (get_success && num_responses > 1)
        {
          get_success = false;
          nack_reason = E137_7_NR_ACTION_NOT_SUPPORTED;
        }
        if (get_success)
        {
          RdmResponse resp_data;

          resp_data.src_uid = llrp_info.uid;
          resp_data.dest_uid = cmd_data.src_uid;
          resp_data.transaction_num = cmd_data.transaction_num;
          resp_data.resp_type = E120_RESPONSE_TYPE_ACK;
          resp_data.msg_count = 0;
          resp_data.subdevice = 0;
          resp_data.command_class = E120_GET_COMMAND_RESPONSE;
          resp_data.param_id = cmd_data.param_id;

          memcpy(resp_data.data, resp_data_list[0].data, resp_data_list[0].datalen);
          resp_data.datalen = resp_data_list[0].datalen;
          if (LWPA_OK == rdmresp_create_response(&resp_data, &resp))
          {
            llrp_send_rdm_response(sock, &llrp_msg->source_cid, &resp, llrp_msg->transaction_num);
            lwpa_log(llrp_info.lparams, LWPA_LOG_DEBUG,
                     "ACK'ing LLRP GET_COMMAND for PID 0x%04x from Controller %04x:%08x", cmd_data.param_id,
                     cmd_data.src_uid.manu, cmd_data.src_uid.id);
          }
        }
        else
        {
          llrp_send_nack(sock, llrp_msg, &cmd_data, nack_reason);
          lwpa_log(llrp_info.lparams, LWPA_LOG_DEBUG,
                   "Sending LLRP GET_COMMAND NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
                   cmd_data.src_uid.manu, cmd_data.src_uid.id, cmd_data.param_id, nack_reason);
        }
        break;
      }
      default:
        break;
    }
  }
}

void device_llrp_update_thread(void *arg)
{
  (void)arg;

  while (llrp_info.llrp_thread_run)
  {
    int update_res = llrp_update(llrp_info.target_socks, llrp_info.num_target_socks, 200);
    if (llrp_info.llrp_thread_run)
    {
      if (update_res >= 1)
      {
        for (size_t i = 0; i < llrp_info.num_target_socks; ++i)
        {
          LlrpPoll *poll = &llrp_info.target_socks[i];
          if (poll->err == LWPA_OK && llrp_data_is_rdm(&poll->data))
          {
            llrp_handle_rdm_command(poll->handle, llrp_data_rdm(&poll->data));
          }
        }
      }
      else if (update_res != LWPA_TIMEDOUT)
      {
        lwpa_log(llrp_info.lparams, LWPA_LOG_ERR, "llrp_update() failed with error: '%s'", lwpa_strerror(update_res));
      }
    }
  }
}

void device_llrp_init(const LwpaUuid *my_cid, const LwpaLogParams *lparams)
{
  size_t num_interfaces;

  if (!my_cid || !lparams)
    return;

  if (LWPA_OK != llrp_init())
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Couldn't initialize LLRP.");
    return;
  }

  num_interfaces = netint_get_num_interfaces();
  if (num_interfaces > 0)
  {
    size_t i;
    LwpaUid my_uid;
    LwpaNetintInfo *netints = calloc(num_interfaces, sizeof(LwpaNetintInfo));
    llrp_info.target_socks = calloc(num_interfaces, sizeof(LlrpPoll));

    rdmnet_init_dynamic_uid_request(&my_uid, 0x6574);

    num_interfaces = netint_get_interfaces(netints, num_interfaces);
    for (i = 0; i < num_interfaces; ++i)
    {
      LwpaNetintInfo *netint = &netints[i];
      LlrpPoll *cur_poll = &llrp_info.target_socks[llrp_info.num_target_socks];

      cur_poll->handle = llrp_create_target_socket(&netint->addr, my_cid, &my_uid, netint->mac, kLLRPCompRPTDevice);
      if (cur_poll->handle != LLRP_SOCKET_INVALID)
      {
        ++llrp_info.num_target_socks;
      }
      else
      {
        if (lwpa_canlog(lparams, LWPA_LOG_WARNING))
        {
          char addr_str[LWPA_INET6_ADDRSTRLEN];
          lwpa_inet_ntop(&netint->addr, addr_str, LWPA_INET6_ADDRSTRLEN);
          lwpa_log(lparams, LWPA_LOG_WARNING, "Warning: couldn't create LLRP Target Socket on network interface %s.",
                   addr_str);
        }
      }
    }
    free(netints);
  }

  if (llrp_info.num_target_socks > 0)
  {
    LwpaThreadParams tparams;
    tparams.thread_name = "LLRP Update Thread";
    tparams.thread_priority = LWPA_THREAD_DEFAULT_PRIORITY;
    tparams.stack_size = LWPA_THREAD_DEFAULT_STACK;
    tparams.platform_data = NULL;

    llrp_info.llrp_thread_run = true;
    if (lwpa_thread_create(&llrp_info.update_thread, &tparams, device_llrp_update_thread, NULL))
    {
      llrp_info.cid = *my_cid;
      llrp_info.uid = *my_uid;
      llrp_info.lparams = lparams;
    }
    else
    {
      size_t i;

      for (i = 0; i < llrp_info.num_target_socks; ++i)
      {
        llrp_close_socket(llrp_info.target_socks[i].handle);
      }
      lwpa_log(lparams, LWPA_LOG_ERR, "Couldn't initialize LLRP - couldn't create update thread.");
    }
  }
  else
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Couldn't initialize LLRP - no LLRP target sockets could be created.");
  }
}

void device_llrp_deinit()
{
  size_t i;

  llrp_info.llrp_thread_run = false;
  lwpa_thread_stop(&llrp_info.update_thread, 1000);

  for (i = 0; i < llrp_info.num_target_socks; ++i)
  {
    llrp_close_socket(llrp_info.target_socks[i].handle);
  }
  free(llrp_info.target_socks);
  memset(&llrp_info, 0, sizeof llrp_info);
}

void device_llrp_set_connected(bool connected, const RdmUid *new_uid)
{
  size_t i;
  for (i = 0; i < llrp_info.num_target_socks; ++i)
  {
    llrp_target_update_connection_state(llrp_info.target_socks[i].handle, connected, new_uid);
  }
}
