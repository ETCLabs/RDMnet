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

#ifndef RDMNET_CORE_COMMON_H_
#define RDMNET_CORE_COMMON_H_

#include <string.h>
#include "etcpal/lock.h"
#include "etcpal/log.h"
#include "etcpal/socket.h"
#include "rdmnet/common.h"
#include "rdmnet/core/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An initializer for an RdmnetSyncRdmResponse struct. */
#define RDMNET_SYNC_RDM_RESPONSE_INIT \
  {                                   \
    kRdmnetRdmResponseActionDefer     \
  }

/* An initializer for an RdmnetSyncRdmResponse struct. */
#define RDMNET_SYNC_EPT_RESPONSE_INIT \
  {                                   \
    kRdmnetEptResponseActionDefer     \
  }

/*
 * If using the externally-managed socket functions (advanced usage), this is the maximum data
 * length that can be given in one call to rdmnet_conn_sock_data_received().
 */
#define RDMNET_RECV_DATA_MAX_SIZE 1200

/*
 * RDMnet Core Library: implementation of the core functions of RDMnet.
 *
 * The core library sits underneath the higher-level RDMnet API modules and contains the
 * functionality that every component of RDMnet needs. This includes discovery, connections, and
 * LLRP, as well as message packing and unpacking.
 *
 * The core library's API is private to RDMnet library consumers and assumes a deeper knowledge of
 * how the library works under the hood.
 */

#define RDMNET_LOG(pri, ...) etcpal_log(rdmnet_log_params, (pri), RDMNET_LOG_MSG_PREFIX __VA_ARGS__)
#define RDMNET_LOG_EMERG(...) RDMNET_LOG(ETCPAL_LOG_EMERG, __VA_ARGS__)
#define RDMNET_LOG_ALERT(...) RDMNET_LOG(ETCPAL_LOG_ALERT, __VA_ARGS__)
#define RDMNET_LOG_CRIT(...) RDMNET_LOG(ETCPAL_LOG_CRIT, __VA_ARGS__)
#define RDMNET_LOG_ERR(...) RDMNET_LOG(ETCPAL_LOG_ERR, __VA_ARGS__)
#define RDMNET_LOG_WARNING(...) RDMNET_LOG(ETCPAL_LOG_WARNING, __VA_ARGS__)
#define RDMNET_LOG_NOTICE(...) RDMNET_LOG(ETCPAL_LOG_NOTICE, __VA_ARGS__)
#define RDMNET_LOG_INFO(...) RDMNET_LOG(ETCPAL_LOG_INFO, __VA_ARGS__)
#define RDMNET_LOG_DEBUG(...) RDMNET_LOG(ETCPAL_LOG_DEBUG, __VA_ARGS__)

#define RDMNET_CAN_LOG(pri) etcpal_can_log(rdmnet_log_params, (pri))

typedef union RCPolledSocketOpaqueData
{
  int   int_val;
  void* ptr;
} RCPolledSocketOpaqueData;

typedef void (*RCPolledSocketActivityCallback)(const EtcPalPollEvent* event, RCPolledSocketOpaqueData data);

typedef struct RCPolledSocketInfo
{
  RCPolledSocketActivityCallback callback;
  RCPolledSocketOpaqueData       data;
} RCPolledSocketInfo;

extern const EtcPalLogParams* rdmnet_log_params;

bool rdmnet_readlock(void);
void rdmnet_readunlock(void);
bool rdmnet_writelock(void);
void rdmnet_writeunlock(void);

etcpal_error_t rc_init(const EtcPalLogParams* log_params, const RdmnetNetintConfig* mcast_netints);
void           rc_deinit(void);
bool           rc_initialized(void);

void rc_tick(void);

etcpal_error_t rc_add_polled_socket(etcpal_socket_t socket, etcpal_poll_events_t events, RCPolledSocketInfo* info);
etcpal_error_t rc_modify_polled_socket(etcpal_socket_t socket, etcpal_poll_events_t events, RCPolledSocketInfo* info);
void           rc_remove_polled_socket(etcpal_socket_t socket);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_COMMON_H_ */
