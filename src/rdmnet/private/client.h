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
#ifndef _RDMNET_PRIVATE_CLIENT_H_
#define _RDMNET_PRIVATE_CLIENT_H_

#include "rdmnet/client.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/discovery.h"
#include "rdmnet/core/connection.h"

typedef enum
{
  kScopeStateDiscovery,
  kScopeStateConnecting,
  kScopeStateConnected
} scope_state_t;

typedef struct RdmnetClient RdmnetClient;

typedef struct ClientScopeListEntry ClientScopeListEntry;
struct ClientScopeListEntry
{
  rdmnet_client_scope_t handle;
  RdmnetScopeConfig config;
  scope_state_t state;
  RdmUid uid;
  uint32_t send_seq_num;

  rdmnet_scope_monitor_t monitor_handle;

  RdmnetClient *client;
  ClientScopeListEntry *next;
};

#define rpt_client_uid_is_dynamic(configuidptr) ((configuidptr)->id == 0)

typedef struct RptClientData
{
  rpt_client_type_t type;
  bool has_static_uid;
  RdmUid uid;
  RptClientCallbacks callbacks;
} RptClientData;

typedef struct EptClientData
{
  EptClientCallbacks callbacks;
} EptClientData;

struct RdmnetClient
{
  rdmnet_client_t handle;
  client_protocol_t type;
  LwpaUuid cid;
  void *callback_context;
  ClientScopeListEntry *scope_list;
  char search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];

  llrp_target_t llrp_handle;

  union
  {
    RptClientData rpt;
    EptClientData ept;
  } data;
};

typedef enum
{
  kClientCallbackNone,
  kClientCallbackConnected,
  kClientCallbackConnectFailed,
  kClientCallbackDisconnected,
  kClientCallbackBrokerMsgReceived,
  kClientCallbackLlrpMsgReceived,
  kClientCallbackMsgReceived
} client_callback_t;

typedef struct ConnectedArgs
{
  rdmnet_client_scope_t scope_handle;
  RdmnetClientConnectedInfo info;
} ConnectedArgs;

typedef struct ConnectFailedArgs
{
  rdmnet_client_scope_t scope_handle;
  RdmnetClientConnectFailedInfo info;
} ConnectFailedArgs;

typedef struct DisconnectedArgs
{
  rdmnet_client_scope_t scope_handle;
  RdmnetClientDisconnectedInfo info;
} DisconnectedArgs;

typedef struct BrokerMsgReceivedArgs
{
  rdmnet_client_scope_t scope_handle;
  const BrokerMessage *msg;
} BrokerMsgReceivedArgs;

typedef struct LlrpMsgReceivedArgs
{
  const LlrpRemoteRdmCommand *cmd;
} LlrpMsgReceivedArgs;

typedef struct RptMsgReceivedArgs
{
  rdmnet_client_scope_t scope_handle;
  RptClientMessage msg;
} RptMsgReceivedArgs;

typedef struct EptMsgReceivedArgs
{
  rdmnet_client_scope_t scope_handle;
  EptClientMessage msg;
} EptMsgReceivedArgs;

typedef struct RptCallbackDispatchInfo
{
  RptClientCallbacks cbs;
  union
  {
    RptMsgReceivedArgs msg_received;
    LlrpMsgReceivedArgs llrp_msg_received;
  } args;
} RptCallbackDispatchInfo;

typedef struct EptCallbackDispatchInfo
{
  EptClientCallbacks cbs;
  EptMsgReceivedArgs msg_received;
} EptCallbackDispatchInfo;

typedef struct ClientCallbackDispatchInfo
{
  rdmnet_client_t handle;
  client_protocol_t type;
  client_callback_t which;
  void *context;
  union
  {
    RptCallbackDispatchInfo rpt;
    EptCallbackDispatchInfo ept;
  } prot_info;
  union
  {
    ConnectedArgs connected;
    ConnectFailedArgs connect_failed;
    DisconnectedArgs disconnected;
    BrokerMsgReceivedArgs broker_msg_received;
  } common_args;
} ClientCallbackDispatchInfo;

#endif /* _RDMNET_PRIVATE_CLIENT_H_ */
