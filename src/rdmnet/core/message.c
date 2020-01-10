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

#include "rdmnet/core/message.h"
#include "rdmnet/private/message.h"

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
StaticMessageBuffer rdmnet_static_msg_buf;
char rpt_status_string_buffer[RPT_STATUS_STRING_MAXLEN + 1];
#endif

/*********************** Private function prototypes *************************/

static void free_broker_message(BrokerMessage* bmsg);
#if RDMNET_DYNAMIC_MEM
static void free_rpt_message(RptMessage* rmsg);
#endif

/*************************** Function definitions ****************************/

/*! \brief Initialize a LocalRdmResponse associated with a received RemoteRdmCommand.
 *
 *  Provide the received command and the array of RdmResponses to be sent in response.
 *
 *  \param[in] received_cmd Received command.
 *  \param[in] responses Array of RDM responses to the command.
 *  \param[in] num_responses Number of RDM responses in responses array.
 *  \param[out] resp Response to fill in.
 */
void rdmnet_create_response_from_command(const RemoteRdmCommand* received_cmd, const RdmResponse* responses,
                                         size_t num_responses, LocalRdmResponse* resp)
{
  if (received_cmd && responses && num_responses > 0 && resp)
  {
    // If we are ACK'ing a SET_COMMAND, we broadcast the response to keep other controllers
    // apprised of state.
    resp->dest_uid =
        (((received_cmd->rdm.command_class == kRdmCCSetCommand) && (responses[0].resp_type == kRdmResponseTypeAck))
             ? kRdmnetControllerBroadcastUid
             : received_cmd->source_uid);
    resp->source_endpoint = received_cmd->dest_endpoint;
    resp->seq_num = received_cmd->seq_num;
    resp->command_included = true;
    resp->cmd = received_cmd->rdm;
    resp->responses = responses;
    resp->num_responses = num_responses;
  }
}

/*! \brief Initialize an unsolicited LocalRdmResponse (without an associated command).
 *
 *  Provide the array of RdmResponses to be sent.
 *
 *  \param[in] source_endpoint Endpoint from which this response is originating.
 *  \param[in] responses Array of RDM responses to be sent.
 *  \param[in] num_responses Number of RDM responses in responses array.
 *  \param[out] resp Response to initialize.
 */
void rdmnet_create_unsolicited_response(uint16_t source_endpoint, const RdmResponse* responses, size_t num_responses,
                                        LocalRdmResponse* resp)
{
  if (responses && num_responses > 0 && resp)
  {
    resp->dest_uid = kRdmnetControllerBroadcastUid;
    resp->source_endpoint = source_endpoint;
    resp->seq_num = 0;
    resp->command_included = false;
    resp->responses = responses;
    resp->num_responses = num_responses;
  }
}

/*! \brief Initialize an RptStatusMsg containing a status string, associated with a received
 *         RemoteRdmCommand.
 *
 *  Provide the status code and string.
 *
 *  \param[in] received_cmd Received command.
 *  \param[in] status_code Status code to be sent.
 *  \param[in] status_str Status string to be sent.
 *  \param[out] status LocalRptStatus to initialize.
 */
void rdmnet_create_status_from_command_with_str(const RemoteRdmCommand* received_cmd, rpt_status_code_t status_code,
                                                const char* status_str, LocalRptStatus* status)
{
  if (received_cmd && status)
  {
    status->dest_uid = received_cmd->source_uid;
    status->source_endpoint = received_cmd->dest_endpoint;
    status->seq_num = received_cmd->seq_num;
    status->msg.status_code = status_code;
    status->msg.status_string = status_str;
  }
}

/*! \brief Initialize an RptStatusMsg associated with a received RemoteRdmCommand.
 *
 *  Provide the status code.
 *
 *  \param[in] received_cmd Received command.
 *  \param[in] status_code Status code to be sent.
 *  \param[out] status LocalRptStatus to initialize.
 */
void rdmnet_create_status_from_command(const RemoteRdmCommand* received_cmd, rpt_status_code_t status_code,
                                       LocalRptStatus* status)
{
  rdmnet_create_status_from_command_with_str(received_cmd, status_code, NULL, status);
}

/*! \brief Free the resources held by an RdmnetMessage returned from another API function.
 *  \param[in] msg Pointer to message to free.
 */
void free_rdmnet_message(RdmnetMessage* msg)
{
  if (msg)
  {
    switch (msg->vector)
    {
      case ACN_VECTOR_ROOT_BROKER:
        free_broker_message(GET_BROKER_MSG(msg));
        break;
#if RDMNET_DYNAMIC_MEM
      case ACN_VECTOR_ROOT_RPT:
        free_rpt_message(GET_RPT_MSG(msg));
        break;
#endif
      default:
        break;
    }
  }
}

void free_broker_message(BrokerMessage* bmsg)
{
  switch (bmsg->vector)
  {
    case VECTOR_BROKER_CLIENT_ADD:
    case VECTOR_BROKER_CLIENT_REMOVE:
    case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
    case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
    {
      ClientList* clist = GET_CLIENT_LIST(bmsg);
      if (IS_EPT_CLIENT_LIST(clist))
      {
        EptClientEntry* ept_entry_list = GET_EPT_CLIENT_LIST(clist)->client_entries;
        size_t ept_entry_list_size = GET_EPT_CLIENT_LIST(clist)->num_client_entries;
        for (EptClientEntry* ept_entry = ept_entry_list; ept_entry < ept_entry_list + ept_entry_list_size; ++ept_entry)
        {
          FREE_EPT_SUBPROT_LIST(ept_entry->protocol_list);
        }
#if RDMNET_DYNAMIC_MEM
        free(ept_entry_list);
#endif
      }
#if RDMNET_DYNAMIC_MEM
      else if (IS_RPT_CLIENT_LIST(clist))
      {
        free(GET_RPT_CLIENT_LIST(clist)->client_entries);
      }
#endif
      break;
    }
#if RDMNET_DYNAMIC_MEM
    case VECTOR_BROKER_REQUEST_DYNAMIC_UIDS:
      free(GET_DYNAMIC_UID_REQUEST_LIST(bmsg)->requests);
      break;
    case VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS:
      free(GET_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg)->mappings);
      break;
    case VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST:
      free(GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg)->uids);
      break;
#endif
    default:
      break;
  }
}

#if RDMNET_DYNAMIC_MEM
void free_rpt_message(RptMessage* rmsg)
{
  switch (rmsg->vector)
  {
    case VECTOR_RPT_REQUEST:
    case VECTOR_RPT_NOTIFICATION:
    {
      RdmBufList* rlist = GET_RDM_BUF_LIST(rmsg);
      RdmBufListEntry* rdmcmd = rlist->list;
      RdmBufListEntry* next_rdmcmd;
      while (rdmcmd)
      {
        next_rdmcmd = rdmcmd->next;
        free_rdm_command(rdmcmd);
        rdmcmd = next_rdmcmd;
      }
      break;
    }
    case VECTOR_RPT_STATUS:
    {
      RptStatusMsg* status = GET_RPT_STATUS_MSG(rmsg);
      if (status->status_string)
      {
        free_rpt_status_str((char*)status->status_string);
        status->status_string = NULL;
      }
      break;
    }
    default:
      break;
  }
}
#endif
