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

#include "rdmnet/core/connection.h"

#include <stdint.h>
#include "etcpal/common.h"
#include "etcpal/mutex.h"
#include "etcpal/rbtree.h"
#include "etcpal/socket.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/broker_prot.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/util.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if RDMNET_USE_TICK_THREAD
#include "etcpal/thread.h"
#endif

/*************************** Private constants *******************************/

#define RDMNET_CONN_MAX_SOCKETS ETCPAL_SOCKET_MAX_POLL_SIZE

/***************************** Private types ********************************/

typedef enum
{
  kRCConnEventNone,
  kRCConnEventConnected,
  kRCConnEventConnectFailed,
  kRCConnEventDisconnected,
  kRCConnEventMsgReceived
} rc_conn_event_t;

typedef struct RCConnEvent
{
  rc_conn_event_t which;

  union
  {
    RCConnectedInfo     connected;
    RCConnectFailedInfo connect_failed;
    RCDisconnectedInfo  disconnected;
    RdmnetMessage*      message;
  } arg;
} RCConnEvent;

#define RC_CONN_EVENT_INIT \
  {                        \
    kRCConnEventNone       \
  }

/***************************** Private macros ********************************/

#define RC_CONN_LOCK(conn_ptr) (RDMNET_ASSERT_VERIFY(conn_ptr) && etcpal_mutex_lock((conn_ptr)->lock))
#define RC_CONN_UNLOCK(conn_ptr)          \
  if (RDMNET_ASSERT_VERIFY(conn_ptr))     \
  {                                       \
    etcpal_mutex_unlock((conn_ptr)->lock) \
  }

/**************************** Private variables ******************************/

RC_DECLARE_REF_LISTS(connections, RDMNET_MAX_CONNECTIONS);

/*********************** Private function prototypes *************************/

// Periodic state processing
static void process_connection_state(RCConnection* conn, const void* context);

// Connection state machine
static uint32_t update_backoff(uint32_t previous_backoff);
static void     start_tcp_connection(RCConnection* conn, RCConnEvent* event);
static void     start_rdmnet_connection(RCConnection* conn);
static void     reset_connection(RCConnection* conn);
static void     retry_connection(RCConnection* conn);
static void     cleanup_connection_resources(RCConnection* conn);

static void destroy_connection(RCConnection* conn, const void* context);

// Incoming message handling
static void                socket_activity_callback(const EtcPalPollEvent* event, RCPolledSocketOpaqueData data);
static void                receive_and_process_messages(RCConnection* conn);
static rc_message_action_t process_message(RCConnection* conn);
static void                handle_tcp_connection_established(RCConnection* conn);
static void                handle_socket_error(RCConnection* conn, etcpal_error_t socket_err);
static void                handle_message(RCConnection* conn, RdmnetMessage* msg, RCConnEvent* event);
static void                handle_rdmnet_message(RCConnection* conn, RdmnetMessage* msg, RCConnEvent* event);
static void                handle_rdmnet_connect_result(RCConnection* conn, RdmnetMessage* msg, RCConnEvent* event);
static void                deliver_event_callback(RCConnection* conn, RCConnEvent* event, rc_message_action_t* action);

/*************************** Function definitions ****************************/

/*
 * Initialize the RDMnet Core Connection module. Do all necessary initialization before other
 * RDMnet Core Connection API functions can be called. This function is called from rdmnet_init().
 */
etcpal_error_t rc_conn_module_init(void)
{
  if (!rc_ref_lists_init(&connections))
    return kEtcPalErrNoMem;
  return kEtcPalErrOk;
}

/*
 * Deinitialize the RDMnet Core Connection module, setting it back to an uninitialized state. All
 * existing connections will be closed/disconnected. This function is called from rdmnet_deinit()
 * after any threads that call rc_conn_module_tick() are joined.
 */
void rc_conn_module_deinit()
{
  rc_ref_lists_remove_all(&connections, (RCRefFunction)destroy_connection, NULL);
  rc_ref_lists_cleanup(&connections);
}

/*
 * Initialize and add an RCConnection structure to the list to be processed as RDMnet connections.
 * The RDMnet connection process will not be started until rc_conn_connect() is called.
 */
etcpal_error_t rc_conn_register(RCConnection* conn)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return kEtcPalErrSys;

  if (!rc_initialized())
    return kEtcPalErrNotInit;

  if (!rc_ref_list_add_ref(&connections.pending, conn))
    return kEtcPalErrNoMem;

  conn->sock = ETCPAL_SOCKET_INVALID;
  ETCPAL_IP_SET_INVALID(&conn->remote_addr.ip);
  conn->remote_addr.port = 0;
  conn->poll_info.callback = socket_activity_callback;
  conn->poll_info.data.ptr = conn;

  conn->state = kRCConnStateNotStarted;
  etcpal_timer_start(&conn->backoff_timer, 0);
  conn->rdmnet_conn_failed = false;
  conn->sent_connected_notification = false;

  rc_msg_buf_init(&conn->recv_buf);
  conn->retry_current_message = false;

  return kEtcPalErrOk;
}

/*
 * Remove an RCConnection structure from internal processing by this module. If the connection is
 * currently healthy, an RDMnet-level disconnect message will be sent with the reason given.
 */
void rc_conn_unregister(RCConnection* conn, const rdmnet_disconnect_reason_t* disconnect_reason)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  if (conn->state == kRCConnStateHeartbeat && disconnect_reason)
  {
    BrokerDisconnectMsg dm;
    dm.disconnect_reason = *disconnect_reason;
    rc_broker_send_disconnect(conn, &dm);
  }
  conn->state = kRCConnStateMarkedForDestruction;
  rc_ref_list_add_ref(&connections.to_remove, conn);
}

/*
 * Connect to an RDMnet Broker. Starts the connection process from the background thread. Handles
 * redirections automatically. On failure, calling this function again on the same connection will
 * wait for the backoff time required by the standard before reconnecting.
 *
 * Use connect_data to supply the information about this client that will be sent to the broker as
 * part of the connection handshake.
 */
etcpal_error_t rc_conn_connect(RCConnection*                 conn,
                               const EtcPalSockAddr*         remote_addr,
                               const BrokerClientConnectMsg* connect_data)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(remote_addr) || !RDMNET_ASSERT_VERIFY(connect_data))
    return kEtcPalErrSys;

  if (conn->state != kRCConnStateNotStarted && conn->state != kRCConnStateDisconnectPending)
    return kEtcPalErrIsConn;

  // Set the data - the connect will be initiated from the background thread.
  conn->remote_addr = *remote_addr;
  conn->conn_data = *connect_data;
  if (conn->state == kRCConnStateNotStarted)
    conn->state = kRCConnStateConnectPending;
  else
    conn->state = kRCConnStateReconnectPending;

  return kEtcPalErrOk;
}

etcpal_error_t rc_conn_reconnect(RCConnection*                 conn,
                                 const EtcPalSockAddr*         new_remote_addr,
                                 const BrokerClientConnectMsg* new_connect_data,
                                 rdmnet_disconnect_reason_t    disconnect_reason)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(new_remote_addr) ||
      !RDMNET_ASSERT_VERIFY(new_connect_data) || !RDMNET_ASSERT_VERIFY(conn->state != kRCConnStateNotStarted) ||
      !RDMNET_ASSERT_VERIFY(conn->state != kRCConnStateMarkedForDestruction) ||
      !RDMNET_ASSERT_VERIFY(conn->state != kRCConnStateDisconnectPending))
  {
    return kEtcPalErrSys;
  }

  if (conn->state == kRCConnStateHeartbeat)
  {
    BrokerDisconnectMsg dm;
    dm.disconnect_reason = disconnect_reason;
    rc_broker_send_disconnect(conn, &dm);
  }
  conn->remote_addr = *new_remote_addr;
  conn->conn_data = *new_connect_data;
  if (conn->state == kRCConnStateBackoff)
    conn->state = kRCConnStateConnectPending;
  else
    conn->state = kRCConnStateReconnectPending;

  return kEtcPalErrOk;
}

etcpal_error_t rc_conn_disconnect(RCConnection* conn, rdmnet_disconnect_reason_t disconnect_reason)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(conn->state != kRCConnStateNotStarted) ||
      !RDMNET_ASSERT_VERIFY(conn->state != kRCConnStateMarkedForDestruction))
  {
    return kEtcPalErrSys;
  }

  if (conn->state == kRCConnStateHeartbeat)
  {
    BrokerDisconnectMsg dm;
    dm.disconnect_reason = disconnect_reason;
    rc_broker_send_disconnect(conn, &dm);
  }
  if (conn->state == kRCConnStateConnectPending || conn->state == kRCConnStateBackoff)
    conn->state = kRCConnStateNotStarted;
  else
    conn->state = kRCConnStateDisconnectPending;
  return kEtcPalErrOk;
}

/*
 * Handle periodic RDMnet connection functionality.
 */
void rc_conn_module_tick()
{
  if (rdmnet_writelock())
  {
    rc_ref_lists_remove_marked(&connections, (RCRefFunction)destroy_connection, NULL);
    rc_ref_lists_add_pending(&connections);
    rdmnet_writeunlock();
  }

  rc_ref_list_for_each(&connections.active, (RCRefFunction)process_connection_state, NULL);
}

static void start_connection(RCConnection* conn, RCConnEvent* event)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(event))
    return;

  if (conn->rdmnet_conn_failed || conn->backoff_timer.interval != 0)
  {
    if (conn->rdmnet_conn_failed)
      etcpal_timer_start(&conn->backoff_timer, update_backoff(conn->backoff_timer.interval));
    conn->state = kRCConnStateBackoff;
  }
  else
  {
    start_tcp_connection(conn, event);
  }
}

void process_connection_state(RCConnection* conn, const void* context)
{
  ETCPAL_UNUSED_ARG(context);

  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  // Some messages need to be retried on the next tick, which happens here.
  receive_and_process_messages(conn);

  if (RC_CONN_LOCK(conn))
  {
    RCConnEvent event = RC_CONN_EVENT_INIT;

    switch (conn->state)
    {
      case kRCConnStateConnectPending:
        start_connection(conn, &event);
        break;
      case kRCConnStateBackoff:
        if (etcpal_timer_is_expired(&conn->backoff_timer))
        {
          start_tcp_connection(conn, &event);
        }
        break;
      case kRCConnStateRDMnetConnPending:
        if (etcpal_timer_is_expired(&conn->hb_timer))
        {
          event.which = kRCConnEventConnectFailed;
          event.arg.connect_failed.event = kRdmnetConnectFailNoReply;
          reset_connection(conn);
        }
        break;
      case kRCConnStateHeartbeat:
        if (etcpal_timer_is_expired(&conn->hb_timer))
        {
          // Heartbeat timeout! Disconnect the connection.
          event.which = kRCConnEventDisconnected;
          event.arg.disconnected.event = kRdmnetDisconnectNoHeartbeat;
          event.arg.disconnected.socket_err = kEtcPalErrOk;
          reset_connection(conn);
        }
        else if (etcpal_timer_is_expired(&conn->send_timer))
        {
          rc_broker_send_null(conn);
          etcpal_timer_reset(&conn->send_timer);
        }
        break;
      case kRCConnStateReconnectPending:
        cleanup_connection_resources(conn);
        rc_msg_buf_init(&conn->recv_buf);
        conn->retry_current_message = false;
        if (conn->sent_connected_notification)
        {
          event.which = kRCConnEventDisconnected;
          event.arg.disconnected.event = kRdmnetDisconnectGracefulLocalInitiated;
          conn->sent_connected_notification = false;
        }
        start_connection(conn, &event);
        break;
      case kRCConnStateDisconnectPending:
        if (conn->sent_connected_notification)
        {
          event.which = kRCConnEventDisconnected;
          event.arg.disconnected.event = kRdmnetDisconnectGracefulLocalInitiated;
          conn->sent_connected_notification = false;
        }
        reset_connection(conn);
        break;
      default:
        break;
    }

    RC_CONN_UNLOCK(conn);

    rc_message_action_t action = kRCMessageActionProcessNext;
    deliver_event_callback(conn, &event, &action);
  }
}

// Update a backoff timer value using the algorithm specified in E1.33. Returns the new value.
uint32_t update_backoff(uint32_t previous_backoff)
{
  uint32_t result = (uint32_t)(((rand() % 4001) + 1000));
  result += previous_backoff;
  // 30 second interval is the max
  if (result > 30000u)
    return 30000u;
  return result;
}

void start_tcp_connection(RCConnection* conn, RCConnEvent* event)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(event))
    return;

  bool ok = true;

  etcpal_error_t res = etcpal_socket(ETCPAL_IP_IS_V6(&conn->remote_addr.ip) ? ETCPAL_AF_INET6 : ETCPAL_AF_INET,
                                     ETCPAL_SOCK_STREAM, &conn->sock);
  if (res != kEtcPalErrOk)
  {
    ok = false;
    event->which = kRCConnEventConnectFailed;
    event->arg.connect_failed.event = kRdmnetConnectFailSocketFailure;
    event->arg.connect_failed.socket_err = res;
  }

  if (ok)
  {
    res = etcpal_setblocking(conn->sock, false);
    if (res != kEtcPalErrOk)
    {
      ok = false;
      event->which = kRCConnEventConnectFailed;
      event->arg.connect_failed.event = kRdmnetConnectFailSocketFailure;
      event->arg.connect_failed.socket_err = res;
    }
  }

  if (ok)
  {
    conn->rdmnet_conn_failed = false;
    res = etcpal_connect(conn->sock, &conn->remote_addr);
    if (res == kEtcPalErrOk)
    {
      // Fast connect condition
      start_rdmnet_connection(conn);
    }
    else if (res == kEtcPalErrInProgress || res == kEtcPalErrWouldBlock)
    {
      conn->state = kRCConnStateTCPConnPending;
      etcpal_error_t add_res = rc_add_polled_socket(conn->sock, ETCPAL_POLL_CONNECT, &conn->poll_info);
      if (add_res != kEtcPalErrOk)
      {
        ok = false;
        event->which = kRCConnEventConnectFailed;
        event->arg.connect_failed.event = kRdmnetConnectFailSocketFailure;
        event->arg.connect_failed.socket_err = add_res;
      }
    }
    else
    {
      ok = false;
      event->which = kRCConnEventConnectFailed;

      // EHOSTUNREACH is sometimes reported synchronously even for a non-blocking connect.
      if (res == kEtcPalErrHostUnreach)
        event->arg.connect_failed.event = kRdmnetConnectFailTcpLevel;
      else
        event->arg.connect_failed.event = kRdmnetConnectFailSocketFailure;
      event->arg.connect_failed.socket_err = res;
    }
  }

  if (!ok)
    reset_connection(conn);
}

void start_rdmnet_connection(RCConnection* conn)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  // Update state
  conn->state = kRCConnStateRDMnetConnPending;
  rc_modify_polled_socket(conn->sock, ETCPAL_POLL_IN, &conn->poll_info);
  rc_broker_send_client_connect(conn, &conn->conn_data);
  etcpal_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
  etcpal_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
}

void reset_connection(RCConnection* conn)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  cleanup_connection_resources(conn);
  rc_msg_buf_init(&conn->recv_buf);
  conn->retry_current_message = false;
  conn->state = kRCConnStateNotStarted;
}

void retry_connection(RCConnection* conn)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  cleanup_connection_resources(conn);
  rc_msg_buf_init(&conn->recv_buf);
  conn->retry_current_message = false;
  conn->state = kRCConnStateConnectPending;
}

void destroy_connection(RCConnection* conn, const void* context)
{
  ETCPAL_UNUSED_ARG(context);

  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  cleanup_connection_resources(conn);
  if (conn->callbacks.destroyed)
    conn->callbacks.destroyed(conn);
}

void cleanup_connection_resources(RCConnection* conn)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  if (conn->sock != ETCPAL_SOCKET_INVALID)
  {
    rc_remove_polled_socket(conn->sock);
    etcpal_close(conn->sock);
    conn->sock = ETCPAL_SOCKET_INVALID;
  }

  if (conn->retry_current_message)
  {
    // In order to be available for a retry later, the current message's resources haven't been freed yet. The retry
    // won't happen now, so free the resources.
    rc_free_message_resources(&conn->recv_buf.msg);
  }
}

void socket_activity_callback(const EtcPalPollEvent* event, RCPolledSocketOpaqueData data)
{
  if (!RDMNET_ASSERT_VERIFY(event))
    return;

  RCConnection* conn = (RCConnection*)data.ptr;
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  if (event->events & ETCPAL_POLL_ERR)
    handle_socket_error(conn, event->err);
  else if (event->events & ETCPAL_POLL_IN)
    receive_and_process_messages(conn);
  else if (event->events & ETCPAL_POLL_CONNECT)
    handle_tcp_connection_established(conn);
}

void receive_and_process_messages(RCConnection* conn)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  etcpal_error_t      recv_res = kEtcPalErrOk;
  rc_message_action_t message_action = kRCMessageActionProcessNext;
  bool                retry_current_message = conn->retry_current_message;

  // This loop alternates between "receive as much data as possible" and "parse and process as much of the received data
  // as possible". This is done in a loop in case the TCP queue has more data than can fit in our buffer in one receive.
  do
  {
    // No matter what, we should continue to fill our buffer if there's data to receive and there's room in the buffer.
    recv_res = rc_msg_buf_recv(&conn->recv_buf, conn->sock);

    if ((recv_res == kEtcPalErrOk) || retry_current_message)
    {
      // This next loop parses and processes as many messages from the buffer as possible, until all complete messages
      // have been processed or we get RetryLater.
      etcpal_error_t parse_res = kEtcPalErrOk;
      do
      {
        // If we should retry the current message, don't parse because that would overwrite the message we're retrying.
        if (retry_current_message)
          retry_current_message = false;
        else
          parse_res = rc_msg_buf_parse_data(&conn->recv_buf);

        if (parse_res == kEtcPalErrOk)
          message_action = process_message(conn);
      } while ((parse_res == kEtcPalErrOk) && (message_action == kRCMessageActionProcessNext));
    }
    else if (recv_res != kEtcPalErrWouldBlock)
    {
      handle_socket_error(conn, recv_res);
    }
  } while ((recv_res == kEtcPalErrOk) && (message_action == kRCMessageActionProcessNext));

  conn->retry_current_message = (message_action == kRCMessageActionRetryLater);
}

rc_message_action_t process_message(RCConnection* conn)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return kRCMessageActionProcessNext;

  rc_message_action_t action = kRCMessageActionProcessNext;

  RCConnEvent event = RC_CONN_EVENT_INIT;
  if (RC_CONN_LOCK(conn))
  {
    if ((conn->state == kRCConnStateHeartbeat) || (conn->state == kRCConnStateRDMnetConnPending))
      handle_message(conn, &conn->recv_buf.msg, &event);

    RC_CONN_UNLOCK(conn);
  }

  deliver_event_callback(conn, &event, &action);

  return action;
}

void handle_tcp_connection_established(RCConnection* conn)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  if (RC_CONN_LOCK(conn))
  {
    // connected successfully!
    start_rdmnet_connection(conn);
    RC_CONN_UNLOCK(conn);
  }
}

void handle_socket_error(RCConnection* conn, etcpal_error_t socket_err)
{
  if (!RDMNET_ASSERT_VERIFY(conn))
    return;

  if (RC_CONN_LOCK(conn))
  {
    RCConnEvent event = RC_CONN_EVENT_INIT;

    if (conn->state == kRCConnStateTCPConnPending || conn->state == kRCConnStateRDMnetConnPending)
    {
      event.which = kRCConnEventConnectFailed;
      event.arg.connect_failed.event = kRdmnetConnectFailTcpLevel;
      event.arg.connect_failed.socket_err = socket_err;
      if (conn->state == kRCConnStateRDMnetConnPending)
        conn->rdmnet_conn_failed = true;

      reset_connection(conn);
    }
    else if (conn->state == kRCConnStateHeartbeat)
    {
      event.which = kRCConnEventDisconnected;
      event.arg.disconnected.event = kRdmnetDisconnectAbruptClose;
      event.arg.disconnected.socket_err = socket_err;

      reset_connection(conn);
    }
    RC_CONN_UNLOCK(conn);

    rc_message_action_t action = kRCMessageActionProcessNext;
    deliver_event_callback(conn, &event, &action);
  }
}

void handle_message(RCConnection* conn, RdmnetMessage* msg, RCConnEvent* event)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(msg) || !RDMNET_ASSERT_VERIFY(event))
    return;

  if (conn->state == kRCConnStateRDMnetConnPending)
    handle_rdmnet_connect_result(conn, msg, event);
  else
    handle_rdmnet_message(conn, msg, event);
}

void handle_rdmnet_message(RCConnection* conn, RdmnetMessage* msg, RCConnEvent* event)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(msg) || !RDMNET_ASSERT_VERIFY(event))
    return;

  // Reset the heartbeat timer every time we receive any message.
  etcpal_timer_reset(&conn->hb_timer);

  // We handle some Broker messages internally
  bool deliver_message = false;
  if (RDMNET_IS_BROKER_MSG(msg))
  {
    BrokerMessage* bmsg = RDMNET_GET_BROKER_MSG(msg);
    if (!RDMNET_ASSERT_VERIFY(bmsg))
      return;

    switch (bmsg->vector)
    {
      case VECTOR_BROKER_CONNECT_REPLY:
      case VECTOR_BROKER_NULL:
        break;
      case VECTOR_BROKER_DISCONNECT:
        event->which = kRCConnEventDisconnected;
        event->arg.disconnected.event = kRdmnetDisconnectGracefulRemoteInitiated;
        event->arg.disconnected.socket_err = kEtcPalErrOk;

        const BrokerDisconnectMsg* disconnect_msg = BROKER_GET_DISCONNECT_MSG(bmsg);
        if (!RDMNET_ASSERT_VERIFY(disconnect_msg))
          return;

        event->arg.disconnected.rdmnet_reason = disconnect_msg->disconnect_reason;

        reset_connection(conn);
        break;
      default:
        deliver_message = true;
        break;
    }
  }
  else
  {
    deliver_message = true;
  }

  if (deliver_message)
  {
    event->which = kRCConnEventMsgReceived;
    event->arg.message = msg;
  }
  else
  {
    rc_free_message_resources(msg);
  }
}

void handle_rdmnet_connect_result(RCConnection* conn, RdmnetMessage* msg, RCConnEvent* event)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(msg) || !RDMNET_ASSERT_VERIFY(event))
    return;

  BrokerMessage* bmsg = RDMNET_GET_BROKER_MSG(msg);
  if (bmsg)
  {
    if (BROKER_IS_CONNECT_REPLY_MSG(bmsg))
    {
      BrokerConnectReplyMsg* reply = BROKER_GET_CONNECT_REPLY_MSG(bmsg);
      if (!RDMNET_ASSERT_VERIFY(reply))
        return;

      switch (reply->connect_status)
      {
        case kRdmnetConnectOk:
          // TODO check version
          conn->state = kRCConnStateHeartbeat;
          conn->sent_connected_notification = true;
          etcpal_timer_start(&conn->backoff_timer, 0);
          event->which = kRCConnEventConnected;

          RCConnectedInfo* connect_info = &event->arg.connected;
          connect_info->broker_cid = msg->sender_cid;
          connect_info->broker_uid = reply->broker_uid;
          connect_info->client_uid = reply->client_uid;
          connect_info->connected_addr = conn->remote_addr;
          break;
        default: {
          event->which = kRCConnEventConnectFailed;

          RCConnectFailedInfo* failed_info = &event->arg.connect_failed;
          failed_info->event = kRdmnetConnectFailRejected;
          failed_info->socket_err = kEtcPalErrOk;
          failed_info->rdmnet_reason = reply->connect_status;

          reset_connection(conn);
          conn->rdmnet_conn_failed = true;
          break;
        }
      }
    }
    else if (BROKER_IS_CLIENT_REDIRECT_MSG(bmsg))
    {
      const BrokerClientRedirectMsg* client_redirect_msg = BROKER_GET_CLIENT_REDIRECT_MSG(bmsg);
      if (!RDMNET_ASSERT_VERIFY(client_redirect_msg))
        return;

      conn->remote_addr = client_redirect_msg->new_addr;
      retry_connection(conn);
    }
  }
  rc_free_message_resources(msg);
}

void deliver_event_callback(RCConnection* conn, RCConnEvent* event, rc_message_action_t* action)
{
  if (!RDMNET_ASSERT_VERIFY(conn) || !RDMNET_ASSERT_VERIFY(event) || !RDMNET_ASSERT_VERIFY(action))
    return;

  switch (event->which)
  {
    case kRCConnEventConnected:
      if (conn->callbacks.connected)
        conn->callbacks.connected(conn, &event->arg.connected);
      break;
    case kRCConnEventConnectFailed:
      if (conn->callbacks.connect_failed)
        conn->callbacks.connect_failed(conn, &event->arg.connect_failed);
      break;
    case kRCConnEventDisconnected:
      if (conn->callbacks.disconnected)
        conn->callbacks.disconnected(conn, &event->arg.disconnected);
      break;
    case kRCConnEventMsgReceived:
      if (conn->callbacks.message_received)
        *action = conn->callbacks.message_received(conn, event->arg.message);
      if (*action == kRCMessageActionProcessNext)
        rc_free_message_resources(event->arg.message);
      break;
    case kRCConnEventNone:
    default:
      break;
  }
}
