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

#include "device.h"
#include "lwpa_int.h"
#include "lwpa_pack.h"
#include "lwpa_uid.h"
#include "estardmnet.h"
#include "estardm.h"
#include "rdmnet/rptprot.h"
#include "rdmnet/rdmresponder.h"
#include "defaultresponder.h"

/***************************** Private macros ********************************/

#define rpt_uid_matches_mine(uidptr)                                       \
  (uid_equal(uidptr, &my_uid) || uid_is_rdmnet_device_broadcast(uidptr) || \
   (uid_is_rdmnet_device_manu_broadcast(uidptr) && rdmnet_device_broadcast_manu_matches(uidptr, my_uid.manu)))
#define rdm_uid_matches_mine(uidptr) (uid_equal(uidptr, &my_uid) || uid_is_broadcast(uidptr))

#define swap_header_data(recvhdrptr, sendhdrptr)                       \
  do                                                                   \
  {                                                                    \
    (sendhdrptr)->dest_uid = (recvhdrptr)->source_uid;                 \
    (sendhdrptr)->dest_endpoint_id = (recvhdrptr)->source_endpoint_id; \
    (sendhdrptr)->source_uid = my_uid;                                 \
    (sendhdrptr)->source_endpoint_id = NULL_ENDPOINT;                  \
    (sendhdrptr)->seqnum = (recvhdrptr)->seqnum;                       \
  } while (0)

/**************************** Private variables ******************************/

static LwpaCid my_cid;
static LwpaUid my_uid;
char status_str[RPT_STATUS_STRING_MAXLEN];

/*********************** Private function prototypes *************************/

static void send_status(int conn, uint16_t status_code, const RptHeader *received_header, const LwpaLogParams *lparams);
static void send_nack(int conn, const RptHeader *received_header, const RdmCommand *cmd_data, uint16_t nack_reason,
                      const LwpaLogParams *lparams);
static void send_notification(int conn, const RptHeader *received_header, const RdmCmdListEntry *cmd_list,
                              const LwpaLogParams *lparams);
static void handle_rdm_command(int conn, const RptHeader *received_header, const RdmBuffer *cmd,
                               const LwpaLogParams *lparams, bool *requires_reconnect);

/*************************** Function definitions ****************************/

void device_init(const DeviceSettings *settings)
{
  if (settings)
  {
    default_responder_init(&settings->static_broker_addr, settings->scope);
    my_cid = settings->cid;
    my_uid = settings->uid;
  }
}

void device_deinit()
{
  default_responder_deinit();
}

void device_handle_message(int conn, const RdmnetMessage *msg, const LwpaLogParams *lparams, bool *requires_reconnect)
{
  if (msg->vector == VECTOR_ROOT_RPT)
  {
    const RptMessage *rptmsg = get_rpt_msg(msg);
    if (rptmsg->vector == VECTOR_RPT_REQUEST)
    {
      if (rpt_uid_matches_mine(&rptmsg->header.dest_uid))
      {
        if (rptmsg->header.dest_endpoint_id == NULL_ENDPOINT)
        {
          const RdmCmdList *cmdlist = get_rdm_cmd_list(rptmsg);
          handle_rdm_command(conn, &rptmsg->header, &cmdlist->list->msg, lparams, requires_reconnect);
        }
        else
        {
          send_status(conn, VECTOR_RPT_STATUS_UNKNOWN_ENDPOINT, &rptmsg->header, lparams);
          lwpa_log(lparams, LWPA_LOG_WARNING, "Device received RPT message addressed to unknown Endpoint ID %d",
                   rptmsg->header.dest_endpoint_id);
        }
      }
      else
      {
        send_status(conn, VECTOR_RPT_STATUS_UNKNOWN_RPT_UID, &rptmsg->header, lparams);
        lwpa_log(lparams, LWPA_LOG_WARNING, "Device received RPT message addressed to unknown UID %04x:%08x",
                 rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id);
      }
    }
    else
    {
      send_status(conn, VECTOR_RPT_STATUS_UNKNOWN_VECTOR, &rptmsg->header, lparams);
      lwpa_log(lparams, LWPA_LOG_WARNING, "Device received RPT message with unhandled vector type %d", msg->vector);
    }
  }
  else
  {
    lwpa_log(lparams, LWPA_LOG_WARNING, "Device received root message with unhandled vector type %d", msg->vector);
  }
}

void handle_rdm_command(int conn, const RptHeader *received_header, const RdmBuffer *cmd, const LwpaLogParams *lparams,
                        bool *requires_reconnect)
{
  RdmCommand cmd_data;
  if (LWPA_OK != rdmresp_unpack_command(cmd, &cmd_data))
  {
    send_status(conn, VECTOR_RPT_STATUS_INVALID_MESSAGE, received_header, lparams);
    lwpa_log(lparams, LWPA_LOG_WARNING, "Device received incorrectly-formatted RDM command.");
  }
  else if (!rdm_uid_matches_mine(&cmd_data.dest_uid))
  {
    send_status(conn, VECTOR_RPT_STATUS_UNKNOWN_RDM_UID, received_header, lparams);
    lwpa_log(lparams, LWPA_LOG_WARNING, "Device received RDM command addressed to unknown UID %04x:%08x",
             cmd_data.dest_uid.manu, cmd_data.dest_uid.id);
  }
  else if (cmd_data.command_class != E120_GET_COMMAND && cmd_data.command_class != E120_SET_COMMAND)
  {
    send_status(conn, VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS, received_header, lparams);
    lwpa_log(lparams, LWPA_LOG_WARNING, "Device received RDM command with invalid command class %d",
             cmd_data.command_class);
  }
  else if (!default_responder_supports_pid(cmd_data.param_id))
  {
    send_nack(conn, received_header, &cmd_data, E120_NR_UNKNOWN_PID, lparams);
    lwpa_log(lparams, LWPA_LOG_DEBUG, "Sending NACK to Controller %04x:%08x for unknown PID 0x%04x",
             received_header->source_uid.manu, received_header->source_uid.id, cmd_data.param_id);
  }
  else
  {
    switch (cmd_data.command_class)
    {
      case E120_SET_COMMAND:
      {
        uint16_t nack_reason;
        if (default_responder_set(cmd_data.param_id, cmd_data.data, cmd_data.datalen, &nack_reason, requires_reconnect))
        {
          RdmResponse resp_data;
          RdmCmdListEntry resp;

          resp_data.src_uid = my_uid;
          resp_data.dest_uid = received_header->source_uid;
          resp_data.transaction_num = cmd_data.transaction_num;
          resp_data.resp_type = E120_RESPONSE_TYPE_ACK;
          resp_data.msg_count = 0;
          resp_data.subdevice = 0;
          resp_data.command_class = E120_SET_COMMAND_RESPONSE;
          resp_data.param_id = cmd_data.param_id;
          resp_data.datalen = 0;

          if (LWPA_OK == rdmresp_create_response(&resp_data, &resp.msg))
          {
            resp.next = NULL;
            send_notification(conn, received_header, &resp, lparams);
            lwpa_log(lparams, LWPA_LOG_DEBUG, "ACK'ing SET_COMMAND for PID 0x%04x from Controller %04x:%08x",
                     cmd_data.param_id, received_header->source_uid.manu, received_header->source_uid.id);
          }
        }
        else
        {
          send_nack(conn, received_header, &cmd_data, nack_reason, lparams);
          lwpa_log(lparams, LWPA_LOG_DEBUG,
                   "Sending SET_COMMAND NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
                   received_header->source_uid.manu, received_header->source_uid.id, cmd_data.param_id, nack_reason);
        }
        break;
      }
      case E120_GET_COMMAND:
      {
        static param_data_list_t resp_data_list;
        static RdmCmdListEntry resp_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
        size_t num_responses;
        uint16_t nack_reason;
        if (default_responder_get(cmd_data.param_id, cmd_data.data, cmd_data.datalen, resp_data_list, &num_responses,
                                  &nack_reason))
        {
          RdmResponse resp_data;

          resp_data.src_uid = my_uid;
          resp_data.dest_uid = received_header->source_uid;
          resp_data.transaction_num = cmd_data.transaction_num;
          resp_data.resp_type = num_responses > 1 ? E120_RESPONSE_TYPE_ACK_OVERFLOW : E120_RESPONSE_TYPE_ACK;
          resp_data.msg_count = 0;
          resp_data.subdevice = 0;
          resp_data.command_class = E120_GET_COMMAND_RESPONSE;
          resp_data.param_id = cmd_data.param_id;

          size_t i;
          for (i = 0; i < num_responses; ++i)
          {
            memcpy(resp_data.data, resp_data_list[i].data, resp_data_list[i].datalen);
            resp_data.datalen = resp_data_list[i].datalen;
            if (i == num_responses - 1)
            {
              resp_data.resp_type = E120_RESPONSE_TYPE_ACK;
              resp_list[i].next = NULL;
            }
            else
              resp_list[i].next = &resp_list[i + 1];
            rdmresp_create_response(&resp_data, &resp_list[i].msg);
          }
          send_notification(conn, received_header, resp_list, lparams);
          lwpa_log(lparams, LWPA_LOG_DEBUG, "ACK'ing GET_COMMAND for PID 0x%04x from Controller %04x:%08x",
                   cmd_data.param_id, received_header->source_uid.manu, received_header->source_uid.id);
        }
        else
        {
          send_nack(conn, received_header, &cmd_data, nack_reason, lparams);
          lwpa_log(lparams, LWPA_LOG_DEBUG,
                   "Sending GET_COMMAND NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
                   received_header->source_uid.manu, received_header->source_uid.id, cmd_data.param_id, nack_reason);
        }
        break;
      }
      default:
        break;
    }
  }
}

void send_status(int conn, uint16_t status_code, const RptHeader *received_header, const LwpaLogParams *lparams)
{
  RptHeader header_to_send;
  RptStatusMsg status;
  lwpa_error_t send_res;

  swap_header_data(received_header, &header_to_send);

  status.status_code = status_code;
  status.status_string = NULL;
  send_res = send_rpt_status(conn, &my_cid, &header_to_send, &status);
  if (send_res != LWPA_OK)
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Error sending RPT Status message to Broker.");
  }
}

void send_nack(int conn, const RptHeader *received_header, const RdmCommand *cmd_data, uint16_t nack_reason,
               const LwpaLogParams *lparams)
{
  RdmResponse resp_data;
  RdmCmdListEntry resp;

  resp_data.src_uid = my_uid;
  resp_data.dest_uid = received_header->source_uid;
  resp_data.transaction_num = cmd_data->transaction_num;
  resp_data.resp_type = E120_RESPONSE_TYPE_NACK_REASON;
  resp_data.msg_count = 0;
  resp_data.subdevice = 0;
  resp_data.command_class = cmd_data->command_class + 1;
  resp_data.param_id = cmd_data->param_id;
  resp_data.datalen = 2;
  pack_16b(resp_data.data, nack_reason);

  if (LWPA_OK == rdmresp_create_response(&resp_data, &resp.msg))
  {
    resp.next = NULL;
    send_notification(conn, received_header, &resp, lparams);
  }
}

void send_notification(int conn, const RptHeader *received_header, const RdmCmdListEntry *cmd_list,
                       const LwpaLogParams *lparams)
{
  RptHeader header_to_send;
  lwpa_error_t send_res;

  swap_header_data(received_header, &header_to_send);

  send_res = send_rpt_notification(conn, &my_cid, &header_to_send, cmd_list);
  if (send_res != LWPA_OK)
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Error sending RPT Notification message to Broker.");
  }
}
