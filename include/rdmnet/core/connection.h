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
#include "etcpal/bool.h"
#include "etcpal/log.h"
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/socket.h"
#include "rdmnet/core.h"
#include "rdmnet/core/message.h"

/*! \defgroup rdmnet_conn Connection
 *  \ingroup rdmnet_core_lib
 *  \brief Handle a connection between a Client and a %Broker in RDMnet.
 *
 *  In E1.33, the behavior of this module is dictated by the %Broker Protocol (&sect; 6).
 *
 *  Basic functionality for an RDMnet Client: Initialize the library using rdmnet_init(). Create a
 *  new connection using rdmnet_connection_create(). Connect to a %Broker using rdmnet_connect().
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
  EtcPalSockaddr connected_addr;
} RdmnetConnectedInfo;

/*! A high-level reason for RDMnet connection failure. */
typedef enum
{
  /*! The connection was unable to be started because of an error returned from the system during
   *  a lower-level socket call. */
  kRdmnetConnectFailSocketFailure,
  /*! The connection started but the TCP connection was never established. This could be because of
   *  an incorrect address or port for the remote host or a network issue. */
  kRdmnetConnectFailTcpLevel,
  /*! The TCP connection was established, but no reply was received from the RDMnet protocol
   *  handshake. This probably indicates an error in the remote broker. */
  kRdmnetConnectFailNoReply,
  /*! The remote broker rejected the connection at the RDMnet protocol level. A reason is provided
   *  in the form of an rdmnet_connect_status_t. */
  kRdmnetConnectFailRejected
} rdmnet_connect_fail_event_t;

/*! Information about an unsuccessful RDMnet connection. */
typedef struct RdmnetConnectFailedInfo
{
  /*! The high-level reason that this connection failed. */
  rdmnet_connect_fail_event_t event;
  /*! The system error code associated with the failure; valid if event is
   *  kRdmnetConnectFailSocketFailure or kRdmnetConnectFailTcpLevel. */
  etcpal_error_t socket_err;
  /*! The reason given in the RDMnet-level connection refuse message. Valid if event is
   *  kRdmnetConnectFailRejected. */
  rdmnet_connect_status_t rdmnet_reason;
} RdmnetConnectFailedInfo;

/*! A high-level reason for RDMnet connection to be disconnected after successful connection. */
typedef enum
{
  kRdmnetDisconnectAbruptClose,
  kRdmnetDisconnectNoHeartbeat,
  kRdmnetDisconnectRedirected,
  kRdmnetDisconnectGracefulRemoteInitiated,
  kRdmnetDisconnectGracefulLocalInitiated
} rdmnet_disconnect_event_t;

/*! Information about an RDMnet connection that disconnected after a successful connection. */
typedef struct RdmnetDisconnectedInfo
{
  /*! The high-level reason for the disconnect. */
  rdmnet_disconnect_event_t event;
  /*! The system error code associated with the disconnect; valid if event is
   *  kRdmnetDisconnectAbruptClose. */
  etcpal_error_t socket_err;
  /*! The reason given in the RDMnet-level disconnect message. Valid if event is
   *  kRdmnetDisconnectGracefulRemoteInitiated. */
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
  void (*connected)(rdmnet_conn_t handle, const RdmnetConnectedInfo* connect_info, void* context);

  /*! \brief An RDMnet connection attempt failed.
   *
   *  \param[in] handle Handle to connection which has failed.
   *  \param[in] failed_info More information about the connect failure event.
   *  \param[in] context Context pointer that was given at the creation of the connection.
   */
  void (*connect_failed)(rdmnet_conn_t handle, const RdmnetConnectFailedInfo* failed_info, void* context);

  /*! \brief A previously-connected RDMnet connection has disconnected.
   *
   *  \param[in] handle Handle to connection which has been disconnected.
   *  \param[in] disconn_info More information about the disconnect event.
   *  \param[in] context Context pointer that was given at the creation of the connection.
   */
  void (*disconnected)(rdmnet_conn_t handle, const RdmnetDisconnectedInfo* disconn_info, void* context);

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
  void (*msg_received)(rdmnet_conn_t handle, const RdmnetMessage* message, void* context);
} RdmnetConnCallbacks;

/*! A set of configuration information for a new RDMnet connection. */
typedef struct RdmnetConnectionConfig
{
  /*! The CID of the local component that will be using this connection. */
  EtcPalUuid local_cid;
  /*! A set of callbacks to receive asynchronous notifications of connection events. */
  RdmnetConnCallbacks callbacks;
  /*! Pointer to opaque data passed back with each callback. */
  void* callback_context;
} RdmnetConnectionConfig;

/*! If using the externally-managed socket functions (advanced usage), this is the maximum data
 *  length that can be given in one call to rdmnet_conn_sock_data_received(). */
#define RDMNET_RECV_DATA_MAX_SIZE 1200

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t rdmnet_connection_create(const RdmnetConnectionConfig* config, rdmnet_conn_t* handle);
etcpal_error_t rdmnet_connect(rdmnet_conn_t handle, const EtcPalSockaddr* remote_addr,
                              const ClientConnectMsg* connect_data);
etcpal_error_t rdmnet_set_blocking(rdmnet_conn_t handle, bool blocking);
etcpal_error_t rdmnet_connection_destroy(rdmnet_conn_t handle, const rdmnet_disconnect_reason_t* disconnect_reason);

int rdmnet_send(rdmnet_conn_t handle, const uint8_t* data, size_t size);

/*! \name Externally managed socket functions.
 *
 *  These functions are for advanced usage and are generally only used by broker apps.
 *  @{
 */
etcpal_error_t rdmnet_attach_existing_socket(rdmnet_conn_t handle, etcpal_socket_t sock,
                                             const EtcPalSockaddr* remote_addr);
void rdmnet_socket_data_received(rdmnet_conn_t handle, const uint8_t* data, size_t data_size);
void rdmnet_socket_error(rdmnet_conn_t handle, etcpal_error_t socket_err);
/*! @} */

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _RDMNET_CORE_CONNECTION_H_ */
