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
#include "rdmnet/llrp.h"
#include "llrppriv.h"

#include "lwpa_mempool.h"
#include "lwpa_socket.h"
#include "estardmnet.h"
#include "rdmnet/opts.h"

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define llrp_socket_alloc() malloc(sizeof(LlrpBaseSocket))
#define llrp_socket_dealloc(socket) free(socket)
#else
#define llrp_socket_alloc() lwpa_mempool_alloc(llrp_sockets)
#define llrp_socket_dealloc(socket) lwpa_mempool_free(llrp_sockets, socket)
#endif

/**************************** Private variables ******************************/

/* clang-format off */
#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(llrp_sockets, LlrpBaseSocket, LLRP_MAX_SOCKETS);
#endif
/* clang-format on */

static LlrpBaseSocket *socket_list = NULL;

static LwpaIpAddr kLLRPIPv4RespAddr;
static LwpaIpAddr kLLRPIPv6RespAddr;
static LwpaIpAddr kLLRPIPv4RequestAddr;
static LwpaIpAddr kLLRPIPv6RequestAddr;

/*********************** Private function prototypes *************************/

/* Helper functions for creating sockets */
static llrp_socket_t llrp_create_base_socket(const LwpaIpAddr *net_interface_addr, const LwpaCid *owner_cid,
                                             llrp_socket_type_t socket_type);
static void llrp_add_socket_to_list(llrp_socket_t socket, llrp_socket_t *list);
static void llrp_remove_socket_from_list(llrp_socket_t socket, llrp_socket_t *list);
static lwpa_socket_t create_lwpa_socket(const LwpaSockaddr *saddr, const LwpaIpAddr *netint);
static bool subscribe_multicast(lwpa_socket_t lwpa_sock, llrp_socket_type_t socket_type, const LwpaIpAddr *netint);

/* Functions for the known_uids tree in a Manager socket */
static LwpaRbNode *node_alloc();
static void node_dealloc(LwpaRbNode *node);
static int known_uid_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static void known_uid_clear_cb(const LwpaRbTree *self, LwpaRbNode *node);

/* Functions for Manager discovery */
static void halve_range(LwpaUid *uid1, LwpaUid *uid2);
static bool update_probe_range(LlrpManagerSocketData *mgrdata, KnownUid **uid_list);
static bool send_next_probe(LlrpBaseSocket *sock);

/* Helper functions for llrp_udpate() */
static bool process_manager_state(LlrpBaseSocket *sock, int *timeout_ms, LlrpData *data);
static void process_target_state(LlrpBaseSocket *sock, int *timeout_ms);
static void register_message_interest(LlrpBaseSocket *sock, LlrpMessageInterest *interest);
static bool process_parsed_msg(LlrpBaseSocket *sock, const LlrpMessage *msg, LlrpData *data);

/* Helper function for closing sockets */
static llrp_socket_t llrp_close_socket_priv(llrp_socket_t socket, lwpa_error_t *result);

/*************************** Function definitions ****************************/

/*! \brief Initialize the LLRP module.
 *
 *  Do all necessary initialization before other LLRP functions can be called.
 *
 *  \return #LWPA_OK: Initialization successful.\n
 *          #LWPA_SYSERR: An internal library of system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket
 *          calls.\n
 */
lwpa_error_t llrp_init()
{
  lwpa_error_t res;

#if !RDMNET_DYNAMIC_MEM
  /* Init memory pool */
  res = lwpa_mempool_init(llrp_sockets);
  if (res == LWPA_OK)
  {
#endif
    res = lwpa_socket_init(NULL);
#if !RDMNET_DYNAMIC_MEM
  }
#endif

  if (res == LWPA_OK)
  {
    lwpa_inet_pton(LWPA_IPV4, LLRP_MULTICAST_IPV4_ADDRESS_RESPONSE, &kLLRPIPv4RespAddr);
    lwpa_inet_pton(LWPA_IPV4, LLRP_MULTICAST_IPV6_ADDRESS_RESPONSE, &kLLRPIPv6RespAddr);
    lwpa_inet_pton(LWPA_IPV4, LLRP_MULTICAST_IPV4_ADDRESS_REQUEST, &kLLRPIPv4RequestAddr);
    lwpa_inet_pton(LWPA_IPV4, LLRP_MULTICAST_IPV6_ADDRESS_REQUEST, &kLLRPIPv6RequestAddr);
    llrp_prot_init();
  }
  return res;
}

/*! \brief Deinitialize the LLRP module.
 *
 *  Set the LLRP module back to an uninitialized state. All existing
 *  connections will be closed/disconnected. Calls to other LLRP API functions
 *  will fail until llrp_init() is called again.
 */
void llrp_deinit()
{
  LlrpBaseSocket *iter = socket_list;

  // Close all sockets
  while (iter != NULL)
  {
    lwpa_error_t close_result;
    iter = llrp_close_socket_priv(iter, &close_result);
  }

  lwpa_socket_deinit();
}

/*! \brief Create an LLRP socket to be used by an LLRP Manager.
 *
 *  LLRP Manager Sockets can only be created when #RDMNET_DYNAMIC_MEM is
 *  defined nonzero. Otherwise, this function will always fail.
 *
 *  \param[in] netint The network interface this LLRP socket should send and
 *                    receive on.
 *  \param[in] manager_cid The CID of the LLRP Manager.
 *  \return A new LLRP socket (success) or #LLRP_SOCKET_INVALID (failure).
 */
llrp_socket_t llrp_create_manager_socket(const LwpaIpAddr *netint, const LwpaCid *manager_cid)
{
#if RDMNET_DYNAMIC_MEM
  llrp_socket_t sock = llrp_create_base_socket(netint, manager_cid, kLLRPSocketTypeManager);

  if (sock == LLRP_SOCKET_INVALID)
    return sock;

  // Initialize manager-specific info
  sock->role.manager.transaction_number = 0;
  sock->role.manager.discovery_active = false;

  llrp_add_socket_to_list(sock, &socket_list);

  return sock;
#else
  return LLRP_SOCKET_INVALID;
#endif
}

/*! \brief Create an LLRP socket to be used by an LLRP Target.
 *
 *  \param[in] netint The network interface this LLRP socket should send and
 *                    receive on.
 *  \param[in] target_cid The CID of the LLRP Target.
 *  \param[in] target_uid The UID of the LLRP Target.
 *  \param[in] hardware_address The hardware address of the LLRP Target
 *                              (typically the MAC address of the network
 *                              interface)
 *  \param[in] component_type The component type this LLRP Target is associated
 *                            with; pass kLLRPCompUnknown if the Target is not
 *                            associated with any other type of RDMnet
 *                            Component.
 *  \return A new LLRP socket (success) or #LLRP_SOCKET_INVALID (failure).
 */
llrp_socket_t llrp_create_target_socket(const LwpaIpAddr *netint, const LwpaCid *target_cid, const LwpaUid *target_uid,
                                        const uint8_t *hardware_address, llrp_component_t component_type)
{
  LlrpTargetSocketData *targetdata;

  if (!target_uid || !hardware_address)
    return LLRP_SOCKET_INVALID;

  llrp_socket_t sock = llrp_create_base_socket(netint, target_cid, kLLRPSocketTypeTarget);

  if (sock == LLRP_SOCKET_INVALID)
    return sock;

  // Initialize target-specific info
  targetdata = get_target_data(sock);
  targetdata->target_info.component_type = component_type;
  memcpy(targetdata->target_info.hardware_address, hardware_address, 6);
  targetdata->target_info.target_uid = *target_uid;
  targetdata->target_info.target_cid = *target_cid;
  targetdata->connected_to_broker = false;
  targetdata->reply_pending = false;

  llrp_add_socket_to_list(sock, &socket_list);

  return sock;
}

/*! \brief Close and deallocate an LLRP socket.
 *
 *  Also closes the underlying system sockets.
 *
 *  \param[in] handle LLRP socket to close.
 *  \return true (closed successfully) or false (not closed successfully).
 */
bool llrp_close_socket(llrp_socket_t handle)
{
  if (handle == LLRP_SOCKET_INVALID)
    return false;

  lwpa_error_t result;
  llrp_close_socket_priv(handle, &result);
  return (result == LWPA_OK);
}

/*! \brief Start discovery on an LLRP Manager socket.
 *
 *  Configure a Manager socket to start discovery and send the first discovery
 *  message. Fails if the socket is not a Manager socket, or if a previous
 *  discovery process is still ongoing.
 *
 *  \param[in] handle LLRP socket on which to start discovery.
 *  \param[in] filter Discovery filter, made up of one or more of the
 *                    LLRP_FILTERVAL_* constants defined in estardmnet.h
 *  \return true (Discovery started successfully) or false (failed to start
 *          discovery).
 */
bool llrp_start_discovery(llrp_socket_t handle, uint8_t filter)
{
  if (handle && handle->socket_type == kLLRPSocketTypeManager)
  {
    LlrpManagerSocketData *mgrdata = get_manager_data(handle);
    if (!mgrdata->discovery_active)
    {
      mgrdata->cur_range_low.manu = 0;
      mgrdata->cur_range_low.id = 0;
      mgrdata->cur_range_high = kBroadcastUid;
      mgrdata->num_clean_sends = 0;
      mgrdata->discovery_active = true;
      mgrdata->disc_filter = filter;
      rb_tree_init(&mgrdata->known_uids, known_uid_cmp, node_alloc, node_dealloc);

      send_next_probe(handle);

      return true;
    }
  }
  return false;
}

/*! \brief Stop discovery on an LLRP Manager socket.
 *
 *  Clears all discovery state and known UIDs.
 *
 *  \param[in] handle LLRP socket on which to stop discovery.
 *  \return true (Discovery stopped successfully) or false (invalid argument or
 *          discovery was not running).
 */
bool llrp_stop_discovery(llrp_socket_t handle)
{
  if (handle != NULL)
  {
    if (handle->socket_type == kLLRPSocketTypeManager)
    {
      LlrpManagerSocketData *mgrdata = get_manager_data(handle);
      if (mgrdata->discovery_active)
      {
        rb_tree_clear_with_cb(&mgrdata->known_uids, known_uid_clear_cb);
        mgrdata->discovery_active = false;
        return true;
      }
    }
  }

  return false;
}

void halve_range(LwpaUid *uid1, LwpaUid *uid2)
{
  uint64_t uval1 = ((((uint64_t)uid1->manu) << 32) | uid1->id);
  uint64_t uval2 = ((((uint64_t)uid2->manu) << 32) | uid2->id);
  uint64_t umid = uval1 + uval2 / 2;

  uid2->manu = (uint16_t)((umid >> 32) & 0xffffu);
  uid2->id = (uint32_t)(umid & 0xffffffffu);
}

bool update_probe_range(LlrpManagerSocketData *mgrdata, KnownUid **uid_list)
{
  LwpaRbIter iter;
  KnownUid *cur_uid;
  KnownUid *list_begin = NULL;
  KnownUid *last_uid = NULL;
  unsigned int uids_in_range = 0;

  if (mgrdata->num_clean_sends >= 3)
  {
    /* We are finished with a range; move on to the next range. */
    if (uid_is_broadcast(&mgrdata->cur_range_high))
    {
      /* We're done with discovery. */
      return false;
    }
    else
    {
      /* The new range starts at the old upper limit + 1, and ends at the top
       * of the UID space. */
      if (mgrdata->cur_range_high.id == 0xffffffffu)
      {
        mgrdata->cur_range_low.manu = mgrdata->cur_range_high.manu + 1;
        mgrdata->cur_range_low.id = 0;
      }
      else
      {
        mgrdata->cur_range_low.manu = mgrdata->cur_range_high.manu;
        mgrdata->cur_range_low.id = mgrdata->cur_range_high.id + 1;
      }
      mgrdata->cur_range_high = kBroadcastUid;
      mgrdata->num_clean_sends = 0;
    }
  }

  /* Determine how many known UIDs are in the current range */
  rb_iter_init(&iter);
  cur_uid = rb_iter_first(&iter, &mgrdata->known_uids);
  while (cur_uid && (uidcmp(&cur_uid->uid, &mgrdata->cur_range_high) <= 0))
  {
    if (last_uid)
    {
      if (++uids_in_range <= LLRP_KNOWN_UID_SIZE)
      {
        last_uid->next = cur_uid;
        cur_uid->next = NULL;
        last_uid = cur_uid;
      }
      else
      {
        halve_range(&mgrdata->cur_range_low, &mgrdata->cur_range_high);
        return update_probe_range(mgrdata, uid_list);
      }
    }
    else if (uidcmp(&cur_uid->uid, &mgrdata->cur_range_low) >= 0)
    {
      list_begin = cur_uid;
      cur_uid->next = NULL;
      last_uid = cur_uid;
      ++uids_in_range;
    }
    cur_uid = rb_iter_next(&iter);
  }
  *uid_list = list_begin;
  return true;
}

bool send_next_probe(LlrpBaseSocket *sock)
{
  LlrpManagerSocketData *mgrdata = get_manager_data(sock);
  KnownUid *list_head;

  if (update_probe_range(mgrdata, &list_head))
  {
    LlrpHeader header;
    ProbeRequestSend request;
    LwpaSockaddr dest_addr;

    header.sender_cid = sock->owner_cid;
    header.dest_cid = kLLRPBroadcastCID;
    header.transaction_number = mgrdata->transaction_number++;

    request.filter = mgrdata->disc_filter;
    request.lower_uid = mgrdata->cur_range_low;
    request.upper_uid = mgrdata->cur_range_high;
    request.uid_list = list_head;

    dest_addr.ip = kLLRPIPv4RequestAddr;
    dest_addr.port = LLRP_PORT;
    send_llrp_probe_request(sock, &dest_addr, &header, &request);
    lwpa_timer_start(&mgrdata->disc_timer, LLRP_TIMEOUT_MS);
    ++mgrdata->num_clean_sends;
    return true;
  }
  else
  {
    /* We are done with discovery. */
    return false;
  }
}

void known_uid_clear_cb(const LwpaRbTree *self, LwpaRbNode *node)
{
  KnownUid *uid = (KnownUid *)node->value;
  if (uid)
    free(uid);
  node_dealloc(node);
  (void)self;
}

bool process_manager_state(LlrpBaseSocket *sock, int *timeout_ms, LlrpData *data)
{
  LlrpManagerSocketData *mgrdata = get_manager_data(sock);

  if (mgrdata->discovery_active)
  {
    if (lwpa_timer_isexpired(&mgrdata->disc_timer))
    {
      if (send_next_probe(sock))
      {
        if (*timeout_ms == LWPA_WAIT_FOREVER || *timeout_ms > LLRP_TIMEOUT_MS)
          *timeout_ms = LLRP_TIMEOUT_MS;
      }
      else
      {
        llrp_data_set_disc_finished(data);
        rb_tree_clear_with_cb(&mgrdata->known_uids, known_uid_clear_cb);
        mgrdata->discovery_active = false;
        return true;
      }
    }
    else
    {
      uint32_t remaining_interval = lwpa_timer_remaining(&mgrdata->disc_timer);
      if (*timeout_ms == LWPA_WAIT_FOREVER || (uint32_t)(*timeout_ms) > remaining_interval)
      {
        *timeout_ms = remaining_interval;
      }
    }
  }
  return false;
}

void process_target_state(LlrpBaseSocket *sock, int *timeout_ms)
{
  LlrpTargetSocketData *targetdata = get_target_data(sock);

  if (targetdata->reply_pending)
  {
    if (lwpa_timer_isexpired(&targetdata->reply_backoff))
    {
      LlrpHeader header;
      LwpaSockaddr dest_addr;

      header.sender_cid = sock->owner_cid;
      header.dest_cid = targetdata->pending_reply_cid;
      header.transaction_number = targetdata->pending_reply_trans_num;

      dest_addr.ip = kLLRPIPv4RespAddr;
      dest_addr.port = LLRP_PORT;
      send_llrp_probe_reply(sock, &dest_addr, &header, &targetdata->target_info);

      targetdata->reply_pending = false;
    }
    else
    {
      uint32_t remaining_interval = lwpa_timer_remaining(&targetdata->reply_backoff);
      if (*timeout_ms == LWPA_WAIT_FOREVER || (uint32_t)(*timeout_ms) > remaining_interval)
        *timeout_ms = remaining_interval;
    }
  }
}

void register_message_interest(LlrpBaseSocket *sock, LlrpMessageInterest *interest)
{
  interest->my_cid = sock->owner_cid;
  if (sock->socket_type == kLLRPSocketTypeManager)
  {
    interest->interested_in_probe_request = false;
    if (get_manager_data(sock)->discovery_active)
      interest->interested_in_probe_reply = true;
    else
      interest->interested_in_probe_reply = false;
  }
  else  // socket_type == kLLRPSocketTypeTarget
  {
    interest->interested_in_probe_reply = false;
    interest->interested_in_probe_request = true;
    interest->my_uid = get_target_data(sock)->target_info.target_uid;
  }
}

bool process_parsed_msg(LlrpBaseSocket *sock, const LlrpMessage *msg, LlrpData *data)
{
  if (msg->vector == VECTOR_LLRP_RDM_CMD)
  {
    LlrpRdmMessage rdm_msg;
    rdm_msg.transaction_num = msg->header.transaction_number;
    rdm_msg.source_cid = msg->header.sender_cid;
    rdm_msg.msg = *(llrp_msg_get_rdm_cmd(msg));
    llrp_data_set_rdm(data, rdm_msg);
    return true;
  }
  else if (sock->socket_type == kLLRPSocketTypeManager && msg->vector == VECTOR_LLRP_PROBE_REPLY)
  {
    const LlrpTarget *new_target = llrp_msg_get_probe_reply(msg);
    LlrpManagerSocketData *mgrdata = get_manager_data(sock);

    if (mgrdata->discovery_active && 0 == cidcmp(&msg->header.dest_cid, &sock->owner_cid))
    {
      KnownUid *new_known_uid = malloc(sizeof(KnownUid));
      if (new_known_uid)
      {
        new_known_uid->uid = new_target->target_uid;
        new_known_uid->next = NULL;
        if (rb_tree_find(&mgrdata->known_uids, new_known_uid) || !rb_tree_insert(&mgrdata->known_uids, new_known_uid))
        {
          free(new_known_uid);
        }
      }
      mgrdata->num_clean_sends = 0;
      llrp_data_set_disc_target(data, *new_target);
      return true;
    }
    return false;
  }
  else if (sock->socket_type == kLLRPSocketTypeTarget && msg->vector == VECTOR_LLRP_PROBE_REQUEST)
  {
    const ProbeRequestRecv *request = llrp_msg_get_probe_request(msg);
    LlrpTargetSocketData *targetdata = get_target_data(sock);
    /* TODO allow multiple probe replies to be queued */
    if (request->contains_my_uid && !targetdata->reply_pending)
    {
      int backoff_ms;

      /* Check the filter values. */
      if (((request->filter & LLRP_FILTERVAL_BROKERS_ONLY) &&
           targetdata->target_info.component_type != kLLRPCompBroker) ||
          (request->filter & LLRP_FILTERVAL_CLIENT_CONN_INACTIVE && targetdata->connected_to_broker))
      {
        return false;
      }
      targetdata->reply_pending = true;
      targetdata->pending_reply_cid = msg->header.sender_cid;
      targetdata->pending_reply_trans_num = msg->header.transaction_number;
      backoff_ms = (rand() * LLRP_MAX_BACKOFF_MS / RAND_MAX);
      lwpa_timer_start(&targetdata->reply_backoff, backoff_ms);
    }
    /* Even if we got a valid probe request, we are starting a backoff timer,
     * so there's nothing to report at this time. */
    return false;
  }
  else
    return false;
}

/*! \brief Poll a set of LLRP sockets for updates.
 *
 *  Drives the discovery algorithm on Manager sockets, responds to discovery
 *  queries on Target sockets, and receives RDM messages on both types of
 *  socket.
 *
 *  \param[in,out] poll_array Array of llrp_poll structs, each representing an
 *  LLRP socket to be polled. After this function returns, each structure's err
 *  member indicates the results of this poll:
 *   * #LWPA_OK indicates that the data member contains valid data.
 *   * #LWPA_NODATA indicates that no activity occurred on this socket.
 *   * Any other value indicates a fatal error on this socket; the socket
 *     should be closed.
 *  \param[in] poll_array_size Number of llrp_poll structs in the array.
 *  \param[in] timeout_ms Amount of time to wait for activity, in milliseconds.
 *  This value may be adjusted down to conform to various LLRP timeouts
 *  specified in E1.33.
 *  \return Number of elements in poll_array which have data (success)\n
 *          #LWPA_TIMEDOUT: Timed out waiting for activity.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_NOMEM (only when #RDMNET_DYNAMIC_MEM is defined to 1): Unable
 *          to allocate memory for poll operation.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: other error codes might be propagated from underlying socket
 *          calls.
 */
int llrp_update(LlrpPoll *poll_array, size_t poll_array_size, int timeout_ms)
{
  int res = 0;
  int poll_res;
  size_t i;

#if RDMNET_DYNAMIC_MEM
  LwpaPollfd *pfds = NULL;
#else
  static LwpaPollfd pfds[LLRP_MAX_SOCKETS];
#endif
  size_t nfds = 0;
  int poll_timeout = timeout_ms;

#if RDMNET_DYNAMIC_MEM
  pfds = calloc(poll_array_size, sizeof(LwpaPollfd));
  if (!pfds)
    return LWPA_NOMEM;
#endif

  for (i = 0; i < poll_array_size; ++i)
  {
    LlrpPoll *cur_poll = &poll_array[i];
    llrp_data_set_nodata(&cur_poll->data);
    if (cur_poll->handle != LLRP_SOCKET_INVALID)
    {
      cur_poll->err = LWPA_NODATA;
      pfds[nfds].fd = cur_poll->handle->sys_sock;
      pfds[nfds].events = LWPA_POLLIN;
      ++nfds;

      if (cur_poll->handle->socket_type == kLLRPSocketTypeManager)
      {
        if (process_manager_state(cur_poll->handle, &poll_timeout, &cur_poll->data))
        {
          cur_poll->err = LWPA_OK;
          ++res;
        }
      }
      else
      {
        process_target_state(cur_poll->handle, &poll_timeout);
      }
    }
    else
    {
      cur_poll->err = LWPA_INVALID;
      ++res;
    }
  }

  if (res != 0)
  {
#if RDMNET_DYNAMIC_MEM
    free(pfds);
#endif
    return res;
  }

  // Do the poll
  poll_res = lwpa_poll(pfds, nfds, poll_timeout);
  if (poll_res > 0)
  {
    LlrpPoll *cur_poll;
    LwpaPollfd *pfd;
    for (cur_poll = poll_array, pfd = pfds; pfd < pfds + nfds; ++pfd, ++cur_poll)
    {
      if (pfd->revents & LWPA_POLLERR)
      {
        cur_poll->err = pfd->err;
        ++res;
      }
      else if (pfd->revents & LWPA_POLLIN)
      {
        /* There is data available - receive it. */
        LwpaSockaddr remote_addr;
        int recv_res = lwpa_recvfrom(pfd->fd, cur_poll->handle->recv_buf, LLRP_MAX_MESSAGE_SIZE, 0, &remote_addr);
        if (recv_res >= 0)
        {
          LlrpMessage msg;
          LlrpMessageInterest interest;
          register_message_interest(cur_poll->handle, &interest);

          if (parse_llrp_message(cur_poll->handle->recv_buf, recv_res, &interest, &msg))
          {
            if (process_parsed_msg(cur_poll->handle, &msg, &cur_poll->data))
            {
              cur_poll->err = LWPA_OK;
              ++res;
            }
          }
        }
        else
        {
          cur_poll->err = recv_res;
          ++res;
        }
      }
    }

    if (res == 0)
      res = LWPA_TIMEDOUT;
  }
  else
    res = poll_res;

#if RDMNET_DYNAMIC_MEM
  free(pfds);
#endif
  return res;
}

/*! \brief Update the Broker connection state of an LLRP Target socket.
 *
 *  If an LLRP Target is associated with an RPT Client, this should be called
 *  each time the Client connects or disconnects from the Broker. This affects
 *  whether the LLRP Target responds to filtered LLRP probe requests.
 *
 *  \param[in] handle LLRP Target socket for which to update the connection
 *                    state.
 *  \param[in] connected_to_broker Whether the LLRP Target is currently
 *                                 connected to a Broker.
 */
void llrp_target_update_connection_state(llrp_socket_t handle, bool connected_to_broker)
{
  (void)handle;
  (void)connected_to_broker;
  /* TODO */
}

/*! \brief Send an RDM command on an LLRP Manager socket.
 *
 *  On success, provides the transaction number to correlate with a response.
 *
 *  \param[in] handle LLRP manager socket handle on which to send an RDM
 *                    command.
 *  \param[in] destination CID of LLRP Target to which to send the command.
 *  \param[in] command RDM command to send.
 *  \param[out] transaction_number Filled in on success with the transaction
 *                                 number of the command.
 *  \return #LWPA_OK: Command sent successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          Note: other error codes might be propagated from underlying socket
 *          calls.
 */
lwpa_error_t llrp_send_rdm_command(llrp_socket_t handle, const LwpaCid *destination, const RdmBuffer *command,
                                   uint32_t *transaction_number)
{
  LlrpManagerSocketData *mgrdata;
  LlrpHeader header;
  LwpaSockaddr dest_addr;
  lwpa_error_t res;

  if (!handle || !destination || !command || !transaction_number || handle->socket_type != kLLRPSocketTypeManager)
  {
    return LWPA_INVALID;
  }

  mgrdata = get_manager_data(handle);

  header.dest_cid = *destination;
  header.sender_cid = handle->owner_cid;
  header.transaction_number = mgrdata->transaction_number;

  dest_addr.ip = kLLRPIPv4RequestAddr;
  dest_addr.port = LLRP_PORT;
  res = send_llrp_rdm(handle, &dest_addr, &header, command);

  if (res == LWPA_OK)
    *transaction_number = mgrdata->transaction_number++;
  return res;
}

/*! \brief Send an RDM response on an LLRP Target socket.
 *
 *  \param[in] handle LLRP manager socket handle on which to send an RDM
 *                    command.
 *  \param[in] destination CID of LLRP Manager to which to send the command.
 *  \param[in] command RDM response to send.
 *  \param[in] transaction_number Transaction number of the corresponding LLRP
 *                                RDM command.
 *  \return #LWPA_OK: Command sent successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          Note: other error codes might be propagated from underlying socket
 *          calls.
 */
lwpa_error_t llrp_send_rdm_response(llrp_socket_t handle, const LwpaCid *destination, const RdmBuffer *command,
                                    uint32_t transaction_number)
{
  LlrpHeader header;
  LwpaSockaddr dest_addr;

  if (!handle || !destination || !command || !transaction_number || handle->socket_type != kLLRPSocketTypeTarget)
  {
    return LWPA_INVALID;
  }

  header.dest_cid = *destination;
  header.sender_cid = handle->owner_cid;
  header.transaction_number = transaction_number;

  dest_addr.ip = kLLRPIPv4RespAddr;
  dest_addr.port = LLRP_PORT;
  return send_llrp_rdm(handle, &dest_addr, &header, command);
}

llrp_socket_t llrp_close_socket_priv(llrp_socket_t socket, lwpa_error_t *result)
{
  if (!socket)
    return NULL;

  if (socket->sys_sock != LWPA_SOCKET_INVALID)
    *result = lwpa_close(socket->sys_sock);
  else
    *result = LWPA_OK;

  llrp_socket_t next = socket->next;

  llrp_remove_socket_from_list(socket, &socket_list);
  llrp_socket_dealloc(socket);

  return next;
}

llrp_socket_t llrp_create_base_socket(const LwpaIpAddr *net_interface_addr, const LwpaCid *owner_cid,
                                      llrp_socket_type_t socket_type)
{
  if ((net_interface_addr == NULL) || (owner_cid == NULL))
    return LLRP_SOCKET_INVALID;

  llrp_socket_t sock = llrp_socket_alloc();

  sock->net_int_addr = *net_interface_addr;
  sock->next = NULL;
  sock->owner_cid = *owner_cid;
  sock->socket_type = socket_type;

  // Initialize LWPA sockets
  LwpaSockaddr saddr;
  saddr.ip = (socket_type == kLLRPSocketTypeManager ? kLLRPIPv4RespAddr : kLLRPIPv4RequestAddr);
  saddr.port = LLRP_PORT;

  sock->sys_sock = create_lwpa_socket(&saddr, net_interface_addr);

  if (sock->sys_sock == LWPA_SOCKET_INVALID)
  {
    llrp_socket_dealloc(sock);
    return LLRP_SOCKET_INVALID;
  }

  if (!subscribe_multicast(sock->sys_sock, socket_type, &sock->net_int_addr))
  {
    lwpa_close(sock->sys_sock);
    llrp_socket_dealloc(sock);
    return LLRP_SOCKET_INVALID;
  }

  return sock;
}

void llrp_add_socket_to_list(llrp_socket_t socket, llrp_socket_t *list)
{
  if (list)
  {
    if (!(*list))
    {
      *list = socket;
    }
    else
    {
      llrp_socket_t iter = *list;

      while (iter->next)
        iter = iter->next;

      socket->next = NULL;
      iter->next = socket;
    }
  }
}

void llrp_remove_socket_from_list(llrp_socket_t socket, llrp_socket_t *list)
{
  if (list)
  {
    llrp_socket_t iter = *list;
    llrp_socket_t prev = NULL;

    while (iter && (iter != socket))
    {
      prev = iter;
      iter = iter->next;
    }

    if (iter)
    {
      if (!prev)
        *list = iter->next;
      else
        prev->next = iter->next;

      iter->next = NULL;
    }
  }
}

lwpa_socket_t create_lwpa_socket(const LwpaSockaddr *saddr, const LwpaIpAddr *netint)
{
  lwpa_socket_t sock = lwpa_socket((saddr->ip.type == LWPA_IPV6) ? LWPA_AF_INET6 : LWPA_AF_INET, LWPA_DGRAM);
  bool valid = (sock != LWPA_SOCKET_INVALID);

  if (valid)
  {
    int option = 1;  // Very important for our multicast needs
    valid = (0 == lwpa_setsockopt(sock, LWPA_SOL_SOCKET, LWPA_SO_REUSEADDR, (const void *)(&option), sizeof(option)));
  }

  if (valid)
  {
    if (saddr->ip.type == LWPA_IPV4)
    {
      int value = 20;  // A more reasonable TTL limit, but probably unnecessary
      valid =
          (0 == lwpa_setsockopt(sock, LWPA_IPPROTO_IP, LWPA_IP_MULTICAST_TTL, (const void *)(&value), sizeof(value)));
    }
    else
    {
      // TODO: add IPv6 support
    }
  }

  if (valid)
  {
    // This one is critical for multicast sends to go over the correct
    // interface.
    if (saddr->ip.type == LWPA_IPV4)
    {
      valid = (0 == lwpa_setsockopt(sock, LWPA_IPPROTO_IP, LWPA_IP_MULTICAST_IF, (const void *)(netint),
                                    sizeof(LwpaIpAddr)));
    }
    else
    {
      // TODO: add IPv6 support
    }
  }

  if (valid)
  {
    if (saddr->ip.type == LWPA_IPV4)
    {
#if LLRP_BIND_TO_MCAST_ADDRESS
      // Bind socket to multicast address for IPv4
      valid = (0 == lwpa_bind(sock, saddr));
#else
      // Bind socket to INADDR_ANY and the LLRP port
      LwpaSockaddr bind_addr;
      lwpaip_make_any_v4(&bind_addr.ip);
      bind_addr.port = LLRP_PORT;
      valid = (0 == lwpa_bind(sock, &bind_addr));
#endif
    }
    else
    {
      // TODO: add IPv6 support
    }
  }

  if (!valid)
  {
    lwpa_close(sock);
    return LWPA_SOCKET_INVALID;
  }

  return sock;
}

bool subscribe_multicast(lwpa_socket_t lwpa_sock, llrp_socket_type_t socket_type, const LwpaIpAddr *netint)
{
  if (!lwpa_sock || !netint)
    return false;

  bool result = false;

  if (lwpaip_is_v4(netint))
  {
    LwpaMreq multireq;

    multireq.group = (socket_type == kLLRPSocketTypeTarget ? kLLRPIPv4RequestAddr : kLLRPIPv4RespAddr);
    multireq.netint = *netint;
    result = (0 == lwpa_setsockopt(lwpa_sock, LWPA_IPPROTO_IP, LWPA_MCAST_JOIN_GROUP, (const void *)&multireq,
                                   sizeof(multireq)));
  }
  else
  {
    // TODO: add IPv6 support
  }

  return result;
}

LwpaRbNode *node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return malloc(sizeof(LwpaRbNode));
#else
  /* TODO */
  return NULL;
#endif
}

void node_dealloc(LwpaRbNode *node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
/* TODO */
#endif
}

int known_uid_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;
  const LwpaUid *a = (const LwpaUid *)node_a->value;
  const LwpaUid *b = (const LwpaUid *)node_b->value;
  return uidcmp(a, b);
}
