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

#define rpt_uid_matches_mine(uidptr)                                                        \
  (rdm_uid_equal(uidptr, &device_state.my_uid) || rdmnet_uid_is_device_broadcast(uidptr) || \
   (rdmnet_uid_is_device_manu_broadcast(uidptr) &&                                          \
    rdmnet_device_broadcast_manu_matches(uidptr, device_state.my_uid.manu)))
#define rdm_uid_matches_mine(uidptr) (rdm_uid_equal(uidptr, &device_state.my_uid) || rdm_uid_is_broadcast(uidptr))

#define swap_header_data(recvhdrptr, sendhdrptr)                       \
  do                                                                   \
  {                                                                    \
    (sendhdrptr)->dest_uid = (recvhdrptr)->source_uid;                 \
    (sendhdrptr)->dest_endpoint_id = (recvhdrptr)->source_endpoint_id; \
    (sendhdrptr)->source_uid = device_state.my_uid;                    \
    (sendhdrptr)->source_endpoint_id = E133_NULL_ENDPOINT;             \
    (sendhdrptr)->seqnum = (recvhdrptr)->seqnum;                       \
  } while (0)

/**************************** Private variables ******************************/

static struct device_state
{
  bool configuration_change;

  // LwpaUuid my_cid;
  // RdmUid my_uid;
  rdmnet_device_t device_handle;

  bool connected;

  const LwpaLogParams *lparams;
} device_state;

/*********************** Private function prototypes *************************/

/* Broker connection */
// static bool connect_to_broker(RdmUid *assigned_uid);
// static void get_connect_params(RdmnetConnectParams *connect_params, LwpaSockaddr *broker_addr);

/* RDM command handling */
// static void device_handle_message(const RdmnetMessage *msg, bool *requires_reconnect);
// static void handle_rdm_command(const RptHeader *received_header, const RdmBuffer *cmd, bool *requires_reconnect);
// static void send_status(uint16_t status_code, const RptHeader *received_header);
// static void send_nack(const RptHeader *received_header, const RdmCommand *cmd_data, uint16_t nack_reason);
// static void send_notification(const RptHeader *received_header, const RdmCmdListEntry *cmd_list);

/* Device callbacks */
static void device_connected(rdmnet_device_t handle, const char *scope, void *context);
static void device_disconnected(rdmnet_device_t handle, const char *scope, void *context);

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

lwpa_error_t device_init(const DeviceParams *params, const LwpaLogParams *lparams)
{
  if (!params)
    return LWPA_INVALID;

  device_state.lparams = lparams;

  lwpa_log(lparams, LWPA_LOG_INFO, "ETC Prototype RDMnet Device Version " RDMNET_VERSION_STRING);

  default_responder_init(&params->scope_config);

  lwpa_error_t res = rdmnet_core_init(lparams);
  if (res != LWPA_OK)
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "RDMnet initialization failed with error: '%s'", lwpa_strerror(res));
    return res;
  }

  RdmnetDeviceConfig config;
  config.has_static_uid = false;
  config.cid = params->cid;
  config.scope_config = params->scope_config;
  config.callbacks.connected = device_connected;
  config.callbacks.disconnected = device_disconnected;
  config.callback_context = NULL;

  res = rdmnet_device_create(&config, &device_state.device_handle);
  if (res != LWPA_OK)
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Device initialization failed with error: '%s'", lwpa_strerror(res));
    rdmnet_core_deinit();
  }
  return res;
}

void device_deinit()
{
  /*
  device_state.configuration_change = true;
  if (device_state.connected)
  {
    rdmnet_disconnect(device_state.broker_conn, true, kRdmnetDisconnectShutdown);
  }
  rdmnet_deinit();
  rdmnetdisc_deinit();
  default_responder_deinit();
  */
}

void device_connected(rdmnet_device_t handle, const char *scope, void *context)
{
  lwpa_log(device_state.lparams, LWPA_LOG_INFO, "Device connected to Broker on scope '%s'.", scope);
}

void device_disconnected(rdmnet_device_t handle, const char *scope, void *context)
{
  lwpa_log(device_state.lparams, LWPA_LOG_INFO, "Device disconnected from Broker on scope '%s'.", scope);
}

// void device_run()
//{
//  if (device_state.connected)
//  {
//    static RdmnetData recv_data;
//    lwpa_error_t res = rdmnet_recv(device_state.broker_conn, &recv_data);
//    if (res == LWPA_OK)
//    {
//      bool reconnect_required = false;
//      device_handle_message(rdmnet_data_msg(&recv_data), &reconnect_required);
//      if (reconnect_required)
//      {
//        lwpa_log(device_state.lparams, LWPA_LOG_INFO,
//                 "Device received configuration message that requires re-connection to Broker. Disconnecting...");
//        rdmnet_disconnect(device_state.broker_conn, true, kRdmnetDisconnectRptReconfigure);
//        device_state.connected = false;
//        rdmnet_init_dynamic_uid_request(&device_state.my_uid, 0x6574);
//        device_llrp_set_connected(false, &device_state.my_uid);
//      }
//    }
//    else if (res != LWPA_NODATA && !device_state.configuration_change)
//    {
//      /* Disconnected from Broker. */
//      device_state.connected = false;
//      rdmnet_init_dynamic_uid_request(&device_state.my_uid, 0x6574);
//      device_llrp_set_connected(false, &device_state.my_uid);
//      lwpa_log(device_state.lparams, LWPA_LOG_INFO,
//               "Disconnected from Broker with error: '%s'. Attempting to reconnect...", lwpa_strerror(res));
//
//      /* On an unhealthy TCP event, increment our internal counter. */
//      if (res == LWPA_TIMEDOUT)
//        default_responder_incr_unhealthy_count();
//    }
//  }
//  else
//  {
//    /* Temporary hack - give the old Broker's DNS entry some time to be removed from the Bonjour cache. */
//    Sleep(1000);
//
//    /* Attempt to reconnect to the Broker using our most current connect parameters. */
//    if (connect_to_broker(&device_state.my_uid))
//    {
//      device_state.connected = true;
//      device_llrp_set_connected(true, &device_state.my_uid);
//      lwpa_log(device_state.lparams, LWPA_LOG_INFO, "Re-connected to Broker.");
//    }
//  }
//}
//
// bool device_llrp_set(const RdmCommand *cmd_data, uint16_t *nack_reason)
//{
//  bool reconnect_required;
//
//  lwpa_log(device_state.lparams, LWPA_LOG_INFO, "Handling LLRP SET command...");
//
//  bool set_res =
//      default_responder_set(cmd_data->param_id, cmd_data->data, cmd_data->datalen, nack_reason, &reconnect_required);
//  if (set_res)
//  {
//    device_state.configuration_change = true;
//    if (device_state.connected)
//    {
//      if (reconnect_required)
//      {
//        /* Disconnect from the Broker */
//        lwpa_log(device_state.lparams, LWPA_LOG_INFO,
//                 "A setting was changed using LLRP which requires re-connection to Broker. Disconnecting...");
//        rdmnet_disconnect(device_state.broker_conn, true, kRdmnetDisconnectLlrpReconfigure);
//        device_state.connected = false;
//        rdmnet_init_dynamic_uid_request(&device_state.my_uid, 0x6574);
//        device_llrp_set_connected(false, &device_state.my_uid);
//      }
//      else
//      {
//        /* Send the result of the LLRP change to Controllers */
//        RdmResponse resp_data;
//        RdmCmdListEntry resp;
//        RptHeader header;
//
//        resp_data.src_uid = device_state.my_uid;
//        resp_data.dest_uid = kBroadcastUid;
//        resp_data.transaction_num = cmd_data->transaction_num;
//        resp_data.resp_type = E120_RESPONSE_TYPE_ACK;
//        resp_data.msg_count = 0;
//        resp_data.subdevice = 0;
//        resp_data.command_class = E120_SET_COMMAND_RESPONSE;
//        resp_data.param_id = cmd_data->param_id;
//        resp_data.datalen = 0;
//
//        if (LWPA_OK == rdmresp_create_response(&resp_data, &resp.msg))
//        {
//          RdmCmdListEntry orig_cmd;
//
//          if (LWPA_OK == rdmctl_create_command(cmd_data, &orig_cmd.msg))
//          {
//            orig_cmd.next = &resp;
//            resp.next = NULL;
//            header.source_uid = kRdmnetControllerBroadcastUid;
//            header.source_endpoint_id = E133_NULL_ENDPOINT;
//            header.dest_uid = device_state.my_uid;
//            header.dest_endpoint_id = E133_NULL_ENDPOINT;
//            header.seqnum = 0;
//
//            send_notification(&header, &orig_cmd);
//          }
//        }
//      }
//    }
//  }
//
//  return set_res;
//}
//
///****************************************************************************/
//
// void get_connect_params(RdmnetConnectParams *connect_params, LwpaSockaddr *broker_addr)
//{
//  default_responder_get_e133_params(connect_params);
//
//  /* If we have a static configuration, use it to connect to the Broker. */
//  if (lwpaip_is_invalid(&connect_params->broker_static_addr.ip))
//  {
//    lwpaip_set_invalid(&mdns_broker_addr.ip);
//    mdns_dnssd_resolve_addr(connect_params);
//    *broker_addr = mdns_broker_addr;
//  }
//  else
//  {
//    *broker_addr = connect_params->broker_static_addr;
//  }
//}
//
// bool connect_to_broker(RdmUid *assigned_uid)
//{
//  static RdmnetData connect_data;
//  ClientConnectMsg connect_msg;
//  RdmnetConnectParams connect_params;
//  LwpaSockaddr broker_addr;
//  lwpa_error_t res = LWPA_NOTCONN;
//
//  do
//  {
//    get_connect_params(&connect_params, &broker_addr);
//
//    if (device_state.configuration_change)
//      break;
//
//    /* Fill in the information used in the initial connection handshake. */
//    client_connect_msg_set_scope(&connect_msg, connect_params.scope);
//    client_connect_msg_set_search_domain(&connect_msg, connect_params.search_domain);
//    connect_msg.e133_version = E133_VERSION;
//    connect_msg.connect_flags = 0;
//    create_rpt_client_entry(&device_state.my_cid, &device_state.my_uid, kRPTClientTypeDevice, NULL,
//                            &connect_msg.client_entry);
//
//    /* Attempt to connect. */
//    res = rdmnet_connect(device_state.broker_conn, &broker_addr, &connect_msg, &connect_data);
//    if (res != LWPA_OK)
//    {
//      if (lwpa_canlog(device_state.lparams, LWPA_LOG_WARNING))
//      {
//        char addr_str[LWPA_INET6_ADDRSTRLEN];
//        lwpa_inet_ntop(&broker_addr.ip, addr_str, LWPA_INET6_ADDRSTRLEN);
//        lwpa_log(device_state.lparams, LWPA_LOG_WARNING,
//                 "Connection to Broker at address %s:%d failed with error: '%s'. Retrying...", addr_str,
//                 broker_addr.port, lwpa_strerror(res));
//      }
//    }
//    else
//    {
//      *assigned_uid = get_connect_reply_msg(get_broker_msg(rdmnet_data_msg(&connect_data)))->client_uid;
//    }
//  } while (!device_state.configuration_change && res != LWPA_OK);
//
//  if (device_state.configuration_change)
//    device_state.configuration_change = false;
//  else
//    default_responder_set_tcp_status(&broker_addr);
//
//  return (res == LWPA_OK);
//}
//
// void device_handle_message(const RdmnetMessage *msg, bool *requires_reconnect)
//{
//  if (msg->vector == ACN_VECTOR_ROOT_RPT)
//  {
//    const RptMessage *rptmsg = get_rpt_msg(msg);
//    if (rptmsg->vector == VECTOR_RPT_REQUEST)
//    {
//      if (rpt_uid_matches_mine(&rptmsg->header.dest_uid))
//      {
//        if (rptmsg->header.dest_endpoint_id == E133_NULL_ENDPOINT)
//        {
//          const RdmCmdList *cmdlist = get_rdm_cmd_list(rptmsg);
//          handle_rdm_command(&rptmsg->header, &cmdlist->list->msg, requires_reconnect);
//        }
//        else
//        {
//          send_status(VECTOR_RPT_STATUS_UNKNOWN_ENDPOINT, &rptmsg->header);
//          lwpa_log(device_state.lparams, LWPA_LOG_WARNING,
//                   "Device received RPT message addressed to unknown Endpoint ID %d",
//                   rptmsg->header.dest_endpoint_id);
//        }
//      }
//      else
//      {
//        send_status(VECTOR_RPT_STATUS_UNKNOWN_RPT_UID, &rptmsg->header);
//        lwpa_log(device_state.lparams, LWPA_LOG_WARNING,
//                 "Device received RPT message addressed to unknown UID %04x:%08x", rptmsg->header.dest_uid.manu,
//                 rptmsg->header.dest_uid.id);
//      }
//    }
//    else
//    {
//      send_status(VECTOR_RPT_STATUS_UNKNOWN_VECTOR, &rptmsg->header);
//      lwpa_log(device_state.lparams, LWPA_LOG_WARNING, "Device received RPT message with unhandled vector type %d",
//               msg->vector);
//    }
//  }
//  else
//  {
//    lwpa_log(device_state.lparams, LWPA_LOG_WARNING, "Device received root message with unhandled vector type %d",
//             msg->vector);
//  }
//}
//
// void handle_rdm_command(const RptHeader *received_header, const RdmBuffer *cmd, bool *requires_reconnect)
//{
//  RdmCommand cmd_data;
//  if (LWPA_OK != rdmresp_unpack_command(cmd, &cmd_data))
//  {
//    send_status(VECTOR_RPT_STATUS_INVALID_MESSAGE, received_header);
//    lwpa_log(device_state.lparams, LWPA_LOG_WARNING, "Device received incorrectly-formatted RDM command.");
//  }
//  else if (!rdm_uid_matches_mine(&cmd_data.dest_uid))
//  {
//    send_status(VECTOR_RPT_STATUS_UNKNOWN_RDM_UID, received_header);
//    lwpa_log(device_state.lparams, LWPA_LOG_WARNING, "Device received RDM command addressed to unknown UID %04x:%08x",
//             cmd_data.dest_uid.manu, cmd_data.dest_uid.id);
//  }
//  else if (cmd_data.command_class != E120_GET_COMMAND && cmd_data.command_class != E120_SET_COMMAND)
//  {
//    send_status(VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS, received_header);
//    lwpa_log(device_state.lparams, LWPA_LOG_WARNING, "Device received RDM command with invalid command class %d",
//             cmd_data.command_class);
//  }
//  else if (!default_responder_supports_pid(cmd_data.param_id))
//  {
//    send_nack(received_header, &cmd_data, E120_NR_UNKNOWN_PID);
//    lwpa_log(device_state.lparams, LWPA_LOG_DEBUG, "Sending NACK to Controller %04x:%08x for unknown PID 0x%04x",
//             received_header->source_uid.manu, received_header->source_uid.id, cmd_data.param_id);
//  }
//  else
//  {
//    switch (cmd_data.command_class)
//    {
//      case E120_SET_COMMAND:
//      {
//        uint16_t nack_reason;
//        if (default_responder_set(cmd_data.param_id, cmd_data.data, cmd_data.datalen, &nack_reason,
//        requires_reconnect))
//        {
//          RdmResponse resp_data;
//          RdmCmdListEntry resp;
//
//          resp_data.src_uid = device_state.my_uid;
//          resp_data.dest_uid = kBroadcastUid;
//          resp_data.transaction_num = cmd_data.transaction_num;
//          resp_data.resp_type = E120_RESPONSE_TYPE_ACK;
//          resp_data.msg_count = 0;
//          resp_data.subdevice = 0;
//          resp_data.command_class = E120_SET_COMMAND_RESPONSE;
//          resp_data.param_id = cmd_data.param_id;
//          resp_data.datalen = 0;
//
//          if (LWPA_OK == rdmresp_create_response(&resp_data, &resp.msg))
//          {
//            RdmCmdListEntry orig_cmd;
//            RptHeader header = *received_header;
//            header.source_uid = kRdmnetControllerBroadcastUid;
//
//            orig_cmd.msg = *cmd;
//            orig_cmd.next = &resp;
//            resp.next = NULL;
//
//            send_notification(&header, &orig_cmd);
//            lwpa_log(device_state.lparams, LWPA_LOG_DEBUG,
//                     "ACK'ing SET_COMMAND for PID 0x%04x from Controller %04x:%08x", cmd_data.param_id,
//                     received_header->source_uid.manu, received_header->source_uid.id);
//          }
//        }
//        else
//        {
//          send_nack(received_header, &cmd_data, nack_reason);
//          lwpa_log(device_state.lparams, LWPA_LOG_DEBUG,
//                   "Sending SET_COMMAND NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
//                   received_header->source_uid.manu, received_header->source_uid.id, cmd_data.param_id, nack_reason);
//        }
//        break;
//      }
//      case E120_GET_COMMAND:
//      {
//        static param_data_list_t resp_data_list;
//        static RdmCmdListEntry resp_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
//        size_t num_responses;
//        uint16_t nack_reason;
//        if (default_responder_get(cmd_data.param_id, cmd_data.data, cmd_data.datalen, resp_data_list, &num_responses,
//                                  &nack_reason))
//        {
//          RdmResponse resp_data;
//
//          resp_data.src_uid = device_state.my_uid;
//          resp_data.dest_uid = received_header->source_uid;
//          resp_data.transaction_num = cmd_data.transaction_num;
//          resp_data.resp_type = num_responses > 1 ? E120_RESPONSE_TYPE_ACK_OVERFLOW : E120_RESPONSE_TYPE_ACK;
//          resp_data.msg_count = 0;
//          resp_data.subdevice = 0;
//          resp_data.command_class = E120_GET_COMMAND_RESPONSE;
//          resp_data.param_id = cmd_data.param_id;
//
//          size_t i;
//          for (i = 0; i < num_responses; ++i)
//          {
//            memcpy(resp_data.data, resp_data_list[i].data, resp_data_list[i].datalen);
//            resp_data.datalen = resp_data_list[i].datalen;
//            if (i == num_responses - 1)
//            {
//              resp_data.resp_type = E120_RESPONSE_TYPE_ACK;
//              resp_list[i].next = NULL;
//            }
//            else
//              resp_list[i].next = &resp_list[i + 1];
//            rdmresp_create_response(&resp_data, &resp_list[i].msg);
//          }
//          send_notification(received_header, resp_list);
//          lwpa_log(device_state.lparams, LWPA_LOG_DEBUG, "ACK'ing GET_COMMAND for PID 0x%04x from Controller
//          %04x:%08x",
//                   cmd_data.param_id, received_header->source_uid.manu, received_header->source_uid.id);
//        }
//        else
//        {
//          send_nack(received_header, &cmd_data, nack_reason);
//          lwpa_log(device_state.lparams, LWPA_LOG_DEBUG,
//                   "Sending GET_COMMAND NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
//                   received_header->source_uid.manu, received_header->source_uid.id, cmd_data.param_id, nack_reason);
//        }
//        break;
//      }
//      default:
//        break;
//    }
//  }
//}
//
// void send_status(uint16_t status_code, const RptHeader *received_header)
//{
//  RptHeader header_to_send;
//  RptStatusMsg status;
//  lwpa_error_t send_res;
//
//  swap_header_data(received_header, &header_to_send);
//
//  status.status_code = status_code;
//  rpt_status_msg_set_empty_status_str(&status);
//  send_res = send_rpt_status(device_state.broker_conn, &device_state.my_cid, &header_to_send, &status);
//  if (send_res != LWPA_OK)
//  {
//    lwpa_log(device_state.lparams, LWPA_LOG_ERR, "Error sending RPT Status message to Broker.");
//  }
//}
//
// void send_nack(const RptHeader *received_header, const RdmCommand *cmd_data, uint16_t nack_reason)
//{
//  RdmResponse resp_data;
//  RdmCmdListEntry resp;
//
//  resp_data.src_uid = device_state.my_uid;
//  resp_data.dest_uid = received_header->source_uid;
//  resp_data.transaction_num = cmd_data->transaction_num;
//  resp_data.resp_type = E120_RESPONSE_TYPE_NACK_REASON;
//  resp_data.msg_count = 0;
//  resp_data.subdevice = 0;
//  resp_data.command_class = cmd_data->command_class + 1;
//  resp_data.param_id = cmd_data->param_id;
//  resp_data.datalen = 2;
//  lwpa_pack_16b(resp_data.data, nack_reason);
//
//  if (LWPA_OK == rdmresp_create_response(&resp_data, &resp.msg))
//  {
//    resp.next = NULL;
//    send_notification(received_header, &resp);
//  }
//}
//
// void send_notification(const RptHeader *received_header, const RdmCmdListEntry *cmd_list)
//{
//  RptHeader header_to_send;
//  lwpa_error_t send_res;
//
//  swap_header_data(received_header, &header_to_send);
//
//  send_res = send_rpt_notification(device_state.broker_conn, &device_state.my_cid, &header_to_send, cmd_list);
//  if (send_res != LWPA_OK)
//  {
//    lwpa_log(device_state.lparams, LWPA_LOG_ERR, "Error sending RPT Notification message to Broker.");
//  }
//}
//
