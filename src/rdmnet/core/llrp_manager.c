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
  EtcPalRbTree managers;
  EtcPalRbTree managers_by_cid_and_netint;
  IntHandleManager handle_mgr;
} state;

/*********************** Private function prototypes *************************/

// Creating, destroying, and finding managers
static etcpal_error_t validate_llrp_manager_config(const LlrpManagerConfig* config);
static etcpal_error_t create_new_manager(const LlrpManagerConfig* config, LlrpManager** new_manager);
static etcpal_error_t setup_manager_socket(LlrpManager* manager);
static etcpal_error_t get_manager(llrp_manager_t handle, LlrpManager** manager);
static void release_manager(LlrpManager* manager);
static void destroy_manager(LlrpManager* manager, bool remove_from_tree);
static void destroy_manager_socket(LlrpManager* manager);
static bool manager_handle_in_use(int handle_val);

// Manager state processing
static void handle_llrp_message(LlrpManager* manager, const LlrpMessage* msg, ManagerCallbackDispatchInfo* cb);
static void process_manager_state(LlrpManager* manager, ManagerCallbackDispatchInfo* info);
static void fill_callback_info(const LlrpManager* manager, ManagerCallbackDispatchInfo* info);
static void deliver_callback(ManagerCallbackDispatchInfo* info);

// Functions for the known_uids tree in a manager
static EtcPalRbNode* manager_node_alloc();
static void manager_node_dealloc(EtcPalRbNode* node);
static int manager_cmp_by_handle(const EtcPalRbTree* self, const EtcPalRbNode* node_a, const EtcPalRbNode* node_b);
static int manager_cmp_by_cid_and_netint(const EtcPalRbTree* self, const EtcPalRbNode* node_a,
                                         const EtcPalRbNode* node_b);
static int discovered_target_cmp(const EtcPalRbTree* self, const EtcPalRbNode* node_a, const EtcPalRbNode* node_b);
static void discovered_target_clear_cb(const EtcPalRbTree* self, EtcPalRbNode* node);

// Functions for Manager discovery
static void halve_range(RdmUid* uid1, RdmUid* uid2);
static bool update_probe_range(LlrpManager* manager, KnownUid** uid_list);
static bool send_next_probe(LlrpManager* manager);

/*************************** Function definitions ****************************/

etcpal_error_t rdmnet_llrp_manager_init()
{
  etcpal_rbtree_init(&state.managers, manager_cmp_by_handle, manager_node_alloc, manager_node_dealloc);
  etcpal_rbtree_init(&state.managers_by_cid_and_netint, manager_cmp_by_cid_and_netint, manager_node_alloc,
                     manager_node_dealloc);
  init_int_handle_manager(&state.handle_mgr, manager_handle_in_use);
  return kEtcPalErrOk;
}

static void manager_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  (void)self;

  LlrpManager* manager = (LlrpManager*)node->value;
  if (manager)
    destroy_manager(manager, false);

  free(node);
}

void rdmnet_llrp_manager_deinit()
{
#if RDMNET_DYNAMIC_MEM
  etcpal_rbtree_clear_with_cb(&state.managers, manager_dealloc);
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
 *  \return #kEtcPalErrOk: Manager created successfully.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrNoMem: No memory to allocate additional manager instance.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t rdmnet_llrp_manager_create(const LlrpManagerConfig* config, llrp_manager_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = validate_llrp_manager_config(config);
  if (res != kEtcPalErrOk)
    return res;

  if (rdmnet_writelock())
  {
    // Attempt to create the LLRP manager, give it a unique handle and add it to the map.
    LlrpManager* manager;
    res = create_new_manager(config, &manager);
    if (res == kEtcPalErrOk)
      *handle = manager->keys.handle;

    rdmnet_writeunlock();
  }
  else
  {
    res = kEtcPalErrSys;
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
  LlrpManager* manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res != kEtcPalErrOk)
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
 *  \return #kEtcPalErrOk: Discovery started successfully.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 *  \return #kEtcPalErrAlready: A discovery operation is already in progress.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_llrp_start_discovery(llrp_manager_t handle, uint16_t filter)
{
  LlrpManager* manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res == kEtcPalErrOk)
  {
    if (!manager->discovery_active)
    {
      manager->cur_range_low.manu = 0;
      manager->cur_range_low.id = 0;
      manager->cur_range_high = kBroadcastUid;
      manager->num_clean_sends = 0;
      manager->discovery_active = true;
      manager->disc_filter = filter;
      etcpal_rbtree_init(&manager->discovered_targets, discovered_target_cmp, manager_node_alloc, manager_node_dealloc);

      if (!send_next_probe(manager))
      {
        manager->discovery_active = false;
        res = kEtcPalErrSys;
      }
    }
    else
    {
      res = kEtcPalErrAlready;
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
 *  \return #kEtcPalErrOk: Discovery stopped successfully.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 */
etcpal_error_t rdmnet_llrp_stop_discovery(llrp_manager_t handle)
{
  LlrpManager* manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res == kEtcPalErrOk)
  {
    if (manager->discovery_active)
    {
      etcpal_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
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
 *  \return #kEtcPalErrOk: Command sent successfully.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrNotInit: Module not initialized.
 *  \return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t rdmnet_llrp_send_rdm_command(llrp_manager_t handle, const LlrpLocalRdmCommand* command,
                                            uint32_t* transaction_num)
{
  if (!command)
    return kEtcPalErrInvalid;

  LlrpManager* manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res == kEtcPalErrOk)
  {
    RdmCommand rdm_to_send = command->rdm;
    rdm_to_send.source_uid = manager->uid;
    rdm_to_send.port_id = 1;
    rdm_to_send.transaction_num = (uint8_t)(manager->transaction_number & 0xffu);

    RdmBuffer cmd_buf;
    res = rdmctl_pack_command(&rdm_to_send, &cmd_buf);
    if (res == kEtcPalErrOk)
    {
      LlrpHeader header;
      header.dest_cid = command->dest_cid;
      header.sender_cid = manager->keys.cid;
      header.transaction_number = manager->transaction_number;

      res = send_llrp_rdm_command(manager->send_sock, manager->send_buf,
                                  (manager->keys.netint.ip_type == kEtcPalIpTypeV6), &header, &cmd_buf);
      if (res == kEtcPalErrOk && transaction_num)
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
    LlrpManager* destroy_list = NULL;
    LlrpManager** next_destroy_list_entry = &destroy_list;

    EtcPalRbIter manager_iter;
    etcpal_rbiter_init(&manager_iter);

    LlrpManager* manager = (LlrpManager*)etcpal_rbiter_first(&manager_iter, &state.managers);
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
      manager = (LlrpManager*)etcpal_rbiter_next(&manager_iter);
    }

    // Now do the actual destruction
    if (destroy_list)
    {
      LlrpManager* to_destroy = destroy_list;
      while (to_destroy)
      {
        LlrpManager* next = to_destroy->next_to_destroy;
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
    EtcPalRbIter manager_iter;
    etcpal_rbiter_init(&manager_iter);
    LlrpManager* manager = (LlrpManager*)etcpal_rbiter_first(&manager_iter, &state.managers);

    while (manager)
    {
      process_manager_state(manager, &cb);
      if (cb.which != kManagerCallbackNone)
        break;
      manager = (LlrpManager*)etcpal_rbiter_next(&manager_iter);
    }

    rdmnet_readunlock();
  }

  deliver_callback(&cb);
}

void halve_range(RdmUid* uid1, RdmUid* uid2)
{
  uint64_t uval1 = ((((uint64_t)uid1->manu) << 32) | uid1->id);
  uint64_t uval2 = ((((uint64_t)uid2->manu) << 32) | uid2->id);
  uint64_t umid = uval1 + uval2 / 2;

  uid2->manu = (uint16_t)((umid >> 32) & 0xffffu);
  uid2->id = (uint32_t)(umid & 0xffffffffu);
}

bool update_probe_range(LlrpManager* manager, KnownUid** uid_list)
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
        manager->cur_range_low.manu = (uint16_t)(manager->cur_range_high.manu + 1u);
        manager->cur_range_low.id = 0;
      }
      else
      {
        manager->cur_range_low.manu = manager->cur_range_high.manu;
        manager->cur_range_low.id = (uint32_t)(manager->cur_range_high.id + 1u);
      }
      manager->cur_range_high = kBroadcastUid;
      manager->num_clean_sends = 0;
    }
  }

  // Determine how many known UIDs are in the current range
  KnownUid* list_begin = NULL;
  KnownUid* last_uid = NULL;
  unsigned int uids_in_range = 0;

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  DiscoveredTargetInternal* cur_target =
      (DiscoveredTargetInternal*)etcpal_rbiter_first(&iter, &manager->discovered_targets);
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
    cur_target = (DiscoveredTargetInternal*)etcpal_rbiter_next(&iter);
  }
  *uid_list = list_begin;
  return true;
}

bool send_next_probe(LlrpManager* manager)
{
  KnownUid* list_head;

  if (update_probe_range(manager, &list_head))
  {
    LlrpHeader header;
    header.sender_cid = manager->keys.cid;
    header.dest_cid = kLlrpBroadcastCid;
    header.transaction_number = manager->transaction_number++;

    LocalProbeRequest request;
    request.filter = manager->disc_filter;
    request.lower_uid = manager->cur_range_low;
    request.upper_uid = manager->cur_range_high;
    request.uid_list = list_head;

    etcpal_error_t send_res = send_llrp_probe_request(
        manager->send_sock, manager->send_buf, (manager->keys.netint.ip_type == kEtcPalIpTypeV6), &header, &request);
    if (send_res == kEtcPalErrOk)
    {
      etcpal_timer_start(&manager->disc_timer, LLRP_TIMEOUT_MS);
      ++manager->num_clean_sends;
      return true;
    }
    else
    {
      etcpal_log(rdmnet_log_params, ETCPAL_LOG_WARNING,
                 RDMNET_LOG_MSG("Sending LLRP probe request failed with error: '%s'"), etcpal_strerror(send_res));
      return false;
    }
  }
  else
  {
    // We are done with discovery.
    return false;
  }
}

void discovered_target_clear_cb(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  DiscoveredTargetInternal* target = (DiscoveredTargetInternal*)node->value;
  while (target)
  {
    DiscoveredTargetInternal* next_target = target->next;
    free(target);
    target = next_target;
  }
  manager_node_dealloc(node);
  (void)self;
}

void process_manager_state(LlrpManager* manager, ManagerCallbackDispatchInfo* cb)
{
  if (manager->discovery_active)
  {
    if (etcpal_timer_is_expired(&manager->disc_timer))
    {
      if (!send_next_probe(manager))
      {
        fill_callback_info(manager, cb);
        cb->which = kManagerCallbackDiscoveryFinished;
        etcpal_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
        manager->discovery_active = false;
      }
    }
  }
}

void manager_data_received(const uint8_t* data, size_t data_size, const LlrpNetintId* netint)
{
  (void)netint;

  ManagerCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  LlrpManagerKeys keys;
  if (get_llrp_destination_cid(data, data_size, &keys.cid))
  {
    keys.netint = *netint;
    bool manager_found = false;

    if (rdmnet_readlock())
    {
      LlrpManager* manager = (LlrpManager*)etcpal_rbtree_find(&state.managers_by_cid_and_netint, &keys);
      if (manager)
      {
        manager_found = true;

        LlrpMessage msg;
        LlrpMessageInterest interest;
        interest.my_cid = keys.cid;
        interest.interested_in_probe_reply = true;
        interest.interested_in_probe_request = false;

        if (parse_llrp_message(data, data_size, &interest, &msg))
        {
          handle_llrp_message(manager, &msg, &cb);
        }
      }
      rdmnet_readunlock();
    }

    if (!manager_found && etcpal_can_log(rdmnet_log_params, ETCPAL_LOG_DEBUG))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(cid_str, &keys.cid);
      etcpal_log(rdmnet_log_params, ETCPAL_LOG_DEBUG,
                 RDMNET_LOG_MSG("Ignoring LLRP message addressed to unknown LLRP Manager %s"), cid_str);
    }
  }

  deliver_callback(&cb);
}

void handle_llrp_message(LlrpManager* manager, const LlrpMessage* msg, ManagerCallbackDispatchInfo* cb)
{
  switch (msg->vector)
  {
    case VECTOR_LLRP_PROBE_REPLY:
    {
      const DiscoveredLlrpTarget* target = LLRP_MSG_GET_PROBE_REPLY(msg);

      if (manager->discovery_active && ETCPAL_UUID_CMP(&msg->header.dest_cid, &manager->keys.cid) == 0)
      {
        DiscoveredTargetInternal* new_target = (DiscoveredTargetInternal*)malloc(sizeof(DiscoveredTargetInternal));
        if (new_target)
        {
          new_target->known_uid.uid = target->uid;
          new_target->known_uid.next = NULL;
          new_target->cid = msg->header.sender_cid;
          new_target->next = NULL;

          DiscoveredTargetInternal* found =
              (DiscoveredTargetInternal*)etcpal_rbtree_find(&manager->discovered_targets, new_target);
          if (found)
          {
            // A target has responded that has the same UID as one already in our tree. This is not
            // necessarily an error in LLRP if it has a different CID.
            while (true)
            {
              if (ETCPAL_UUID_CMP(&found->cid, &new_target->cid) == 0)
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
            etcpal_rbtree_insert(&manager->discovered_targets, new_target);
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
      LlrpRemoteRdmResponse* remote_resp = &cb->args.resp_received.resp;
      if (kEtcPalErrOk == rdmctl_unpack_response(LLRP_MSG_GET_RDM(msg), &remote_resp->rdm))
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

void fill_callback_info(const LlrpManager* manager, ManagerCallbackDispatchInfo* info)
{
  info->handle = manager->keys.handle;
  info->cbs = manager->callbacks;
  info->context = manager->callback_context;
}

void deliver_callback(ManagerCallbackDispatchInfo* info)
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

etcpal_error_t setup_manager_socket(LlrpManager* manager)
{
  etcpal_error_t res = get_llrp_send_socket(&manager->keys.netint, &manager->send_sock);
  if (res == kEtcPalErrOk)
  {
    res = llrp_recv_netint_add(&manager->keys.netint, kLlrpSocketTypeManager);
    if (res != kEtcPalErrOk)
      release_llrp_send_socket(&manager->keys.netint);
  }
  return res;
}

etcpal_error_t validate_llrp_manager_config(const LlrpManagerConfig* config)
{
  if ((config->netint.ip_type != kEtcPalIpTypeV4 && config->netint.ip_type != kEtcPalIpTypeV6) ||
      ETCPAL_UUID_IS_NULL(&config->cid))
  {
    return kEtcPalErrInvalid;
  }
  return kEtcPalErrOk;
}

etcpal_error_t create_new_manager(const LlrpManagerConfig* config, LlrpManager** new_manager)
{
  etcpal_error_t res = kEtcPalErrNoMem;

  llrp_manager_t new_handle = get_next_int_handle(&state.handle_mgr);
  if (new_handle == LLRP_MANAGER_INVALID)
    return res;

  LlrpManager* manager = (LlrpManager*)llrp_manager_alloc();
  if (manager)
  {
    manager->keys.netint = config->netint;
    res = setup_manager_socket(manager);
    if (res == kEtcPalErrOk)
    {
      manager->keys.handle = new_handle;
      manager->keys.cid = config->cid;
      res = etcpal_rbtree_insert(&state.managers, manager);
      if (res == kEtcPalErrOk)
      {
        res = etcpal_rbtree_insert(&state.managers_by_cid_and_netint, manager);
        if (res == kEtcPalErrOk)
        {
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
          etcpal_rbtree_remove(&state.managers, manager);
          destroy_manager_socket(manager);
          llrp_manager_dealloc(manager);
        }
      }
      else
      {
        destroy_manager_socket(manager);
        llrp_manager_dealloc(manager);
        res = kEtcPalErrNoMem;
      }
    }
    else
    {
      llrp_manager_dealloc(manager);
    }
  }
  return res;
}

etcpal_error_t get_manager(llrp_manager_t handle, LlrpManager** manager)
{
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;
  if (!rdmnet_readlock())
    return kEtcPalErrSys;

  LlrpManager* found_manager = (LlrpManager*)etcpal_rbtree_find(&state.managers, &handle);
  if (!found_manager || found_manager->marked_for_destruction)
  {
    rdmnet_readunlock();
    return kEtcPalErrNotFound;
  }
  *manager = found_manager;
  return kEtcPalErrOk;
}

void release_manager(LlrpManager* manager)
{
  (void)manager;
  rdmnet_readunlock();
}

void destroy_manager(LlrpManager* manager, bool remove_from_tree)
{
  if (manager)
  {
    destroy_manager_socket(manager);
    if (manager->discovery_active)
    {
      etcpal_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
    }
    if (remove_from_tree)
    {
      etcpal_rbtree_remove(&state.managers, manager);
    }
    llrp_manager_dealloc(manager);
  }
}

void destroy_manager_socket(LlrpManager* manager)
{
  llrp_recv_netint_remove(&manager->keys.netint, kLlrpSocketTypeManager);
  release_llrp_send_socket(&manager->keys.netint);
}

/* Callback for IntHandleManager to determine whether a handle is in use */
bool manager_handle_in_use(int handle_val)
{
  return etcpal_rbtree_find(&state.managers, &handle_val);
}

int manager_cmp_by_handle(const EtcPalRbTree* self, const EtcPalRbNode* node_a, const EtcPalRbNode* node_b)
{
  (void)self;

  const LlrpManager* a = (const LlrpManager*)node_a->value;
  const LlrpManager* b = (const LlrpManager*)node_b->value;

  return (a->keys.handle > b->keys.handle) - (a->keys.handle < b->keys.handle);
}

int manager_cmp_by_cid_and_netint(const EtcPalRbTree* self, const EtcPalRbNode* node_a, const EtcPalRbNode* node_b)
{
  (void)self;

  const LlrpManager* a = (const LlrpManager*)node_a->value;
  const LlrpManager* b = (const LlrpManager*)node_b->value;

  if (a->keys.netint.ip_type == b->keys.netint.ip_type)
  {
    if (a->keys.netint.index == b->keys.netint.index)
    {
      return ETCPAL_UUID_CMP(&a->keys.cid, &b->keys.cid);
    }
    else
    {
      return (a->keys.netint.index > b->keys.netint.index) - (a->keys.netint.index < b->keys.netint.index);
    }
  }
  else
  {
    return (a->keys.netint.ip_type == kEtcPalIpTypeV6) ? 1 : -1;
  }
}

int discovered_target_cmp(const EtcPalRbTree* self, const EtcPalRbNode* node_a, const EtcPalRbNode* node_b)
{
  (void)self;
  const DiscoveredTargetInternal* a = (const DiscoveredTargetInternal*)node_a->value;
  const DiscoveredTargetInternal* b = (const DiscoveredTargetInternal*)node_b->value;
  return RDM_UID_CMP(&a->known_uid.uid, &b->known_uid.uid);
}

EtcPalRbNode* manager_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  /* TODO */
  return NULL;
#endif
}

void manager_node_dealloc(EtcPalRbNode* node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
/* TODO */
#endif
}
