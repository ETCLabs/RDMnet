/******************************************************************************
 * Copyright 2020 ETC Inc.
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
#include "rdmnet/core/opts.h"

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
StaticMessageBuffer rdmnet_static_msg_buf;
char                rpt_status_string_buffer[RPT_STATUS_STRING_MAXLEN + 1];
#endif

/*********************** Private function prototypes *************************/

static void free_broker_message(BrokerMessage* bmsg);
#if RDMNET_DYNAMIC_MEM
static void free_rpt_message(RptMessage* rmsg);
#endif

/*************************** Function definitions ****************************/

/*
 * Free the resources held by an RdmnetMessage returned from another API function.
 * [in] msg Pointer to message to free.
 */
void rc_free_message_resources(RdmnetMessage* msg)
{
  if (msg)
  {
    switch (msg->vector)
    {
      case ACN_VECTOR_ROOT_BROKER:
        free_broker_message(RDMNET_GET_BROKER_MSG(msg));
        break;
#if RDMNET_DYNAMIC_MEM
      case ACN_VECTOR_ROOT_RPT:
        free_rpt_message(RDMNET_GET_RPT_MSG(msg));
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
    case VECTOR_BROKER_CONNECTED_CLIENT_LIST: {
      BrokerClientList* clist = BROKER_GET_CLIENT_LIST(bmsg);
      if (BROKER_IS_EPT_CLIENT_LIST(clist))
      {
        RdmnetEptClientEntry* ept_entry_list = BROKER_GET_EPT_CLIENT_LIST(clist)->client_entries;
        size_t                ept_entry_list_size = BROKER_GET_EPT_CLIENT_LIST(clist)->num_client_entries;
        for (RdmnetEptClientEntry* ept_entry = ept_entry_list; ept_entry < ept_entry_list + ept_entry_list_size;
             ++ept_entry)
        {
          FREE_EPT_SUBPROT_LIST(ept_entry->protocols);
        }
#if RDMNET_DYNAMIC_MEM
        free(ept_entry_list);
#endif
      }
#if RDMNET_DYNAMIC_MEM
      else if (BROKER_IS_RPT_CLIENT_LIST(clist))
      {
        free(BROKER_GET_RPT_CLIENT_LIST(clist)->client_entries);
      }
#endif
      break;
    }
#if RDMNET_DYNAMIC_MEM
    case VECTOR_BROKER_REQUEST_DYNAMIC_UIDS:
      free(BROKER_GET_DYNAMIC_UID_REQUEST_LIST(bmsg)->requests);
      break;
    case VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS:
      free(BROKER_GET_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg)->mappings);
      break;
    case VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST:
      free(BROKER_GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg)->uids);
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
      free(RPT_GET_RDM_BUF_LIST(rmsg)->rdm_buffers);
      break;
    case VECTOR_RPT_STATUS: {
      RptStatusMsg* status = RPT_GET_STATUS_MSG(rmsg);
      if (status->status_string)
      {
        free((char*)status->status_string);
        status->status_string = NULL;
      }
      break;
    }
    default:
      break;
  }
}
#endif
