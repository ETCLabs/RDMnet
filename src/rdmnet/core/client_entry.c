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

#include "etcpal/common.h"
#include "rdmnet/core/client_entry.h"

bool create_rpt_client_entry(const EtcPalUuid* cid, const RdmUid* uid, rpt_client_type_t client_type,
                             const EtcPalUuid* binding_cid, RptClientEntry* entry)
{
  if (!cid || !uid || !entry)
    return false;

  entry->cid = *cid;
  entry->uid = *uid;
  entry->type = client_type;
  if (binding_cid)
    entry->binding_cid = *binding_cid;
  else
    entry->binding_cid = kEtcPalNullUuid;
  return true;
}

bool create_ept_client_entry(const EtcPalUuid* cid, const EptSubProtocol* protocol_arr, size_t protocol_arr_size,
                             EptClientEntry* entry)
{
  ETCPAL_UNUSED_ARG(cid);
  ETCPAL_UNUSED_ARG(protocol_arr);
  ETCPAL_UNUSED_ARG(protocol_arr_size);
  ETCPAL_UNUSED_ARG(entry);
  // TODO
  return false;
}
