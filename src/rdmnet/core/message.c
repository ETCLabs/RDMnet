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

#include "rdmnet/core/message.h"
#include "rdmnet/private/message.h"

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(client_entries, ClientEntryData, RDMNET_MAX_CLIENT_ENTRIES);
LWPA_MEMPOOL_DEFINE(ept_subprots, EptSubProtocol, RDMNET_MAX_EPT_SUBPROTS);
LWPA_MEMPOOL_DEFINE(dynamic_uid_request_entries, DynamicUidRequestListEntry, RDM_MAX_DYNAMIC_UID_ENTRIES);
LWPA_MEMPOOL_DEFINE(dynamic_uid_mappings, DynamicUidMapping, RDMNET_MAX_DYNAMIC_UID_ENTRIES);
LWPA_MEMPOOL_DEFINE(fetch_uid_assignment_entries, FetchUidAssignmentListEntry, RDMNET_MAX_DYNAMIC_UID_ENTRIES);
LWPA_MEMPOOL_DEFINE(rdm_commands, RdmCmdListEntry, RDMNET_MAX_RDM_COMMANDS);
LWPA_MEMPOOL_DEFINE_ARRAY(rpt_status_strings, char, RPT_STATUS_STRING_MAXLEN + 1, 1);
#endif

/*********************** Private function prototypes *************************/

static void free_broker_message(BrokerMessage *bmsg);
static void free_rpt_message(RptMessage *rmsg);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_message_init()
{
  lwpa_error_t res = LWPA_OK;
#if !RDMNET_DYNAMIC_MEM
  res |= lwpa_mempool_init(client_entries);
  res |= lwpa_mempool_init(ept_subprots);
  res |= lwpa_mempool_init(dynamic_uid_request_entries);
  res |= lwpa_mempool_init(dynamic_uid_mappings);
  res |= lwpa_mempool_init(fetch_uid_assignment_entries);
  res |= lwpa_mempool_init(rdm_commands);
  res |= lwpa_mempool_init(rpt_status_strings);
#endif
  return res;
}

/*! \brief Free the resources held by an RdmnetMessage returned from another API function.
 *  \param[in] msg Pointer to message to free.
 */
void free_rdmnet_message(RdmnetMessage *msg)
{
  if (msg)
  {
    switch (msg->vector)
    {
      case ACN_VECTOR_ROOT_BROKER:
        free_broker_message(get_broker_msg(msg));
        break;
      case ACN_VECTOR_ROOT_RPT:
        free_rpt_message(get_rpt_msg(msg));
        break;
      default:
        break;
    }
  }
}

void free_broker_message(BrokerMessage *bmsg)
{
  switch (bmsg->vector)
  {
    case VECTOR_BROKER_CLIENT_ADD:
    case VECTOR_BROKER_CLIENT_REMOVE:
    case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
    case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
    {
      ClientList *clist = get_client_list(bmsg);
      ClientEntryData *entry = clist->client_entry_list;
      ClientEntryData *next_entry;
      while (entry)
      {
        if (entry->client_protocol == E133_CLIENT_PROTOCOL_EPT)
        {
          ClientEntryDataEpt *eptdata = get_ept_client_entry_data(entry);
          EptSubProtocol *subprot = eptdata->protocol_list;
          EptSubProtocol *next_subprot;
          while (subprot)
          {
            next_subprot = subprot->next;
            free_ept_subprot(subprot);
            subprot = next_subprot;
          }
        }
        next_entry = entry->next;
        free_client_entry(entry);
        entry = next_entry;
      }
      break;
    }
    case VECTOR_BROKER_REQUEST_DYNAMIC_UIDS:
    {
      DynamicUidRequestList *list = get_dynamic_uid_request_list(bmsg);
      DynamicUidRequestListEntry *entry = list->request_list;
      DynamicUidRequestListEntry *next_entry;
      while (entry)
      {
        next_entry = entry->next;
        free_dynamic_uid_request_entry(entry);
        entry = next_entry;
      }
      break;
    }
    case VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS:
    {
      DynamicUidAssignmentList *list = get_dynamic_uid_assignment_list(bmsg);
      DynamicUidMapping *mapping = list->mapping_list;
      DynamicUidMapping *next_mapping;
      while (mapping)
      {
        next_mapping = mapping->next;
        free_dynamic_uid_mapping(mapping);
        mapping = next_mapping;
      }
      break;
    }
    case VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST:
    {
      FetchUidAssignmentList *list = get_fetch_dynamic_uid_assignment_list(bmsg);
      FetchUidAssignmentListEntry *entry = list->assignment_list;
      FetchUidAssignmentListEntry *next_entry;
      while (entry)
      {
        next_entry = entry->next;
        free_fetch_uid_assignment_entry(entry);
        entry = next_entry;
      }
      break;
    }
    default:
      break;
  }
}

void free_rpt_message(RptMessage *rmsg)
{
  switch (rmsg->vector)
  {
    case VECTOR_RPT_REQUEST:
    case VECTOR_RPT_NOTIFICATION:
    {
      RdmCmdList *rlist = get_rdm_cmd_list(rmsg);
      RdmCmdListEntry *rdmcmd = rlist->list;
      RdmCmdListEntry *next_rdmcmd;
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
      RptStatusMsg *status = get_rpt_status_msg(rmsg);
      if (status->status_string)
      {
        free_rpt_status_str((char *)status->status_string);
        status->status_string = NULL;
      }
      break;
    }
    default:
      break;
  }
}
