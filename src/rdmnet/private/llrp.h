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
#ifndef _RDMNET_PRIVATE_LLRP_H_
#define _RDMNET_PRIVATE_LLRP_H_

#include "lwpa/bool.h"
#include "lwpa/error.h"
#include "lwpa/inet.h"
#include "lwpa/rbtree.h"
#include "lwpa/socket.h"
#include "rdmnet/core/llrp.h"

#define LLRP_MULTICAST_TTL_VAL 20

typedef enum
{
  kLlrpSocketTypeManager,
  kLlrpSocketTypeTarget
} llrp_socket_t;

typedef struct LlrpNetint
{
  LlrpNetintId id;
  lwpa_socket_t send_sock;
  size_t send_ref_count;
  size_t recv_ref_count;
} LlrpNetint;

#ifdef __cplusplus
extern "C" {
#endif

extern const LwpaSockaddr* kLlrpIpv4RespAddr;
extern const LwpaSockaddr* kLlrpIpv6RespAddr;
extern const LwpaSockaddr* kLlrpIpv4RequestAddr;
extern const LwpaSockaddr* kLlrpIpv6RequestAddr;

extern uint8_t kLlrpLowestHardwareAddr[6];

lwpa_error_t rdmnet_llrp_init();
void rdmnet_llrp_deinit();

void rdmnet_llrp_tick();

void get_llrp_netint_list(LwpaRbIter* list_iter);

lwpa_error_t get_llrp_send_socket(const LlrpNetintId* netint, lwpa_socket_t* socket);
void release_llrp_send_socket(const LlrpNetintId* netint);

lwpa_error_t llrp_recv_netint_add(const LlrpNetintId* netint, llrp_socket_t llrp_type);
void llrp_recv_netint_remove(const LlrpNetintId* netint, llrp_socket_t llrp_type);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_PRIVATE_LLRP_H_ */
