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

#ifndef RDMNET_PRIVATE_LLRP_H_
#define RDMNET_PRIVATE_LLRP_H_

#include "etcpal/bool.h"
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/rbtree.h"
#include "etcpal/socket.h"
#include "rdmnet/core.h"
#include "rdmnet/core/llrp.h"

typedef enum
{
  kLlrpSocketTypeManager,
  kLlrpSocketTypeTarget
} llrp_socket_t;

#ifdef __cplusplus
extern "C" {
#endif

extern const EtcPalSockAddr* kLlrpIpv4RespAddr;
extern const EtcPalSockAddr* kLlrpIpv6RespAddr;
extern const EtcPalSockAddr* kLlrpIpv4RequestAddr;
extern const EtcPalSockAddr* kLlrpIpv6RequestAddr;

etcpal_error_t rdmnet_llrp_init(void);
void rdmnet_llrp_deinit(void);

void rdmnet_llrp_tick(void);

etcpal_error_t llrp_recv_netint_add(const RdmnetMcastNetintId* netint, llrp_socket_t llrp_type);
void llrp_recv_netint_remove(const RdmnetMcastNetintId* netint, llrp_socket_t llrp_type);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_LLRP_H_ */
