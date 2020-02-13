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

#include "rdmnet/core/connection.h"

#include <stdint.h>
#include "etcpal/lock.h"
#include "etcpal/socket.h"
#include "etcpal/rbtree.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/message.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/message.h"
#include "rdmnet/private/connection.h"
#include "rdmnet/private/broker_prot.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/util.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif
#if RDMNET_USE_TICK_THREAD
#include "etcpal/thread.h"
#endif

/*************************** Private constants *******************************/

/* When waiting on the backoff timer for a new connection, the interval at which to wake up and make
 * sure that we haven't been deinitted/closed. */
#define BLOCKING_BACKOFF_WAIT_INTERVAL 500
#define RDMNET_CONN_MAX_SOCKETS ETCPAL_SOCKET_MAX_POLL_SIZE

#if RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS
#define CB_STORAGE_CLASS
#else
#define CB_STORAGE_CLASS static
#endif

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_connection() malloc(sizeof(RdmnetConnection))
#define free_rdmnet_connection(ptr) free(ptr)
#else
#define alloc_rdmnet_connection() etcpal_mempool_alloc(rdmnet_connections)
#define free_rdmnet_connection(ptr) etcpal_mempool_free(rdmnet_connections, ptr)
#endif

#define INIT_CALLBACK_INFO(cbptr) ((cbptr)->which = kConnCallbackNone)

/**************************** Private variables ******************************/

// clang-format off
static const char* kRdmnetConnectFailEventStrings[] =
{
  "Socket failure on connection initiation",
  "TCP connection failure",
  "No reply received to RDMnet handshake",
  "RDMnet connection rejected"
};
#define NUM_CONNECT_FAIL_EVENT_STRINGS (sizeof(kRdmnetConnectFailEventStrings) / sizeof(const char*))

static const char* kRdmnetDisconnectEventStrings[] =
{
  "Connection was closed abruptly",
  "No heartbeat message was received within the heartbeat timeout",
  "Connection was redirected to another Broker",
  "Remote component sent a disconnect message",
  "Local component sent a disconnect message"
};
#define NUM_DISCONNECT_EVENT_STRINGS (sizeof(kRdmnetDisconnectEventStrings) / sizeof(const char*))
// clang-format on

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(rdmnet_connections, RdmnetConnection, RDMNET_MAX_CONNECTIONS);
ETCPAL_MEMPOOL_DEFINE(rdmnet_conn_rb_nodes, EtcPalRbNode, RDMNET_MAX_CONNECTIONS);
#endif

static struct RdmnetConnectionState
{
  EtcPalRbTree connections;
  IntHandleManager handle_mgr;
} state;

/*********************** Private function prototypes *************************/

static uint32_t update_backoff(uint32_t previous_backoff);
static void start_tcp_connection(RdmnetConnection* conn, ConnCallbackDispatchInfo* cb);
static void start_rdmnet_connection(RdmnetConnection* conn);
static void reset_connection(RdmnetConnection* conn);
static void retry_connection(RdmnetConnection* conn);

static void destroy_marked_connections();
static void process_all_connection_state(ConnCallbackDispatchInfo* cb);

static void fill_callback_info(const RdmnetConnection* conn, ConnCallbackDispatchInfo* info);
static void deliver_callback(ConnCallbackDispatchInfo* info);

static void rdmnet_conn_socket_activity(const EtcPalPollEvent* event, PolledSocketOpaqueData data);

// Connection management, lookup, destruction
static bool conn_handle_in_use(int handle_val);
static RdmnetConnection* create_new_connection(const RdmnetConnectionConfig* config);
static void destroy_connection(RdmnetConnection* conn, bool remove_from_tree);
static etcpal_error_t get_conn(rdmnet_conn_t handle, RdmnetConnection** conn);
static void release_conn(RdmnetConnection* conn);
static int conn_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static EtcPalRbNode* conn_node_alloc(void);
static void conn_node_free(EtcPalRbNode* node);

/*************************** Function definitions ****************************/

/* Initialize the RDMnet Connection module. Do all necessary initialization before other RDMnet
 * Connection API functions can be called. This private function is called from rdmnet_core_init().
 */
etcpal_error_t rdmnet_conn_init()
{
  etcpal_error_t res = kEtcPalErrOk;

#if !RDMNET_DYNAMIC_MEM
  /* Init memory pools */
  res |= etcpal_mempool_init(rdmnet_connections);
  res |= etcpal_mempool_init(rdmnet_conn_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&state.connections, conn_compare, conn_node_alloc, conn_node_free);
    init_int_handle_manager(&state.handle_mgr, conn_handle_in_use);
  }

  return res;
}

static void conn_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  RDMNET_UNUSED_ARG(self);

  RdmnetConnection* conn = (RdmnetConnection*)node->value;
  if (conn)
    destroy_connection(conn, false);
  conn_node_free(node);
}

/* Deinitialize the RDMnet Connection module, setting it back to an uninitialized state. All
 * existing connections will be closed/disconnected. Calls to other RDMnet Connection API
 * functions will fail until rdmnet_init() is called again. This private function is called from
 * rdmnet_core_deinit().
 */
void rdmnet_conn_deinit()
{
  etcpal_rbtree_clear_with_cb(&state.connections, conn_dealloc);
  memset(&state, 0, sizeof state);
}

/*! \brief Create a new handle to use for an RDMnet Connection.
 *
 *  This function simply allocates a connection handle - use rdmnet_connect() to actually start the
 *  connection process.
 *
 *  \param[in] config Configuration parameters for the connection to be created.
 *  \param[out] handle Handle to the newly-created connection
 *  \return #kEtcPalErrOk: Handle created successfully.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrNoMem: No room to allocate additional connection.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_connection_create(const RdmnetConnectionConfig* config, rdmnet_conn_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = kEtcPalErrSys;
  if (rdmnet_writelock())
  {
    res = kEtcPalErrOk;
    // Passed the quick checks, try to create a struct to represent a new connection. This function
    // creates the new connection, gives it a unique handle and inserts it into the connection map.
    RdmnetConnection* conn = create_new_connection(config);
    if (!conn)
      res = kEtcPalErrNoMem;

    if (res == kEtcPalErrOk)
    {
      *handle = conn->handle;
    }
    rdmnet_writeunlock();
  }
  return res;
}

/*! \brief Connect to an RDMnet %Broker.
 *
 *  If this connection is set to blocking, attempts to do the TCP connection and complete the RDMnet
 *  connection handshake within this function. Otherwise, starts a non-blocking TCP connect and
 *  returns immediately; use rdmnet_connect_poll() to check connection status. Handles redirections
 *  automatically. On failure, calling this function again on the same connection will wait for the
 *  backoff time required by the standard before reconnecting. This backoff time is added to the
 *  blocking time for blocking connections, or run in the background for nonblocking connections.
 *
 *  \param[in] handle Connection handle to connect. Must have been previously created using
 *                    rdmnet_connection_create().
 *  \param[in] remote_addr %Broker's IP address and port.
 *  \param[in] connect_data The information about this client that will be sent to the %Broker as
 *                          part of the connection handshake. Caller maintains ownership.
 *  \return #kEtcPalErrOk: Connection completed successfully.
 *  \return #kEtcPalErrInProgress: Non-blocking connection started.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrNotFound: Connection handle not previously created.
 *  \return #kEtcPalErrIsConn: Already connected on this handle.
 *  \return #kEtcPalErrTimedOut: Timed out waiting for connection handshake to complete.
 *  \return #kEtcPalErrConnRefused: Connection refused either at the TCP or RDMnet level.
 *                                additional_data may contain a reason code.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t rdmnet_connect(rdmnet_conn_t handle, const EtcPalSockAddr* remote_addr,
                              const ClientConnectMsg* connect_data)
{
  if (!remote_addr || !connect_data)
    return kEtcPalErrInvalid;

  RdmnetConnection* conn;
  etcpal_error_t res = get_conn(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  if (conn->state != kCSConnectNotStarted)
    res = kEtcPalErrIsConn;

  if (res == kEtcPalErrOk)
  {
    conn->remote_addr = *remote_addr;
    conn->conn_data = *connect_data;
    conn->state = kCSConnectPending;
  }

  release_conn(conn);
  return res;
}

/*! \brief Set an RDMnet connection handle to be either blocking or non-blocking.
 *
 *  The blocking state of a connection controls how other API calls behave. If a connection is:
 *
 *  * Blocking:
 *    - rdmnet_send() and related functions will block until all data is sent.
 *  * Non-blocking:
 *    - rdmnet_send() will return immediately with error code #kEtcPalErrWouldBlock if there is too much
 *      data to fit in the underlying send buffer.
 *
 *  \param[in] handle Connection handle for which to change blocking state.
 *  \param[in] blocking Whether the connection should be blocking.
 *  \return #kEtcPalErrOk: Blocking state was changed successfully.
 *  \return #kEtcPalErrInvalid: Invalid connection handle.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrBusy: A connection is currently in progress.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t rdmnet_set_blocking(rdmnet_conn_t handle, bool blocking)
{
  RdmnetConnection* conn;
  etcpal_error_t res = get_conn(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  if (!(conn->state == kCSConnectNotStarted || conn->state == kCSHeartbeat))
  {
    // Can't change the blocking state while a connection is in progress.
    release_conn(conn);
    return kEtcPalErrBusy;
  }

  if (conn->state == kCSHeartbeat)
  {
    res = etcpal_setblocking(conn->sock, blocking);
    if (res == kEtcPalErrOk)
      conn->is_blocking = blocking;
  }
  else
  {
    // State is NotConnected, just change the flag
    conn->is_blocking = blocking;
  }
  release_conn(conn);
  return res;
}

/*! \brief ADVANCED USAGE: Attach an RDMnet connection handle to an already-connected system socket.
 *
 *  This function is typically only used by brokers. The RDMnet connection is assumed to have
 *  already completed and be at the Heartbeat stage.
 *
 *  \param[in] handle Connection handle to attach the socket to. Must have been previously created
 *                    using rdmnet_connection_create().
 *  \param[in] sock System socket to attach to the connection handle. Must be an already-connected
 *                  stream socket.
 *  \param[in] remote_addr The remote network address to which the socket is currently connected.
 *  \return #kEtcPalErrOk: Socket was attached successfully.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrIsConn: The connection handle provided is already connected using another socket.
 *  \return #kEtcPalErrNotImpl: RDMnet has been compiled with #RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS=0
 *          and thus this function is not available.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_attach_existing_socket(rdmnet_conn_t handle, etcpal_socket_t sock,
                                             const EtcPalSockAddr* remote_addr)
{
#if RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS
  if (sock == ETCPAL_SOCKET_INVALID || !remote_addr)
    return kEtcPalErrInvalid;

  RdmnetConnection* conn;
  etcpal_error_t res = get_conn(handle, &conn);
  if (res == kEtcPalErrOk)
  {
    if (conn->state != kCSConnectNotStarted)
    {
      res = kEtcPalErrIsConn;
    }
    else
    {
      conn->sock = sock;
      conn->remote_addr = *remote_addr;
      conn->state = kCSHeartbeat;
      conn->external_socket_attached = true;
      etcpal_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
      etcpal_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
    }
    release_conn(conn);
  }
  return res;
#else   // RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS
  RDMNET_UNUSED_ARG(handle);
  RDMNET_UNUSED_ARG(sock);
  RDMNET_UNUSED_ARG(remote_addr);
  return kEtcPalErrNotImpl;
#endif  // RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS
}

/*! \brief Destroy an RDMnet connection handle.
 *
 *  If the connection is currently healthy, call rdmnet_disconnect() first to do a graceful
 *  RDMnet-level disconnect.
 *
 *  \param[in] handle Connection handle to destroy.
 *  \param[in] disconnect_reason If not NULL, an RDMnet Disconnect message will be sent with this
 *                               reason code. This is the proper way to gracefully close a
 *                               connection in RDMnet.
 *  \return #kEtcPalErrOk: Connection was successfully destroyed
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_connection_destroy(rdmnet_conn_t handle, const rdmnet_disconnect_reason_t* disconnect_reason)
{
  RdmnetConnection* conn;
  etcpal_error_t res = get_conn(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  if (conn->state == kCSHeartbeat && disconnect_reason)
  {
    DisconnectMsg dm;
    dm.disconnect_reason = *disconnect_reason;
    send_disconnect(conn, &dm);
  }
  conn->state = kCSMarkedForDestruction;

  release_conn(conn);
  return res;
}

/*! \brief Send data on an RDMnet connection.
 *
 *  Thin wrapper over the underlying socket send function. Use rdmnet_set_blocking() to control the
 *  blocking behavior of this send.
 *
 *  \param[in] handle Connection handle on which to send.
 *  \param[in] data Data buffer to send.
 *  \param[in] size Size of data buffer.
 *  \return Number of bytes sent (success)
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrNotConn: The connection handle has not been successfully connected.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
int rdmnet_send(rdmnet_conn_t handle, const uint8_t* data, size_t size)
{
  if (!data || size == 0)
    return kEtcPalErrInvalid;

  RdmnetConnection* conn;
  int res = get_conn(handle, &conn);
  if (res == kEtcPalErrOk)
  {
    if (conn->state != kCSHeartbeat)
      res = kEtcPalErrNotConn;
    else
      res = etcpal_send(conn->sock, data, size, 0);

    release_conn(conn);
  }

  return res;
}

/* Internal function to start an atomic send operation on an RDMnet connection.
 *
 * Because RDMnet uses stream sockets, it is sometimes convenient to send messages piece by piece.
 * This function, together with rdmnet_end_message(), can be used to guarantee an atomic piece-wise
 * send operation in a multithreaded environment. Once started, any other calls to rdmnet_send() or
 * rdmnet_start_message() will block waiting for this operation to end using rdmnet_end_message().
 *
 * \param[in] handle Connection handle on which to start an atomic send operation.
 * \param[out] conn_out Filled in on success with a pointer to the underlying connection structure.
 *                      Its socket can be used to send using etcpal_send() and it should be passed
 *                      back with a subsequent call to rdmnet_end_message().
 * \return #kEtcPalErrOk: Send operation started successfully.
 * \return #kEtcPalErrInvalid: Invalid argument provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotConn: The connection handle has not been successfully connected.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_start_message(rdmnet_conn_t handle, RdmnetConnection** conn_out)
{
  RdmnetConnection* conn;
  etcpal_error_t res = get_conn(handle, &conn);
  if (res == kEtcPalErrOk)
  {
    if (conn->state != kCSHeartbeat)
    {
      res = kEtcPalErrNotConn;
      release_conn(conn);
    }
    else
    {
      // Intentionally keep the conn locked after returning
      *conn_out = conn;
    }
  }

  return res;
}

/* Internal function to end an atomic send operation on an RDMnet connection.
 *
 * MUST call rdmnet_start_message() first to begin an atomic send operation.
 *
 * Because RDMnet uses stream sockets, it is sometimes convenient to send messages piece by piece.
 * This function, together with rdmnet_start_message() and rdmnet_send_partial_message(), can be
 * used to guarantee an atomic piece-wise send operation in a multithreaded environment.
 *
 * \param[in] handle Connection handle on which to end an atomic send operation.
 * \return #kEtcPalErrOk: Send operation ended successfully.
 * \return #kEtcPalErrInvalid: Invalid argument provided.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_end_message(RdmnetConnection* conn)
{
  if (!conn)
    return kEtcPalErrInvalid;

  release_conn(conn);
  return kEtcPalErrOk;
}

/*!
 * \brief Get a string description of an RDMnet connection failure event.
 *
 * An RDMnet connection failure event provides a high-level reason why an RDMnet connection failed.
 *
 * \param[in] event Event code.
 * \return String, or NULL if event is invalid.
 */
const char* rdmnet_connect_fail_event_to_string(rdmnet_connect_fail_event_t event)
{
  if (event >= 0 && event < NUM_CONNECT_FAIL_EVENT_STRINGS)
    return kRdmnetConnectFailEventStrings[event];
  return NULL;
}

/*!
 * \brief Get a string description of an RDMnet disconnect event.
 *
 * An RDMnet disconnect event provides a high-level reason why an RDMnet connection was
 * disconnected.
 *
 * \param[in] event Event code.
 * \return String, or NULL if event is invalid.
 */
const char* rdmnet_disconnect_event_to_string(rdmnet_disconnect_event_t event)
{
  if (event >= 0 && event < NUM_DISCONNECT_EVENT_STRINGS)
    return kRdmnetDisconnectEventStrings[event];
  return NULL;
}

void start_rdmnet_connection(RdmnetConnection* conn)
{
  if (conn->is_blocking)
    etcpal_setblocking(conn->sock, true);

  // Update state
  conn->state = kCSRDMnetConnPending;
  rdmnet_core_modify_polled_socket(conn->sock, ETCPAL_POLL_IN, &conn->poll_info);
  send_client_connect(conn, &conn->conn_data);
  etcpal_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
  etcpal_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
}

void start_tcp_connection(RdmnetConnection* conn, ConnCallbackDispatchInfo* cb)
{
  bool ok = true;
  RdmnetConnectFailedInfo* failed_info = &cb->args.connect_failed.failed_info;

  etcpal_error_t res = etcpal_socket(conn->remote_addr.ip.type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET,
                                     ETCPAL_STREAM, &conn->sock);
  if (res != kEtcPalErrOk)
  {
    ok = false;
    failed_info->event = kRdmnetConnectFailSocketFailure;
    failed_info->socket_err = res;
  }

  if (ok)
  {
    res = etcpal_setblocking(conn->sock, false);
    if (res != kEtcPalErrOk)
    {
      ok = false;
      failed_info->event = kRdmnetConnectFailSocketFailure;
      failed_info->socket_err = res;
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
      conn->state = kCSTCPConnPending;
      etcpal_error_t add_res = rdmnet_core_add_polled_socket(conn->sock, ETCPAL_POLL_CONNECT, &conn->poll_info);
      if (add_res != kEtcPalErrOk)
      {
        ok = false;
        failed_info->socket_err = add_res;
      }
    }
    else
    {
      ok = false;
      // EHOSTUNREACH is sometimes reported synchronously even for a non-blocking connect.

      if (res == kEtcPalErrHostUnreach)
        failed_info->event = kRdmnetConnectFailTcpLevel;
      else
        failed_info->event = kRdmnetConnectFailSocketFailure;
      failed_info->socket_err = res;
    }
  }

  if (!ok)
  {
    cb->which = kConnCallbackConnectFailed;
    fill_callback_info(conn, cb);
    reset_connection(conn);
  }
}

/*! \brief Handle periodic RDMnet functionality.
 *
 *  If #RDMNET_USE_TICK_THREAD is defined nonzero, this is an internal function called automatically
 *  by the library. Otherwise, it must be called by the application preiodically to handle
 *  health-checked TCP functionality. Recommended calling interval is ~1s.
 */
void rdmnet_conn_tick()
{
  if (!rdmnet_core_initialized())
    return;

  // Remove any connections marked for destruction.
  if (rdmnet_writelock())
  {
    destroy_marked_connections();
    rdmnet_writeunlock();
  }

  // Do the rest of the periodic functionality with a read lock

  ConnCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  if (rdmnet_readlock())
  {
    process_all_connection_state(&cb);
    rdmnet_readunlock();
  }
  deliver_callback(&cb);
}

void destroy_marked_connections()
{
  RdmnetConnection* destroy_list = NULL;
  RdmnetConnection** next_destroy_list_entry = &destroy_list;

  EtcPalRbIter conn_iter;
  etcpal_rbiter_init(&conn_iter);

  RdmnetConnection* conn = (RdmnetConnection*)etcpal_rbiter_first(&conn_iter, &state.connections);
  while (conn)
  {
    // Can't destroy while iterating as that would invalidate the iterator
    // So the connections are added to a linked list of connections pending destruction
    if (conn->state == kCSMarkedForDestruction)
    {
      *next_destroy_list_entry = conn;
      conn->next_to_destroy = NULL;
      next_destroy_list_entry = &conn->next_to_destroy;
    }
    conn = etcpal_rbiter_next(&conn_iter);
  }

  // Now do the actual destruction
  if (destroy_list)
  {
    RdmnetConnection* to_destroy = destroy_list;
    while (to_destroy)
    {
      RdmnetConnection* next = to_destroy->next_to_destroy;
      destroy_connection(to_destroy, true);
      to_destroy = next;
    }
  }
}

void process_all_connection_state(ConnCallbackDispatchInfo* cb)
{
  EtcPalRbIter conn_iter;
  etcpal_rbiter_init(&conn_iter);
  RdmnetConnection* conn = (RdmnetConnection*)etcpal_rbiter_first(&conn_iter, &state.connections);
  while (conn)
  {
    if (etcpal_mutex_lock(&conn->lock))
    {
      switch (conn->state)
      {
        case kCSConnectPending:
          if (conn->rdmnet_conn_failed || conn->backoff_timer.interval != 0)
          {
            if (conn->rdmnet_conn_failed)
            {
              etcpal_timer_start(&conn->backoff_timer, update_backoff(conn->backoff_timer.interval));
            }
            conn->state = kCSBackoff;
          }
          else
          {
            start_tcp_connection(conn, cb);
          }
          break;
        case kCSBackoff:
          if (etcpal_timer_is_expired(&conn->backoff_timer))
          {
            start_tcp_connection(conn, cb);
          }
          break;
        case kCSHeartbeat:
          if (etcpal_timer_is_expired(&conn->hb_timer))
          {
            // Heartbeat timeout! Disconnect the connection.
            if (cb->which == kConnCallbackNone)
            {
              // Currently we have a limit of processing one heartbeat timeout per tick. This
              // helps simplify the implementation, since heartbeat timeouts aren't anticipated
              // to come in big bursts.

              // If it causes performance issues, it should be revisited.

              cb->which = kConnCallbackDisconnected;
              fill_callback_info(conn, cb);

              RdmnetDisconnectedInfo* disconn_info = &cb->args.disconnected.disconn_info;
              disconn_info->event = kRdmnetDisconnectNoHeartbeat;
              disconn_info->socket_err = kEtcPalErrOk;

              reset_connection(conn);
            }
          }
          else if (etcpal_timer_is_expired(&conn->send_timer))
          {
            send_null(conn);
            etcpal_timer_reset(&conn->send_timer);
          }
          break;
        default:
          break;
      }
      etcpal_mutex_unlock(&conn->lock);
    }

    conn = etcpal_rbiter_next(&conn_iter);
  }
}

void tcp_connection_established(rdmnet_conn_t handle)
{
  if (handle < 0)
    return;

  RdmnetConnection* conn;
  if (kEtcPalErrOk == get_conn(handle, &conn))
  {
    // connected successfully!
    start_rdmnet_connection(conn);
    release_conn(conn);
  }
}

void rdmnet_socket_error(rdmnet_conn_t handle, etcpal_error_t socket_err)
{
  if (handle < 0)
    return;

  CB_STORAGE_CLASS ConnCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  RdmnetConnection* conn;
  if (kEtcPalErrOk == get_conn(handle, &conn))
  {
    fill_callback_info(conn, &cb);

    if (conn->state == kCSTCPConnPending || conn->state == kCSRDMnetConnPending)
    {
      cb.which = kConnCallbackConnectFailed;

      RdmnetConnectFailedInfo* failed_info = &cb.args.connect_failed.failed_info;
      failed_info->event = kRdmnetConnectFailTcpLevel;
      failed_info->socket_err = socket_err;
      if (conn->state == kCSRDMnetConnPending)
        conn->rdmnet_conn_failed = true;

      reset_connection(conn);
    }
    else if (conn->state == kCSHeartbeat)
    {
      cb.which = kConnCallbackDisconnected;

      RdmnetDisconnectedInfo* disconn_info = &cb.args.disconnected.disconn_info;
      disconn_info->event = kRdmnetDisconnectAbruptClose;
      disconn_info->socket_err = socket_err;

      reset_connection(conn);
    }
    release_conn(conn);
  }

  deliver_callback(&cb);
}

void handle_rdmnet_connect_result(RdmnetConnection* conn, RdmnetMessage* msg, ConnCallbackDispatchInfo* cb)
{
  if (GET_BROKER_MSG(msg))
  {
    BrokerMessage* bmsg = GET_BROKER_MSG(msg);
    if (IS_CONNECT_REPLY_MSG(bmsg))
    {
      ConnectReplyMsg* reply = GET_CONNECT_REPLY_MSG(bmsg);
      switch (reply->connect_status)
      {
        case kRdmnetConnectOk:
          // TODO check version
          conn->state = kCSHeartbeat;
          etcpal_timer_start(&conn->backoff_timer, 0);
          cb->which = kConnCallbackConnected;

          RdmnetConnectedInfo* connect_info = &cb->args.connected.connect_info;
          connect_info->broker_uid = reply->broker_uid;
          connect_info->client_uid = reply->client_uid;
          connect_info->connected_addr = conn->remote_addr;
          break;
        default:
        {
          cb->which = kConnCallbackConnectFailed;

          RdmnetConnectFailedInfo* failed_info = &cb->args.connect_failed.failed_info;
          failed_info->event = kRdmnetConnectFailRejected;
          failed_info->socket_err = kEtcPalErrOk;
          failed_info->rdmnet_reason = reply->connect_status;

          reset_connection(conn);
          conn->rdmnet_conn_failed = true;
          break;
        }
      }
    }
    else if (IS_CLIENT_REDIRECT_MSG(bmsg))
    {
      conn->remote_addr = GET_CLIENT_REDIRECT_MSG(bmsg)->new_addr;
      retry_connection(conn);
    }
  }
  free_rdmnet_message(msg);
}

void handle_rdmnet_message(RdmnetConnection* conn, RdmnetMessage* msg, ConnCallbackDispatchInfo* cb)
{
  // We've received something on this connection. Reset the heartbeat timer.
  etcpal_timer_reset(&conn->hb_timer);

  // We handle some Broker messages internally
  bool deliver_message = false;
  if (GET_BROKER_MSG(msg))
  {
    BrokerMessage* bmsg = GET_BROKER_MSG(msg);
    switch (bmsg->vector)
    {
      case VECTOR_BROKER_CONNECT_REPLY:
      case VECTOR_BROKER_NULL:
        break;
      case VECTOR_BROKER_DISCONNECT:
        fill_callback_info(conn, cb);
        cb->which = kConnCallbackDisconnected;

        RdmnetDisconnectedInfo* disconn_info = &cb->args.disconnected.disconn_info;
        disconn_info->event = kRdmnetDisconnectGracefulRemoteInitiated;
        disconn_info->socket_err = kEtcPalErrOk;
        disconn_info->rdmnet_reason = GET_DISCONNECT_MSG(bmsg)->disconnect_reason;

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
    cb->which = kConnCallbackMsgReceived;
    cb->args.msg_received.message = *msg;
  }
  else
  {
    free_rdmnet_message(msg);
  }
}

etcpal_error_t rdmnet_do_recv(rdmnet_conn_t handle, const uint8_t* data, size_t data_size, ConnCallbackDispatchInfo* cb)
{
  RdmnetConnection* conn;
  etcpal_error_t res = get_conn(handle, &conn);
  if (res == kEtcPalErrOk)
  {
    fill_callback_info(conn, cb);
    if (conn->state == kCSHeartbeat || conn->state == kCSRDMnetConnPending)
    {
      RdmnetMsgBuf* msgbuf = &conn->recv_buf;
      res = rdmnet_msg_buf_recv(msgbuf, data, data_size);
      if (res == kEtcPalErrOk)
      {
        if (conn->state == kCSRDMnetConnPending)
        {
          handle_rdmnet_connect_result(conn, &msgbuf->msg, cb);
        }
        else
        {
          handle_rdmnet_message(conn, &msgbuf->msg, cb);
        }
      }
    }
    else
    {
      res = kEtcPalErrInvalid;
    }
    release_conn(conn);
  }
  return res;
}

void rdmnet_socket_data_received(rdmnet_conn_t handle, const uint8_t* data, size_t data_size)
{
  if (handle < 0 || !data || !data_size)
    return;

  CB_STORAGE_CLASS ConnCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  etcpal_error_t res = rdmnet_do_recv(handle, data, data_size, &cb);
  while (res == kEtcPalErrOk)
  {
    deliver_callback(&cb);
    INIT_CALLBACK_INFO(&cb);
    res = rdmnet_do_recv(handle, NULL, 0, &cb);
  }
}

void fill_callback_info(const RdmnetConnection* conn, ConnCallbackDispatchInfo* info)
{
  info->handle = conn->handle;
  info->cbs = conn->callbacks;
  info->context = conn->callback_context;
}

void deliver_callback(ConnCallbackDispatchInfo* info)
{
  switch (info->which)
  {
    case kConnCallbackConnected:
      if (info->cbs.connected)
        info->cbs.connected(info->handle, &info->args.connected.connect_info, info->context);
      break;
    case kConnCallbackConnectFailed:
      if (info->cbs.connect_failed)
        info->cbs.connect_failed(info->handle, &info->args.connect_failed.failed_info, info->context);
      break;
    case kConnCallbackDisconnected:
      if (info->cbs.disconnected)
        info->cbs.disconnected(info->handle, &info->args.disconnected.disconn_info, info->context);
      break;
    case kConnCallbackMsgReceived:
      if (info->cbs.msg_received)
        info->cbs.msg_received(info->handle, &info->args.msg_received.message, info->context);
      free_rdmnet_message(&info->args.msg_received.message);
      break;
    case kConnCallbackNone:
    default:
      break;
  }
}

void rdmnet_conn_socket_activity(const EtcPalPollEvent* event, PolledSocketOpaqueData data)
{
  static uint8_t rdmnet_poll_recv_buf[RDMNET_RECV_DATA_MAX_SIZE];

  if (event->events & ETCPAL_POLL_ERR)
  {
    rdmnet_socket_error(data.conn_handle, event->err);
  }
  else if (event->events & ETCPAL_POLL_IN)
  {
    int recv_res = etcpal_recv(event->socket, rdmnet_poll_recv_buf, RDMNET_RECV_DATA_MAX_SIZE, 0);
    if (recv_res <= 0)
      rdmnet_socket_error(data.conn_handle, recv_res);
    else
      rdmnet_socket_data_received(data.conn_handle, rdmnet_poll_recv_buf, (size_t)recv_res);
  }
  else if (event->events & ETCPAL_POLL_CONNECT)
  {
    tcp_connection_established(data.conn_handle);
  }
}

/* Callback for IntHandleManager to determine whether a handle is in use. */
bool conn_handle_in_use(int handle_val)
{
  return etcpal_rbtree_find(&state.connections, &handle_val);
}

/* Internal function which attempts to allocate and track a new connection, including allocating the
 * structure, creating a new handle value, and inserting it into the global map.
 *
 *  Must have write lock.
 */
RdmnetConnection* create_new_connection(const RdmnetConnectionConfig* config)
{
  rdmnet_conn_t new_handle = get_next_int_handle(&state.handle_mgr);
  if (new_handle == RDMNET_CONN_INVALID)
    return NULL;

  RdmnetConnection* conn = alloc_rdmnet_connection();
  if (conn)
  {
    bool ok;
    bool lock_created = false;

    // Try to create the locks and signal
    ok = lock_created = etcpal_mutex_create(&conn->lock);
    if (ok)
    {
      conn->handle = new_handle;
      ok = (kEtcPalErrOk == etcpal_rbtree_insert(&state.connections, conn));
    }
    if (ok)
    {
      conn->local_cid = config->local_cid;

      conn->sock = ETCPAL_SOCKET_INVALID;
      ETCPAL_IP_SET_INVALID(&conn->remote_addr.ip);
      conn->remote_addr.port = 0;
      conn->external_socket_attached = false;
      conn->is_blocking = true;
      conn->poll_info.callback = rdmnet_conn_socket_activity;
      conn->poll_info.data.conn_handle = conn->handle;

      conn->state = kCSConnectNotStarted;
      etcpal_timer_start(&conn->backoff_timer, 0);
      conn->rdmnet_conn_failed = false;

      rdmnet_msg_buf_init(&conn->recv_buf);

      conn->callbacks = config->callbacks;
      conn->callback_context = config->callback_context;

      conn->next_to_destroy = NULL;
    }
    else
    {
      // Clean up
      if (lock_created)
        etcpal_mutex_destroy(&conn->lock);
      free_rdmnet_connection(conn);
      conn = NULL;
    }
  }
  return conn;
}

/* Internal function to update a backoff timer value using the algorithm specified in E1.33. Returns
 * the new value. */
uint32_t update_backoff(uint32_t previous_backoff)
{
  uint32_t result = (uint32_t)(((rand() % 4001) + 1000));
  result += previous_backoff;
  /* 30 second interval is the max */
  if (result > 30000u)
    return 30000u;
  return result;
}

void reset_connection(RdmnetConnection* conn)
{
  if (conn->sock != ETCPAL_SOCKET_INVALID)
  {
    if (!conn->external_socket_attached)
    {
      rdmnet_core_remove_polled_socket(conn->sock);
      etcpal_close(conn->sock);
    }
    conn->sock = ETCPAL_SOCKET_INVALID;
  }
  conn->state = kCSConnectNotStarted;
}

void retry_connection(RdmnetConnection* conn)
{
  if (conn->sock != ETCPAL_SOCKET_INVALID)
  {
    etcpal_close(conn->sock);
    conn->sock = ETCPAL_SOCKET_INVALID;
  }
  conn->state = kCSConnectPending;
}

void destroy_connection(RdmnetConnection* conn, bool remove_from_tree)
{
  if (conn)
  {
    if (!conn->external_socket_attached && conn->sock != ETCPAL_SOCKET_INVALID)
    {
      rdmnet_core_remove_polled_socket(conn->sock);
      etcpal_close(conn->sock);
    }
    etcpal_mutex_destroy(&conn->lock);
    if (remove_from_tree)
      etcpal_rbtree_remove(&state.connections, conn);
    free_rdmnet_connection(conn);
  }
}

etcpal_error_t get_conn(rdmnet_conn_t handle, RdmnetConnection** conn)
{
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;
  if (!rdmnet_readlock())
    return kEtcPalErrSys;

  RdmnetConnection* found_conn = (RdmnetConnection*)etcpal_rbtree_find(&state.connections, &handle);
  if (!found_conn)
  {
    rdmnet_readunlock();
    return kEtcPalErrNotFound;
  }
  if (!etcpal_mutex_lock(&found_conn->lock))
  {
    rdmnet_readunlock();
    return kEtcPalErrSys;
  }
  if (found_conn->state == kCSMarkedForDestruction)
  {
    etcpal_mutex_unlock(&found_conn->lock);
    rdmnet_readunlock();
    return kEtcPalErrNotFound;
  }
  *conn = found_conn;
  return kEtcPalErrOk;
}

void release_conn(RdmnetConnection* conn)
{
  etcpal_mutex_unlock(&conn->lock);
  rdmnet_readunlock();
}

int conn_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  RDMNET_UNUSED_ARG(self);

  const RdmnetConnection* a = (const RdmnetConnection*)value_a;
  const RdmnetConnection* b = (const RdmnetConnection*)value_b;
  return (a->handle > b->handle) - (a->handle < b->handle);
}

EtcPalRbNode* conn_node_alloc(void)
{
#if RDMNET_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(rdmnet_conn_rb_nodes);
#endif
}

void conn_node_free(EtcPalRbNode* node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(rdmnet_conn_rb_nodes, node);
#endif
}
