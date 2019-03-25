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

/***************************** Private macros ********************************/

#define release_conn_and_readlock(connptr) \
  do                                       \
  {                                        \
    lwpa_mutex_give(&(connptr)->lock);     \
    rdmnet_readunlock();                   \
  } while (0)

#define release_conn_and_writelock(connptr) rdmnet_writeunlock()

/* Macros for dynamic vs static allocation. Static allocation is done using lwpa_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_connection() malloc(sizeof(RdmnetConnectionInternal))
#define free_rdmnet_connection(ptr) free(ptr)
#else
#define alloc_rdmnet_connection() lwpa_mempool_alloc(rdmnet_connections)
#define free_rdmnet_connection(ptr) lwpa_mempool_free(rdmnet_connections, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(rdmnet_connections, RdmnetConnectionInternal, RDMNET_MAX_CONNECTIONS);
#endif

static struct ConnectionState
{
  RdmnetConnectionInternal *connections;
} conn_state;

/*********************** Private function prototypes *************************/

static lwpa_error_t get_readlock_and_conn(rdmnet_conn_t handle);
static lwpa_error_t get_writelock_and_conn(rdmnet_conn_t handle);

static RdmnetConnectionInternal *create_new_connection(const RdmnetConnectionConfig *config);

static int update_backoff(int previous_backoff);
static void deliver_callback(const CallbackDispatchInfo *info);
static void start_tcp_connection(RdmnetConnectionInternal *conn);
static void start_rdmnet_connection(RdmnetConnectionInternal *conn);
static void reset_connection(RdmnetConnectionInternal *conn);
static void destroy_connection(RdmnetConnectionInternal *conn);

/*************************** Function definitions ****************************/

/*! \brief Initialize the RDMnet Connection module.
 *
 *  Do all necessary initialization before other RDMnet Connection API functions can be called.
 *
 *  \return #LWPA_OK: Initialization successful.\n
 *          #LWPA_SYSERR: An internal library of system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_connection_init()
{
  lwpa_error_t res = LWPA_OK;

#if !RDMNET_DYNAMIC_MEM
  /* Init memory pools */
  res |= lwpa_mempool_init(rdmnet_connections);
#endif

  return res;
}

void destroy_connection(RdmnetConnectionInternal *conn)
{
  if (conn)
  {
    lwpa_close(conn->sock);
    lwpa_mutex_destroy(&conn->lock);
    lwpa_mutex_destroy(&conn->send_lock);
    free_rdmnet_connection(conn);
  }
}

/*! \brief Deinitialize the RDMnet Connection module.
 *
 *  Set the RDMnet Connection module back to an uninitialized state. All existing connections will
 *  be closed/disconnected. Calls to other RDMnet Connection API functions will fail until
 *  rdmnet_init() is called again.
 */
void rdmnet_connection_deinit()
{
  RdmnetConnectionInternal *to_destroy = conn_state.connections;
  RdmnetConnectionInternal *next;
  while (to_destroy)
  {
    next = to_destroy->next;
    destroy_connection(to_destroy);
    to_destroy = next;
  }
  memset(&conn_state, 0, sizeof conn_state);
}

/*! \brief Create a new handle to use for an RDMnet Connection.
 *
 *  This function simply allocates a connection handle - use rdmnet_connect() to actually start the
 *  connection process.
 *
 *  \param[in] config Configuration parameters for the connection to be created.
 *  \param[out] handle Handle to the newly-created connection
 *  \return #LWPA_OK: Handle created successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOMEM: No room to allocate additional connection.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_new_connection(const RdmnetConnectionConfig *config, rdmnet_conn_t *handle)
{
  if (!config || !handle)
    return LWPA_INVALID;
  if (!rdmnet_core_initialized())
    return LWPA_NOTINIT;

  lwpa_error_t res = LWPA_SYSERR;
  if (rdmnet_writelock())
  {
    res = LWPA_OK;
    /* Passed the quick checks, try to create a struct to represent a new connection. This function
     * creates the new connection, gives it a unique handle and inserts it into the connection map. */
    RdmnetConnectionInternal *conn = create_new_connection(config);
    if (!conn)
      res = LWPA_NOMEM;

    if (res == LWPA_OK)
    {
      // Append the new connection to the list
      RdmnetConnectionInternal **conn_ptr = &conn_state.connections;
      while (*conn_ptr)
        conn_ptr = &(*conn_ptr)->next;
      *conn_ptr = conn;
      *handle = conn;
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
 *  \return #LWPA_OK: Connection completed successfully.\n
 *          #LWPA_INPROGRESS: Non-blocking connection started.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOTFOUND: Connection handle not previously created.\n
 *          #LWPA_ISCONN: Already connected on this handle.\n
 *          #LWPA_TIMEDOUT: Timed out waiting for connection handshake to complete.\n
 *          #LWPA_CONNREFUSED: Connection refused either at the TCP or RDMnet level. additional_data
 *                             may contain a reason code.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_connect(rdmnet_conn_t handle, const LwpaSockaddr *remote_addr, const ClientConnectMsg *connect_data)
{
  if (!remote_addr || !connect_data)
    return LWPA_INVALID;

  lwpa_error_t res = get_readlock_and_conn(handle);
  if (res != LWPA_OK)
    return res;

  if (handle->state != kCSConnectNotStarted)
    res = LWPA_ISCONN;

  if (res == LWPA_OK)
  {
    handle->remote_addr = *remote_addr;
    handle->conn_data = *connect_data;
    handle->state = kCSConnectPending;
  }

  release_conn_and_readlock(handle);
  return res;
}

/*! \brief Set an RDMnet connection handle to be either blocking or non-blocking.
 *
 *  The blocking state of a connection controls how other API calls behave. If a connection is:
 *
 *  * Blocking:
 *    - rdmnet_send() and related functions will block until all data is sent.
 *  * Non-blocking:
 *    - rdmnet_send() will return immediately with error code #LWPA_WOULDBLOCK if there is too much
 *      data to fit in the underlying send buffer.
 *
 *  \param[in] handle Connection handle for which to change blocking state.
 *  \param[in] blocking Whether the connection should be blocking.
 *  \return #LWPA_OK: Blocking state was changed successfully.\n
 *          #LWPA_INVALID: Invalid connection handle.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_BUSY: A connection is currently in progress.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_set_blocking(rdmnet_conn_t handle, bool blocking)
{
  lwpa_error_t res = get_readlock_and_conn(handle);
  if (res != LWPA_OK)
    return res;

  if (!(handle->state == kCSConnectNotStarted || handle->state == kCSHeartbeat))
  {
    /* Can't change the blocking state while a connection is in progress. */
    release_conn_and_readlock(handle);
    return LWPA_BUSY;
  }

  if (handle->state == kCSHeartbeat)
  {
    res = lwpa_setblocking(handle->sock, blocking);
    if (res == LWPA_OK)
      handle->is_blocking = blocking;
  }
  else
  {
    /* State is NotConnected, just change the flag */
    handle->is_blocking = blocking;
  }
  release_conn_and_readlock(handle);
  return res;
}

/*! \brief ADVANCED USAGE: Attach an RDMnet connection handle to an already-connected system socket.
 *
 *  This function is typically only used by %Brokers. The RDMnet connection is assumed to have
 *  already completed and be at the Heartbeat stage.
 *
 *  \param[in] handle Connection handle to attach the socket to. Must have been previously created
 *                    using rdmnet_new_connection().
 *  \param[in] sock System socket to attach to the connection handle. Must be an already-connected
 *                  stream socket.
 *  \param[in] remote_addr The remote network address to which the socket is currently connected.
 *  \return #LWPA_OK: Socket was attached successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_ISCONN: The connection handle provided is already connected using another socket.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_attach_existing_socket(rdmnet_conn_t handle, lwpa_socket_t sock, const LwpaSockaddr *remote_addr)
{
  if (sock == LWPA_SOCKET_INVALID || !remote_addr)
    return LWPA_INVALID;

  lwpa_error_t res = get_readlock_and_conn(handle);
  if (res == LWPA_OK)
  {
    if (handle->state != kCSConnectNotStarted)
    {
      res = LWPA_ISCONN;
    }
    else
    {
      handle->sock = sock;
      handle->remote_addr = *remote_addr;
      handle->state = kCSHeartbeat;
      handle->is_client = false;
      lwpa_timer_start(&handle->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
      lwpa_timer_start(&handle->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
    }
    release_conn_and_readlock(handle);
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
 *  \return #LWPA_OK: Connection was successfully destroyed\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_destroy_connection(rdmnet_conn_t handle, const rdmnet_disconnect_reason_t *disconnect_reason)
{
  lwpa_error_t res = get_readlock_and_conn(handle);
  if (res != LWPA_OK)
    return res;

  if (handle->state == kCSHeartbeat && disconnect_reason)
  {
    DisconnectMsg dm;
    dm.disconnect_reason = *disconnect_reason;
    send_disconnect(handle, &dm);
  }
  handle->state = kCSMarkedForDestruction;
  lwpa_shutdown(handle->sock, LWPA_SHUT_WR);
  lwpa_close(handle->sock);
  handle->sock = LWPA_SOCKET_INVALID;

  if (handle->sock != LWPA_SOCKET_INVALID)
    lwpa_close(handle->sock);
  lwpa_mutex_destroy(&handle->lock);
  lwpa_mutex_destroy(&handle->send_lock);

  /* TODO remove from list and free
  lwpa_rbtree_remove(&conn_.handleections, handle);
  free_rdmnet_handleection(handle);
  */

  release_conn_and_writelock(handle);
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
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOTCONN: The connection handle has not been successfully connected.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
int rdmnet_send(rdmnet_conn_t handle, const uint8_t *data, size_t size)
{
  if (data || size == 0)
    return LWPA_INVALID;

  int res = get_readlock_and_conn(handle);
  if (res == LWPA_OK)
  {
    if (handle->state != kCSHeartbeat)
      res = LWPA_NOTCONN;
    lwpa_mutex_give(&handle->lock);
  }

  if (res == LWPA_OK && lwpa_mutex_take(&handle->send_lock, LWPA_WAIT_FOREVER))
  {
    res = lwpa_send(handle->sock, data, size, 0);
    lwpa_mutex_give(&handle->send_lock);
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
 *  \return #LWPA_OK: Send operation started successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOTCONN: The connection handle has not been successfully connected.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_start_message(rdmnet_conn_t handle)
{
  lwpa_error_t res = get_readlock_and_conn(handle);
  if (res == LWPA_OK)
  {
    if (handle->state != kCSHeartbeat)
      res = LWPA_NOTCONN;
    lwpa_mutex_give(&handle->lock);
  }

  if (res == LWPA_OK)
  {
    if (lwpa_mutex_take(&handle->send_lock, LWPA_WAIT_FOREVER))
    {
      /* Return, keeping the readlock and the send lock. */
      return res;
    }
    else
    {
      res = LWPA_SYSERR;
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
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOTCONN: The connection handle has not been successfully connected.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
int rdmnet_send_partial_message(rdmnet_conn_t handle, const uint8_t *data, size_t size)
{
  if (!data || size == 0)
    return LWPA_INVALID;

  int res = get_readlock_and_conn(handle);
  if (res == LWPA_OK)
  {
    if (handle->state != kCSHeartbeat)
      res = LWPA_NOTCONN;
    lwpa_mutex_give(&handle->lock);
  }

  if (res == LWPA_OK)
    res = lwpa_send(handle->sock, data, size, 0);

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
 *  \return #LWPA_OK: Send operation ended successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_end_message(rdmnet_conn_t handle)
{
  lwpa_error_t res = get_readlock_and_conn(handle);
  if (res == LWPA_OK)
  {
    /* Release the send lock and the read lock that we had before. */
    lwpa_mutex_give(&handle->lock);
    lwpa_mutex_give(&handle->send_lock);
    rdmnet_readunlock();
  }
  /* And release the read lock that we took at the beginning of this function. */
  rdmnet_readunlock();
  return res;
}

void start_rdmnet_connection(RdmnetConnectionInternal *conn)
{
  if (conn->is_blocking)
    lwpa_setblocking(conn->sock, true);

  // Update state
  conn->state = kCSRDMnetConnPending;
  send_client_connect(conn, &conn->conn_data);
  lwpa_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
  lwpa_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
}

void start_tcp_connection(RdmnetConnectionInternal *conn)
{
  conn->sock = lwpa_socket(LWPA_AF_INET, LWPA_STREAM);
  if (conn->sock == LWPA_SOCKET_INVALID)
  {
    // TODO log
    return;
  }
  lwpa_error_t res = lwpa_setblocking(conn->sock, false);
  if (res != LWPA_OK)
  {
    // TODO log
    lwpa_close(conn->sock);
    return;
  }

  conn->rdmnet_conn_failed = false;
  res = lwpa_connect(conn->sock, &conn->remote_addr);
  if (res == LWPA_OK)
  {
    // Fast connect condition
    start_rdmnet_connection(conn);
  }
  else if (res == LWPA_INPROGRESS || res == LWPA_WOULDBLOCK)
  {
    conn->state = kCSTCPConnPending;
  }
  else
  {
    // TODO log
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
    RdmnetConnectionInternal *conn = conn_state.connections;
    RdmnetConnectionInternal *prev_conn = NULL;
    while (conn)
    {
      RdmnetConnectionInternal *next_conn = conn->next;
      if (conn->state == kCSMarkedForDestruction)
      {
        if (prev_conn)
          prev_conn->next = next_conn;
        else
          conn_state.connections = next_conn;
        destroy_connection(conn);
      }
      else
      {
        prev_conn = conn;
      }
      conn = next_conn;
    }
    rdmnet_writeunlock();
  }

  // Do the rest of the periodic functionality with a read lock
  if (rdmnet_readlock())
  {
    RdmnetConnectionInternal *conn = conn_state.connections;
    while (conn)
    {
      CallbackDispatchInfo cb;
      cb.which = kConnCallbackNone;

      if (lwpa_mutex_take(&conn->lock, LWPA_WAIT_FOREVER))
      {
        cb.handle = conn;
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
              start_tcp_connection(conn);
            }
            break;
          case kCSBackoff:
            if (lwpa_timer_isexpired(&conn->backoff_timer))
            {
              start_tcp_connection(conn);
            }
            break;
          case kCSHeartbeat:
            if (lwpa_timer_isexpired(&conn->hb_timer))
            {
              /* Heartbeat timeout! Disconnect the connection. */
              reset_connection(conn);
              // TODO log
              cb.which = kConnCallbackDisconnected;
              RdmnetDisconnectInfo *disconn_info = &cb.args.disconnected.disconn_info;
              disconn_info->cause = kRdmnetDisconnectNoHeartbeat;
              disconn_info->socket_err = LWPA_OK;
            }
            else if (lwpa_timer_isexpired(&conn->send_timer))
            {
              /* Just poll the send lock. If another context is in the middle of a partial message,
               * no need to block and send a heartbeat. */
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
      conn = conn->next;
    }
    rdmnet_readunlock();
  }
}

void handle_tcp_connect_result(RdmnetConnectionInternal *conn, const LwpaPollfd *result, CallbackDispatchInfo *cb)
{
  (void)cb;

  if (result->revents & LWPA_POLLERR)
  {
    // TODO Log
    reset_connection(conn);
  }
  else if (result->revents & LWPA_POLLOUT)
  {
    // connected successfully!
    start_rdmnet_connection(conn);
  }
}

void handle_rdmnet_connect_result(RdmnetConnectionInternal *conn, const LwpaPollfd *result, CallbackDispatchInfo *cb)
{
  if (result->revents & LWPA_POLLERR)
  {
    reset_connection(conn);
    conn->rdmnet_conn_failed = true;
    // TODO log
  }
  else if (result->revents & LWPA_POLLIN)
  {
    if (LWPA_OK == rdmnet_msg_buf_recv(conn->sock, &conn->recv_buf))
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
              // TODO check version and Broker UID
              conn->state = kCSHeartbeat;
              lwpa_timer_start(&conn->backoff_timer, 0);
              cb->which = kConnCallbackConnected;
              break;
            default:
              reset_connection(conn);
              conn->rdmnet_conn_failed = true;
              conn->state = kCSConnectPending;
              // TODO Log
              break;
          }
        }
        else if (is_client_redirect_msg(bmsg))
        {
          conn->remote_addr = get_client_redirect_msg(bmsg)->new_addr;
          reset_connection(conn);
        }
      }
    }
  }
}

void handle_rdmnet_data(RdmnetConnectionInternal *conn, const LwpaPollfd *result, CallbackDispatchInfo *cb)
{
  if (result->revents & LWPA_POLLERR)
  {
    cb->which = kConnCallbackDisconnected;

    RdmnetDisconnectInfo *disconn_info = &cb->args.disconnected.disconn_info;
    disconn_info->cause = kRdmnetDisconnectSocketFailure;
    disconn_info->socket_err = result->err;

    reset_connection(conn);
  }
  else if (result->revents & LWPA_POLLIN)
  {
    RdmnetMsgBuf *msgbuf = &conn->recv_buf;
    lwpa_error_t res = rdmnet_msg_buf_recv(conn->sock, msgbuf);
    if (res == LWPA_OK)
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

            RdmnetDisconnectInfo *disconn_info = &cb->args.disconnected.disconn_info;
            disconn_info->cause = kRdmnetDisconnectGracefulRemoteInitiated;
            disconn_info->socket_err = LWPA_OK;
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
    else if (res != LWPA_NODATA)
    {
      cb->which = kConnCallbackDisconnected;

      RdmnetDisconnectInfo *disconn_info = &cb->args.disconnected.disconn_info;
      disconn_info->cause = kRdmnetDisconnectSocketFailure;
      disconn_info->socket_err = res;

      reset_connection(conn);
    }
  }
}

void deliver_callback(const CallbackDispatchInfo *info)
{
  switch (info->which)
  {
    case kConnCallbackConnected:
      info->cbs.connected(info->handle, info->context);
      break;
    case kConnCallbackDisconnected:
      info->cbs.disconnected(info->handle, &info->args.disconnected.disconn_info, info->context);
      break;
    case kConnCallbackMsgReceived:
      info->cbs.msg_received(info->handle, &info->args.msg_received.message, info->context);
      break;
    case kConnCallbackNone:
    default:
      break;
  }
}

size_t rdmnet_connection_get_sockets(LwpaPollfd *poll_arr, void **context_arr)
{
  size_t res = 0;

  if (rdmnet_readlock())
  {
    for (RdmnetConnectionInternal *conn = conn_state.connections; conn; conn = conn->next)
    {
      if (conn->state == kCSTCPConnPending || conn->state == kCSRDMnetConnPending || conn->state == kCSHeartbeat)
      {
        if (conn->state == kCSTCPConnPending)
          poll_arr[res].events = LWPA_POLLOUT;
        else
          poll_arr[res].events = LWPA_POLLIN;
        poll_arr[res].fd = conn->sock;
        context_arr[res] = conn;
        ++res;
      }
    }
    rdmnet_readunlock();
  }
  return res;
}

void rdmnet_connection_socket_activity(const LwpaPollfd *poll, void *context)
{
  RdmnetConnectionInternal *conn = (RdmnetConnectionInternal *)context;
  if (!conn || LWPA_OK != get_readlock_and_conn(conn))
    return;

  static CallbackDispatchInfo callback_info;
  callback_info.which = kConnCallbackNone;
  callback_info.handle = conn;
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
      break;
  }

  release_conn_and_readlock(conn);
  deliver_callback(&callback_info);
}

/* Internal function which attempts to allocate and track a new connection, including allocating the
 * structure, creating a new handle value, and inserting it into the global map.
 *
 *  Must have write lock.
 */
RdmnetConnectionInternal *create_new_connection(const RdmnetConnectionConfig *config)
{
  RdmnetConnectionInternal *conn = alloc_rdmnet_connection();
  if (conn)
  {
    bool ok;
    bool lock_created = false;
    bool send_lock_created = false;

    // Try to create the locks and signal
    ok = lock_created = lwpa_mutex_create(&conn->lock);
    if (ok)
      ok = send_lock_created = lwpa_mutex_create(&conn->send_lock);
    if (ok)
    {
      conn->local_cid = config->local_cid;
      conn->sock = LWPA_SOCKET_INVALID;
      lwpaip_set_v4_address(&conn->remote_addr.ip, 0);
      conn->remote_addr.port = 0;
      conn->is_client = true;
      conn->is_blocking = true;
      conn->state = kCSConnectNotStarted;
      lwpa_timer_start(&conn->backoff_timer, 0);
      conn->rdmnet_conn_failed = false;
      conn->recv_disconn_err = LWPA_TIMEDOUT;
      rdmnet_msg_buf_init(&conn->recv_buf, rdmnet_log_params);
      conn->callbacks = config->callbacks;
      conn->callback_context = config->callback_context;
      conn->next = NULL;
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

void reset_connection(RdmnetConnectionInternal *conn)
{
  if (conn->sock != LWPA_SOCKET_INVALID)
  {
    lwpa_close(conn->sock);
    conn->sock = LWPA_SOCKET_INVALID;
  }
  conn->state = kCSConnectPending;
}

lwpa_error_t get_readlock_and_conn(RdmnetConnectionInternal *conn)
{
  if (!conn)
    return LWPA_INVALID;
  if (!rdmnet_core_initialized())
    return LWPA_NOTINIT;
  if (!rdmnet_readlock())
    return LWPA_SYSERR;

  bool found = false;
  for (RdmnetConnectionInternal *conn_entry = conn_state.connections; conn_entry; conn_entry = conn_entry->next)
  {
    if (conn_entry == conn)
    {
      found = true;
      break;
    }
  }
  if (!found)
  {
    rdmnet_readunlock();
    return LWPA_NOTFOUND;
  }
  if (!lwpa_mutex_take(&conn->lock, LWPA_WAIT_FOREVER))
  {
    rdmnet_readunlock();
    return LWPA_SYSERR;
  }
  return LWPA_OK;
}

lwpa_error_t get_writelock_and_conn(RdmnetConnectionInternal *conn)
{
  if (!rdmnet_core_initialized())
    return LWPA_NOTINIT;
  if (!rdmnet_writelock())
    return LWPA_SYSERR;

  bool found = false;
  for (RdmnetConnectionInternal *conn_entry = conn_state.connections; conn_entry; conn_entry = conn_entry->next)
  {
    if (conn_entry == conn)
    {
      found = true;
      break;
    }
  }
  if (!found)
  {
    rdmnet_writeunlock();
    return LWPA_NOTFOUND;
  }

  /* Taking the global write lock means we don't have to take the conn mutex. */
  return LWPA_OK;
}
