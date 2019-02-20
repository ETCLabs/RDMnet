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

#include "rdmnet/client.h"

#include <string.h>
#include "lwpa/lock.h"
#include "rdmnet/core.h"

/**************************** Private variables ******************************/

static bool client_lock_initted = false;
static lwpa_mutex_t client_lock;

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_rpt_client_create(const RdmnetRptClientConfig *config, rdmnet_client_t *handle)
{
  // The lock is created only on the first call to this function.
  if (!client_lock_initted)
  {
    if (lwpa_rwlock_create(&client_lock))
      client_lock_initted = true;
    else
      return LWPA_SYSERR;
  }

  //  /* Create a new connection handle */
  //  device_state.broker_conn = rdmnet_new_connection(&settings->cid);
  //  if (device_state.broker_conn < 0)
  //  {
  //    res = device_state.broker_conn;
  //    lwpa_log(lparams, LWPA_LOG_ERR, "Couldn't create a new RDMnet Connection due to error: '%s'.",
  //    lwpa_strerror(res)); rdmnet_deinit(); rdmnetdisc_deinit(); return res;
  //  }
}

bool create_rpt_client_entry(const LwpaUuid *cid, const RdmUid *uid, rpt_client_type_t client_type,
                             const LwpaUuid *binding_cid, ClientEntryData *entry)
{
  if (!cid || !uid || !entry)
    return false;

  entry->client_protocol = (client_protocol_t)E133_CLIENT_PROTOCOL_RPT;
  entry->client_cid = *cid;
  entry->data.rpt_data.client_uid = *uid;
  entry->data.rpt_data.client_type = client_type;
  if (binding_cid)
    entry->data.rpt_data.binding_cid = *binding_cid;
  else
    memset(entry->data.rpt_data.binding_cid.data, 0, LWPA_UUID_BYTES);
  entry->next = NULL;
  return true;
}
