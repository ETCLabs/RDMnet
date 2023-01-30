/******************************************************************************
 * Copyright 2020 ETC Inc.
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

#ifndef RDMNET_CORE_LLRP_TARGET_H_
#define RDMNET_CORE_LLRP_TARGET_H_

#include "etcpal/inet.h"
#include "etcpal/mutex.h"
#include "etcpal/timer.h"
#include "rdmnet/llrp_target.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/llrp_prot.h"
#include "rdmnet/core/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RCLlrpTarget RCLlrpTarget;

typedef struct RCLlrpTargetSyncRdmResponse
{
  RdmnetSyncRdmResponse resp;
  uint8_t*              response_buf;
} RCLlrpTargetSyncRdmResponse;

#define RC_LLRP_TARGET_SYNC_RDM_RESPONSE_INIT \
  {                                           \
    RDMNET_SYNC_RDM_RESPONSE_INIT, NULL       \
  }

// An RDM command has been received addressed to an LLRP target. Fill in response with response
// data if responding synchronously.
typedef void (*RCLlrpTargetRdmCommandReceivedCallback)(RCLlrpTarget*                target,
                                                       const LlrpRdmCommand*        cmd,
                                                       RCLlrpTargetSyncRdmResponse* response);

// An LLRP target has been destroyed and unregistered. This is called from the background thread,
// after the resources associated with the LLRP target (e.g. sockets) have been cleaned up.
typedef void (*RCLlrpTargetDestroyedCallback)(RCLlrpTarget* target);

// The set of callbacks which are called with notifications about LLRP targets.
typedef struct RCLlrpTargetCallbacks
{
  RCLlrpTargetRdmCommandReceivedCallback rdm_command_received;
  RCLlrpTargetDestroyedCallback          destroyed;
} RCLlrpTargetCallbacks;

typedef struct RCLlrpTargetNetintInfo
{
  EtcPalMcastNetintId id;
  etcpal_socket_t     send_sock;
  uint8_t             send_buf[LLRP_TARGET_MAX_MESSAGE_SIZE];

  bool        reply_pending;
  EtcPalUuid  pending_reply_cid;
  uint32_t    pending_reply_trans_num;
  EtcPalTimer reply_backoff;
} RCLlrpTargetNetintInfo;

typedef struct RCLlrpTarget
{
  /////////////////////////////////////////////////////////////////////////////
  // Fill this in before initialization.

  EtcPalUuid            cid;
  RdmUid                uid;
  llrp_component_t      component_type;
  RCLlrpTargetCallbacks callbacks;
  etcpal_mutex_t*       lock;

  /////////////////////////////////////////////////////////////////////////////

  RC_DECLARE_BUF(RCLlrpTargetNetintInfo, netints, RDMNET_MAX_MCAST_NETINTS);

  // Global target state info
  bool connected_to_broker;
} RCLlrpTarget;

etcpal_error_t rc_llrp_target_module_init(const RdmnetNetintConfig* netint_config);
void           rc_llrp_target_module_deinit(void);

etcpal_error_t rc_llrp_target_register(RCLlrpTarget* target);
void           rc_llrp_target_unregister(RCLlrpTarget* target);
void           rc_llrp_target_update_connection_state(RCLlrpTarget* target, bool connected_to_broker);

etcpal_error_t rc_llrp_target_send_ack(RCLlrpTarget*              target,
                                       const LlrpSavedRdmCommand* received_cmd,
                                       const uint8_t*             response_data,
                                       uint8_t                    response_data_len);
etcpal_error_t rc_llrp_target_send_nack(RCLlrpTarget*              target,
                                        const LlrpSavedRdmCommand* received_cmd,
                                        rdm_nack_reason_t          nack_reason);

void rc_llrp_target_module_tick(void);
void rc_llrp_target_data_received(const uint8_t* data, size_t data_len, const EtcPalMcastNetintId* netint);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_LLRP_TARGET_H_ */
