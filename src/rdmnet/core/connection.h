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

/*
 * rdmnet/core/connection.h: Handle a connection between a Client and a Broker in RDMnet.
 *
 * In E1.33, the behavior of this module is dictated by the Broker Protocol (&sect; 6).
 *
 * Add a connection using rc_connection_register(). Start a connection to a broker using
 * rc_connection_connect(). The status of the connection will be communicated via the callbacks.
 * Send data over the broker connection using rc_connection_send(). Data received over the broker
 * connection will be forwarded via the RCMessageReceivedCallback.
 *
 * All connection functions in this module should only be called after having the connection lock.
 */

#ifndef RDMNET_CORE_CONNECTION_H_
#define RDMNET_CORE_CONNECTION_H_

#include <stdbool.h>
#include <stddef.h>
#include "etcpal/log.h"
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/mutex.h"
#include "etcpal/timer.h"
#include "etcpal/socket.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/msg_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RCConnection RCConnection;

// Information about a successful RDMnet connection.
typedef struct RCConnectedInfo
{
  EtcPalUuid broker_cid;  // The broker's CID.
  RdmUid     broker_uid;  // The broker's UID.
  RdmUid     client_uid;  // The client's UID (relevant if assigned dynamically)

  // The remote address to which we are connected. This could be different from the original
  // address requested in the case of a redirect.
  EtcPalSockAddr connected_addr;
} RCConnectedInfo;

// Information about an unsuccessful RDMnet connection.
typedef struct RCConnectFailedInfo
{
  // The high-level reason that this connection failed.
  rdmnet_connect_fail_event_t event;
  // The system error code associated with the failure; valid if event is
  // kRdmnetConnectFailSocketFailure or kRdmnetConnectFailTcpLevel.
  etcpal_error_t socket_err;
  // The reason given in the RDMnet-level connection refuse message. Valid if event is
  // kRdmnetConnectFailRejected.
  rdmnet_connect_status_t rdmnet_reason;
} RCConnectFailedInfo;

// Information about an RDMnet connection that disconnected after a successful connection.
typedef struct RCDisconnectedInfo
{
  // The high-level reason for the disconnect.
  rdmnet_disconnect_event_t event;
  // The system error code associated with the disconnect; valid if event is
  // kRdmnetDisconnectAbruptClose.
  etcpal_error_t socket_err;
  // The reason given in the RDMnet-level disconnect message. Valid if event is
  // kRdmnetDisconnectGracefulRemoteInitiated.
  rdmnet_disconnect_reason_t rdmnet_reason;
} RCDisconnectedInfo;

// When an RDMnet message is received, this determines whether to retry processing this message later (which will narrow
// the TCP window, potentially throttling the connection), or move on to the next message.
typedef enum
{
  kRCMessageActionRetryLater,
  kRCMessageActionProcessNext
} rc_message_action_t;

// An RDMnet connection has connected successfully. connect_info contains more information about
// the successful connection.
typedef void (*RCConnConnectedCallback)(RCConnection* conn, const RCConnectedInfo* connect_info);

// An RDMnet connection attempt failed. failed_info contains more information about the connect
// failure event.
typedef void (*RCConnConnectFailedCallback)(RCConnection* conn, const RCConnectFailedInfo* failed_info);

// A previously-connected RDMnet connection has disconnected. disconn_info contains more
// information about the disconnect event.
typedef void (*RCConnDisconnectedCallback)(RCConnection* conn, const RCDisconnectedInfo* disconn_info);

// A message has been received on an RDMnet connection. Broker Protocol messages that affect
// connection status are consumed internally by the connection module and thus will not result in
// this callback. All other valid messages will be delivered.
//
// Use the macros in rdmnet/core/message.h to decode the message type.
//
// Return kRCMessageActionRetryLater if resources are not available to process the message at this time. Otherwise, if
// the message has been processed or should be dropped, return kRCMessageActionProcessNext.
typedef rc_message_action_t (*RCConnMessageReceivedCallback)(RCConnection* conn, const RdmnetMessage* message);

// An RDMnet connection has been destroyed and unregistered. This is called from the background
// thread, after the resources associated with the connection (e.g. sockets) have been cleaned up.
// It is safe to deallocate the connection from this callback.
typedef void (*RCConnDestroyedCallback)(RCConnection* conn);

// The set of callbacks which are called with notifications about RDMnet connections.
typedef struct RCConnectionCallbacks
{
  RCConnConnectedCallback       connected;
  RCConnConnectFailedCallback   connect_failed;
  RCConnDisconnectedCallback    disconnected;
  RCConnMessageReceivedCallback message_received;
  RCConnDestroyedCallback       destroyed;
} RCConnectionCallbacks;

// The connection state machine.
typedef enum
{
  kRCConnStateNotStarted,
  kRCConnStateConnectPending,
  kRCConnStateBackoff,
  kRCConnStateTCPConnPending,
  kRCConnStateRDMnetConnPending,
  kRCConnStateHeartbeat,
  kRCConnStateDisconnectPending,
  kRCConnStateReconnectPending,
  kRCConnStateMarkedForDestruction
} rc_client_conn_state_t;

// A structure representing the state of an RDMnet connection. Register it with the connection
// module using rc_connection_register(), after filling out the identifying information at the top
// (the rest of the members are initialized by rc_connection_register()).
struct RCConnection
{
  /////////////////////////////////////////////////////////////////////////////
  // Fill this in before initialization.

  EtcPalUuid            local_cid;
  etcpal_mutex_t*       lock;
  RCConnectionCallbacks callbacks;

  /////////////////////////////////////////////////////////////////////////////

  etcpal_socket_t    sock;
  EtcPalSockAddr     remote_addr;
  RCPolledSocketInfo poll_info;

  rc_client_conn_state_t state;
  BrokerClientConnectMsg conn_data;
  EtcPalTimer            backoff_timer;
  bool                   rdmnet_conn_failed;
  bool                   sent_connected_notification;
  EtcPalTimer            send_timer;
  EtcPalTimer            hb_timer;

  // Send and receive tracking
  RCMsgBuf recv_buf;
  bool     retry_current_message;  // recv_buf.msg couldn't be processed - retry processing it at a later time.
};

etcpal_error_t rc_conn_module_init(const RdmnetNetintConfig* netint_config);
void           rc_conn_module_deinit(void);
void           rc_conn_module_tick(void);

etcpal_error_t rc_conn_register(RCConnection* conn);
void           rc_conn_unregister(RCConnection* conn, const rdmnet_disconnect_reason_t* disconnect_reason);
etcpal_error_t rc_conn_connect(RCConnection*                 conn,
                               const EtcPalSockAddr*         remote_addr,
                               const BrokerClientConnectMsg* connect_data);
etcpal_error_t rc_conn_reconnect(RCConnection*                 conn,
                                 const EtcPalSockAddr*         new_remote_addr,
                                 const BrokerClientConnectMsg* new_connect_data,
                                 rdmnet_disconnect_reason_t    disconnect_reason);
etcpal_error_t rc_conn_disconnect(RCConnection* conn, rdmnet_disconnect_reason_t disconnect_reason);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_CONNECTION_H_ */
