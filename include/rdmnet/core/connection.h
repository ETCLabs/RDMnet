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
#ifndef _RDMNET_CORE_CONNECTION_H_
#define _RDMNET_CORE_CONNECTION_H_

#include <stddef.h>
#include "lwpa/bool.h"
#include "lwpa/log.h"
#include "lwpa/error.h"
#include "lwpa/inet.h"
#include "lwpa/socket.h"
#include "rdmnet/core.h"
#include "rdmnet/core/message.h"

/*! \defgroup rdmnet_conn Connection
 *  \ingroup rdmnet_core_lib
 *  \brief Handle a connection between a Client and a %Broker in RDMnet.
 *
 *  In E1.33, the behavior of this module is dictated by the %Broker Protocol (&sect; 6).
 *
 *  Basic functionality for an RDMnet Client: Initialize the library using rdmnet_init(). Create a
 *  new connection using rdmnet_new_connection(). Connect to a %Broker using rdmnet_connect().
 *  Depending on the value of #RDMNET_USE_TICK_THREAD, may need to call rdmnet_tick() at regular
 *  intervals. Send data over the %Broker connection using rdmnet_send(), and receive data over the
 *  %Broker connection using rdmnet_recv().
 *
 *  @{
 */

/*! Information about a successful RDMnet connection. */
typedef struct RdmnetConnectedInfo
{
  /*! The broker's UID. */
  RdmUid broker_uid;
  /*! The client's UID (relevant if assigned dynamically) */
  RdmUid client_uid;
  /*! The remote address to which we are connected. This could be different from the original
   *  address requested in the case of a redirect. */
  LwpaSockaddr connected_addr;
} RdmnetConnectedInfo;

typedef enum
{
  kRdmnetConnectFailSocketFailure,
  kRdmnetConnectFailTcpLevel,
  kRdmnetConnectFailNoReply,
  kRdmnetConnectFailRejected
} rdmnet_connect_fail_event_t;

typedef struct RdmnetConnectFailedInfo
{
  rdmnet_connect_fail_event_t event;
  lwpa_error_t socket_err;
  rdmnet_connect_status_t rdmnet_reason;
} RdmnetConnectFailedInfo;

/*! An enumeration of the possible reasons an RDMnet connection could be disconnected. */
typedef enum
{
  kRdmnetDisconnectAbruptClose,
  kRdmnetDisconnectNoHeartbeat,
  kRdmnetDisconnectRedirected,
  kRdmnetDisconnectGracefulRemoteInitiated,
  kRdmnetDisconnectGracefulLocalInitiated
} rdmnet_disconnect_event_t;

typedef struct RdmnetDisconnectedInfo
{
  rdmnet_disconnect_event_t event;
  lwpa_error_t socket_err;
  rdmnet_disconnect_reason_t rdmnet_reason;
} RdmnetDisconnectedInfo;

/*! A set of callbacks which are called with notifications about RDMnet connections. */
typedef struct RdmnetConnCallbacks
{
  /*! \brief An RDMnet connection has connected successfully.
   *
   *  \param[in] handle Handle to connection which has connected.
   *  \param[in] connect_info More information about the successful connection.
   *  \param[in] context Context pointer that was given at the creation of the connection.
   */
  void (*connected)(rdmnet_conn_t handle, const RdmnetConnectedInfo *connect_info, void *context);

  /*! \brief An RDMnet connection attempt failed.
   *
   *  \param[in] handle Handle to connection which has failed.
   *  \param[in] failed_info More information about the connect failure event.
   *  \param[in] context Context pointer that was given at the creation of the connection.
   */
  void (*connect_failed)(rdmnet_conn_t handle, const RdmnetConnectFailedInfo *failed_info, void *context);

  /*! \brief A previously-connected RDMnet connection has disconnected.
   *
   *  \param[in] handle Handle to connection which has been disconnected.
   *  \param[in] disconn_info More information about the disconnect event.
   *  \param[in] context Context pointer that was given at the creation of the connection.
   */
  void (*disconnected)(rdmnet_conn_t handle, const RdmnetDisconnectedInfo *disconn_info, void *context);

  /*! \brief A message has been received on an RDMnet connection.
   *
   *  %Broker Protocol messages that affect connection status are consumed internally by the
   *  connection library and thus will not result in this callback. All other valid messages will be
   *  delivered.
   *
   *  \param[in] handle Handle to connection on which the message has been received.
   *  \param[in] message Contains the message received. Use the macros in \ref rdmnet_message to
   *                     decode.
   *  \param[in] context Context pointer that was given at creation of the connection.
   */
  void (*msg_received)(rdmnet_conn_t handle, const RdmnetMessage *message, void *context);
} RdmnetConnCallbacks;

typedef struct RdmnetConnectionConfig
{
  LwpaUuid local_cid;
  RdmnetConnCallbacks callbacks;
  void *callback_context;
} RdmnetConnectionConfig;

/*! If using the externally-managed socket functions (advanced usage), this is the maximum data
 *  length that can be given in one call to rdmnet_conn_sock_data_received(). */
#define RDMNET_RECV_DATA_MAX_SIZE 1200

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_new_connection(const RdmnetConnectionConfig *config, rdmnet_conn_t *handle);
lwpa_error_t rdmnet_connect(rdmnet_conn_t handle, const LwpaSockaddr *remote_addr,
                            const ClientConnectMsg *connect_data);
lwpa_error_t rdmnet_set_blocking(rdmnet_conn_t handle, bool blocking);
lwpa_error_t rdmnet_destroy_connection(rdmnet_conn_t handle, const rdmnet_disconnect_reason_t *disconnect_reason);

int rdmnet_send(rdmnet_conn_t handle, const uint8_t *data, size_t size);
lwpa_error_t rdmnet_start_message(rdmnet_conn_t handle);
int rdmnet_send_partial_message(rdmnet_conn_t handle, const uint8_t *data, size_t size);
lwpa_error_t rdmnet_end_message(rdmnet_conn_t handle);

void rdmnet_conn_tick();

/*! \name Externally managed socket functions.
 *
 *  These functions are for advanced usage and are generally only used by broker apps.
 *  @{
 */
lwpa_error_t rdmnet_attach_existing_socket(rdmnet_conn_t handle, lwpa_socket_t sock, const LwpaSockaddr *remote_addr);
void rdmnet_socket_data_received(rdmnet_conn_t handle, const uint8_t *data, size_t data_size);
void rdmnet_socket_error(rdmnet_conn_t handle, lwpa_error_t socket_err);
/*! @} */

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _RDMNET_CORE_CONNECTION_H_ */
