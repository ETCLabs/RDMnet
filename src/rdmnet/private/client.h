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

/*! \file rdmnet/private/client.h
 */

#include "rdmnet/client.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/discovery.h"
#include "rdmnet/core/connection.h"

typedef struct RdmnetClientInternal RdmnetClientInternal;

typedef enum
{
  kScopeStateDiscovery,
  kScopeStateConnecting,
  kScopeStateConnected
} scope_state_t;

typedef struct ClientScopeListEntry ClientScopeListEntry;
struct ClientScopeListEntry
{
  RdmnetScopeConfig scope_config;
  rdmnet_conn_t conn_handle;
  scope_state_t state;
  rdmnet_scope_monitor_t monitor_handle;
  RdmnetClientInternal *cli;
  ClientScopeListEntry *next;
};

typedef struct RptClientData
{
  rpt_client_type_t type;
  bool has_static_uid;
  RdmUid static_uid;
} RptClientData;

typedef struct EptClientData
{
  int placeholder;  // TODO, to get it to compile for now
} EptClientData;

struct RdmnetClientInternal
{
  client_protocol_t type;
  LwpaUuid cid;
  RdmnetClientCallbacks callbacks;
  void *callback_context;
  ClientScopeListEntry *scope_list;

  union
  {
    RptClientData rpt;
    EptClientData ept;
  } data;

  RdmnetClientInternal *next;
};
