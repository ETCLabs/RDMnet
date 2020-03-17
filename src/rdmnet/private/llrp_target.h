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

#ifndef RDMNET_PRIVATE_LLRP_TARGET_H_
#define RDMNET_PRIVATE_LLRP_TARGET_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/acn_rlp.h"
#include "etcpal/inet.h"
#include "etcpal/rbtree.h"
#include "etcpal/timer.h"
#include "etcpal/socket.h"
#include "etcpal/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/core.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/llrp_target.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/llrp_prot.h"

typedef struct LlrpTarget LlrpTarget;

typedef struct LlrpTargetNetintInfo
{
  RdmnetMcastNetintId id;
  etcpal_socket_t send_sock;
  uint8_t send_buf[LLRP_TARGET_MAX_MESSAGE_SIZE];

  bool reply_pending;
  EtcPalUuid pending_reply_cid;
  uint32_t pending_reply_trans_num;
  EtcPalTimer reply_backoff;
} LlrpTargetNetintInfo;

// A struct containing the map keys we use for LLRP targets.
typedef struct LlrpTargetKeys
{
  llrp_target_t handle;
  EtcPalUuid cid;
} LlrpTargetKeys;

struct LlrpTarget
{
  // Identifying info
  LlrpTargetKeys keys;
  RdmUid uid;
  llrp_component_t component_type;

  // Network interfaces on which the target is operating (value type is LlrpTargetNetintInfo)
#if RDMNET_DYNAMIC_MEM
  LlrpTargetNetintInfo* netints;
#else
  LlrpTargetNetintInfo netints[RDMNET_MAX_MCAST_NETINTS];
#endif
  size_t num_netints;

  // Global target state info
  bool connected_to_broker;

  // Callback dispatch info
  LlrpTargetCallbacks callbacks;
  void* callback_context;

  // Synchronized destruction tracking
  bool marked_for_destruction;
  LlrpTarget* next_to_destroy;
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
  void* context;

  target_callback_t which;
  union
  {
    RdmCmdReceivedArgs cmd_received;
  } args;
} TargetCallbackDispatchInfo;

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t rdmnet_llrp_target_init();
void rdmnet_llrp_target_deinit();

void rdmnet_llrp_target_tick();

void target_data_received(const uint8_t* data, size_t data_size, const RdmnetMcastNetintId* netint);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_LLRP_TARGET_H_ */
