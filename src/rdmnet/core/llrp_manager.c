/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "rdmnet/core/llrp_manager.h"

#include "rdm/controller.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/llrp_manager.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/util.h"

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define llrp_manager_alloc() malloc(sizeof(LlrpManager))
#define llrp_manager_dealloc(ptr) free(ptr)
#else
#define llrp_manager_alloc() NULL
#define llrp_manager_dealloc()
#endif

#define INIT_CALLBACK_INFO(cbptr) ((cbptr)->which = kManagerCallbackNone)

/**************************** Private variables ******************************/

static struct LlrpManagerState
{
  LwpaRbTree managers;
  IntHandleManager handle_mgr;
} state;

/*********************** Private function prototypes *************************/

// Creating, destroying, and finding managers
static lwpa_error_t validate_llrp_manager_config(const LlrpManagerConfig *config);
static lwpa_error_t create_new_manager(const LlrpManagerConfig *config, LlrpManager **new_manager);
static lwpa_error_t get_manager(llrp_manager_t handle, LlrpManager **manager);
static void release_manager(LlrpManager *manager);
static void destroy_manager(LlrpManager *manager, bool remove_from_tree);
static bool manager_handle_in_use(int handle_val);

// Manager state processing
static void manager_socket_activity(const LwpaPollEvent *event, PolledSocketOpaqueData data);
static void manager_socket_error(LlrpManager *manager, lwpa_error_t err);
static void manager_data_received(LlrpManager *manager, const uint8_t *data, size_t size);
static void handle_llrp_message(LlrpManager *manager, const LlrpMessage *msg, ManagerCallbackDispatchInfo *cb);
static void process_manager_state(LlrpManager *manager, ManagerCallbackDispatchInfo *info);
static void fill_callback_info(const LlrpManager *manager, ManagerCallbackDispatchInfo *info);
static void deliver_callback(ManagerCallbackDispatchInfo *info);

// Functions for the known_uids tree in a manager
static LwpaRbNode *manager_node_alloc();
static void manager_node_dealloc(LwpaRbNode *node);
static int manager_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static int discovered_target_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);
static void discovered_target_clear_cb(const LwpaRbTree *self, LwpaRbNode *node);

// Functions for Manager discovery
static void halve_range(RdmUid *uid1, RdmUid *uid2);
static bool update_probe_range(LlrpManager *manager, KnownUid **uid_list);
static bool send_next_probe(LlrpManager *manager);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_llrp_manager_init()
{
  lwpa_rbtree_init(&state.managers, manager_cmp, manager_node_alloc, manager_node_dealloc);
  init_int_handle_manager(&state.handle_mgr, manager_handle_in_use);
  return kLwpaErrOk;
}

static void manager_dealloc(const LwpaRbTree *self, LwpaRbNode *node)
{
  (void)self;

  LlrpManager *manager = (LlrpManager *)node->value;
  if (manager)
    destroy_manager(manager, false);

  free(node);
}

void rdmnet_llrp_manager_deinit()
{
#if RDMNET_DYNAMIC_MEM
  lwpa_rbtree_clear_with_cb(&state.managers, manager_dealloc);
#endif
  memset(&state, 0, sizeof state);
}

/*! \brief Create a new LLRP manager instance.
 *
 *  LLRP managers can only be created when #RDMNET_DYNAMIC_MEM is defined nonzero. Otherwise, this
 *  function will always fail.
 *
 *  \param[in] config Configuration parameters for the LLRP manager to be created.
 *  \param[out] handle Handle to the newly-created manager instance.
 *  \return #kLwpaErrOk: Manager created successfully.
 *  \return #kLwpaErrInvalid: Invalid argument provided.
 *  \return #kLwpaErrNotInit: Module not initialized.
 *  \return #kLwpaErrNoMem: No memory to allocate additional manager instance.
 *  \return #kLwpaErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
lwpa_error_t rdmnet_llrp_manager_create(const LlrpManagerConfig *config, llrp_manager_t *handle)
{
  if (!config || !handle)
    return kLwpaErrInvalid;
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;

  lwpa_error_t res = validate_llrp_manager_config(config);
  if (res != kLwpaErrOk)
    return res;

  if (rdmnet_writelock())
  {
    // Attempt to create the LLRP manager, give it a unique handle and add it to the map.
    LlrpManager *manager;
    res = create_new_manager(config, &manager);
    if (res == kLwpaErrOk)
      *handle = manager->handle;

    rdmnet_writeunlock();
  }
  else
  {
    res = kLwpaErrSys;
  }
  return res;
}

/*! \brief Destroy an LLRP manager instance.
 *
 *  The handle will be invalidated for any future calls to API functions.
 *
 *  \param[in] handle Handle to manager to destroy.
 */
void rdmnet_llrp_manager_destroy(llrp_manager_t handle)
{
  LlrpManager *manager;
  lwpa_error_t res = get_manager(handle, &manager);
  if (res != kLwpaErrOk)
    return;

  manager->marked_for_destruction = true;
  release_manager(manager);
}

/*! \brief Start discovery on an LLRP manager.
 *
 *  Configure a manager to start discovery and send the first discovery message. Fails if a previous
 *  discovery process is still ongoing.
 *
 *  \param[in] handle Handle to LLRP manager on which to start discovery.
 *  \param[in] filter Discovery filter, made up of one or more of the LLRP_FILTERVAL_* constants
 *                    defined in rdmnet/defs.h
 *  \return #kLwpaErrOk: Discovery started successfully.
 *  \return #kLwpaErrInvalid: Invalid argument provided.
 *  \return #kLwpaErrNotInit: Module not initialized.
 *  \return #kLwpaErrNotFound: Handle is not associated with a valid LLRP manager instance.
 *  \return #kLwpaErrAlready: A discovery operation is already in progress.
 *  \return #kLwpaErrSys: An internal library or system call error occurred.
 */
lwpa_error_t rdmnet_llrp_start_discovery(llrp_manager_t handle, uint16_t filter)
{
  LlrpManager *manager;
  lwpa_error_t res = get_manager(handle, &manager);
  if (res == kLwpaErrOk)
  {
    if (!manager->discovery_active)
    {
      manager->cur_range_low.manu = 0;
      manager->cur_range_low.id = 0;
      manager->cur_range_high = kBroadcastUid;
      manager->num_clean_sends = 0;
      manager->discovery_active = true;
      manager->disc_filter = filter;
      lwpa_rbtree_init(&manager->discovered_targets, discovered_target_cmp, manager_node_alloc, manager_node_dealloc);

      if (!send_next_probe(manager))
      {
        manager->discovery_active = false;
        res = kLwpaErrSys;
      }
    }
    else
    {
      res = kLwpaErrAlready;
    }
    release_manager(manager);
  }
  return res;
}

/*! \brief Stop discovery on an LLRP manager.
 *
 *  Clears all discovery state and known discovered targets.
 *
 *  \param[in] handle Handle to LLRP manager on which to stop discovery.
 *  \return #kLwpaErrOk: Discovery stopped successfully.
 *  \return #kLwpaErrInvalid: Invalid argument provided.
 *  \return #kLwpaErrNotInit: Module not initialized.
 *  \return #kLwpaErrNotFound: Handle is not associated with a valid LLRP manager instance.
 */
lwpa_error_t rdmnet_llrp_stop_discovery(llrp_manager_t handle)
{
  LlrpManager *manager;
  lwpa_error_t res = get_manager(handle, &manager);
  if (res == kLwpaErrOk)
  {
    if (manager->discovery_active)
    {
      lwpa_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
      manager->discovery_active = false;
    }
  }

  return res;
}

/*! \brief Send an RDM command from an LLRP manager.
 *
 *  On success, provides the transaction number to correlate with a response.
 *
 *  \param[in] handle Handle to LLRP manager from which to send an RDM command.
 *  \param[in] command Command to send.
 *  \param[out] transaction_num Filled in on success with the transaction number of the command.
 *  \return #kLwpaErrOk: Command sent successfully.
 *  \return #kLwpaErrInvalid: Invalid argument provided.
 *  \return #kLwpaErrNotInit: Module not initialized.
 *  \return #kLwpaErrNotFound: Handle is not associated with a valid LLRP manager instance.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
lwpa_error_t rdmnet_llrp_send_rdm_command(llrp_manager_t handle, const LlrpLocalRdmCommand *command,
                                          uint32_t *transaction_num)
{
  if (!command)
    return kLwpaErrInvalid;

  LlrpManager *manager;
  lwpa_error_t res = get_manager(handle, &manager);
  if (res == kLwpaErrOk)
  {
    RdmCommand rdm_to_send = command->rdm;
    rdm_to_send.source_uid = manager->uid;
    rdm_to_send.port_id = 1;
    rdm_to_send.transaction_num = (uint8_t)(manager->transaction_number & 0xffu);

    RdmBuffer cmd_buf;
    res = rdmctl_pack_command(&rdm_to_send, &cmd_buf);
    if (res == kLwpaErrOk)
    {
      LlrpHeader header;
      header.dest_cid = command->dest_cid;
      header.sender_cid = manager->cid;
      header.transaction_number = manager->transaction_number;

      res = send_llrp_rdm_command(manager->sys_sock, manager->send_buf, (manager->ip_type == kLwpaIpTypeV6), &header,
                                  &cmd_buf);
      if (res == kLwpaErrOk && transaction_num)
        *transaction_num = manager->transaction_number++;
    }
    release_manager(manager);
  }
  return res;
}

void rdmnet_llrp_manager_tick()
{
  if (!rdmnet_core_initialized())
    return;

  // Remove any managers marked for destruction.
  if (rdmnet_writelock())
  {
    LlrpManager *destroy_list = NULL;
    LlrpManager **next_destroy_list_entry = &destroy_list;

    LwpaRbIter manager_iter;
    lwpa_rbiter_init(&manager_iter);

    LlrpManager *manager = (LlrpManager *)lwpa_rbiter_first(&manager_iter, &state.managers);
    while (manager)
    {
      // Can't destroy while iterating as that would invalidate the iterator
      // So the managers are added to a linked list of manager pending destruction
      if (manager->marked_for_destruction)
      {
        *next_destroy_list_entry = manager;
        manager->next_to_destroy = NULL;
        next_destroy_list_entry = &manager->next_to_destroy;
      }
      manager = (LlrpManager *)lwpa_rbiter_next(&manager_iter);
    }

    // Now do the actual destruction
    if (destroy_list)
    {
      LlrpManager *to_destroy = destroy_list;
      while (to_destroy)
      {
        LlrpManager *next = to_destroy->next_to_destroy;
        destroy_manager(to_destroy, true);
        to_destroy = next;
      }
    }

    rdmnet_writeunlock();
  }

  ManagerCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  // Do the rest of the periodic functionality with a read lock
  if (rdmnet_readlock())
  {
    LwpaRbIter manager_iter;
    lwpa_rbiter_init(&manager_iter);
    LlrpManager *manager = (LlrpManager *)lwpa_rbiter_first(&manager_iter, &state.managers);

    while (manager)
    {
      process_manager_state(manager, &cb);
      if (cb.which != kManagerCallbackNone)
        break;
      manager = (LlrpManager *)lwpa_rbiter_next(&manager_iter);
    }

    rdmnet_readunlock();
  }

  deliver_callback(&cb);
}

void halve_range(RdmUid *uid1, RdmUid *uid2)
{
  uint64_t uval1 = ((((uint64_t)uid1->manu) << 32) | uid1->id);
  uint64_t uval2 = ((((uint64_t)uid2->manu) << 32) | uid2->id);
  uint64_t umid = uval1 + uval2 / 2;

  uid2->manu = (uint16_t)((umid >> 32) & 0xffffu);
  uid2->id = (uint32_t)(umid & 0xffffffffu);
}

bool update_probe_range(LlrpManager *manager, KnownUid **uid_list)
{
  if (manager->num_clean_sends >= 3)
  {
    // We are finished with a range; move on to the next range.
    if (RDM_UID_IS_BROADCAST(&manager->cur_range_high))
    {
      // We're done with discovery.
      return false;
    }
    else
    {
      // The new range starts at the old upper limit + 1, and ends at the top of the UID space.
      if (manager->cur_range_high.id == 0xffffffffu)
      {
        manager->cur_range_low.manu = manager->cur_range_high.manu + 1;
        manager->cur_range_low.id = 0;
      }
      else
      {
        manager->cur_range_low.manu = manager->cur_range_high.manu;
        manager->cur_range_low.id = manager->cur_range_high.id + 1;
      }
      manager->cur_range_high = kBroadcastUid;
      manager->num_clean_sends = 0;
    }
  }

  // Determine how many known UIDs are in the current range
  KnownUid *list_begin = NULL;
  KnownUid *last_uid = NULL;
  unsigned int uids_in_range = 0;

  LwpaRbIter iter;
  lwpa_rbiter_init(&iter);
  DiscoveredTargetInternal *cur_target =
      (DiscoveredTargetInternal *)lwpa_rbiter_first(&iter, &manager->discovered_targets);
  while (cur_target && (RDM_UID_CMP(&cur_target->known_uid.uid, &manager->cur_range_high) <= 0))
  {
    if (last_uid)
    {
      if (++uids_in_range <= LLRP_KNOWN_UID_SIZE)
      {
        last_uid->next = &cur_target->known_uid;
        cur_target->known_uid.next = NULL;
        last_uid = &cur_target->known_uid;
      }
      else
      {
        halve_range(&manager->cur_range_low, &manager->cur_range_high);
        return update_probe_range(manager, uid_list);
      }
    }
    else if (RDM_UID_CMP(&cur_target->known_uid.uid, &manager->cur_range_low) >= 0)
    {
      list_begin = &cur_target->known_uid;
      cur_target->known_uid.next = NULL;
      last_uid = &cur_target->known_uid;
      ++uids_in_range;
    }
    cur_target = (DiscoveredTargetInternal *)lwpa_rbiter_next(&iter);
  }
  *uid_list = list_begin;
  return true;
}

bool send_next_probe(LlrpManager *manager)
{
  KnownUid *list_head;

  if (update_probe_range(manager, &list_head))
  {
    LlrpHeader header;
    header.sender_cid = manager->cid;
    header.dest_cid = kLlrpBroadcastCid;
    header.transaction_number = manager->transaction_number++;

    LocalProbeRequest request;
    request.filter = manager->disc_filter;
    request.lower_uid = manager->cur_range_low;
    request.upper_uid = manager->cur_range_high;
    request.uid_list = list_head;

    lwpa_error_t send_res = send_llrp_probe_request(manager->sys_sock, manager->send_buf,
                                                    (manager->ip_type == kLwpaIpTypeV6), &header, &request);
    if (send_res == kLwpaErrOk)
    {
      lwpa_timer_start(&manager->disc_timer, LLRP_TIMEOUT_MS);
      ++manager->num_clean_sends;
      return true;
    }
    else
    {
      lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
               RDMNET_LOG_MSG("Sending LLRP probe request failed with error: '%s'"), lwpa_strerror(send_res));
      return false;
    }
  }
  else
  {
    // We are done with discovery.
    return false;
  }
}

void discovered_target_clear_cb(const LwpaRbTree *self, LwpaRbNode *node)
{
  DiscoveredTargetInternal *target = (DiscoveredTargetInternal *)node->value;
  while (target)
  {
    DiscoveredTargetInternal *next_target = target->next;
    free(target);
    target = next_target;
  }
  manager_node_dealloc(node);
  (void)self;
}

void process_manager_state(LlrpManager *manager, ManagerCallbackDispatchInfo *cb)
{
  if (manager->discovery_active)
  {
    if (lwpa_timer_is_expired(&manager->disc_timer))
    {
      if (!send_next_probe(manager))
      {
        fill_callback_info(manager, cb);
        cb->which = kManagerCallbackDiscoveryFinished;
        lwpa_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
        manager->discovery_active = false;
      }
    }
  }
}

void manager_socket_activity(const LwpaPollEvent *event, PolledSocketOpaqueData data)
{
  static uint8_t llrp_manager_recv_buf[LLRP_MAX_MESSAGE_SIZE];

  if (event->events & LWPA_POLL_ERR)
  {
    manager_socket_error((LlrpManager *)data.ptr, event->err);
  }
  else if (event->events & LWPA_POLL_IN)
  {
    int recv_res = lwpa_recv(event->socket, llrp_manager_recv_buf, LLRP_MAX_MESSAGE_SIZE, 0);
    if (recv_res <= 0)
    {
      if (recv_res != kLwpaErrMsgSize)
      {
        manager_socket_error((LlrpManager *)data.ptr, event->err);
      }
    }
    else
    {
      manager_data_received((LlrpManager *)data.ptr, llrp_manager_recv_buf, recv_res);
    }
  }
}

void manager_socket_error(LlrpManager *manager, lwpa_error_t err)
{
  (void)manager;
  (void)err;
  // TODO
}

void manager_data_received(LlrpManager *manager, const uint8_t *data, size_t size)
{
  ManagerCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  if (rdmnet_readlock())
  {
    LlrpMessage msg;
    LlrpMessageInterest interest;
    interest.my_cid = manager->cid;
    interest.interested_in_probe_reply = true;
    interest.interested_in_probe_request = false;

    if (parse_llrp_message(data, size, &interest, &msg))
    {
      handle_llrp_message(manager, &msg, &cb);
    }
    rdmnet_readunlock();
  }

  deliver_callback(&cb);
}

void handle_llrp_message(LlrpManager *manager, const LlrpMessage *msg, ManagerCallbackDispatchInfo *cb)
{
  switch (msg->vector)
  {
    case VECTOR_LLRP_PROBE_REPLY:
    {
      const DiscoveredLlrpTarget *target = LLRP_MSG_GET_PROBE_REPLY(msg);

      if (manager->discovery_active && LWPA_UUID_CMP(&msg->header.dest_cid, &manager->cid) == 0)
      {
        DiscoveredTargetInternal *new_target = (DiscoveredTargetInternal *)malloc(sizeof(DiscoveredTargetInternal));
        if (new_target)
        {
          new_target->known_uid.uid = target->uid;
          new_target->known_uid.next = NULL;
          new_target->cid = msg->header.sender_cid;
          new_target->next = NULL;

          DiscoveredTargetInternal *found =
              (DiscoveredTargetInternal *)lwpa_rbtree_find(&manager->discovered_targets, new_target);
          if (found)
          {
            // A target has responded that has the same UID as one already in our tree. This is not
            // necessarily an error in LLRP if it has a different CID.
            while (true)
            {
              if (LWPA_UUID_CMP(&found->cid, &new_target->cid) == 0)
              {
                // This target has already responded. It is not new.
                free(new_target);
                new_target = NULL;
                break;
              }
              if (!found->next)
                break;
              found = found->next;
            }
            if (new_target)
            {
              // Insert at the end of the list.
              found->next = new_target;
            }
          }
          else
          {
            // Newly discovered Target with a new UID.
            lwpa_rbtree_insert(&manager->discovered_targets, new_target);
          }

          if (new_target)
          {
            fill_callback_info(manager, cb);
            cb->which = kManagerCallbackTargetDiscovered;
            cb->args.target_discovered.target = &msg->data.probe_reply;
          }
        }
      }
      break;
    }
    case VECTOR_LLRP_RDM_CMD:
    {
      LlrpRemoteRdmResponse *remote_resp = &cb->args.resp_received.resp;
      if (kLwpaErrOk == rdmctl_unpack_response(LLRP_MSG_GET_RDM(msg), &remote_resp->rdm))
      {
        remote_resp->seq_num = msg->header.transaction_number;
        remote_resp->src_cid = msg->header.sender_cid;

        fill_callback_info(manager, cb);
        cb->which = kManagerCallbackRdmRespReceived;
      }
      break;
    }
  }
}

void fill_callback_info(const LlrpManager *manager, ManagerCallbackDispatchInfo *info)
{
  info->handle = manager->handle;
  info->cbs = manager->callbacks;
  info->context = manager->callback_context;
}

void deliver_callback(ManagerCallbackDispatchInfo *info)
{
  switch (info->which)
  {
    case kManagerCallbackTargetDiscovered:
      if (info->cbs.target_discovered)
        info->cbs.target_discovered(info->handle, info->args.target_discovered.target, info->context);
      break;
    case kManagerCallbackDiscoveryFinished:
      if (info->cbs.discovery_finished)
        info->cbs.discovery_finished(info->handle, info->context);
      break;
    case kManagerCallbackRdmRespReceived:
      if (info->cbs.rdm_resp_received)
        info->cbs.rdm_resp_received(info->handle, &info->args.resp_received.resp, info->context);
      break;
    case kManagerCallbackNone:
    default:
      break;
  }
}

lwpa_error_t setup_manager_socket(LlrpManager *manager)
{
  lwpa_error_t res = create_llrp_socket(manager->ip_type, manager->netint_index, true, &manager->sys_sock);
  if (res != kLwpaErrOk)
    return res;

  manager->poll_info.callback = manager_socket_activity;
  manager->poll_info.data.ptr = manager;
  res = rdmnet_core_add_polled_socket(manager->sys_sock, LWPA_POLL_IN, &manager->poll_info);
  if (res != kLwpaErrOk)
    lwpa_close(manager->sys_sock);
  return res;
}

lwpa_error_t validate_llrp_manager_config(const LlrpManagerConfig *config)
{
  if ((config->ip_type != kLwpaIpTypeV4 && config->ip_type != kLwpaIpTypeV6) || LWPA_UUID_IS_NULL(&config->cid))
    return kLwpaErrInvalid;
  return kLwpaErrOk;
}

lwpa_error_t create_new_manager(const LlrpManagerConfig *config, LlrpManager **new_manager)
{
  lwpa_error_t res = kLwpaErrNoMem;

  llrp_manager_t new_handle = get_next_int_handle(&state.handle_mgr);
  if (new_handle == LLRP_MANAGER_INVALID)
    return res;

  LlrpManager *manager = (LlrpManager *)llrp_manager_alloc();
  if (manager)
  {
    manager->ip_type = config->ip_type;
    manager->netint_index = config->netint_index;
    res = setup_manager_socket(manager);
    if (res == kLwpaErrOk)
    {
      manager->handle = new_handle;
      if (0 != lwpa_rbtree_insert(&state.managers, manager))
      {
        manager->cid = config->cid;
        manager->uid.manu = 0x8000 | config->manu_id;
        manager->uid.id = (uint32_t)rand();

        manager->transaction_number = 0;
        manager->discovery_active = false;

        manager->callbacks = config->callbacks;
        manager->callback_context = config->callback_context;

        manager->marked_for_destruction = false;
        manager->next_to_destroy = NULL;

        *new_manager = manager;
      }
      else
      {
        llrp_manager_dealloc(manager);
        res = kLwpaErrNoMem;
      }
    }
    else
    {
      llrp_manager_dealloc(manager);
    }
  }
  return res;
}

lwpa_error_t get_manager(llrp_manager_t handle, LlrpManager **manager)
{
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;
  if (!rdmnet_readlock())
    return kLwpaErrSys;

  LlrpManager *found_manager = (LlrpManager *)lwpa_rbtree_find(&state.managers, &handle);
  if (!found_manager || found_manager->marked_for_destruction)
  {
    rdmnet_readunlock();
    return kLwpaErrNotFound;
  }
  *manager = found_manager;
  return kLwpaErrOk;
}

void release_manager(LlrpManager *manager)
{
  (void)manager;
  rdmnet_readunlock();
}

void destroy_manager(LlrpManager *manager, bool remove_from_tree)
{
  if (manager)
  {
    rdmnet_core_remove_polled_socket(manager->sys_sock);
    lwpa_close(manager->sys_sock);
    if (manager->discovery_active)
    {
      lwpa_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
    }
    if (remove_from_tree)
    {
      lwpa_rbtree_remove(&state.managers, manager);
    }
    llrp_manager_dealloc(manager);
  }
}

/* Callback for IntHandleManager to determine whether a handle is in use */
bool manager_handle_in_use(int handle_val)
{
  return lwpa_rbtree_find(&state.managers, &handle_val);
}

int manager_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;
  const LlrpManager *a = (const LlrpManager *)node_a->value;
  const LlrpManager *b = (const LlrpManager *)node_b->value;
  return a->handle - b->handle;
}

int discovered_target_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;
  const DiscoveredTargetInternal *a = (const DiscoveredTargetInternal *)node_a->value;
  const DiscoveredTargetInternal *b = (const DiscoveredTargetInternal *)node_b->value;
  return RDM_UID_CMP(&a->known_uid.uid, &b->known_uid.uid);
}

LwpaRbNode *manager_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode *)malloc(sizeof(LwpaRbNode));
#else
  /* TODO */
  return NULL;
#endif
}

void manager_node_dealloc(LwpaRbNode *node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
/* TODO */
#endif
}
