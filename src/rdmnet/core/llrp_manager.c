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

#include "rdmnet/core/llrp_manager.h"

#include "rdmnet/private/llrp_manager.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/util.h"

/*************************** Private constants *******************************/

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define llrp_manager_alloc() malloc(sizeof(LlrpManager))
#define llrp_manager_dealloc(ptr) free(ptr)
#else
#define llrp_manager_alloc() NULL
#define llrp_manager_dealloc()
#endif

/**************************** Private variables ******************************/

static struct LlrpManagerState
{
  LwpaRbTree managers;
  IntHandleManager handle_mgr;
} state;

/*********************** Private function prototypes *************************/

static LlrpManager *create_new_manager(const LlrpManagerConfig *config);
static void destroy_manager(LlrpManager *mgr);

static bool process_manager_state(LlrpManager *manager, int *timeout_ms, LlrpData *data);

// Functions for the known_uids tree in a manager
static LwpaRbNode *llrp_node_alloc();
static void llrp_node_dealloc(LwpaRbNode *node);
static int known_uid_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static void known_uid_clear_cb(const LwpaRbTree *self, LwpaRbNode *node);

// Functions for Manager discovery
static void halve_range(RdmUid *uid1, RdmUid *uid2);
static bool update_probe_range(LlrpManager *manager, KnownUid **uid_list);
static bool send_next_probe(LlrpManager *manager);

/*************************** Function definitions ****************************/

static void manager_dealloc(const LwpaRbTree *self, LwpaRbNode *node)
{
  LlrpManager *mgr = (LlrpManager *)node->value;
  if (mgr)

    free(node);
}

void rdmnet_llrp_manager_deinit()
{
#if RDMNET_DYNAMIC_MEM
  lwpa_rbtree_clear_with_cb(&state.managers, manager_dealloc);
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
lwpa_error_t rdmnet_llrp_create_manager_socket(const LlrpManagerConfig *config, llrp_manager_t *handle)
{
#if RDMNET_DYNAMIC_MEM
  LlrpManager *mgr = create_new_manager(config);

  if (!mgr)
    return kLwpaErrNoMem;

  *handle = mgr->handle;
  return kLwpaErrOk;
#else
  (void)config;
  (void)handle;
  return kLwpaErrNotImpl;
#endif
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

bool process_parsed_msg(LlrpManager *manager, const LlrpMessage *msg, LlrpData *data)
{
  switch(
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
  else
  {
    return false;
  }
}

int known_uid_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;
  const RdmUid *a = (const RdmUid *)node_a->value;
  const RdmUid *b = (const RdmUid *)node_b->value;
  return rdm_uid_cmp(a, b);
}

LwpaRbNode *llrp_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode *)malloc(sizeof(LwpaRbNode));
#else
  /* TODO */
  return NULL;
#endif
}

void llrp_node_dealloc(LwpaRbNode *node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
/* TODO */
#endif
}
