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

/*! \file rdmnet/core/connection.h
 *  \brief RDMnet Connection API definitions
 *
 *  Functions and definitions for the \ref rdmnet_conn "RDMnet Connection API" are contained in this
 *  header.
 *
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CONNECTION_H_
#define _RDMNET_CONNECTION_H_

#include <stddef.h>
#include "lwpa/bool.h"
#include "lwpa/log.h"
#include "lwpa/error.h"
#include "lwpa/inet.h"
#include "lwpa/socket.h"
#include "rdmnet/core/message.h"

/*! \defgroup rdmnet_conn Connection
 *  \ingroup rdmnet_core_lib
 *  \brief Handle a connection between a Client and a %Broker in RDMnet.
 *
 *  In E1.33, the behavior of this module is dictated by the %Broker Protocol
 *  (&sect; 6).
 *
 *  Basic functionality for an RDMnet Client: Initialize the library using rdmnet_init(). Create a
 *  new connection using rdmnet_new_connection(). Connect to a %Broker using rdmnet_connect().
 *  Depending on the value of #RDMNET_USE_TICK_THREAD, may need to call rdmnet_tick() at regular
 *  intervals. Send data over the %Broker connection using rdmnet_send(), and receive data over the
 *  %Broker connection using rdmnet_recv().
 *
 *  @{
 */

typedef struct RdmnetConnectionInternal *rdmnet_conn_t;

typedef enum
{
  kRdmnetDisconnectSocketFailure,
  kRdmnetDisconnectNoHeartbeat,
  kRdmnetDisconnectGracefulRemoteInitiated,
  kRdmnetDisconnectGracefulLocalInitiated
} rdmnet_disconnect_type_t;

typedef struct RdmnetDisconnectInfo
{
  rdmnet_disconnect_type_t type;
  lwpa_error_t socket_err;
  rdmnet_disconnect_reason_t rdmnet_reason;
} RdmnetDisconnectInfo;

typedef struct RdmnetConnCallbacks
{
  void (*connected)(rdmnet_conn_t handle, void *context);
  void (*disconnected)(rdmnet_conn_t handle, const RdmnetDisconnectInfo *disconn_info, void *context);
  void (*msg_received)(rdmnet_conn_t handle, const RdmnetMessage *message, void *context);
} RdmnetConnCallbacks;

typedef struct RdmnetConnectionConfig
{
  LwpaUuid local_cid;
  RdmnetConnCallbacks callbacks;
  void *callback_context;
} RdmnetConnectionConfig;

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_new_connection(const RdmnetConnectionConfig *config, rdmnet_conn_t *handle);
lwpa_error_t rdmnet_connect(rdmnet_conn_t handle, const LwpaSockaddr *remote_addr,
                            const ClientConnectMsg *connect_data);
lwpa_error_t rdmnet_attach_existing_socket(rdmnet_conn_t handle, lwpa_socket_t sock, const LwpaSockaddr *remote_addr);
lwpa_error_t rdmnet_disconnect(rdmnet_conn_t handle, bool send_disconnect_msg,
                               rdmnet_disconnect_reason_t disconnect_reason);
lwpa_error_t rdmnet_destroy_connection(rdmnet_conn_t handle);

int rdmnet_send(int handle, const uint8_t *data, size_t size);

lwpa_error_t rdmnet_start_message(int handle);
int rdmnet_send_partial_message(int handle, const uint8_t *data, size_t size);
lwpa_error_t rdmnet_end_message(int handle);

void rdmnet_conn_tick();

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _RDMNET_CONNECTION_H_ */
