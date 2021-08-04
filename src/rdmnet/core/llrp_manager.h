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

#ifndef RDMNET_CORE_LLRP_MANAGER_H_
#define RDMNET_CORE_LLRP_MANAGER_H_

#include "etcpal/inet.h"
#include "etcpal/mutex.h"
#include "etcpal/rbtree.h"
#include "etcpal/timer.h"
#include "etcpal/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/llrp.h"
#include "rdmnet/message.h"
#include "rdmnet/core/llrp_prot.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RCLlrpManager RCLlrpManager;

// An LLRP target has been discovered.
typedef void (*RCLlrpManagerTargetDiscoveredCallback)(RCLlrpManager* manager, const LlrpDiscoveredTarget* target);

// An RDM response has been received from an LLRP target.
typedef void (*RCLlrpManagerRdmResponseReceivedCallback)(RCLlrpManager* manager, const LlrpRdmResponse* resp);

// The previously-started LLRP discovery process has finished.
typedef void (*RCLlrpManagerDiscoveryFinishedCallback)(RCLlrpManager* manager);

// An LLRP manager has been destroyed and unregistered. This is called from the background thread,
// after the resources associated with the LLRP manager (e.g. sockets) have been cleaned up.
typedef void (*RCLlrpManagerDestroyedCallback)(RCLlrpManager* manager);

typedef struct RCLlrpManagerCallbacks
{
  RCLlrpManagerTargetDiscoveredCallback    target_discovered;
  RCLlrpManagerRdmResponseReceivedCallback rdm_response_received;
  RCLlrpManagerDiscoveryFinishedCallback   discovery_finished;
  RCLlrpManagerDestroyedCallback           destroyed;
} RCLlrpManagerCallbacks;

struct RCLlrpManager
{
  /////////////////////////////////////////////////////////////////////////////
  // Fill this in before initialization.
  EtcPalUuid             cid;
  RdmUid                 uid;
  EtcPalMcastNetintId    netint;
  RCLlrpManagerCallbacks callbacks;
  etcpal_mutex_t*        lock;

  // Underlying networking info
  etcpal_socket_t send_sock;

  // Send tracking
  uint8_t  send_buf[LLRP_MANAGER_MAX_MESSAGE_SIZE];
  uint32_t transaction_number;

  // Discovery tracking
  bool         discovery_active;
  bool         response_received_since_last_probe;
  unsigned int num_clean_sends;
  EtcPalTimer  disc_timer;
  uint16_t     disc_filter;
  EtcPalRbTree discovered_targets;
  RdmUid       cur_range_low;
  RdmUid       cur_range_high;
  RdmUid       known_uids[LLRP_KNOWN_UID_SIZE];
  size_t       num_known_uids;
};

etcpal_error_t rc_llrp_manager_module_init(void);
void           rc_llrp_manager_module_deinit(void);

etcpal_error_t rc_llrp_manager_register(RCLlrpManager* manager);
void           rc_llrp_manager_unregister(RCLlrpManager* manager);

etcpal_error_t rc_llrp_manager_start_discovery(RCLlrpManager* manager, uint16_t filter);
etcpal_error_t rc_llrp_manager_stop_discovery(RCLlrpManager* manager);

etcpal_error_t rc_llrp_manager_send_rdm_command(RCLlrpManager*             manager,
                                                const LlrpDestinationAddr* destination,
                                                rdmnet_command_class_t     command_class,
                                                uint16_t                   param_id,
                                                const uint8_t*             data,
                                                uint8_t                    data_len,
                                                uint32_t*                  seq_num);

void rc_llrp_manager_module_tick(void);
void rc_llrp_manager_data_received(const uint8_t* data, size_t data_len, const EtcPalMcastNetintId* netint);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_LLRP_MANAGER_H_ */
