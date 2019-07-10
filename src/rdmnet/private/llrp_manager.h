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

#ifndef _RDMNET_PRIVATE_LLRP_MANAGER_H_
#define _RDMNET_PRIVATE_LLRP_MANAGER_H_

#include "lwpa/inet.h"
#include "lwpa/socket.h"
#include "lwpa/timer.h"
#include "lwpa/rbtree.h"
#include "rdm/uid.h"
#include "rdmnet/core/llrp_manager.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/llrp_prot.h"

typedef struct DiscoveredTargetInternal DiscoveredTargetInternal;
struct DiscoveredTargetInternal
{
  KnownUid known_uid;
  LwpaUuid cid;
  DiscoveredTargetInternal *next;
};

typedef struct LlrpManager LlrpManager;
struct LlrpManager
{
  // Identification
  llrp_manager_t handle;
  LwpaUuid cid;
  RdmUid uid;

  // Underlying networking info
  lwpa_iptype_t ip_type;
  unsigned int netint_index;
  lwpa_socket_t sys_sock;
  PolledSocketInfo poll_info;

  // Send tracking
  uint8_t send_buf[LLRP_MANAGER_MAX_MESSAGE_SIZE];
  uint32_t transaction_number;

  // Discovery tracking
  bool discovery_active;
  unsigned int num_clean_sends;
  LwpaTimer disc_timer;
  uint16_t disc_filter;
  LwpaRbTree discovered_targets;
  RdmUid cur_range_low;
  RdmUid cur_range_high;

  // Callback dispatch info
  LlrpManagerCallbacks callbacks;
  void *callback_context;

  // Synchronized destruction tracking
  bool marked_for_destruction;
  LlrpManager *next_to_destroy;
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
  const DiscoveredLlrpTarget *target;
} TargetDiscoveredArgs;

typedef struct RdmRespReceivedArgs
{
  LlrpRemoteRdmResponse resp;
} RdmRespReceivedArgs;

typedef struct ManagerCallbackDispatchInfo
{
  llrp_manager_t handle;
  LlrpManagerCallbacks cbs;
  void *context;

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

lwpa_error_t rdmnet_llrp_manager_init();
void rdmnet_llrp_manager_deinit();

void rdmnet_llrp_manager_tick();

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_PRIVATE_LLRP_MANAGER_H_ */
