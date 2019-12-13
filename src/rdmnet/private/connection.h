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

/*!
 * \file rdmnet/private/connection.h
 * \brief The internal definition for an RDMnet connection.
 * \author Sam Kearney
 */

#ifndef RDMNET_PRIVATE_CONNECTION_H_
#define RDMNET_PRIVATE_CONNECTION_H_

#include "etcpal/bool.h"
#include "etcpal/lock.h"
#include "etcpal/timer.h"
#include "etcpal/inet.h"
#include "etcpal/socket.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/msg_buf.h"

typedef enum
{
  kCSConnectNotStarted,
  kCSConnectPending,
  kCSBackoff,
  kCSTCPConnPending,
  kCSRDMnetConnPending,
  kCSHeartbeat,
  kCSMarkedForDestruction
} conn_state_t;

typedef struct RdmnetConnection RdmnetConnection;
struct RdmnetConnection
{
  // Identification
  // Because of the way the comparisons are optimized, the handle MUST always be the first member
  // of the struct.
  rdmnet_conn_t handle;
  EtcPalUuid local_cid;

  // Underlying socket connection
  etcpal_socket_t sock;
  EtcPalSockAddr remote_addr;
  bool external_socket_attached;
  bool is_blocking;
  PolledSocketInfo poll_info;

  // Connection state
  conn_state_t state;
  ClientConnectMsg conn_data;
  EtcPalTimer send_timer;
  EtcPalTimer hb_timer;
  EtcPalTimer backoff_timer;
  bool rdmnet_conn_failed;

  // Send and receive tracking
  RdmnetMsgBuf recv_buf;

  // Synchronization
  etcpal_mutex_t lock;

  // Callbacks
  RdmnetConnCallbacks callbacks;
  void* callback_context;

  // Destruction
  RdmnetConnection* next_to_destroy;
};

typedef enum
{
  kConnCallbackNone,
  kConnCallbackConnected,
  kConnCallbackConnectFailed,
  kConnCallbackDisconnected,
  kConnCallbackMsgReceived
} conn_callback_t;

typedef struct ConnConnectedArgs
{
  RdmnetConnectedInfo connect_info;
} ConnConnectedArgs;

typedef struct ConnConnectFailedArgs
{
  RdmnetConnectFailedInfo failed_info;
} ConnConnectFailedArgs;

typedef struct ConnDisconnectedArgs
{
  RdmnetDisconnectedInfo disconn_info;
} ConnDisconnectedArgs;

typedef struct ConnMsgReceivedArgs
{
  RdmnetMessage message;
} ConnMsgReceivedArgs;

typedef struct ConnCallbackDispatchInfo
{
  rdmnet_conn_t handle;
  RdmnetConnCallbacks cbs;
  void* context;

  conn_callback_t which;
  union
  {
    ConnConnectedArgs connected;
    ConnConnectFailedArgs connect_failed;
    ConnDisconnectedArgs disconnected;
    ConnMsgReceivedArgs msg_received;
  } args;
} ConnCallbackDispatchInfo;

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t rdmnet_conn_init();
void rdmnet_conn_deinit();

etcpal_error_t rdmnet_start_message(rdmnet_conn_t handle, RdmnetConnection** conn_out);
etcpal_error_t rdmnet_end_message(RdmnetConnection* conn);

void rdmnet_conn_tick();

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_CONNECTION_H_ */
