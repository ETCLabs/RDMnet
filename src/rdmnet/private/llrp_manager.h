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

#ifndef _RDMNET_PRIVATE_LLRP_MANAGER_H_
#define _RDMNET_PRIVATE_LLRP_MANAGER_H_

#include "lwpa/inet.h"
#include "lwpa/socket.h"
#include "lwpa/timer.h"
#include "lwpa/rbtree.h"
#include "rdm/uid.h"
#include "rdmnet/core/llrp_manager.h"
#include "rdmnet/private/llrp_prot.h"

typedef struct LlrpManager
{
  llrp_manager_t handle;

  LwpaIpAddr netint;
  uint8_t send_buf[LLRP_MANAGER_MAX_MESSAGE_SIZE];
  lwpa_socket_t sys_sock;

  uint32_t transaction_number;
  bool discovery_active;

  unsigned int num_clean_sends;
  LwpaTimer disc_timer;
  uint16_t disc_filter;

  LwpaRbTree known_uids;
  RdmUid cur_range_low;
  RdmUid cur_range_high;
} LlrpManager;

typedef enum
{
  kManagerCallbackNone,
  kManagerCallbackTargetDiscovered,
  kManagerCallbackDiscoveryFinished,
  kManagerCallbackRdmRespReceived
} manager_callback_t;

typedef struct TargetDiscoveredArgs
{
  DiscoveredLlrpTarget target;
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

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_PRIVATE_LLRP_MANAGER_H_ */