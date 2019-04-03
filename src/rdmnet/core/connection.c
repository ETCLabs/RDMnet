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

#include "rdmnet/core/connection.h"

#include "rdmnet/private/opts.h"
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "lwpa/mempool.h"
#endif
#if RDMNET_USE_TICK_THREAD
#include "lwpa/thread.h"
#endif
#include "lwpa/lock.h"
#include "lwpa/socket.h"
#include "lwpa/rbtree.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/message.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/message.h"
#include "rdmnet/private/connection.h"
#include "rdmnet/private/broker_prot.h"

/*************************** Private constants *******************************/

/* When waiting on the backoff timer for a new connection, the interval at which to wake up and make
 * sure that we haven't been deinitted/closed. */
#define BLOCKING_BACKOFF_WAIT_INTERVAL 500
#define RDMNET_CONN_POLL_TIMEOUT 120 /* ms */
#define RDMNET_CONN_MAX_SOCKETS LWPA_SOCKET_MAX_POLL_SIZE

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using lwpa_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_connection() malloc(sizeof(RdmnetConnection))
#define free_rdmnet_connection(ptr) free(ptr)
#else
#define alloc_rdmnet_connection() lwpa_mempool_alloc(rdmnet_connections)
#define free_rdmnet_connection(ptr) lwpa_mempool_free(rdmnet_connections, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(rdmnet_connections, RdmnetConnection, RDMNET_MAX_CONNECTIONS);
LWPA_MEMPOOL_DEFINE(rdmnet_conn_rb_nodes, LwpaRbNode, RDMNET_MAX_CONNECTIONS);
#endif

static struct RdmnetConnectionState
{
  LwpaRbTree connections;
  rdmnet_conn_t next_handle;
  bool handle_has_wrapped_around;
} state;

/*********************** Private function prototypes *************************/

static int update_backoff(int previous_backoff);
static void deliver_callback(ConnCallbackDispatchInfo *info);
static void start_tcp_connection(RdmnetConnection *conn, ConnCallbackDispatchInfo *cb);
static void start_rdmnet_connection(RdmnetConnection *conn);
static void reset_connection(RdmnetConnection *conn);
static void retry_connection(RdmnetConnection *conn);

// Connection management, lookup, destruction
static rdmnet_conn_t get_new_conn_handle();
static RdmnetConnection *create_new_connection(const RdmnetConnectionConfig *config);
static void destroy_connection(RdmnetConnection *conn);
static lwpa_error_t get_conn(rdmnet_conn_t handle, RdmnetConnection **conn);
static void release_conn(RdmnetConnection *conn);
static int conn_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static LwpaRbNode *conn_node_alloc();
static void conn_node_free(LwpaRbNode *node);

/*************************** Function definitions ****************************/

/*! \brief Initialize the RDMnet Connection module.
 *
 *  Do all necessary initialization before other RDMnet Connection API functions can be called.
 *
 *  \return #kLwpaErrOk: Initialization successful.\n
 *          #kLwpaErrSys: An internal library of system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_connection_init()
{
  lwpa_error_t res = kLwpaErrOk;

#if !RDMNET_DYNAMIC_MEM
  /* Init memory pools */
  res |= lwpa_mempool_init(rdmnet_connections);
#endif

  if (res == kLwpaErrOk)
  {
    lwpa_rbtree_init(&state.connections, conn_cmp, conn_node_alloc, conn_node_free);

    state.next_handle = 0;
    state.handle_has_wrapped_around = false;
  }

  return res;
}

static void conn_dealloc(const LwpaRbTree *self, LwpaRbNode *node)
{
  (void)self;

  RdmnetConnection *conn = (RdmnetConnection *)node->value;
  if (conn)
    destroy_connection(conn);
  conn_node_free(node);
}

/*! \brief Deinitialize the RDMnet Connection module.
 *
 *  Set the RDMnet Connection module back to an uninitialized state. All existing connections will
 *  be closed/disconnected. Calls to other RDMnet Connection API functions will fail until
 *  rdmnet_init() is called again.
 */
void rdmnet_connection_deinit()
{
  lwpa_rbtree_clear_with_cb(&state.connections, conn_dealloc);
  memset(&state, 0, sizeof state);
}

/*! \brief Create a new handle to use for an RDMnet Connection.
 *
 *  This function simply allocates a connection handle - use rdmnet_connect() to actually start the
 *  connection process.
 *
 *  \param[in] config Configuration parameters for the connection to be created.
 *  \param[out] handle Handle to the newly-created connection
 *  \return #kLwpaErrOk: Handle created successfully.\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrNoMem: No room to allocate additional connection.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_new_connection(const RdmnetConnectionConfig *config, rdmnet_conn_t *handle)
{
  if (!config || !handle)
    return kLwpaErrInvalid;
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;

  lwpa_error_t res = kLwpaErrSys;
  if (rdmnet_writelock())
  {
    res = kLwpaErrOk;
    /* Passed the quick checks, try to create a struct to represent a new connection. This function
     * creates the new connection, gives it a unique handle and inserts it into the connection map. */
    RdmnetConnection *conn = create_new_connection(config);
    if (!conn)
      res = kLwpaErrNoMem;

    if (res == kLwpaErrOk)
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
 *                    rdmnet_new_connection().
 *  \param[in] remote_addr %Broker's IP address and port.
 *  \param[in] connect_data The information about this client that will be sent to the %Broker as
 *                          part of the connection handshake. Caller maintains ownership.
 *  \return #kLwpaErrOk: Connection completed successfully.\n
 *          #kLwpaErrInProgress: Non-blocking connection started.\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrNotFound: Connection handle not previously created.\n
 *          #kLwpaErrIsConn: Already connected on this handle.\n
 *          #kLwpaErrTimedOut: Timed out waiting for connection handshake to complete.\n
 *          #kLwpaConnRefused: Connection refused either at the TCP or RDMnet level. additional_data
 *                             may contain a reason code.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_connect(rdmnet_conn_t handle, const LwpaSockaddr *remote_addr, const ClientConnectMsg *connect_data)
{
  if (!remote_addr || !connect_data)
    return kLwpaErrInvalid;

  RdmnetConnection *conn;
  lwpa_error_t res = get_conn(handle, &conn);
  if (res != kLwpaErrOk)
    return res;

  if (conn->state != kCSConnectNotStarted)
    res = kLwpaErrIsConn;

  if (res == kLwpaErrOk)
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
 *    - rdmnet_send() will return immediately with error code #kLwpaErrWouldBlock if there is too much
 *      data to fit in the underlying send buffer.
 *
 *  \param[in] handle Connection handle for which to change blocking state.
 *  \param[in] blocking Whether the connection should be blocking.
 *  \return #kLwpaErrOk: Blocking state was changed successfully.\n
 *          #kLwpaErrInvalid: Invalid connection handle.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrBusy: A connection is currently in progress.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_set_blocking(rdmnet_conn_t handle, bool blocking)
{
  RdmnetConnection *conn;
  lwpa_error_t res = get_conn(handle, &conn);
  if (res != kLwpaErrOk)
    return res;

  if (!(conn->state == kCSConnectNotStarted || conn->state == kCSHeartbeat))
  {
    /* Can't change the blocking state while a connection is in progress. */
    release_conn(conn);
    return kLwpaErrBusy;
  }

  if (conn->state == kCSHeartbeat)
  {
    res = lwpa_setblocking(conn->sock, blocking);
    if (res == kLwpaErrOk)
      conn->is_blocking = blocking;
  }
  else
  {
    /* State is NotConnected, just change the flag */
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
 *                    using rdmnet_new_connection().
 *  \param[in] sock System socket to attach to the connection handle. Must be an already-connected
 *                  stream socket.
 *  \param[in] remote_addr The remote network address to which the socket is currently connected.
 *  \return #kLwpaErrOk: Socket was attached successfully.\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrIsConn: The connection handle provided is already connected using another socket.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_attach_existing_socket(rdmnet_conn_t handle, lwpa_socket_t sock, const LwpaSockaddr *remote_addr)
{
  if (sock == LWPA_SOCKET_INVALID || !remote_addr)
    return kLwpaErrInvalid;

  RdmnetConnection *conn;
  lwpa_error_t res = get_conn(handle, &conn);
  if (res == kLwpaErrOk)
  {
    if (conn->state != kCSConnectNotStarted)
    {
      res = kLwpaErrIsConn;
    }
    else
    {
      conn->sock = sock;
      conn->remote_addr = *remote_addr;
      conn->state = kCSHeartbeat;
      conn->is_client = false;
      lwpa_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
      lwpa_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
    }
    release_conn(conn);
  }
  return res;
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
 *  \return #kLwpaErrOk: Connection was successfully destroyed\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_destroy_connection(rdmnet_conn_t handle, const rdmnet_disconnect_reason_t *disconnect_reason)
{
  RdmnetConnection *conn;
  lwpa_error_t res = get_conn(handle, &conn);
  if (res != kLwpaErrOk)
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
 *  \return Number of bytes sent (success)\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrNotConn: The connection handle has not been successfully connected.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
int rdmnet_send(rdmnet_conn_t handle, const uint8_t *data, size_t size)
{
  if (data || size == 0)
    return kLwpaErrInvalid;

  RdmnetConnection *conn;
  int res = get_conn(handle, &conn);
  if (res == kLwpaErrOk)
  {
    if (conn->state != kCSHeartbeat)
      res = kLwpaErrNotConn;
    lwpa_mutex_give(&conn->lock);
  }

  if (res == kLwpaErrOk && lwpa_mutex_take(&conn->send_lock, LWPA_WAIT_FOREVER))
  {
    res = lwpa_send(conn->sock, data, size, 0);
    lwpa_mutex_give(&conn->send_lock);
  }
  rdmnet_readunlock();
  return res;
}

/*! \brief Start an atomic send operation on an RDMnet connection.
 *
 *  Because RDMnet uses stream sockets, it is sometimes convenient to send messages piece by piece.
 *  This function, together with rdmnet_send_partial_message() and rdmnet_end_message(), can be used
 *  to guarantee an atomic piece-wise send operation in a multithreaded environment. Once started,
 *  any other calls to rdmnet_send() or rdmnet_start_message() will block waiting for this operation
 *  to end using rdmnet_end_message().
 *
 *  \param[in] handle Connection handle on which to start an atomic send operation.
 *  \return #kLwpaErrOk: Send operation started successfully.\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrNotConn: The connection handle has not been successfully connected.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_start_message(rdmnet_conn_t handle)
{
  RdmnetConnection *conn;
  lwpa_error_t res = get_conn(handle, &conn);
  if (res == kLwpaErrOk)
  {
    if (conn->state != kCSHeartbeat)
      res = kLwpaErrNotConn;
    lwpa_mutex_give(&conn->lock);
  }

  if (res == kLwpaErrOk)
  {
    if (lwpa_mutex_take(&conn->send_lock, LWPA_WAIT_FOREVER))
    {
      /* Return, keeping the readlock and the send lock. */
      return res;
    }
    else
    {
      res = kLwpaErrSys;
    }
  }
  rdmnet_readunlock();
  return res;
}

/*! \brief Send a partial message as part of an atomic send operation on an RDMnet connection.
 *
 *  MUST call rdmnet_start_message() first to begin an atomic send operation.
 *
 *  Because RDMnet uses stream sockets, it is sometimes convenient to send messages piece-by-piece.
 *  This function, together with rdmnet_start_message() and rdmnet_end_message(), can be used to
 *  guarantee an atomic piece-wise send operation in a multithreaded environment.
 *
 *  \param[in] handle Connection handle on which to send a partial message.
 *  \param[in] data Data buffer to send.
 *  \param[in] size Size of data buffer.
 *  \return Number of bytes sent (success)\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrNotConn: The connection handle has not been successfully connected.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
int rdmnet_send_partial_message(rdmnet_conn_t handle, const uint8_t *data, size_t size)
{
  if (!data || size == 0)
    return kLwpaErrInvalid;

  RdmnetConnection *conn;
  int res = get_conn(handle, &conn);
  if (res == kLwpaErrOk)
  {
    if (conn->state != kCSHeartbeat)
      res = kLwpaErrNotConn;
    lwpa_mutex_give(&conn->lock);
  }

  if (res == kLwpaErrOk)
    res = lwpa_send(conn->sock, data, size, 0);

  rdmnet_readunlock();
  return res;
}

/*! \brief End an atomic send operation on an RDMnet connection.
 *
 *  MUST call rdmnet_start_message() first to begin an atomic send operation.
 *
 *  Because RDMnet uses stream sockets, it is sometimes convenient to send messages piece by piece.
 *  This function, together with rdmnet_start_message() and rdmnet_send_partial_message(), can be
 *  used to guarantee an atomic piece-wise send operation in a multithreaded environment.
 *
 *  \param[in] handle Connection handle on which to end an atomic send operation.
 *  \return #kLwpaErrOk: Send operation ended successfully.\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNotInit: Module not initialized.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_end_message(rdmnet_conn_t handle)
{
  RdmnetConnection *conn;
  lwpa_error_t res = get_conn(handle, &conn);
  if (res == kLwpaErrOk)
  {
    /* Release the send lock and the read lock that we had before. */
    lwpa_mutex_give(&conn->lock);
    lwpa_mutex_give(&conn->send_lock);
    rdmnet_readunlock();
  }
  /* And release the read lock that we took at the beginning of this function. */
  rdmnet_readunlock();
  return res;
}

void start_rdmnet_connection(RdmnetConnection *conn)
{
  if (conn->is_blocking)
    lwpa_setblocking(conn->sock, true);

  // Update state
  conn->state = kCSRDMnetConnPending;
  send_client_connect(conn, &conn->conn_data);
  lwpa_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
  lwpa_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
}

void start_tcp_connection(RdmnetConnection *conn, ConnCallbackDispatchInfo *cb)
{
  bool ok = true;
  RdmnetConnectFailedInfo *failed_info = &cb->args.connect_failed.failed_info;

  lwpa_error_t res = lwpa_socket(LWPA_AF_INET, LWPA_STREAM, &conn->sock);
  if (res != kLwpaErrOk)
  {
    ok = false;
    failed_info->socket_err = res;
  }

  if (ok)
  {
    res = lwpa_setblocking(conn->sock, false);
    if (res != kLwpaErrOk)
    {
      ok = false;
      failed_info->socket_err = res;
    }
  }

  if (ok)
  {
    conn->rdmnet_conn_failed = false;
    res = lwpa_connect(conn->sock, &conn->remote_addr);
    if (res == kLwpaErrOk)
    {
      // Fast connect condition
      start_rdmnet_connection(conn);
    }
    else if (res == kLwpaErrInProgress || res == kLwpaErrWouldBlock)
    {
      conn->state = kCSTCPConnPending;
    }
    else
    {
      ok = false;
      failed_info->socket_err = res;
    }
  }

  if (!ok)
  {
    cb->which = kConnCallbackConnectFailed;
    failed_info->event = kRdmnetConnectFailSocketFailure;
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

  // Remove any sockets marked for destruction.
  if (rdmnet_writelock())
  {
    RdmnetConnection *destroy_list = NULL;
    RdmnetConnection **next_destroy_list_entry = &destroy_list;

    LwpaRbIter conn_iter;
    lwpa_rbiter_init(&conn_iter);

    RdmnetConnection *conn = (RdmnetConnection *)lwpa_rbiter_first(&conn_iter, &state.connections);
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
      conn = lwpa_rbiter_next(&conn_iter);
    }

    // Now do the actual destruction
    if (destroy_list)
    {
      RdmnetConnection *to_destroy = destroy_list;
      while (to_destroy)
      {
        RdmnetConnection *next = to_destroy->next_to_destroy;
        destroy_connection(to_destroy);
        to_destroy = next;
      }
    }
    rdmnet_writeunlock();
  }

  // Do the rest of the periodic functionality with a read lock
  if (rdmnet_readlock())
  {
    LwpaRbIter conn_iter;
    lwpa_rbiter_init(&conn_iter);
    RdmnetConnection *conn = (RdmnetConnection *)lwpa_rbiter_first(&conn_iter, &state.connections);
    while (conn)
    {
      ConnCallbackDispatchInfo cb;
      cb.which = kConnCallbackNone;

      if (lwpa_mutex_take(&conn->lock, LWPA_WAIT_FOREVER))
      {
        cb.handle = conn->handle;
        cb.cbs = conn->callbacks;
        cb.context = conn->callback_context;

        switch (conn->state)
        {
          case kCSConnectPending:
            if (conn->rdmnet_conn_failed || conn->backoff_timer.interval != 0)
            {
              if (conn->rdmnet_conn_failed)
              {
                lwpa_timer_start(&conn->backoff_timer, update_backoff(conn->backoff_timer.interval));
              }
              conn->state = kCSBackoff;
            }
            else
            {
              start_tcp_connection(conn, &cb);
            }
            break;
          case kCSBackoff:
            if (lwpa_timer_isexpired(&conn->backoff_timer))
            {
              start_tcp_connection(conn, &cb);
            }
            break;
          case kCSHeartbeat:
            if (lwpa_timer_isexpired(&conn->hb_timer))
            {
              // Heartbeat timeout! Disconnect the connection.
              cb.which = kConnCallbackDisconnected;
              RdmnetDisconnectedInfo *disconn_info = &cb.args.disconnected.disconn_info;
              disconn_info->event = kRdmnetDisconnectNoHeartbeat;
              disconn_info->socket_err = kLwpaErrOk;

              reset_connection(conn);
            }
            else if (lwpa_timer_isexpired(&conn->send_timer))
            {
              // Just poll the send lock. If another context is in the middle of a partial message,
              // no need to block and send a heartbeat.
              if (lwpa_mutex_take(&conn->send_lock, 0))
              {
                send_null(conn);
                lwpa_timer_reset(&conn->send_timer);
                lwpa_mutex_give(&conn->send_lock);
              }
            }
            break;
          default:
            break;
        }
        lwpa_mutex_give(&conn->lock);
      }

      if (cb.which != kConnCallbackNone)
      {
        // Calling back inside the read lock is OK because API functions only take the read lock
        deliver_callback(&cb);
      }
      conn = lwpa_rbiter_next(&conn_iter);
    }
    rdmnet_readunlock();
  }
}

void handle_tcp_connect_result(RdmnetConnection *conn, const LwpaPollfd *result, ConnCallbackDispatchInfo *cb)
{
  (void)cb;

  if (result->revents & LWPA_POLLERR)
  {
    cb->which = kConnCallbackConnectFailed;

    RdmnetConnectFailedInfo *failed_info = &cb->args.connect_failed.failed_info;
    failed_info->event = kRdmnetConnectFailTcpLevel;
    failed_info->socket_err = result->err;

    reset_connection(conn);
  }
  else if (result->revents & LWPA_POLLOUT)
  {
    // connected successfully!
    start_rdmnet_connection(conn);
  }
}

void handle_rdmnet_connect_result(RdmnetConnection *conn, const LwpaPollfd *result, ConnCallbackDispatchInfo *cb)
{
  if (result->revents & LWPA_POLLERR)
  {
    cb->which = kConnCallbackConnectFailed;

    RdmnetConnectFailedInfo *failed_info = &cb->args.connect_failed.failed_info;
    failed_info->event = kRdmnetConnectFailTcpLevel;
    failed_info->socket_err = result->err;

    reset_connection(conn);
    conn->rdmnet_conn_failed = true;
  }
  else if (result->revents & LWPA_POLLIN)
  {
    if (kLwpaErrOk == rdmnet_msg_buf_recv(conn->sock, &conn->recv_buf))
    {
      RdmnetMessage *msg = &conn->recv_buf.msg;
      if (is_broker_msg(msg))
      {
        BrokerMessage *bmsg = get_broker_msg(msg);
        if (is_connect_reply_msg(bmsg))
        {
          ConnectReplyMsg *reply = get_connect_reply_msg(bmsg);
          switch (reply->connect_status)
          {
            case kRdmnetConnectOk:
              // TODO check version
              conn->state = kCSHeartbeat;
              lwpa_timer_start(&conn->backoff_timer, 0);
              cb->which = kConnCallbackConnected;

              RdmnetConnectedInfo *connect_info = &cb->args.connected.connect_info;
              connect_info->broker_uid = reply->broker_uid;
              connect_info->client_uid = reply->client_uid;
              connect_info->connected_addr = conn->remote_addr;
              break;
            default:
            {
              cb->which = kConnCallbackConnectFailed;

              RdmnetConnectFailedInfo *failed_info = &cb->args.connect_failed.failed_info;
              failed_info->event = kRdmnetConnectFailRejected;
              failed_info->socket_err = kLwpaErrOk;
              failed_info->rdmnet_reason = reply->connect_status;

              reset_connection(conn);
              conn->rdmnet_conn_failed = true;
              break;
            }
          }
        }
        else if (is_client_redirect_msg(bmsg))
        {
          conn->remote_addr = get_client_redirect_msg(bmsg)->new_addr;
          retry_connection(conn);
        }
      }

      free_rdmnet_message(msg);
    }
  }
}

void handle_rdmnet_data(RdmnetConnection *conn, const LwpaPollfd *result, ConnCallbackDispatchInfo *cb)
{
  if (result->revents & LWPA_POLLERR)
  {
    cb->which = kConnCallbackDisconnected;

    RdmnetDisconnectedInfo *disconn_info = &cb->args.disconnected.disconn_info;
    disconn_info->event = kRdmnetDisconnectSocketFailure;
    disconn_info->socket_err = result->err;

    reset_connection(conn);
  }
  else if (result->revents & LWPA_POLLIN)
  {
    RdmnetMsgBuf *msgbuf = &conn->recv_buf;
    lwpa_error_t res = rdmnet_msg_buf_recv(conn->sock, msgbuf);
    if (res == kLwpaErrOk)
    {
      // We've received something on this connection. Reset the heartbeat timer.
      lwpa_timer_reset(&conn->hb_timer);

      // We handle some Broker messages internally
      bool deliver_message = false;
      if (is_broker_msg(&msgbuf->msg))
      {
        BrokerMessage *bmsg = get_broker_msg(&msgbuf->msg);
        switch (bmsg->vector)
        {
          case VECTOR_BROKER_CONNECT_REPLY:
          case VECTOR_BROKER_NULL:
            break;
          case VECTOR_BROKER_DISCONNECT:
            cb->which = kConnCallbackDisconnected;

            RdmnetDisconnectedInfo *disconn_info = &cb->args.disconnected.disconn_info;
            disconn_info->event = kRdmnetDisconnectGracefulRemoteInitiated;
            disconn_info->socket_err = kLwpaErrOk;
            disconn_info->rdmnet_reason = get_disconnect_msg(bmsg)->disconnect_reason;

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
        cb->args.msg_received.message = msgbuf->msg;
      }
    }
    else if (res != kLwpaErrNoData)
    {
      cb->which = kConnCallbackDisconnected;

      RdmnetDisconnectedInfo *disconn_info = &cb->args.disconnected.disconn_info;
      disconn_info->event = kRdmnetDisconnectSocketFailure;
      disconn_info->socket_err = res;

      reset_connection(conn);
    }
  }
}

void deliver_callback(ConnCallbackDispatchInfo *info)
{
  switch (info->which)
  {
    case kConnCallbackConnected:
      info->cbs.connected(info->handle, &info->args.connected.connect_info, info->context);
      break;
    case kConnCallbackConnectFailed:
      info->cbs.connect_failed(info->handle, &info->args.connect_failed.failed_info, info->context);
      break;
    case kConnCallbackDisconnected:
      info->cbs.disconnected(info->handle, &info->args.disconnected.disconn_info, info->context);
      break;
    case kConnCallbackMsgReceived:
      info->cbs.msg_received(info->handle, &info->args.msg_received.message, info->context);
      free_rdmnet_message(&info->args.msg_received.message);
      break;
    case kConnCallbackNone:
    default:
      break;
  }
}

#if RDMNET_POLL_CONNECTIONS_INTERNALLY

void rdmnet_connection_recv()
{
  static LwpaPollfd poll_arr[RDMNET_CONN_MAX_SOCKETS];
  static rdmnet_conn_t conn_arr[RDMNET_CONN_MAX_SOCKETS];
  size_t num_to_poll = 0;

  if (rdmnet_readlock())
  {
    LwpaRbIter iter;
    lwpa_rbiter_init(&iter);

    RdmnetConnection *conn = lwpa_rbiter_first(&iter, &state.connections);
    while (conn)
    {
      if (conn->state == kCSTCPConnPending || conn->state == kCSRDMnetConnPending || conn->state == kCSHeartbeat)
      {
        if (conn->state == kCSTCPConnPending)
          poll_arr[num_to_poll].events = LWPA_POLLOUT;
        else
          poll_arr[num_to_poll].events = LWPA_POLLIN;
        poll_arr[num_to_poll].fd = conn->sock;
        conn_arr[num_to_poll] = conn->handle;
        ++num_to_poll;
      }
      conn = lwpa_rbiter_next(&iter);
    }
    rdmnet_readunlock();
  }

  if (num_to_poll > 0)
  {
    int poll_res = lwpa_poll(poll_arr, num_to_poll, RDMNET_CONN_POLL_TIMEOUT);
    if (poll_res > 0)
    {
      size_t read_index = 0;
      while (poll_res > 0 && read_index < num_to_poll)
      {
        if (poll_arr[read_index].revents)
        {
          rdmnet_conn_socket_activity(conn_arr[read_index], &poll_arr[read_index]);
          --poll_res;
        }
        ++read_index;
      }
    }
    else if (poll_res != kLwpaErrTimedOut)
    {
      lwpa_log(rdmnet_log_params, LWPA_LOG_ERR, RDMNET_LOG_MSG("Error ('%s') while polling sockets."),
               lwpa_strerror(poll_res));
      lwpa_thread_sleep(1000);  // Sleep to avoid spinning on errors
    }
  }
  else
  {
    // Prevent the calling thread from spinning continuously
    lwpa_thread_sleep(RDMNET_CONN_POLL_TIMEOUT);
  }
}

#endif

void rdmnet_conn_socket_activity(rdmnet_conn_t handle, const LwpaPollfd *poll)
{
  RdmnetConnection *conn;
  if (handle < 0 || kLwpaErrOk != get_conn(handle, &conn))
    return;

#if RDMNET_POLL_CONNECTIONS_INTERNALLY
  static ConnCallbackDispatchInfo callback_info;
#else
  ConnCallbackDispatchInfo callback_info;
#endif
  callback_info.which = kConnCallbackNone;
  callback_info.handle = handle;
  callback_info.cbs = conn->callbacks;
  callback_info.context = conn->callback_context;

  switch (conn->state)
  {
    case kCSTCPConnPending:
      handle_tcp_connect_result(conn, poll, &callback_info);
      break;
    case kCSRDMnetConnPending:
      handle_rdmnet_connect_result(conn, poll, &callback_info);
      break;
    case kCSHeartbeat:
      handle_rdmnet_data(conn, poll, &callback_info);
      break;
    case kCSConnectNotStarted:
    case kCSConnectPending:
    case kCSBackoff:
    case kCSMarkedForDestruction:
    default:
      // States in which we are not waiting for socket activity
      break;
  }

  release_conn(conn);
  deliver_callback(&callback_info);
}

/* Get a new connection handle to assign to a new connection.
 * Must have write lock.
 */
rdmnet_conn_t get_new_conn_handle()
{
  rdmnet_conn_t new_handle = state.next_handle;
  if (++state.next_handle < 0)
  {
    state.next_handle = 0;
    state.handle_has_wrapped_around = true;
  }
  // Optimization - keep track of whether the handle counter has wrapped around.
  // If not, we don't need to check if the new handle is in use.
  if (state.handle_has_wrapped_around)
  {
    // We have wrapped around at least once, we need to check for handles in use
    rdmnet_conn_t original = new_handle;
    while (lwpa_rbtree_find(&state.connections, &new_handle))
    {
      if (state.next_handle == original)
      {
        // Incredibly unlikely case of all handles used
        new_handle = RDMNET_CONN_INVALID;
        break;
      }
      new_handle = state.next_handle;
      if (++state.next_handle < 0)
        state.next_handle = 0;
    }
  }
  return new_handle;
}

/* Internal function which attempts to allocate and track a new connection, including allocating the
 * structure, creating a new handle value, and inserting it into the global map.
 *
 *  Must have write lock.
 */
RdmnetConnection *create_new_connection(const RdmnetConnectionConfig *config)
{
  rdmnet_conn_t new_handle = get_new_conn_handle();
  if (new_handle == RDMNET_CONN_INVALID)
    return NULL;

  RdmnetConnection *conn = alloc_rdmnet_connection();
  if (conn)
  {
    bool ok;
    bool lock_created = false;
    bool send_lock_created = false;

    // Try to create the locks and signal
    ok = lock_created = lwpa_mutex_create(&conn->lock);
    if (ok)
    {
      ok = send_lock_created = lwpa_mutex_create(&conn->send_lock);
    }
    if (ok)
    {
      conn->handle = new_handle;
      ok = (0 != lwpa_rbtree_insert(&state.connections, conn));
    }
    if (ok)
    {
      conn->local_cid = config->local_cid;
      conn->sock = LWPA_SOCKET_INVALID;
      lwpaip_set_invalid(&conn->remote_addr.ip);
      conn->remote_addr.port = 0;
      conn->is_client = true;
      conn->is_blocking = true;
      conn->state = kCSConnectNotStarted;
      lwpa_timer_start(&conn->backoff_timer, 0);
      conn->rdmnet_conn_failed = false;
      rdmnet_msg_buf_init(&conn->recv_buf);
      conn->callbacks = config->callbacks;
      conn->callback_context = config->callback_context;
      conn->next_to_destroy = NULL;
    }
    else
    {
      // Clean up
      if (send_lock_created)
        lwpa_mutex_destroy(&conn->send_lock);
      if (lock_created)
        lwpa_mutex_destroy(&conn->lock);
      free_rdmnet_connection(conn);
      conn = NULL;
    }
  }
  return conn;
}

/* Internal function to update a backoff timer value using the algorithm specified in E1.33. Returns
 * the new value. */
int update_backoff(int previous_backoff)
{
  int result = ((rand() % 4001) + 1000);
  result += previous_backoff;
  /* 30 second interval is the max */
  if (result > 30000)
    return 30000;
  return result;
}

void reset_connection(RdmnetConnection *conn)
{
  if (conn->sock != LWPA_SOCKET_INVALID)
  {
    lwpa_close(conn->sock);
    conn->sock = LWPA_SOCKET_INVALID;
  }
  conn->state = kCSConnectNotStarted;
}

void retry_connection(RdmnetConnection *conn)
{
  if (conn->sock != LWPA_SOCKET_INVALID)
  {
    lwpa_close(conn->sock);
    conn->sock = LWPA_SOCKET_INVALID;
  }
  conn->state = kCSConnectPending;
}

void destroy_connection(RdmnetConnection *conn)
{
  if (conn)
  {
    if (conn->sock != LWPA_SOCKET_INVALID)
      lwpa_close(conn->sock);
    lwpa_mutex_destroy(&conn->lock);
    lwpa_mutex_destroy(&conn->send_lock);
    free_rdmnet_connection(conn);
  }
}

lwpa_error_t get_conn(rdmnet_conn_t handle, RdmnetConnection **conn)
{
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;
  if (!rdmnet_readlock())
    return kLwpaErrSys;

  RdmnetConnection *found_conn = (RdmnetConnection *)lwpa_rbtree_find(&state.connections, &handle);
  if (!found_conn)
  {
    rdmnet_readunlock();
    return kLwpaErrNotFound;
  }
  if (!lwpa_mutex_take(&found_conn->lock, LWPA_WAIT_FOREVER))
  {
    rdmnet_readunlock();
    return kLwpaErrSys;
  }
  *conn = found_conn;
  return kLwpaErrOk;
}

void release_conn(RdmnetConnection *conn)
{
  lwpa_mutex_give(&conn->lock);
  rdmnet_readunlock();
}

int conn_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;

  RdmnetConnection *a = (RdmnetConnection *)node_a->value;
  RdmnetConnection *b = (RdmnetConnection *)node_b->value;

  return a->handle - b->handle;
}

LwpaRbNode *conn_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode *)malloc(sizeof(LwpaRbNode));
#else
  return lwpa_mempool_alloc(rdmnet_conn_rb_nodes);
#endif
}

void conn_node_free(LwpaRbNode *node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  lwpa_mempool_free(rdmnet_conn_rb_nodes, node);
#endif
}
