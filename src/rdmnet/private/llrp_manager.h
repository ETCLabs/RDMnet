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

#ifndef RDMNET_PRIVATE_LLRP_MANAGER_H_
#define RDMNET_PRIVATE_LLRP_MANAGER_H_

#include "etcpal/inet.h"
#include "etcpal/socket.h"
#include "etcpal/timer.h"
#include "etcpal/rbtree.h"
#include "rdm/uid.h"
#include "rdmnet/core/llrp_manager.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/llrp_prot.h"

typedef struct DiscoveredTargetInternal DiscoveredTargetInternal;
struct DiscoveredTargetInternal
{
  RdmUid uid;
  EtcPalUuid cid;
  DiscoveredTargetInternal* next;
};

// A struct containing the map keys we use for LLRP managers.
typedef struct LlrpManagerKeys
{
  llrp_manager_t handle;
  EtcPalUuid cid;
  RdmnetMcastNetintId netint;
} LlrpManagerKeys;

typedef struct LlrpManager LlrpManager;
struct LlrpManager
{
  // Identification
  LlrpManagerKeys keys;
  RdmUid uid;

  // Underlying networking info
  etcpal_socket_t send_sock;

  // Send tracking
  uint8_t send_buf[LLRP_MANAGER_MAX_MESSAGE_SIZE];
  uint32_t transaction_number;

  // Discovery tracking
  bool discovery_active;
  unsigned int num_clean_sends;
  EtcPalTimer disc_timer;
  uint16_t disc_filter;
  EtcPalRbTree discovered_targets;
  RdmUid cur_range_low;
  RdmUid cur_range_high;
  RdmUid known_uids[LLRP_KNOWN_UID_SIZE];
  size_t num_known_uids;

  // Callback dispatch info
  LlrpManagerCallbacks callbacks;
  void* callback_context;

  // Synchronized destruction tracking
  bool marked_for_destruction;
  LlrpManager* next_to_destroy;
};

typedef enum
{
  kManagerCallbackNone,
  kManagerCallbackTargetDiscovered,
  kManagerCallbackDiscoveryFinished,
  kManagerCallbackRdmRespReceived
} manager_callback_t;

typedef struct TargetDiscoveredArgs
{
  const DiscoveredLlrpTarget* target;
} TargetDiscoveredArgs;

typedef struct RdmRespReceivedArgs
{
  LlrpRemoteRdmResponse resp;
} RdmRespReceivedArgs;

typedef struct ManagerCallbackDispatchInfo
{
  llrp_manager_t handle;
  LlrpManagerCallbacks cbs;
  void* context;

  manager_callback_t which;
  union
  {
    TargetDiscoveredArgs target_discovered;
    RdmRespReceivedArgs resp_received;
  } args;
} ManagerCallbackDispatchInfo;

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t rdmnet_llrp_manager_init();
void rdmnet_llrp_manager_deinit();

void rdmnet_llrp_manager_tick();

void manager_data_received(const uint8_t* data, size_t data_size, const RdmnetMcastNetintId* netint);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_LLRP_MANAGER_H_ */
