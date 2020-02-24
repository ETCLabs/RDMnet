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

#ifndef RDMNET_CORE_COMMON_H_
#define RDMNET_CORE_COMMON_H_

#include "etcpal/lock.h"
#include "etcpal/log.h"
#include "etcpal/socket.h"
#include "rdmnet/core.h"
#include "rdmnet/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * \defgroup rdmnet_core_lib RDMnet Core Library
 * \brief Implementation of the core functions of RDMnet.
 *
 * The core library sits underneath the higher-level RDMnet API modules and contains the
 * functionality that every component of RDMnet needs. This includes discovery, connections, and
 * LLRP, as well as message packing and unpacking.
 *
 * Most applications will not need to interact with the API functions in the core library directly,
 * although it does define types that are exposed through the higher-level APIs.
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

typedef union PolledSocketOpaqueData
{
  int int_val;
  rdmnet_conn_t conn_handle;
  void* ptr;
} PolledSocketOpaqueData;

typedef void (*PolledSocketActivityCallback)(const EtcPalPollEvent* event, PolledSocketOpaqueData data);

typedef struct PolledSocketInfo
{
  PolledSocketActivityCallback callback;
  PolledSocketOpaqueData data;
} PolledSocketInfo;

typedef void (*RdmnetTickFunction)(void);

extern const EtcPalLogParams* rdmnet_log_params;

bool rdmnet_core_initialized();

void rdmnet_core_tick();

bool rdmnet_readlock();
void rdmnet_readunlock();
bool rdmnet_writelock();
void rdmnet_writeunlock();

etcpal_error_t rdmnet_core_add_polled_socket(etcpal_socket_t socket, etcpal_poll_events_t events,
                                             PolledSocketInfo* info);
etcpal_error_t rdmnet_core_modify_polled_socket(etcpal_socket_t socket, etcpal_poll_events_t events,
                                                PolledSocketInfo* info);
void rdmnet_core_remove_polled_socket(etcpal_socket_t socket);

etcpal_error_t rdmnet_add_tick_function(RdmnetTickFunction function);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_COMMON_H_ */
