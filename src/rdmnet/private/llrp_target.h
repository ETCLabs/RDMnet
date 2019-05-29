/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#ifndef _RDMNET_PRIVATE_LLRP_TARGET_H_
#define _RDMNET_PRIVATE_LLRP_TARGET_H_

#include "lwpa/uuid.h"
#include "lwpa/bool.h"
#include "lwpa/int.h"
#include "lwpa/inet.h"
#include "lwpa/rbtree.h"
#include "lwpa/timer.h"
#include "lwpa/socket.h"
#include "lwpa/root_layer_pdu.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/llrp_target.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/llrp_prot.h"

typedef struct LlrpTarget LlrpTarget;

typedef struct LlrpTargetNetintInfo
{
  LwpaIpAddr ip;
  lwpa_socket_t sys_sock;
  uint8_t send_buf[LLRP_TARGET_MAX_MESSAGE_SIZE];
  PolledSocketInfo poll_info;

  bool reply_pending;
  LwpaUuid pending_reply_cid;
  uint32_t pending_reply_trans_num;
  LwpaTimer reply_backoff;

  LlrpTarget *target;
} LlrpTargetNetintInfo;

struct LlrpTarget
{
  llrp_target_t handle;

  // Identifying info
  LwpaUuid cid;
  RdmUid uid;
  llrp_component_t component_type;

  // Network interfaces
#if RDMNET_DYNAMIC_MEM
  LlrpTargetNetintInfo *netints;
#else
  LlrpTargetNetintInfo netints[RDMNET_LLRP_MAX_TARGET_NETINTS];
#endif
  size_t num_netints;

  bool connected_to_broker;

  // Callback dispatch info
  LlrpTargetCallbacks callbacks;
  void *callback_context;

  // Synchronized destruction tracking
  bool marked_for_destruction;
  LlrpTarget *next_to_destroy;
};

typedef enum
{
  kTargetCallbackNone,
  kTargetCallbackRdmCmdReceived
} target_callback_t;

typedef struct RdmCmdReceivedArgs
{
  LlrpRemoteRdmCommand cmd;
} RdmCmdReceivedArgs;

typedef struct TargetCallbackDispatchInfo
{
  llrp_target_t handle;
  LlrpTargetCallbacks cbs;
  void *context;

  target_callback_t which;
  union
  {
    RdmCmdReceivedArgs cmd_received;
  } args;
} TargetCallbackDispatchInfo;

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_llrp_target_init();
void rdmnet_llrp_target_deinit();

void rdmnet_llrp_target_tick();

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_PRIVATE_LLRP_TARGET_H_ */
