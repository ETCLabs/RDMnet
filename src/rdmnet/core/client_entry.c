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

#include "rdmnet/core/client_entry.h"
#include "rdmnet/core/util.h"

bool create_rpt_client_entry(const EtcPalUuid* cid, const RdmUid* uid, rpt_client_type_t client_type,
                             const EtcPalUuid* binding_cid, ClientEntryData* entry)
{
  if (!cid || !uid || !entry)
    return false;

  entry->client_protocol = kClientProtocolRPT;
  entry->client_cid = *cid;
  entry->data.rpt_data.client_uid = *uid;
  entry->data.rpt_data.client_type = client_type;
  if (binding_cid)
    entry->data.rpt_data.binding_cid = *binding_cid;
  else
    memset(entry->data.rpt_data.binding_cid.data, 0, ETCPAL_UUID_BYTES);
  entry->next = NULL;
  return true;
}

bool create_ept_client_entry(const EtcPalUuid* cid, const EptSubProtocol* protocol_arr, size_t protocol_arr_size,
                             ClientEntryData* entry)
{
  RDMNET_UNUSED_ARG(cid);
  RDMNET_UNUSED_ARG(protocol_arr);
  RDMNET_UNUSED_ARG(protocol_arr_size);
  RDMNET_UNUSED_ARG(entry);
  // TODO
  return false;
}
