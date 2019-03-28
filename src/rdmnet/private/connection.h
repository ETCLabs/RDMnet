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

/*! \file rdmnet/private/connection.h
 *  \brief The internal definition for an RDMnet connection.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_PRIVATE_CONNECTION_H_
#define _RDMNET_PRIVATE_CONNECTION_H_

#include "lwpa/bool.h"
#include "lwpa/int.h"
#include "lwpa/lock.h"
#include "lwpa/timer.h"
#include "lwpa/inet.h"
#include "lwpa/socket.h"
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

typedef struct RdmnetConnectionInternal RdmnetConnectionInternal;
struct RdmnetConnectionInternal
{
  /* Identification */
  LwpaUuid local_cid;
  lwpa_socket_t sock;
  LwpaSockaddr remote_addr;
  bool is_client;
  bool is_blocking;

  /* Connection state */
  conn_state_t state;
  ClientConnectMsg conn_data;
  LwpaTimer send_timer;
  LwpaTimer hb_timer;
  LwpaTimer backoff_timer;
  bool rdmnet_conn_failed;

  /* Send tracking */
  lwpa_mutex_t send_lock;

  /* Receive tracking */
  RdmnetMsgBuf recv_buf;

  /* Synchronization */
  lwpa_mutex_t lock;

  /* Callbacks */
  RdmnetConnCallbacks callbacks;
  void *callback_context;

  RdmnetConnectionInternal *next;
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
  void *context;

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

lwpa_error_t rdmnet_connection_init();
void rdmnet_connection_deinit();

#if RDMNET_POLL_CONNECTIONS_INTERNALLY
void rdmnet_connection_recv();
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_PRIVATE_CONNECTION_H_ */
