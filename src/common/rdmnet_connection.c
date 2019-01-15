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

#include "rdmnet/common/connection.h"

#include "rdmnet_opts.h"
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "lwpa/mempool.h"
#endif
#if RDMNET_USE_TICK_THREAD
#include "lwpa/thread.h"
#endif
#include "lwpa/lock.h"
#include "lwpa/rbtree.h"
#include "lwpa/socket.h"
#include "rdmnet/defs.h"
#include "rdmnet/common/message.h"
#include "rdmnet_message_priv.h"
#include "rdmnet_conn_priv.h"
#include "broker_prot_priv.h"

/************************* The draft warning message *************************/

/* clang-format off */
#pragma message("************ THIS CODE IMPLEMENTS A DRAFT STANDARD ************")
#pragma message("*** PLEASE DO NOT INCLUDE THIS CODE IN ANY SHIPPING PRODUCT ***")
#pragma message("************* SEE THE README FOR MORE INFORMATION *************")
/* clang-format on */

/*************************** Private constants *******************************/

/* When waiting on the backoff timer for a new connection, the interval at which to wake up and make
 * sure that we haven't been deinitted/closed. */
#define BLOCKING_BACKOFF_WAIT_INTERVAL 500

/***************************** Private macros ********************************/

#define rdmnet_create_lock_or_die()       \
  if (!rdmnet_lock_initted)               \
  {                                       \
    if (lwpa_rwlock_create(&rdmnet_lock)) \
      rdmnet_lock_initted = true;         \
    else                                  \
      return LWPA_SYSERR;                 \
  }
#define rdmnet_readlock() lwpa_rwlock_readlock(&rdmnet_lock, LWPA_WAIT_FOREVER)
#define rdmnet_readunlock() lwpa_rwlock_readunlock(&rdmnet_lock)
#define rdmnet_writelock() lwpa_rwlock_writelock(&rdmnet_lock, LWPA_WAIT_FOREVER)
#define rdmnet_writeunlock() lwpa_rwlock_writeunlock(&rdmnet_lock)

#define release_conn_and_readlock(connptr) \
  do                                       \
  {                                        \
    lwpa_mutex_give(&(connptr)->lock);     \
    rdmnet_readunlock();                   \
  } while (0)

#define release_conn_and_writelock(connptr) rdmnet_writeunlock()

/* Macros for dynamic vs static allocation. Static allocation is done using lwpa_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_connection() malloc(sizeof(RdmnetConnection))
#define free_rdmnet_connection(ptr) free(ptr)
#else
#define alloc_rdmnet_connection() lwpa_mempool_alloc(rdmnet_connections)
#define free_rdmnet_connection(ptr) lwpa_mempool_free(rdmnet_connections, ptr)
#endif

/**************************** Private variables ******************************/

/* clang-format off */
#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(rdmnet_connections, RdmnetConnection, RDMNET_MAX_CONNECTIONS);
LWPA_MEMPOOL_DEFINE(rdmnet_lwpa_rbnodes, LwpaRbNode, RDMNET_MAX_CONNECTIONS);
#endif
/* clang-format on */

static bool rdmnet_lock_initted = false;
static lwpa_rwlock_t rdmnet_lock;

static struct rc_state
{
  bool initted;
  const LwpaLogParams *log_params;

  LwpaRbTree connections;
  int next_conn_handle;

  lwpa_mutex_t poll_lock;
  lwpa_signal_t conn_sig;

#if RDMNET_USE_TICK_THREAD
  bool tickthread_run;
  lwpa_thread_t tick_thread;
#endif
} rc_state;

/*********************** Private function prototypes *************************/

#if RDMNET_USE_TICK_THREAD
static void rdmnet_tick_thread(void *arg);
#endif
static LwpaRbNode *node_alloc();
static void node_dealloc(LwpaRbNode *node);
static int conn_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static lwpa_error_t get_conn(int handle, RdmnetConnection **conn_ptr);
static lwpa_error_t get_readlock_and_conn(int handle, RdmnetConnection **conn_ptr);
static lwpa_error_t get_writelock_and_conn(int handle, RdmnetConnection **conn);

static RdmnetConnection *create_new_connection();

static int update_backoff(int previous_backoff);
static lwpa_error_t handle_redirect(RdmnetConnection *conn, ClientRedirectMsg *reply);

/*************************** Function definitions ****************************/

/*! \brief Initialize the RDMnet Connection module.
 *
 *  Do all necessary initialization before other RDMnet Connection API functions can be called.
 *
 *  \param[in] log_params A struct used by the library to log messages, or NULL for no logging.
 *  \return #LWPA_OK: Initialization successful.\n
 *          #LWPA_SYSERR: An internal library of system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_init(const LwpaLogParams *log_params)
{
  lwpa_error_t res;

  /* The lock is created only the first call to this function. */
  rdmnet_create_lock_or_die();

  res = LWPA_SYSERR;
  if (rdmnet_writelock())
  {
    bool poll_lock_created = false;
#if RDMNET_USE_TICK_THREAD
    LwpaThreadParams thread_params;
#endif

    res = LWPA_OK;
#if !RDMNET_DYNAMIC_MEM
    /* Init memory pools */
    res |= lwpa_mempool_init(rdmnet_connections);
#endif
    if (res == LWPA_OK)
      res = rdmnet_message_init();
    if (res == LWPA_OK)
      res = lwpa_socket_init(NULL);

    if (res == LWPA_OK)
    {
      poll_lock_created = lwpa_mutex_create(&rc_state.poll_lock);
      if (!poll_lock_created)
        res = LWPA_SYSERR;
    }

#if RDMNET_USE_TICK_THREAD
    if (res == LWPA_OK)
    {
      thread_params.thread_priority = RDMNET_TICK_THREAD_PRIORITY;
      thread_params.stack_size = RDMNET_TICK_THREAD_STACK;
      thread_params.thread_name = "rdmnet_tick";
      thread_params.platform_data = NULL;
      rc_state.tickthread_run = true;
      if (!lwpa_thread_create(&rc_state.tick_thread, &thread_params, rdmnet_tick_thread, NULL))
      {
        res = LWPA_SYSERR;
      }
    }
#endif

    if (res == LWPA_OK)
    {
      /* Do all initialization that doesn't have a failure condition */
      lwpa_rbtree_init(&rc_state.connections, conn_cmp, node_alloc, node_dealloc);
      rc_state.next_conn_handle = 0;
      rc_state.log_params = log_params;
      rc_state.initted = true;
    }
    else
    {
      if (poll_lock_created)
        lwpa_mutex_destroy(&rc_state.poll_lock);
      memset(&rc_state, 0, sizeof rc_state);
    }
    rdmnet_writeunlock();
  }
  return res;
}

static void conn_tree_dealloc(const LwpaRbTree *self, LwpaRbNode *node)
{
  RdmnetConnection *conn = (RdmnetConnection *)node->value;
  if (conn)
  {
    lwpa_close(conn->sock);
    lwpa_mutex_destroy(&conn->lock);
    lwpa_mutex_destroy(&conn->send_lock);
    free_rdmnet_connection(conn);
  }
  node_dealloc(node);
  (void)self;
}

/*! \brief Deinitialize the RDMnet Connection module.
 *
 *  Set the RDMnet Connection module back to an uninitialized state. All existing connections will
 *  be closed/disconnected. Calls to other RDMnet Connection API functions will fail until
 *  rdmnet_init() is called again.
 */
void rdmnet_deinit()
{
  if (!rc_state.initted)
    return;

  rc_state.initted = false;

#if RDMNET_USE_TICK_THREAD
  rc_state.tickthread_run = false;
  lwpa_thread_stop(&rc_state.tick_thread, LWPA_WAIT_FOREVER);
#endif

  if (rdmnet_writelock())
  {
    lwpa_rbtree_clear_with_cb(&rc_state.connections, conn_tree_dealloc);
    lwpa_mutex_destroy(&rc_state.poll_lock);
    lwpa_socket_deinit();
    memset(&rc_state, 0, sizeof rc_state);
    rdmnet_writeunlock();
  }
}

/*! \brief Create a new handle to use for an RDMnet Connection.
 *
 *  This function simply allocates a connection handle - use rdmnet_connect() to actually start the
 *  connection process.
 *
 *  \param[in] local_cid The CID of the local component using the connection.
 *  \return A new connection handle (valid values are >= 0) or an enumerated error value:\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOMEM: No room to allocate additional connection.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
int rdmnet_new_connection(const LwpaUuid *local_cid)
{
  RdmnetConnection *conn;
  bool send_lock_created = false;
  bool lock_created = false;
  lwpa_error_t res = LWPA_OK;

  if (!local_cid)
    return LWPA_INVALID;
  if (!rc_state.initted)
    return LWPA_NOTINIT;
  if (!rdmnet_writelock())
    return LWPA_SYSERR;

  /* Passed the quick checks, try to create a struct to represent a new connection. This function
   * creates the new connection, gives it a unique handle and inserts it into the connection map. */
  conn = create_new_connection();
  if (!conn)
    res = LWPA_NOMEM;

  /* Try to create the locks and signal */
  if (res == LWPA_OK)
  {
    lock_created = lwpa_mutex_create(&conn->lock);
    if (!lock_created)
      res = LWPA_SYSERR;
  }
  if (res == LWPA_OK)
  {
    send_lock_created = lwpa_mutex_create(&conn->send_lock);
    if (!send_lock_created)
      res = LWPA_SYSERR;
  }

  if (res == LWPA_OK)
  {
    conn->local_cid = *local_cid;
    conn->sock = LWPA_SOCKET_INVALID;
    lwpaip_set_v4_address(&conn->remote_addr.ip, 0);
    conn->remote_addr.port = 0;
    conn->is_blocking = true;
    conn->state = kCSNotConnected;
    conn->poll_list = NULL;
    lwpa_timer_start(&conn->backoff_timer, 0);
    conn->rdmnet_conn_failed = false;
    conn->recv_disconn_err = LWPA_TIMEDOUT;
    conn->recv_waiting = false;
    rdmnet_msg_buf_init(&conn->recv_buf, rc_state.log_params);
  }
  else
  {
    if (conn)
    {
      if (lock_created)
        lwpa_mutex_destroy(&conn->send_lock);
      if (conn->sock != LWPA_SOCKET_INVALID)
        lwpa_close(conn->sock);
      lwpa_rbtree_remove(&rc_state.connections, conn);
      free_rdmnet_connection(conn);
    }
  }
  rdmnet_writeunlock();
  return (res == LWPA_OK ? conn->handle : (int)res);
}

/* Internal function to update the backoff timer. If the connection is a blocking connection,
 * proceeds to wait the backoff time. Returns LWPA_INPROGRESS for a non-blocking connection to
 * indicate that the timer has been started, LWPA_OK to indicate we waited successfully, or an error
 * code.
 */
lwpa_error_t update_backoff_and_wait_if_blocking(RdmnetConnection *conn, const LwpaSockaddr *remote_addr)
{
  int handle = conn->handle;
  lwpa_error_t res = LWPA_OK;
  conn->state = kCSBackoff;

  if (conn->rdmnet_conn_failed && lwpasock_ip_port_equal(&conn->remote_addr, remote_addr))
  {
    lwpa_timer_start(&conn->backoff_timer, update_backoff(conn->backoff_timer.interval));
    if (conn->is_blocking)
    {
      while (!lwpa_timer_isexpired(&conn->backoff_timer))
      {
        release_conn_and_readlock(conn);
        lwpa_thread_sleep(BLOCKING_BACKOFF_WAIT_INTERVAL);
        /* Check if we are still initted and the conn is still valid */
        res = get_readlock_and_conn(handle, &conn);
        if (res != LWPA_OK)
          break;
      }
      if (res == LWPA_OK)
      {
        /* We've made it through the backoff wait. */
        conn->state = kCSTCPConnPending;
      }
    }
    else
      res = LWPA_INPROGRESS;
  }

  /* We always save the remote address that was requested, for updating the backoff timer. */
  conn->remote_addr = *remote_addr;
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
 *  \param[out] additional_data If the connection was redirected to another network address, this
 *                              structure contains the new address. On error, additional error data
 *                              is stored in this structure.
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
lwpa_error_t rdmnet_connect(int handle, const LwpaSockaddr *remote_addr, const ClientConnectMsg *connect_data,
                            RdmnetData *additional_data)
{
  lwpa_error_t res;
  RdmnetConnection *conn;
  bool blocking_wait = false;
  lwpa_socket_t conn_sock = LWPA_SOCKET_INVALID;
  LwpaTimer *block_timer = NULL;

  if (handle < 0 || !remote_addr || !connect_data)
    return LWPA_INVALID;
  res = get_readlock_and_conn(handle, &conn);
  if (res != LWPA_OK)
    return res;

  if (conn->state != kCSNotConnected)
    res = LWPA_ISCONN;
  else if (conn->is_blocking)
  {
    /* If this is going to be a blocking connect, the user needs to provide an additional_data
     * argument to capture the result of the connect. */
    if (!additional_data)
      res = LWPA_INVALID;
    else
      rdmnet_data_set_nodata(additional_data);
  }

  /* Try to create a new socket to use for the connection. */
  if (res == LWPA_OK)
  {
    conn->sock = lwpa_socket(LWPA_AF_INET, LWPA_STREAM);
    if (conn->sock == LWPA_SOCKET_INVALID)
      res = LWPA_SYSERR;
  }

  /* If it's a blocking connection, wait on the backoff timer. */
  if (res == LWPA_OK)
    res = update_backoff_and_wait_if_blocking(conn, remote_addr);
  /* Any error other than LWPA_INPROGRESS indicates that there was a problem reacquiring the locks
   * and we should return now. */
  if (res != LWPA_OK && res != LWPA_INPROGRESS && res != LWPA_ISCONN)
    return res;

  if (res == LWPA_OK)
  {
    bool reacquire_locks = conn->is_blocking;
    lwpa_error_t find_res;

    /* Reset the RDMnet connection failure flag for a new connection attempt */
    conn->rdmnet_conn_failed = false;

    /* Release the locks before a potentially long blocking connect */
    if (conn->is_blocking)
      release_conn_and_readlock(conn);

    res = lwpa_connect(conn->sock, remote_addr);

    if (reacquire_locks)
    {
      find_res = get_readlock_and_conn(handle, &conn);
      if (find_res != LWPA_OK)
        return find_res;
    }
  }

  /* If we are nonblocking, LWPA_INPROGRESS or LWPA_WOULDBLOCK indicates that we can return now and
   * process the connection later. */
  if (!conn->is_blocking && (res == LWPA_INPROGRESS || res == LWPA_WOULDBLOCK))
  {
    res = LWPA_INPROGRESS;
    /* Store the connect data for later sending */
    conn->conn_data = *connect_data;
  }
  else if (res == LWPA_OK)
  {
    /* We are connected! */
    conn->state = kCSRDMnetConnPending;
    /* Flag that if the connection fails after this point, we increment the backoff timer. */
    conn->rdmnet_conn_failed = true;
    /* TODO capture error from this */
    send_client_connect(conn, connect_data);
    lwpa_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
    lwpa_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
    blocking_wait = conn->is_blocking;
    if (blocking_wait)
    {
      conn_sock = conn->sock;
      block_timer = &conn->hb_timer;
    }
    else
      res = LWPA_INPROGRESS;
  }
  else if (res != LWPA_ISCONN)
  {
    /* The connection failed. */
    conn->state = kCSNotConnected;
    lwpa_close(conn->sock);
    conn->sock = LWPA_SOCKET_INVALID;
  }
  release_conn_and_readlock(conn);

  /* For a blocking connect, time to block until the connection handshake is complete. */
  if (res == LWPA_OK && blocking_wait)
  {
    while (true)
    {
      /* Do a poll to check for received data. */
      int timeout_ms = ((E133_HEARTBEAT_TIMEOUT_SEC * 1000 > lwpa_timer_elapsed(block_timer))
                            ? E133_HEARTBEAT_TIMEOUT_SEC * 1000 - lwpa_timer_elapsed(block_timer)
                            : 0);

      LwpaPollfd pfd;
      int poll_res;
      bool should_break = false;
      lwpa_error_t find_res;

      pfd.fd = conn_sock;
      pfd.events = LWPA_POLLIN;
      poll_res = lwpa_poll(&pfd, 1, timeout_ms);

      find_res = get_readlock_and_conn(handle, &conn);
      if (find_res == LWPA_OK)
      {
        if (poll_res < 0)
        {
          res = (lwpa_error_t)poll_res;
          conn->state = kCSNotConnected;
          lwpa_close(conn->sock);
          conn->sock = LWPA_SOCKET_INVALID;
          should_break = true;
        }
        else if (pfd.revents & LWPA_POLLERR)
        {
          res = pfd.err;
          conn->state = kCSNotConnected;
          lwpa_close(conn->sock);
          conn->sock = LWPA_SOCKET_INVALID;
          should_break = true;
        }
        else if (pfd.revents & LWPA_POLLIN)
        {
          /* We have data. */
          res = rdmnet_msg_buf_recv(conn->sock, &conn->recv_buf);
          if (res == LWPA_OK)
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
                    /* TODO check version and Broker UID */
                    conn->state = kCSHeartbeat;
                    conn->rdmnet_conn_failed = false;
                    lwpa_timer_start(&conn->backoff_timer, 0);
                    should_break = true;
                    break;
                  default:
                    conn->state = kCSNotConnected;
                    res = LWPA_CONNREFUSED;
                    rdmnet_data_set_code(additional_data, reply->connect_status);
                    should_break = true;
                    break;
                }
              }
              else if (is_client_redirect_msg(bmsg))
              {
                res = handle_redirect(conn, get_client_redirect_msg(bmsg));
                if (res != LWPA_OK)
                  should_break = true;
              }
            }
          }
          else if (res != LWPA_NODATA)
          {
            conn->state = kCSNotConnected;
            lwpa_close(conn->sock);
            conn->sock = LWPA_SOCKET_INVALID;
            should_break = true;
          }
        }
        release_conn_and_readlock(conn);
      }
      else
        should_break = true;
      if (should_break)
        break;
    }
  }
  return res;
}

/*! THIS FUNCTION IS NOT IMPLEMENTED YET. */
int rdmnet_connect_poll(RdmnetPoll *poll_arr, size_t poll_arr_size, int timeout_ms)
{
  (void)poll_arr;
  (void)poll_arr_size;
  (void)timeout_ms;
  return LWPA_NOTIMPL;
  //          case kCSTCPConnPending:
  //            if (pfds[i].revents & LWPA_POLLERR)
  //              rdmnet_pfds[i]->result.code =
  //              RDMNET_CONNECTFAILED;
  //            else if (pfds[i].revents & (LWPA_POLLIN |
  //            LWPA_POLLOUT))
  //            {
  //              /* We are connected! */
  //              conn->state = kCSRDMnetConnPending;
  //              send_client_connect(conn, &conn->conn_data);
  //              lwpa_timer_start(&conn->hb_timer,
  //                               E133_HEARTBEAT_TIMEOUT_SEC *
  //                               1000);
  //              lwpa_timer_start(&conn->send_timer,
  //                               E133_TCP_HEARTBEAT_INTERVAL_SEC
  //                               * 1000);
  //              rdmnet_pfds[i]->result.code = RDMNET_NOEVENT;
  //              --poll_res;
  //            }
  //            break;
  //          case kCSRDMnetConnPending:
  //            if (pfds[i].revents & LWPA_POLLERR)
  //              rdmnet_pfds[i]->result.code =
  //              RDMNET_CONNECTFAILED;
  //            else if (pfds[i].revents & LWPA_POLLIN)
  //            {
  //              /* We have data. */
  //              RdmnetMessage msg;
  //              if (rdmnet_msg_buf_recv(conn->sock,
  //              &conn->recv_buf, &msg))
  //              {
  //                if (is_broker_msg(&msg))
  //                {
  //                  BrokerMessage *bmsg =
  //                  get_broker_msg(&msg); if
  //                  (is_connect_reply_msg(bmsg))
  //                  {
  //                    /* TODO check version and Broker UID */
  //                    ConnectReplyMsg *reply =
  //                        get_connect_reply_msg(bmsg);
  //                    switch (reply->connect_status)
  //                    {
  //                      case CONNECT_OK:
  //                        rdmnet_pfds[i]->result.code =
  //                        RDMNET_CONNECTED;
  //                        rdmnet_pfds[i]->result.additional_data
  //                        =
  //                            reply->connect_status;
  //                        should_break = true;
  //                        break;
  //                      case CONNECT_REDIRECT:
  //                        handle_connect_redirect(conn, reply);
  //                        break;
  //                      default:
  //                        result->code = RDMNET_CONNECTREFUSED;
  //                        result->additional_data =
  //                        reply->connect_status; should_break =
  //                        true; break;
  //                    }
  //                  }
  //                  else
  //                    --poll_res;
  //                }
  //                else
  //                  --poll_res;
  //              }
  //              else
  //                --poll_res;
  //            }
  //            else
  //              --poll_res;
  //            break;
}

/*! \brief Set an RDMnet connection handle to be either blocking or
 *         non-blocking.
 *
 *  The blocking state of a connection controls how other API calls behave. If a connection is:
 *
 *  * Blocking:
 *    - rdmnet_connect() will block until the RDMnet connection handshake is completed.
 *    - rdmnet_send() and related functions will block until all data is sent.
 *    - rdmnet_recv() will block until a fully-parsed message is received.
 *  * Non-blocking:
 *    - rdmnet_connect() will return immediately with error code #LWPA_INPROGRESS. The connection
 *      can be polled for completion status using rdmnet_connect_poll().
 *    - rdmnet_send() will return immediately with error code #LWPA_WOULDBLOCK if there is too much
 *      data to fit in the underlying send buffer.
 *    - rdmnet_recv() will return immediately with error code #LWPA_WOULDBLOCK if there is no
 *      immediate data to be received.
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
lwpa_error_t rdmnet_set_blocking(int handle, bool blocking)
{
  RdmnetConnection *conn;
  lwpa_error_t res;

  if (handle < 0)
    return LWPA_INVALID;
  res = get_readlock_and_conn(handle, &conn);
  if (res != LWPA_OK)
    return res;

  if (conn->state == kCSBackoff || conn->state == kCSTCPConnPending || conn->state == kCSRDMnetConnPending)
  {
    /* Can't change the blocking state while a connection is in progress. */
    release_conn_and_readlock(conn);
    return LWPA_BUSY;
  }

  if (conn->state == kCSHeartbeat)
  {
    res = lwpa_setblocking(conn->sock, blocking);
    if (res == LWPA_OK)
      conn->is_blocking = blocking;
  }
  else
  {
    /* State is NotConnected, just change the flag */
    conn->is_blocking = blocking;
  }
  release_conn_and_readlock(conn);
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
lwpa_error_t rdmnet_attach_existing_socket(int handle, lwpa_socket_t sock, const LwpaSockaddr *remote_addr)
{
  RdmnetConnection *conn;
  lwpa_error_t res;

  if (handle < 0 || sock == LWPA_SOCKET_INVALID || !remote_addr)
    return LWPA_INVALID;

  res = get_readlock_and_conn(handle, &conn);
  if (res == LWPA_OK)
  {
    if (conn->state != kCSNotConnected)
      res = LWPA_ISCONN;
    else
    {
      conn->sock = sock;
      conn->remote_addr = *remote_addr;
      conn->state = kCSHeartbeat;
      lwpa_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
      lwpa_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
    }
    release_conn_and_readlock(conn);
  }
  return res;
}

/*! \brief Disconnect an RDMnet connection.
 *
 *  The connection handle can be reused for another connection; if it is not to be reused, make sure
 *  to clean up its resources using rdmnet_destroy_connection().
 *
 *  \param[in] handle Connection handle to disconnect.
 *  \param[in] send_disconnect_msg Whether to send an RDMnet Disconnect message. This is the proper
 *                                 way to gracefully close a connection in RDMnet.
 *  \param[in] disconnect_reason If send_disconnect_msg is true, the RDMnet disconnect reason code
 *                               to send.
 *  \return #LWPA_OK: Connection was successfully disconnected\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOTCONN: The connection handle provided is not currently connected.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_disconnect(int handle, bool send_disconnect_msg, rdmnet_disconnect_reason_t disconnect_reason)
{
  lwpa_error_t res;
  RdmnetConnection *conn;

  if (handle < 0)
    return LWPA_INVALID;

  res = get_writelock_and_conn(handle, &conn);
  if (res != LWPA_OK)
    return res;

  if (conn->state != kCSHeartbeat)
    res = LWPA_NOTCONN;
  else
  {
    conn->state = kCSNotConnected;
    if (send_disconnect_msg)
    {
      DisconnectMsg dm;
      dm.disconnect_reason = disconnect_reason;
      send_disconnect(conn, &dm);
    }
    lwpa_shutdown(conn->sock, LWPA_SHUT_WR);
    lwpa_close(conn->sock);
    conn->sock = LWPA_SOCKET_INVALID;
  }

  release_conn_and_writelock(conn);
  return res;
}

/*! \brief Destroy an RDMnet connection handle.
 *
 *  If the connection is currently healthy, call rdmnet_disconnect() first to do a graceful
 *  RDMnet-level disconnect.
 *
 *  \param[in] handle Connection handle to destroy.
 *  \return #LWPA_OK: Connection was successfully destroyed\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_destroy_connection(int handle)
{
  lwpa_error_t res;
  RdmnetConnection *conn;

  if (handle < 0)
    return LWPA_INVALID;

  res = get_writelock_and_conn(handle, &conn);
  if (res != LWPA_OK)
    return res;

  if (conn->sock != LWPA_SOCKET_INVALID)
    lwpa_close(conn->sock);
  lwpa_mutex_destroy(&conn->lock);
  lwpa_mutex_destroy(&conn->send_lock);
  lwpa_rbtree_remove(&rc_state.connections, conn);
  free_rdmnet_connection(conn);

  release_conn_and_writelock(conn);
  return res;
}

/*! \brief Poll for received data on a group of RDMnet connections.
 *
 *  For an application which maintains multiple RDMnet connections, this function can be used to
 *  poll for received data on all of them at once.
 *
 *  \param poll_arr Array of rdmnet_poll structs, each representing a connection to be polled. After
 *                  this function returns, each structure's err member indicates the result of this
 *                  poll:\n
 *    * #LWPA_OK indicates that there is data to be received on this connection. Use rdmnet_recv()
 *      to receive the data.
 *    * #LWPA_NODATA indicates that there was no activity on this connection.
 *    * Any other value indicates an error that caused the connection to be disconnected.
 *  \param poll_arr_size Number of rdmnet_poll structs in the poll_arr.
 *  \param timeout_ms Amount of time to wait for activity, in milliseconds.
 *  \return Number of elements in poll_arr which have data (success)\n
 *          0 (timed out)\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOMEM: (only when #RDMNET_DYNAMIC_MEM is defined to 1) Unable to allocate memory
 *                       for poll operation.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
int rdmnet_poll(RdmnetPoll *poll_arr, size_t poll_arr_size, int timeout_ms)
{
  RdmnetPoll *poll;
  int res = 0;
#if RDMNET_DYNAMIC_MEM
  LwpaPollfd *pfds = NULL;
#else
  static LwpaPollfd pfds[RDMNET_MAX_CONNECTIONS];
#endif
  size_t nfds = 0;

  /* clang-format off */
  if (!poll_arr
#if !RDMNET_DYNAMIC_MEM
      || poll_arr_size > RDMNET_MAX_CONNECTIONS
#endif
     )
  {
    return LWPA_INVALID;
  }
  /* clang-format on */
  if (!rc_state.initted)
    return LWPA_NOTINIT;
  if (!rdmnet_readlock())
    return LWPA_SYSERR;
  if (!lwpa_mutex_take(&rc_state.poll_lock, LWPA_WAIT_FOREVER))
  {
    rdmnet_readunlock();
    return LWPA_SYSERR;
  }

#if RDMNET_DYNAMIC_MEM
  pfds = (LwpaPollfd *)calloc(poll_arr_size, sizeof(LwpaPollfd));
  if (!pfds)
  {
    lwpa_mutex_give(&rc_state.poll_lock);
    rdmnet_readunlock();
    return LWPA_NOMEM;
  }
#endif

  for (poll = poll_arr; poll < poll_arr + poll_arr_size; ++poll)
  {
    RdmnetConnection *conn;
    RdmnetConnection conn_cmp;

    conn_cmp.handle = poll->handle;
    conn = (RdmnetConnection *)lwpa_rbtree_find(&rc_state.connections, &conn_cmp);
    if (!conn)
    {
      ++res;
      poll->err = LWPA_NOTFOUND;
      continue;
    }
    if (!res && lwpa_mutex_take(&conn->lock, LWPA_WAIT_FOREVER))
    {
      if (conn->state != kCSHeartbeat)
      {
        ++res;
        poll->err = LWPA_NOTCONN;
      }
      else if (conn->recv_buf.data_remaining)
      {
        ++res;
        poll->err = LWPA_OK;
      }
      else
      {
        pfds[nfds].fd = conn->sock;
        pfds[nfds].events = LWPA_POLLIN;
        poll->err = LWPA_NODATA;
        ++nfds;
      }
      lwpa_mutex_give(&conn->lock);
    }
  }
  rdmnet_readunlock();

  if (res == 0 && nfds)
  {
    /* No immediate poll data to report. Do the poll. */
    int poll_res = lwpa_poll(pfds, nfds, timeout_ms);
    if (poll_res <= 0)
      res = poll_res;
    else if (rdmnet_readlock())
    {
      /* We got something. Check to see what it is. */
      size_t i;
      for (i = 0; i < nfds && poll_res; ++i)
      {
        if (pfds[i].revents)
        {
          /* We have some returned events. Find the socket */
          RdmnetConnection *conn;
          RdmnetConnection conn_cmp;

          conn_cmp.handle = poll_arr[i].handle;
          conn = (RdmnetConnection *)lwpa_rbtree_find(&rc_state.connections, &conn_cmp);
          if (conn && lwpa_mutex_take(&conn->lock, LWPA_WAIT_FOREVER))
          {
            if (conn->state == kCSHeartbeat)
            {
              if (pfds[i].revents & LWPA_POLLERR)
              {
                poll_arr[i].err = pfds[i].err;
                ++res;
              }
              else if (pfds[i].revents & LWPA_POLLIN)
              {
                poll_arr[i].err = LWPA_OK;
                ++res;
              }
            }
            else
            {
              poll_arr[i].err = conn->recv_disconn_err;
              ++res;
            }
            lwpa_mutex_give(&conn->lock);
          }
          else
          {
            poll_arr[i].err = LWPA_NOTFOUND;
            ++res;
          }
        }
      }
      rdmnet_readunlock();
    }
  }

#if RDMNET_DYNAMIC_MEM
  if (pfds)
    free(pfds);
#endif

  lwpa_mutex_give(&rc_state.poll_lock);
  rdmnet_readunlock();
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
int rdmnet_send(int handle, const uint8_t *data, size_t size)
{
  int res;
  RdmnetConnection *conn;

  if (handle < 0 || !data || size == 0)
    return LWPA_INVALID;
  if (!rc_state.initted)
    return LWPA_NOTINIT;
  if (!rdmnet_readlock())
    return LWPA_SYSERR;

  res = get_conn(handle, &conn);

  if (res == LWPA_OK)
  {
    if (conn->state != kCSHeartbeat)
      res = LWPA_NOTCONN;
    lwpa_mutex_give(&conn->lock);
  }

  if (res == LWPA_OK && lwpa_mutex_take(&conn->send_lock, LWPA_WAIT_FOREVER))
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
 *  \return #LWPA_OK: Send operation started successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOTCONN: The connection handle has not been successfully connected.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_start_message(int handle)
{
  lwpa_error_t res;
  RdmnetConnection *conn;

  if (handle < 0)
    return LWPA_INVALID;
  if (!rc_state.initted)
    return LWPA_NOTINIT;
  if (!rdmnet_readlock())
    return LWPA_SYSERR;

  res = get_conn(handle, &conn);
  if (res == LWPA_OK)
  {
    if (conn->state != kCSHeartbeat)
      res = LWPA_NOTCONN;
    lwpa_mutex_give(&conn->lock);
  }

  if (res == LWPA_OK)
  {
    if (lwpa_mutex_take(&conn->send_lock, LWPA_WAIT_FOREVER))
    {
      /* Return, keeping the readlock and the send lock. */
      return res;
    }
    else
      res = LWPA_SYSERR;
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
int rdmnet_send_partial_message(int handle, const uint8_t *data, size_t size)
{
  int res;
  RdmnetConnection *conn;

  if (handle < 0 || !data || size == 0)
    return LWPA_INVALID;
  if (!rc_state.initted)
    return LWPA_NOTINIT;
  if (!rdmnet_readlock())
    return LWPA_SYSERR;

  res = get_conn(handle, &conn);
  if (res == LWPA_OK)
  {
    if (conn->state != kCSHeartbeat)
      res = LWPA_NOTCONN;
    lwpa_mutex_give(&conn->lock);
  }

  if (res == LWPA_OK)
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
 *  \return #LWPA_OK: Send operation ended successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 */
lwpa_error_t rdmnet_end_message(int handle)
{
  lwpa_error_t res;
  RdmnetConnection *conn;

  if (handle < 0)
    return LWPA_INVALID;
  if (!rc_state.initted)
    return LWPA_NOTINIT;
  if (!rdmnet_readlock())
    return LWPA_SYSERR;

  res = get_conn(handle, &conn);
  if (res == LWPA_OK)
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

/*! \brief Receive data on an RDMnet connection.
 *
 *  The RDMnet Connection library uses a stream parser under the hood to reassemble RDMnet data
 *  packets from the TCP stream. Each time this function is called, the parser executes the
 *  following steps:
 *    * Attempt to parse a full message from any data already received, and return on success.
 *    * If there is no data or not enough data available, do a socket-level recv().
 *    * If the socket-level recv() returns data, attempt to parse a full message from the data
 *      currently available, and return the result.
 *
 *  The result might be #LWPA_NODATA, which indicates one of two conditions:
 *    * Not enough data has yet been received to parse a full message.
 *    * A message was parsed and consumed internally (i.e. an RDMnet TCP heartbeat message)
 *
 *  Your application should handle #LWPA_NODATA as a special case which indicates that the context
 *  calling this function should simply continue with normal operation.
 *
 *  This function may allocate lists of structures (either dynamically or from memory pools, based
 *  on the value of #RDMNET_DYNAMIC_MEM) to represent repeated data contained in a message. If this
 *  function returns #LWPA_OK, you MUST free the resulting message with free_rdmnet_message() to
 *  avoid memory leaks.
 *
 *  \param[in] handle Connection handle on which to receive.
 *  \param[out] data Data structure which contains a received message or status code.
 *  \return #LWPA_OK: Message received successfully and stored in data.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOTINIT: Module not initialized.\n
 *          #LWPA_NOTCONN: The connection handle has not been successfully connected.\n
 *          #LWPA_ALREADY: rdmnet_recv() is already being called and blocked on from another
 *                         context.\n
 *          #LWPA_NODATA: See above.\n
 *          #LWPA_CONNCLOSED: The connection was closed gracefully, and data contains the RDMnet
 *                            disconnect reason code.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_recv(int handle, RdmnetData *data)
{
  lwpa_error_t res;
  RdmnetConnection *conn;
  lwpa_socket_t recv_sock = LWPA_SOCKET_INVALID;
  RdmnetMsgBuf *msgbuf = NULL;
  bool do_recv = false;

  if (handle < 0 || !data)
    return LWPA_INVALID;
  res = get_readlock_and_conn(handle, &conn);
  if (res != LWPA_OK)
    return res;

  if (conn->state != kCSHeartbeat)
    res = LWPA_NOTCONN;
  else if (conn->recv_waiting)
    res = LWPA_ALREADY;
  else
  {
    conn->recv_waiting = true;
    msgbuf = &conn->recv_buf;
    recv_sock = conn->sock;
    do_recv = true;
  }
  release_conn_and_readlock(conn);

  if (do_recv)
  {
    lwpa_error_t find_res;

    res = rdmnet_msg_buf_recv(recv_sock, msgbuf);
    if (res == LWPA_OK)
    {
      if (is_broker_msg(&msgbuf->msg))
      {
        BrokerMessage *bmsg = get_broker_msg(&msgbuf->msg);
        switch (bmsg->vector)
        {
          case VECTOR_BROKER_CONNECT_REPLY:
          case VECTOR_BROKER_NULL:
            res = LWPA_NODATA;
            break;
          case VECTOR_BROKER_DISCONNECT:
            res = LWPA_CONNCLOSED;
            rdmnet_data_set_code(data, get_disconnect_msg(bmsg)->disconnect_reason);
            break;
          default:
            rdmnet_data_set_msg(data, msgbuf->msg);
            break;
        }
      }
      else
        rdmnet_data_set_msg(data, msgbuf->msg);
    }

    find_res = get_readlock_and_conn(handle, &conn);
    if (find_res == LWPA_OK)
    {
      conn->recv_waiting = false;
      if (conn->state == kCSNotConnected)
        res = conn->recv_disconn_err;
      else if (res != LWPA_OK && res != LWPA_NODATA)
      {
        conn->state = kCSNotConnected;
        lwpa_close(conn->sock);
        conn->sock = LWPA_SOCKET_INVALID;
      }
      else
      {
        /* We've received something on this connection. Reset the heartbeat timer. */
        lwpa_timer_reset(&conn->hb_timer);
      }
      release_conn_and_readlock(conn);
    }
    else
      res = find_res;
  }
  return res;
}

#if RDMNET_USE_TICK_THREAD
void rdmnet_tick_thread(void *arg)
{
  (void)arg;
  while (rc_state.tickthread_run)
  {
    rdmnet_tick();
    lwpa_thread_sleep(RDMNET_TICK_THREAD_SLEEP_MS);
  }
}
#endif

/*! \brief Handle periodic RDMnet functionality.
 *
 *  If #RDMNET_USE_TICK_THREAD is defined nonzero, this is an internal function called automatically
 *  by the library. Otherwise, it must be called by the application preiodically to handle
 *  health-checked TCP functionality. Recommended calling interval is ~1s.
 */
void rdmnet_tick()
{
  LwpaRbIter iter;
  RdmnetConnection *conn;

  if (!rc_state.initted)
    return;

  if (rdmnet_readlock())
  {
    lwpa_rbiter_init(&iter);
    conn = (RdmnetConnection *)lwpa_rbiter_first(&iter, &rc_state.connections);
    while (conn)
    {
      if (lwpa_mutex_take(&conn->lock, LWPA_WAIT_FOREVER))
      {
        switch (conn->state)
        {
          case kCSHeartbeat:
            if (lwpa_timer_isexpired(&conn->hb_timer))
            {
              /* Heartbeat timeout! Disconnect the connection. */
              conn->state = kCSNotConnected;
              conn->recv_disconn_err = LWPA_TIMEDOUT;
              /* TODO explore shutdown here */
              lwpa_close(conn->sock);
              conn->sock = LWPA_SOCKET_INVALID;
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
      conn = (RdmnetConnection *)lwpa_rbiter_next(&iter);
    }
    rdmnet_readunlock();
  }
}

/* Internal function which attempts to allocate and track a new connection, including allocating the
 * structure, creating a new handle value, and inserting it into the global map.
 *
 *  Must have write lock.
 */
RdmnetConnection *create_new_connection()
{
  RdmnetConnection *conn;
  int original_handle = rc_state.next_conn_handle;

  conn = (RdmnetConnection *)alloc_rdmnet_connection();
  if (!conn)
    return NULL;

  /* Grab a new integer handle for this connection, making sure we don't overlap with one that's
   * already in use. */
  conn->handle = rc_state.next_conn_handle;
  if (++rc_state.next_conn_handle < 0)
    rc_state.next_conn_handle = 0;
  while (lwpa_rbtree_find(&rc_state.connections, conn))
  {
    if (rc_state.next_conn_handle == original_handle)
    {
      /* Incredibly unlikely case of all handles used */
      free_rdmnet_connection(conn);
      return NULL;
    }
    conn->handle = rc_state.next_conn_handle;
    if (++rc_state.next_conn_handle < 0)
      rc_state.next_conn_handle = 0;
  }

  if (0 == lwpa_rbtree_insert(&rc_state.connections, conn))
  {
    free_rdmnet_connection(conn);
    return NULL;
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

/* Internal function to handle an RDMnet redirect. Attempts to connect to the new address and
 * returns the result. */
lwpa_error_t handle_redirect(RdmnetConnection *conn, ClientRedirectMsg *reply)
{
  lwpa_error_t conn_res;

  /* First, close the old connection and try to create a new socket */
  lwpa_close(conn->sock);
  conn->sock = lwpa_socket(LWPA_AF_INET, LWPA_STREAM);
  if (conn->sock == LWPA_SOCKET_INVALID)
  {
    conn->state = kCSNotConnected;
    return LWPA_SYSERR;
  }

  /* Connect to the new address and store the address info. */
  conn->remote_addr = reply->new_addr;
  conn_res = lwpa_connect(conn->sock, &reply->new_addr);
  if (conn_res == LWPA_INPROGRESS && !conn->is_blocking)
  {
    conn->state = kCSTCPConnPending;
    conn_res = LWPA_OK;
  }
  else if (conn_res == LWPA_OK)
  {
    conn->state = kCSRDMnetConnPending;
    send_client_connect(conn, &conn->conn_data);
    lwpa_timer_start(&conn->send_timer, E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
    lwpa_timer_start(&conn->hb_timer, E133_HEARTBEAT_TIMEOUT_SEC * 1000);
  }
  else
  {
    conn->state = kCSNotConnected;
    lwpa_close(conn->sock);
  }
  return conn_res;
}

int conn_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;
  RdmnetConnection *a = (RdmnetConnection *)node_a->value;
  RdmnetConnection *b = (RdmnetConnection *)node_b->value;
  return (a->handle - b->handle);
}

LwpaRbNode *node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode *)malloc(sizeof(LwpaRbNode));
#else
  return lwpa_mempool_alloc(rdmnet_lwpa_rbnodes);
#endif
}

void node_dealloc(LwpaRbNode *node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  lwpa_mempool_free(rdmnet_lwpa_rbnodes, node);
#endif
}

lwpa_error_t get_conn(int handle, RdmnetConnection **conn_ptr)
{
  RdmnetConnection conn_to_find;
  RdmnetConnection *conn;

  conn_to_find.handle = handle;
  conn = (RdmnetConnection *)lwpa_rbtree_find(&rc_state.connections, &conn_to_find);

  if (!conn)
    return LWPA_NOTFOUND;
  if (!lwpa_mutex_take(&conn->lock, LWPA_WAIT_FOREVER))
    return LWPA_SYSERR;

  *conn_ptr = conn;
  return LWPA_OK;
}

lwpa_error_t get_readlock_and_conn(int handle, RdmnetConnection **conn_ptr)
{
  lwpa_error_t res;

  if (!rc_state.initted)
    return LWPA_NOTINIT;
  if (!rdmnet_readlock())
    return LWPA_SYSERR;

  res = get_conn(handle, conn_ptr);
  if (res != LWPA_OK)
    rdmnet_readunlock();
  return res;
}

lwpa_error_t get_writelock_and_conn(int handle, RdmnetConnection **conn_ptr)
{
  RdmnetConnection conn_to_find;
  RdmnetConnection *conn;

  if (!rc_state.initted)
    return LWPA_NOTINIT;
  if (!rdmnet_writelock())
    return LWPA_SYSERR;

  conn_to_find.handle = handle;
  conn = (RdmnetConnection *)lwpa_rbtree_find(&rc_state.connections, &conn_to_find);

  if (!conn)
  {
    rdmnet_writeunlock();
    return LWPA_NOTFOUND;
  }

  /* Taking the global write lock means we don't have to take the conn mutex. */
  *conn_ptr = conn;
  return LWPA_OK;
}
