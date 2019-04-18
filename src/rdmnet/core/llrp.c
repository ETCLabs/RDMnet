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

#include "rdmnet/core/llrp.h"

#include <string.h>
#include "rdmnet/private/opts.h"
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "lwpa/mempool.h"
#endif
#include "lwpa/rbtree.h"
#include "lwpa/socket.h"
#include "lwpa/netint.h"
#include "rdmnet/defs.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/util.h"

/*************************** Private constants *******************************/

#define LLRP_MULTICAST_TTL_VAL 20

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define llrp_manager_alloc() malloc(sizeof(LlrpManager))
#define llrp_target_alloc() malloc(sizeof(LlrpTarget))
#define llrp_manager_dealloc(ptr) free(ptr)
#define llrp_target_dealloc(ptr) free(ptr)
#else
#define llrp_manager_alloc() NULL
#define llrp_target_alloc() lwpa_mempool_alloc(llrp_targets)
#define llrp_manager_dealloc()
#define llrp_target_dealloc(ptr) lwpa_mempool_free(llrp_targets, ptr)
#endif

/**************************** Private variables ******************************/

/* clang-format off */
#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(llrp_targets, LlrpTarget, RDMNET_LLRP_MAX_TARGETS);
LWPA_MEMPOOL_DEFINE(llrp_target_rb_nodes, LwpaRbNode, RDMNET_LLRP_MAX_TARGETS);
#endif
/* clang-format on */

static struct LlrpState
{
  LwpaRbTree managers;
  LwpaRbTree targets;

  IntHandleManager handle_mgr;

#if RDMNET_DYNAMIC_MEM
  LwpaNetintInfo *sys_netints;
#else
  LwpaNetintInfo sys_netints[RDMNET_LLRP_MAX_NETWORK_INTERFACES];
#endif
  size_t num_sys_netints;
  uint8_t lowest_hardware_addr[6];

  LwpaIpAddr kLlrpIpv4RespAddr;
  LwpaIpAddr kLlrpIpv6RespAddr;
  LwpaIpAddr kLlrpIpv4RequestAddr;
  LwpaIpAddr kLlrpIpv6RequestAddr;
} state;

/*********************** Private function prototypes *************************/

static lwpa_error_t init_sys_netints();

// Helper functions for creating managers and targets
static bool target_handle_in_use(int handle_val);
static LlrpTarget *create_new_target(const LlrpTargetConfig *config);
static LlrpManager *create_new_manager(const LlrpManagerConfig *config);
static lwpa_socket_t create_sys_socket(const LwpaSockaddr *saddr, const LwpaIpAddr *netint);
static bool subscribe_multicast(lwpa_socket_t lwpa_sock, bool manager, const LwpaIpAddr *netint);

static LwpaRbNode *llrp_sock_node_alloc();
static void llrp_sock_node_free();

// Functions for the known_uids tree in a manager
static LwpaRbNode *node_alloc();
static void node_dealloc(LwpaRbNode *node);
static int known_uid_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static void known_uid_clear_cb(const LwpaRbTree *self, LwpaRbNode *node);

/* Functions for Manager discovery */
static void halve_range(RdmUid *uid1, RdmUid *uid2);
static bool update_probe_range(LlrpManagerSocketData *mgrdata, KnownUid **uid_list);
static bool send_next_probe(LlrpSocket *sock);

/* Helper functions for rdmnet_llrp_udpate() */
static bool process_manager_state(LlrpManager *manager, int *timeout_ms, LlrpData *data);
static void process_target_state(LlrpTarget *target, int *timeout_ms);
static void register_message_interest(LlrpSocket *sock, LlrpMessageInterest *interest);
static bool process_parsed_msg(LlrpSocket *sock, const LlrpMessage *msg, LlrpData *data);

/* Helper function for closing sockets */
static llrp_socket_t llrp_close_socket_priv(llrp_socket_t socket, lwpa_error_t *result);

/*************************** Function definitions ****************************/

/*! \brief Initialize the LLRP module.
 *
 *  Do all necessary initialization before other LLRP functions can be called.
 *
 *  \return #kLwpaErrOk: Initialization successful.\n
 *          #kLwpaErrSys: An internal library of system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t rdmnet_llrp_init()
{
  lwpa_error_t res = kLwpaErrOk;

#if !RDMNET_DYNAMIC_MEM
  /* Init memory pool */
  res = lwpa_mempool_init(llrp_sockets);
#endif

  if (res == kLwpaErrOk)
  {
    lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_RESPONSE, &state.kLlrpIpv4RespAddr);
    lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV6_ADDRESS_RESPONSE, &state.kLlrpIpv6RespAddr);
    lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_REQUEST, &state.kLlrpIpv4RequestAddr);
    lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV6_ADDRESS_REQUEST, &state.kLlrpIpv6RequestAddr);
    llrp_prot_init();
    init_int_handle_manager(&state.handle_mgr, target_handle_in_use);
  }
  return res;
}

lwpa_error_t init_sys_netints()
{
  size_t num_sys_netints = lwpa_netint_get_num_interfaces();
#if RDMNET_DYNAMIC_MEM
  LwpaNetintInfo *sys_netints = calloc(num_sys_netints, sizeof(LwpaNetintInfo));
  if (!sys_netints)
    return false;
#else
  if (num_sys_netints > RDMNET_LLRP_MAX_TARGET_NETINTS)
    return false;
#endif
  num_sys_netints = lwpa_netint_get_interfaces(sys_netints, num_sys_netints);

  for (const LwpaNetintInfo *netint = netint_arr; netint < netint_arr + num_netints; ++netint)
  {
    if (netint == netint_arr)
    {
      memcpy(lowest_address, netint->mac, 6);
    }
    else
    {
      if (memcmp(netint->mac, lowest_address, 6) < 0)
      {
        memcpy(lowest_address, netint->mac, 6);
      }
    }
  }
}

/*! \brief Deinitialize the LLRP module.
 *
 *  Set the LLRP module back to an uninitialized state. All existing connections will be
 *  closed/disconnected. Calls to other LLRP API functions will fail until rdmnet_llrp_init() is called
 *  again.
 */
void rdmnet_llrp_deinit()
{
  LlrpSocket *iter = socket_list;

  // Close all sockets
  while (iter != NULL)
  {
    lwpa_error_t close_result;
    iter = llrp_close_socket_priv(iter, &close_result);
  }

#if RDMNET_DYNAMIC_MEM
  free(state.sys_netints);
#endif
}

/*! \brief Create an LLRP socket to be used by an LLRP Manager.
 *
 *  <b>TODO stale docs</b>
 *
 *  LLRP Manager Sockets can only be created when #RDMNET_DYNAMIC_MEM is defined nonzero. Otherwise,
 *  this function will always fail.
 *
 *  \param[in] netint The network interface this LLRP socket should send and receive on.
 *  \param[in] manager_cid The CID of the LLRP Manager.
 *  \return A new LLRP socket (success) or #LLRP_SOCKET_INVALID (failure).
 */
lwpa_error_t rdmnet_llrp_create_manager_socket(const LlrpManagerConfig *config, llrp_socket_t *handle)
{
#if RDMNET_DYNAMIC_MEM
  LlrpSocket *sock = llrp_create_base_socket(&config->netint, &config->cid, kLlrpSocketTypeManager);

  if (!sock)
    return kLwpaErrNoMem;

  // Initialize manager-specific info
  sock->role.manager.transaction_number = 0;
  sock->role.manager.discovery_active = false;
  *handle = sock->handle;

  return kLwpaErrOk;
#else
  (void)config;
  (void)handle;
  return kLwpaErrNotImpl;
#endif
}

/*! \brief Create an LLRP socket to be used by an LLRP Target.
 *
 *  <b>TODO stale docs</b>
 *
 *  \param[in] netint The network interface this LLRP socket should send and receive on.
 *  \param[in] target_cid The CID of the LLRP Target.
 *  \param[in] target_uid The UID of the LLRP Target.
 *  \param[in] hardware_address The hardware address of the LLRP Target (typically the MAC address
 *                              of the network interface)
 *  \param[in] component_type The component type this LLRP Target is associated with; pass
 *                            kLlrpCompUnknown if the Target is not associated with any other type
 *                            of RDMnet Component.
 *  \return A new LLRP socket (success) or #LLRP_SOCKET_INVALID (failure).
 */
lwpa_error_t rdmnet_llrp_create_target_socket(const LlrpTargetConfig *config, llrp_socket_t *handle)
{
  LlrpTargetSocketData *targetdata;

  if (!config || !handle)
    return kLwpaErrInvalid;

  LlrpSocket *sock = llrp_create_base_socket(&config->netint, &config->cid, kLlrpSocketTypeTarget);

  if (!sock)
    return kLwpaErrNoMem;

  // Initialize target-specific info
  targetdata = GET_TARGET_DATA(sock);
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
bool rdmnet_llrp_close_socket(llrp_socket_t handle)
{
  if (handle == LLRP_SOCKET_INVALID)
    return false;

  lwpa_error_t result;
  llrp_close_socket_priv(handle, &result);
  return (result == kLwpaErrOk);
}

/*! \brief Start discovery on an LLRP Manager socket.
 *
 *  <b>TODO stale docs</b>
 *
 *  Configure a Manager socket to start discovery and send the first discovery message. Fails if the
 *  socket is not a Manager socket, or if a previous discovery process is still ongoing.
 *
 *  \param[in] handle LLRP socket on which to start discovery.
 *  \param[in] filter Discovery filter, made up of one or more of the LLRP_FILTERVAL_* constants
 *                    defined in rdmnet/defs.h
 *  \return true (Discovery started successfully) or false (failed to start discovery).
 */
lwpa_error_t rdmnet_llrp_start_discovery(llrp_manager_t handle, uint16_t filter)
{
  if (handle && handle->socket_type == kLlrpSocketTypeManager)
  {
    LlrpManagerSocketData *mgrdata = GET_MANAGER_DATA(handle);
    if (!mgrdata->discovery_active)
    {
      mgrdata->cur_range_low.manu = 0;
      mgrdata->cur_range_low.id = 0;
      mgrdata->cur_range_high = kBroadcastUid;
      mgrdata->num_clean_sends = 0;
      mgrdata->discovery_active = true;
      mgrdata->disc_filter = filter;
      lwpa_rbtree_init(&mgrdata->known_uids, known_uid_cmp, node_alloc, node_dealloc);

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
 *  \return true (Discovery stopped successfully) or false (invalid argument or discovery was not
 *          running).
 */
bool rdmnet_llrp_stop_discovery(llrp_manager_t handle)
{
  if (handle != NULL)
  {
    if (handle->socket_type == kLlrpSocketTypeManager)
    {
      LlrpManagerSocketData *mgrdata = GET_MANAGER_DATA(handle);
      if (mgrdata->discovery_active)
      {
        lwpa_rbtree_clear_with_cb(&mgrdata->known_uids, known_uid_clear_cb);
        mgrdata->discovery_active = false;
        return true;
      }
    }
  }

  return false;
}

void halve_range(RdmUid *uid1, RdmUid *uid2)
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
    if (rdm_uid_is_broadcast(&mgrdata->cur_range_high))
    {
      /* We're done with discovery. */
      return false;
    }
    else
    {
      /* The new range starts at the old upper limit + 1, and ends at the top of the UID space. */
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
  lwpa_rbiter_init(&iter);
  cur_uid = (KnownUid *)lwpa_rbiter_first(&iter, &mgrdata->known_uids);
  while (cur_uid && (rdm_uid_cmp(&cur_uid->uid, &mgrdata->cur_range_high) <= 0))
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
    else if (rdm_uid_cmp(&cur_uid->uid, &mgrdata->cur_range_low) >= 0)
    {
      list_begin = cur_uid;
      cur_uid->next = NULL;
      last_uid = cur_uid;
      ++uids_in_range;
    }
    cur_uid = (KnownUid *)lwpa_rbiter_next(&iter);
  }
  *uid_list = list_begin;
  return true;
}

bool send_next_probe(LlrpSocket *sock)
{
  LlrpManagerSocketData *mgrdata = GET_MANAGER_DATA(sock);
  KnownUid *list_head;

  if (update_probe_range(mgrdata, &list_head))
  {
    LlrpHeader header;
    ProbeRequestSend request;
    LwpaSockaddr dest_addr;

    header.sender_cid = sock->owner_cid;
    header.dest_cid = kLlrpBroadcastCID;
    header.transaction_number = mgrdata->transaction_number++;

    request.filter = mgrdata->disc_filter;
    request.lower_uid = mgrdata->cur_range_low;
    request.upper_uid = mgrdata->cur_range_high;
    request.uid_list = list_head;

    dest_addr.ip = state.kLlrpIpv4RequestAddr;
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

bool process_manager_state(LlrpSocket *sock, int *timeout_ms, LlrpData *data)
{
  LlrpManagerSocketData *mgrdata = GET_MANAGER_DATA(sock);

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
        lwpa_rbtree_clear_with_cb(&mgrdata->known_uids, known_uid_clear_cb);
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

void process_target_state(LlrpSocket *sock, int *timeout_ms)
{
  LlrpTargetSocketData *targetdata = GET_TARGET_DATA(sock);

  if (targetdata->reply_pending)
  {
    if (lwpa_timer_isexpired(&targetdata->reply_backoff))
    {
      LlrpHeader header;
      LwpaSockaddr dest_addr;

      header.sender_cid = sock->owner_cid;
      header.dest_cid = targetdata->pending_reply_cid;
      header.transaction_number = targetdata->pending_reply_trans_num;

      dest_addr.ip = state.kLlrpIpv4RespAddr;
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

void register_message_interest(LlrpSocket *sock, LlrpMessageInterest *interest)
{
  interest->my_cid = sock->owner_cid;
  if (sock->socket_type == kLlrpSocketTypeManager)
  {
    interest->interested_in_probe_request = false;
    if (GET_MANAGER_DATA(sock)->discovery_active)
      interest->interested_in_probe_reply = true;
    else
      interest->interested_in_probe_reply = false;
  }
  else  // socket_type == kLlrpSocketTypeTarget
  {
    interest->interested_in_probe_reply = false;
    interest->interested_in_probe_request = true;
    interest->my_uid = GET_TARGET_DATA(sock)->target_info.target_uid;
  }
}

bool process_parsed_msg(LlrpSocket *sock, const LlrpMessage *msg, LlrpData *data)
{
  if (msg->vector == VECTOR_LLRP_RDM_CMD)
  {
    LlrpRdmMessage rdm_msg;
    rdm_msg.transaction_num = msg->header.transaction_number;
    rdm_msg.source_cid = msg->header.sender_cid;
    rdm_msg.msg = *(LLRP_MSG_GET_RDM(msg));
    llrp_data_set_rdm(data, rdm_msg);
    return true;
  }
  else if (sock->socket_type == kLlrpSocketTypeManager && msg->vector == VECTOR_LLRP_PROBE_REPLY)
  {
    const LlrpTarget *new_target = LLRP_MSG_GET_PROBE_REPLY(msg);
    LlrpManagerSocketData *mgrdata = GET_MANAGER_DATA(sock);

    if (mgrdata->discovery_active && 0 == lwpa_uuid_cmp(&msg->header.dest_cid, &sock->owner_cid))
    {
      KnownUid *new_known_uid = (KnownUid *)malloc(sizeof(KnownUid));
      if (new_known_uid)
      {
        new_known_uid->uid = new_target->target_uid;
        new_known_uid->next = NULL;
        if (lwpa_rbtree_find(&mgrdata->known_uids, new_known_uid) ||
            !lwpa_rbtree_insert(&mgrdata->known_uids, new_known_uid))
        {
          /* This Target was already in our known UIDs, but is replying anyway. */
          free(new_known_uid);
        }
        else
        {
          /* Newly discovered Target. */
          mgrdata->num_clean_sends = 0;
          llrp_data_set_disc_target(data, *new_target);
          return true;
        }
      }
    }
    return false;
  }
  else if (sock->socket_type == kLlrpSocketTypeTarget && msg->vector == VECTOR_LLRP_PROBE_REQUEST)
  {
    const ProbeRequestRecv *request = llrp_msg_get_probe_request(msg);
    LlrpTargetSocketData *targetdata = GET_TARGET_DATA(sock);
    /* TODO allow multiple probe replies to be queued */
    if (request->contains_my_uid && !targetdata->reply_pending)
    {
      int backoff_ms;

      /* Check the filter values. */
      if (((request->filter & LLRP_FILTERVAL_BROKERS_ONLY) &&
           targetdata->target_info.component_type != kLlrpCompBroker) ||
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
    /* Even if we got a valid probe request, we are starting a backoff timer, so there's nothing to
     * report at this time. */
    return false;
  }
  else
  {
    return false;
  }
}

/*! \brief Poll a set of LLRP sockets for updates.
 *
 *  Drives the discovery algorithm on Manager sockets, responds to discovery queries on Target
 *  sockets, and receives RDM messages on both types of socket.
 *
 *  \param[in,out] poll_array Array of llrp_poll structs, each representing an LLRP socket to be
 *  polled. After this function returns, each structure's err member indicates the results of this
 *  poll:
 *   * #kLwpaErrOk indicates that the data member contains valid data.
 *   * #kLwpaErrNoData indicates that no activity occurred on this socket.
 *   * Any other value indicates a fatal error on this socket; the socket should be closed.
 *
 *  \param[in] poll_array_size Number of llrp_poll structs in the array.
 *  \param[in] timeout_ms Amount of time to wait for activity, in milliseconds. This value may be
 *                        adjusted down to conform to various LLRP timeouts specified in E1.33.
 *  \return Number of elements in poll_array which have data (success)\n
 *          #kLwpaErrTimedOut: Timed out waiting for activity.\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          #kLwpaErrNoMem (only when #RDMNET_DYNAMIC_MEM is defined to 1): Unable to allocate
 *                         memory for poll operation.\n
 *          #kLwpaErrSys: An internal library or system call error occurred.\n
 *          Note: other error codes might be propagated from underlying socket calls.
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
  pfds = (LwpaPollfd *)calloc(poll_array_size, sizeof(LwpaPollfd));
  if (!pfds)
    return kLwpaErrNoMem;
#endif

  for (i = 0; i < poll_array_size; ++i)
  {
    LlrpPoll *cur_poll = &poll_array[i];
    llrp_data_set_nodata(&cur_poll->data);
    if (cur_poll->handle != LLRP_SOCKET_INVALID)
    {
      cur_poll->err = kLwpaErrNoData;
      pfds[nfds].fd = cur_poll->handle->sys_sock;
      pfds[nfds].events = LWPA_POLLIN;
      ++nfds;

      if (cur_poll->handle->socket_type == kLlrpSocketTypeManager)
      {
        if (process_manager_state(cur_poll->handle, &poll_timeout, &cur_poll->data))
        {
          cur_poll->err = kLwpaErrOk;
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
      cur_poll->err = kLwpaErrInvalid;
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
              cur_poll->err = kLwpaErrOk;
              ++res;
            }
          }
        }
        else
        {
          cur_poll->err = (lwpa_error_t)recv_res;
          ++res;
        }
      }
    }

    if (res == 0)
      res = kLwpaErrTimedOut;
  }
  else
  {
    res = poll_res;
  }

#if RDMNET_DYNAMIC_MEM
  free(pfds);
#endif
  return res;
}

/*! \brief Update the Broker connection state of an LLRP Target socket.
 *
 *  If an LLRP Target is associated with an RPT Client, this should be called each time the Client
 *  connects or disconnects from the Broker. This affects whether the LLRP Target responds to
 *  filtered LLRP probe requests.
 *
 *  \param[in] handle LLRP Target socket for which to update the connection state.
 *  \param[in] connected_to_broker Whether the LLRP Target is currently connected to a Broker.
 *  \param[in] new_uid The LLRP Target's new UID, if it is using a Dynamic UID, or NULL if the UID
 *                     hasn't changed.
 */
void rdmnet_llrp_update_target_connection_state(llrp_socket_t handle, bool connected_to_broker, const RdmUid *new_uid)
{
  (void)handle;
  (void)connected_to_broker;
  (void)new_uid;
  /* TODO */
}

/*! \brief Send an RDM command on an LLRP Manager socket.
 *
 *  On success, provides the transaction number to correlate with a response.
 *
 *  \param[in] handle LLRP manager socket handle on which to send an RDM command.
 *  \param[in] destination CID of LLRP Target to which to send the command.
 *  \param[in] command RDM command to send.
 *  \param[out] transaction_number Filled in on success with the transaction number of the command.
 *  \return #kLwpaErrOk: Command sent successfully.\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          Note: other error codes might be propagated from underlying socket calls.
 */
lwpa_error_t rdmnet_llrp_send_rdm_command(llrp_socket_t handle, const LwpaUuid *destination, const RdmBuffer *command,
                                          uint32_t *transaction_number)
{
  LlrpManagerSocketData *mgrdata;
  LlrpHeader header;
  LwpaSockaddr dest_addr;
  lwpa_error_t res;

  if (!handle || !destination || !command || !transaction_number || handle->socket_type != kLlrpSocketTypeManager)
  {
    return kLwpaErrInvalid;
  }

  mgrdata = GET_MANAGER_DATA(handle);

  header.dest_cid = *destination;
  header.sender_cid = handle->owner_cid;
  header.transaction_number = mgrdata->transaction_number;

  dest_addr.ip = state.kLlrpIpv4RequestAddr;
  dest_addr.port = LLRP_PORT;
  res = send_llrp_rdm(handle, &dest_addr, &header, command);

  if (res == kLwpaErrOk)
    *transaction_number = mgrdata->transaction_number++;
  return res;
}

/*! \brief Send an RDM response on an LLRP Target socket.
 *
 *  \param[in] handle LLRP manager socket handle on which to send an RDM command.
 *  \param[in] destination CID of LLRP Manager to which to send the command.
 *  \param[in] command RDM response to send.
 *  \param[in] transaction_number Transaction number of the corresponding LLRP RDM command.
 *  \return #kLwpaErrOk: Command sent successfully.\n
 *          #kLwpaErrInvalid: Invalid argument provided.\n
 *          Note: other error codes might be propagated from underlying socket calls.
 */
lwpa_error_t rdmnet_llrp_send_rdm_response(llrp_socket_t handle, const LwpaUuid *destination, const RdmBuffer *command,
                                           uint32_t transaction_number)
{
  LlrpHeader header;
  LwpaSockaddr dest_addr;

  if (!handle || !destination || !command || !transaction_number || handle->socket_type != kLlrpSocketTypeTarget)
  {
    return kLwpaErrInvalid;
  }

  header.dest_cid = *destination;
  header.sender_cid = handle->owner_cid;
  header.transaction_number = transaction_number;

  dest_addr.ip = state.kLlrpIpv4RespAddr;
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
    *result = kLwpaErrOk;

  llrp_socket_t next = socket->next;

  llrp_remove_socket_from_list(socket, &socket_list);
  llrp_socket_dealloc(socket);

  return next;
}

bool setup_target_netint(const LwpaSockaddr *saddr, const LwpaIpAddr *netint, LlrpTargetNetintInfo *netint_info)
{
  netint_info->sys_sock = create_sys_socket(&saddr, netint);
  if (netint_info->sys_sock == LWPA_SOCKET_INVALID)
  {
    return false;
  }

  if (!subscribe_multicast(netint_info->sys_sock, false, netint))
  {
    lwpa_close(netint_info->sys_sock);
    return false;
  }

  return true;
}

bool setup_target_netints(const LlrpTargetConfig *config, LlrpTarget *target)
{
  // Initialize LWPA targetets
  LwpaSockaddr saddr;
  saddr.ip = state.kLlrpIpv4RequestAddr;
  saddr.port = LLRP_PORT;

  bool ok = true;
  if (config->netint_arr && config->num_netints > 0)
  {
#if RDMNET_DYNAMIC_MEM
    target->netints = calloc(config->num_netints, sizeof(LlrpTargetNetintInfo));
    if (!target->netints)
      ok = false;
#else
    if (config->num_netints > RDMNET_LLRP_MAX_TARGET_NETINTS)
      ok = false;
#endif
    if (ok)
    {
      for (size_t i = 0; i < config->num_netints; ++i)
      {
        ok = setup_target_netint(&saddr, &state.sys_netints[i], &target->netints[i]);
        if (!ok)
          break;
      }
    }
  }
  else
  {
#if RDMNET_DYNAMIC_MEM
    target->netints = calloc(state.num_sys_netints, sizeof(LlrpTargetNetintInfo));
    if (!target->netints)
      ok = false;
#endif
  }

  if (!ok)
  {
#if RDMNET_DYNAMIC_MEM
    if (target->netints)
    {
#endif

#if RDMNET_DYNAMIC_MEM
      free(target->netints);
    }
#endif
  }
}

LlrpTarget *create_new_target(const LlrpTargetConfig *config)
{
  llrp_target_t new_handle = get_new_handle();
  if (new_handle == LLRP_TARGET_INVALID)
    return NULL;

  LlrpTarget *target = llrp_target_alloc();
  if (target)
  {
    bool ok = setup_target_netints(config, target);

    if (ok)
    {
      target->cid = config->cid;
      target->uid = config->uid;
    }
  }

  return target;
}

lwpa_socket_t create_sys_socket(const LwpaSockaddr *saddr, const LwpaIpAddr *netint)
{
  lwpa_socket_t sock = LWPA_SOCKET_INVALID;
  lwpa_error_t res = lwpa_socket((saddr->ip.type == kLwpaIpTypeV6) ? LWPA_AF_INET6 : LWPA_AF_INET, LWPA_DGRAM, &sock);
  bool valid = (res == kLwpaErrOk);

  if (valid)
  {
    int option = 1;  // Very important for our multicast needs
    valid = (0 == lwpa_setsockopt(sock, LWPA_SOL_SOCKET, LWPA_SO_REUSEADDR, (const void *)(&option), sizeof(option)));
  }

  if (valid)
  {
    if (saddr->ip.type == kLwpaIpTypeV4)
    {
      int value = LLRP_MULTICAST_TTL_VAL;
      valid =
          (0 == lwpa_setsockopt(sock, LWPA_IPPROTO_IP, LWPA_IP_MULTICAST_TTL, (const void *)(&value), sizeof(value)));
    }
    else
    {
      // TODO: add Ipv6 support
    }
  }

  if (valid)
  {
    // This one is critical for multicast sends to go over the correct interface.
    if (saddr->ip.type == kLwpaIpTypeV4)
    {
      valid = (0 == lwpa_setsockopt(sock, LWPA_IPPROTO_IP, LWPA_IP_MULTICAST_IF, (const void *)(netint),
                                    sizeof(LwpaIpAddr)));
    }
    else
    {
      // TODO: add Ipv6 support
    }
  }

  if (valid)
  {
    if (saddr->ip.type == kLwpaIpTypeV4)
    {
#if LLRP_BIND_TO_MCAST_ADDRESS
      // Bind socket to multicast address for Ipv4
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
      // TODO: add Ipv6 support
    }
  }

  if (!valid)
  {
    lwpa_close(sock);
    return LWPA_SOCKET_INVALID;
  }

  return sock;
}

bool subscribe_multicast(lwpa_socket_t lwpa_sock, bool manager, const LwpaIpAddr *netint)
{
  if (!lwpa_sock || !netint)
    return false;

  bool result = false;

  if (lwpaip_is_v4(netint))
  {
    LwpaMreq multireq;

    multireq.group = (manager ? state.kLlrpIpv4RespAddr : state.kLlrpIpv4RequestAddr);
    multireq.netint = *netint;
    result = (0 == lwpa_setsockopt(lwpa_sock, LWPA_IPPROTO_IP, LWPA_MCAST_JOIN_GROUP, (const void *)&multireq,
                                   sizeof(multireq)));
  }
  else
  {
    // TODO: add Ipv6 support
  }

  return result;
}

LwpaRbNode *node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode *)malloc(sizeof(LwpaRbNode));
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
  const RdmUid *a = (const RdmUid *)node_a->value;
  const RdmUid *b = (const RdmUid *)node_b->value;
  return rdm_uid_cmp(a, b);
}

LwpaRbNode *llrp_sock_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode *)malloc(sizeof(LwpaRbNode));
#else
  return lwpa_mempool_alloc(llrp_socket_rb_nodes);
#endif
}

void llrp_sock_node_free(LwpaRbNode *node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  lwpa_mempool_free(llrp_socket_rb_nodes, node);
#endif
}
