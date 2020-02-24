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

#include "rdmnet/core/llrp_target.h"

#include <string.h>
#include <stdlib.h>
#include "etcpal/common.h"
#include "etcpal/netint.h"
#include "etcpal/rbtree.h"
#include "etcpal/socket.h"
#include "rdm/responder.h"
#include "rdmnet/defs.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/llrp_target.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/mcast.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/util.h"

#if !RDMNET_DYNAMIC_MEM
#include "etcpal/mempool.h"
#endif

/**************************** Private constants ******************************/

#define LLRP_TARGET_MAX_RB_NODES (RDMNET_LLRP_MAX_TARGETS * 2)

#if RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS
#define CB_STORAGE_CLASS
#else
#define CB_STORAGE_CLASS static
#endif

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define LLRP_TARGET_ALLOC() (LlrpTarget*)malloc(sizeof(LlrpTarget))
#define LLRP_TARGET_DEALLOC(ptr) free(ptr)
#else
#define LLRP_TARGET_ALLOC() (LlrpTarget*)etcpal_mempool_alloc(llrp_targets)
#define LLRP_TARGET_DEALLOC(ptr) etcpal_mempool_free(llrp_targets, ptr)
#endif

#define INIT_CALLBACK_INFO(cbptr) ((cbptr)->which = kTargetCallbackNone)

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(llrp_targets, LlrpTarget, RDMNET_LLRP_MAX_TARGETS);
ETCPAL_MEMPOOL_DEFINE(llrp_target_rb_nodes, EtcPalRbNode, LLRP_TARGET_MAX_RB_NODES);
#endif

static struct LlrpTargetState
{
  EtcPalRbTree targets;
  EtcPalRbTree targets_by_cid;
  IntHandleManager handle_mgr;
} state;

/*********************** Private function prototypes *************************/

// Creating, destroying, and finding targets
static etcpal_error_t create_new_target(const LlrpTargetConfig* config, LlrpTarget** new_target);
static etcpal_error_t setup_target_netints(const LlrpTargetOptionalConfig* config, LlrpTarget* target);
static etcpal_error_t setup_target_netint(const RdmnetMcastNetintId* netint_id, LlrpTargetNetintInfo* netint);
static etcpal_error_t get_target(llrp_target_t handle, LlrpTarget** target);
static LlrpTargetNetintInfo* get_target_netint(LlrpTarget* target, const RdmnetMcastNetintId* id);
static void release_target(LlrpTarget* target);
static void cleanup_target_netints(LlrpTarget* target);
static void destroy_target(LlrpTarget* target, bool remove_from_tree);
static bool target_handle_in_use(int handle_val);

// Target state processing
static void handle_llrp_message(const uint8_t* data, size_t data_size, LlrpTarget* target, LlrpTargetNetintInfo* netint,
                                TargetCallbackDispatchInfo* cb);
static void fill_callback_info(const LlrpTarget* target, TargetCallbackDispatchInfo* info);
static void deliver_callback(TargetCallbackDispatchInfo* info);
static void process_target_state(LlrpTarget* target);

// Target tracking using EtcPalRbTrees
static EtcPalRbNode* target_node_alloc(void);
static void target_node_free(EtcPalRbNode* node);
static int target_compare_by_handle(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int target_compare_by_cid(const EtcPalRbTree* self, const void* value_a, const void* value_b);

/*************************** Function definitions ****************************/

etcpal_error_t llrp_target_init(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !RDMNET_DYNAMIC_MEM
  // Init memory pools
  res |= etcpal_mempool_init(llrp_targets);
  res |= etcpal_mempool_init(llrp_target_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&state.targets, target_compare_by_handle, target_node_alloc, target_node_free);
    etcpal_rbtree_init(&state.targets_by_cid, target_compare_by_cid, target_node_alloc, target_node_free);
    init_int_handle_manager(&state.handle_mgr, target_handle_in_use);

    if (RDMNET_CAN_LOG(ETCPAL_LOG_DEBUG))
    {
      char mac_str[ETCPAL_MAC_STRING_BYTES];
      etcpal_mac_to_string(rdmnet_get_lowest_mac_addr(), mac_str, ETCPAL_MAC_STRING_BYTES);
      RDMNET_LOG_DEBUG("Using '%s' as the LLRP lowest hardware address identifier.", mac_str);
    }
  }
  return res;
}

static void target_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  LlrpTarget* target = (LlrpTarget*)node->value;
  if (target)
    destroy_target(target, false);
  target_node_free(node);
}

void llrp_target_deinit()
{
  etcpal_rbtree_clear_with_cb(&state.targets, target_dealloc);
  memset(&state, 0, sizeof state);
}

/*!
 * \brief Create a new LLRP target instance.
 *
 * \param[in] config Configuration parameters for the LLRP target to be created.
 * \param[out] handle Handle to the newly-created target instance.
 * \return #kEtcPalErrOk: Target created successfully.
 * \return #kEtcPalErrInvalid: Invalid argument provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No memory to allocate additional target instance.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 * \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_target_create(const LlrpTargetConfig* config, llrp_target_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = kEtcPalErrSys;
  if (rdmnet_writelock())
  {
    // Attempt to create the LLRP target, give it a unique handle and add it to the map.
    LlrpTarget* target;
    res = create_new_target(config, &target);

    if (res == kEtcPalErrOk)
    {
      *handle = target->keys.handle;
    }
    rdmnet_writeunlock();
  }

  return res;
}

/*!
 * \brief Destroy an LLRP target instance.
 *
 * The handle will be invalidated for any future calls to API functions.
 *
 * \param[in] handle Handle to target to destroy.
 */
void llrp_target_destroy(llrp_target_t handle)
{
  LlrpTarget* target;
  etcpal_error_t res = get_target(handle, &target);
  if (res != kEtcPalErrOk)
    return;

  target->marked_for_destruction = true;
  release_target(target);
}

/*!
 * \brief Update the Broker connection state of an LLRP target.
 *
 * If an LLRP target is associated with an RPT client, this should be called each time the client
 * connects or disconnects from a broker. Controllers are considered not connected when they are
 * not connected to any broker. This affects whether the LLRP target responds to filtered LLRP
 * probe requests.
 *
 * \param[in] handle Handle to LLRP target for which to update the connection state.
 * \param[in] connected_to_broker Whether the LLRP target is currently connected to a broker.
 */
void llrp_target_update_connection_state(llrp_target_t handle, bool connected_to_broker)
{
  LlrpTarget* target;
  etcpal_error_t res = get_target(handle, &target);
  if (res == kEtcPalErrOk)
  {
    target->connected_to_broker = connected_to_broker;
    release_target(target);
  }
}

/*!
 * \brief Send an RDM ACK response from an LLRP target.
 *
 * \param[in] handle Handle to LLRP target from which to send the response.
 * \param[in] received_cmd Previously-received command that the ACK is a response to.
 * \param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * \param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * \return #kEtcPalErrOk: ACK sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP target instance.
 * \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_target_send_ack(llrp_target_t handle, const LlrpRemoteRdmCommand* received_cmd,
                                    const uint8_t* response_data, uint8_t response_data_len)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(response_data);
  ETCPAL_UNUSED_ARG(response_data_len);
  return kEtcPalErrNotImpl;
  //  if (!resp)
  //    return kEtcPalErrInvalid;
  //
  //  RdmBuffer resp_buf;
  //  etcpal_error_t res = rdmresp_pack_response(&resp->rdm, &resp_buf);
  //  if (res != kEtcPalErrOk)
  //    return res;
  //
  //  LlrpTarget* target;
  //  res = get_target(handle, &target);
  //  if (res == kEtcPalErrOk)
  //  {
  //    LlrpTargetNetintInfo* netint = get_target_netint(target, &resp->netint_id);
  //    if (netint)
  //    {
  //      LlrpHeader header;
  //
  //      header.dest_cid = resp->dest_cid;
  //      header.sender_cid = target->keys.cid;
  //      header.transaction_number = resp->seq_num;
  //
  //      res = send_llrp_rdm_response(netint->send_sock, netint->send_buf, (netint->id.ip_type == kEtcPalIpTypeV6),
  //                                   &header, &resp_buf);
  //    }
  //    else
  //    {
  //      // Something has changed about the system network interfaces since this command was received.
  //      res = kEtcPalErrSys;
  //    }
  //    release_target(target);
  //  }
  //  return res;
}

/*!
 * \brief Send an RDM NACK response from an LLRP target.
 *
 * \param[in] handle Handle to LLRP target from which to send the response.
 * \param[in] received_cmd Previously-received command that the NACK is a response to.
 * \param[in] nack_reason RDM NACK reason code to send with the NACK.
 * \return #kEtcPalErrOk: NACK sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP target instance.
 * \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_target_send_nack(llrp_target_t handle, const LlrpRemoteRdmCommand* received_cmd,
                                     rdm_nack_reason_t nack_reason)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(received_cmd);
  ETCPAL_UNUSED_ARG(nack_reason);
  return kEtcPalErrNotImpl;
}

void process_target_state(LlrpTarget* target)
{
  for (LlrpTargetNetintInfo* netint = target->netints; netint < target->netints + target->num_netints; ++netint)
  {
    if (netint->reply_pending)
    {
      if (etcpal_timer_is_expired(&netint->reply_backoff))
      {
        LlrpHeader header;
        header.sender_cid = target->keys.cid;
        header.dest_cid = netint->pending_reply_cid;
        header.transaction_number = netint->pending_reply_trans_num;

        DiscoveredLlrpTarget target_info;
        target_info.cid = target->keys.cid;
        target_info.uid = target->uid;
        target_info.hardware_address = *(rdmnet_get_lowest_mac_addr());
        target_info.component_type = target->component_type;

        etcpal_error_t send_res = send_llrp_probe_reply(netint->send_sock, netint->send_buf,
                                                        (netint->id.ip_type == kEtcPalIpTypeV6), &header, &target_info);
        if (send_res != kEtcPalErrOk && RDMNET_CAN_LOG(ETCPAL_LOG_WARNING))
        {
          char cid_str[ETCPAL_UUID_STRING_BYTES];
          etcpal_uuid_to_string(&header.dest_cid, cid_str);
          RDMNET_LOG_WARNING("Error sending probe reply to manager CID %s on interface index %u", cid_str,
                             netint->id.index);
        }

        netint->reply_pending = false;
      }
    }
  }
}

void llrp_target_tick(void)
{
  if (!rdmnet_core_initialized())
    return;

  // Remove any targets marked for destruction.
  if (rdmnet_writelock())
  {
    LlrpTarget* destroy_list = NULL;
    LlrpTarget** next_destroy_list_entry = &destroy_list;

    EtcPalRbIter target_iter;
    etcpal_rbiter_init(&target_iter);

    LlrpTarget* target = (LlrpTarget*)etcpal_rbiter_first(&target_iter, &state.targets);
    while (target)
    {
      // Can't destroy while iterating as that would invalidate the iterator
      // So the targets are added to a linked list of target pending destruction
      if (target->marked_for_destruction)
      {
        *next_destroy_list_entry = target;
        target->next_to_destroy = NULL;
        next_destroy_list_entry = &target->next_to_destroy;
      }
      target = (LlrpTarget*)etcpal_rbiter_next(&target_iter);
    }

    // Now do the actual destruction
    if (destroy_list)
    {
      LlrpTarget* to_destroy = destroy_list;
      while (to_destroy)
      {
        LlrpTarget* next = to_destroy->next_to_destroy;
        destroy_target(to_destroy, true);
        to_destroy = next;
      }
    }

    rdmnet_writeunlock();
  }

  // Do the rest of the periodic functionality with a read lock
  if (rdmnet_readlock())
  {
    EtcPalRbIter target_iter;
    etcpal_rbiter_init(&target_iter);
    LlrpTarget* target = (LlrpTarget*)etcpal_rbiter_first(&target_iter, &state.targets);

    while (target)
    {
      process_target_state(target);
      target = (LlrpTarget*)etcpal_rbiter_next(&target_iter);
    }

    rdmnet_readunlock();
  }
}

void target_data_received(const uint8_t* data, size_t data_size, const RdmnetMcastNetintId* netint)
{
  CB_STORAGE_CLASS TargetCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  LlrpTargetKeys keys;
  if (get_llrp_destination_cid(data, data_size, &keys.cid))
  {
    bool target_found = false;

    if (rdmnet_readlock())
    {
      if (0 == ETCPAL_UUID_CMP(&keys.cid, &kLlrpBroadcastCid))
      {
        // Broadcast LLRP message - handle with all targets
        target_found = true;

        EtcPalRbIter target_iter;
        etcpal_rbiter_init(&target_iter);
        for (LlrpTarget* target = (LlrpTarget*)etcpal_rbiter_first(&target_iter, &state.targets); target;
             target = (LlrpTarget*)etcpal_rbiter_next(&target_iter))
        {
          LlrpTargetNetintInfo* target_netint = get_target_netint(target, netint);
          if (target_netint)
          {
            handle_llrp_message(data, data_size, target, target_netint, &cb);
          }
        }
      }
      else
      {
        LlrpTarget* target = (LlrpTarget*)etcpal_rbtree_find(&state.targets_by_cid, &keys);
        if (target)
        {
          target_found = true;

          LlrpTargetNetintInfo* target_netint = get_target_netint(target, netint);
          if (target_netint)
          {
            handle_llrp_message(data, data_size, target, target_netint, &cb);
          }
        }
      }
      rdmnet_readunlock();
    }

    if (!target_found && RDMNET_CAN_LOG(ETCPAL_LOG_DEBUG))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&keys.cid, cid_str);
      RDMNET_LOG_DEBUG("Ignoring LLRP message addressed to unknown LLRP Target %s", cid_str);
    }
  }

  deliver_callback(&cb);
}

void handle_llrp_message(const uint8_t* data, size_t data_size, LlrpTarget* target, LlrpTargetNetintInfo* netint,
                         TargetCallbackDispatchInfo* cb)
{
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_size);
  ETCPAL_UNUSED_ARG(target);
  ETCPAL_UNUSED_ARG(netint);
  ETCPAL_UNUSED_ARG(cb);
  //  LlrpMessage msg;
  //  LlrpMessageInterest interest;
  //  interest.my_cid = target->keys.cid;
  //  interest.interested_in_probe_reply = false;
  //  interest.interested_in_probe_request = true;
  //  interest.my_uid = target->uid;
  //
  //  if (parse_llrp_message(data, data_size, &interest, &msg))
  //  {
  //    switch (msg.vector)
  //    {
  //      case VECTOR_LLRP_PROBE_REQUEST:
  //      {
  //        const RemoteProbeRequest* request = LLRP_MSG_GET_PROBE_REQUEST(&msg);
  //        // TODO allow multiple probe replies to be queued
  //        if (request->contains_my_uid && !netint->reply_pending)
  //        {
  //          uint32_t backoff_ms;
  //
  //          // Check the filter values.
  //          if (!((request->filter & LLRP_FILTERVAL_BROKERS_ONLY) && target->component_type != kLlrpCompBroker) &&
  //              !(request->filter & LLRP_FILTERVAL_CLIENT_CONN_INACTIVE && target->connected_to_broker))
  //          {
  //            netint->reply_pending = true;
  //            netint->pending_reply_cid = msg.header.sender_cid;
  //            netint->pending_reply_trans_num = msg.header.transaction_number;
  //            backoff_ms = (uint32_t)(rand() * LLRP_MAX_BACKOFF_MS / RAND_MAX);
  //            etcpal_timer_start(&netint->reply_backoff, backoff_ms);
  //          }
  //        }
  //        // Even if we got a valid probe request, we are starting a backoff timer, so there's nothing
  //        // else to do at this time.
  //        break;
  //      }
  //      case VECTOR_LLRP_RDM_CMD:
  //      {
  //        LlrpRemoteRdmCommand* remote_cmd = &cb->args.cmd_received.cmd;
  //        if (kEtcPalErrOk == rdmresp_unpack_command(LLRP_MSG_GET_RDM(&msg), &remote_cmd->rdm))
  //        {
  //          remote_cmd->src_cid = msg.header.sender_cid;
  //          remote_cmd->seq_num = msg.header.transaction_number;
  //          remote_cmd->netint_id = netint->id;
  //
  //          cb->which = kTargetCallbackRdmCmdReceived;
  //          fill_callback_info(target, cb);
  //        }
  //      }
  //      default:
  //        break;
  //    }
  //  }
}

void fill_callback_info(const LlrpTarget* target, TargetCallbackDispatchInfo* info)
{
  info->handle = target->keys.handle;
  info->cbs = target->callbacks;
  info->context = target->callback_context;
}

void deliver_callback(TargetCallbackDispatchInfo* info)
{
  ETCPAL_UNUSED_ARG(info);
  //  switch (info->which)
  //  {
  //    case kTargetCallbackRdmCmdReceived:
  //      if (info->cbs.rdm_cmd_received)
  //        info->cbs.rdm_cmd_received(info->handle, &info->args.cmd_received.cmd, info->context);
  //      break;
  //    case kTargetCallbackNone:
  //    default:
  //      break;
  //  }
}

etcpal_error_t setup_target_netint(const RdmnetMcastNetintId* netint_id, LlrpTargetNetintInfo* netint)
{
  netint->id = *netint_id;

  etcpal_error_t res = rdmnet_get_mcast_send_socket(netint_id, &netint->send_sock);
  if (res != kEtcPalErrOk)
    return res;

  res = llrp_recv_netint_add(netint_id, kLlrpSocketTypeTarget);
  if (res != kEtcPalErrOk)
  {
    rdmnet_release_mcast_send_socket(netint_id);
    return res;
  }

  // Remaining initialization
  netint->reply_pending = false;
  return res;
}

void cleanup_target_netints(LlrpTarget* target)
{
  for (const LlrpTargetNetintInfo* netint_info = target->netints; netint_info < target->netints + target->num_netints;
       ++netint_info)
  {
    rdmnet_release_mcast_send_socket(&netint_info->id);
    llrp_recv_netint_remove(&netint_info->id, kLlrpSocketTypeTarget);
  }
#if RDMNET_DYNAMIC_MEM
  free(target->netints);
#endif
}

etcpal_error_t setup_target_netints(const LlrpTargetOptionalConfig* config, LlrpTarget* target)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (config->netint_arr && config->num_netints > 0)
  {
    // Check or initialize the target's network interface array.
#if RDMNET_DYNAMIC_MEM
    target->netints = (LlrpTargetNetintInfo*)calloc(config->num_netints, sizeof(LlrpTargetNetintInfo));
    if (!target->netints)
      return kEtcPalErrNoMem;
#else
    if (config->num_netints > RDMNET_MAX_MCAST_NETINTS)
      return kEtcPalErrNoMem;
#endif
    target->num_netints = 0;

    for (size_t i = 0; i < config->num_netints; ++i)
    {
      res = setup_target_netint(&config->netint_arr[i], &target->netints[i]);
      if (res == kEtcPalErrOk)
        ++target->num_netints;
      else
        break;
    }

    if (res != kEtcPalErrOk)
    {
      cleanup_target_netints(target);
    }
  }
  else
  {
    // Check or initialize the target's network interface array.
    const RdmnetMcastNetintId* mcast_netint_arr;
    size_t mcast_netint_arr_size = rdmnet_get_mcast_netint_array(&mcast_netint_arr);

#if RDMNET_DYNAMIC_MEM
    target->netints = (LlrpTargetNetintInfo*)calloc(mcast_netint_arr_size, sizeof(LlrpTargetNetintInfo));
    if (!target->netints)
      return kEtcPalErrNoMem;
#endif

    target->num_netints = 0;
    for (const RdmnetMcastNetintId* netint_id = mcast_netint_arr; netint_id < mcast_netint_arr + mcast_netint_arr_size;
         ++netint_id)
    {
      // If the user hasn't provided a list of network interfaces to operate on, failing to
      // intialize on a network interface will be non-fatal and we will log it.
      res = setup_target_netint(netint_id, &target->netints[target->num_netints]);
      if (res == kEtcPalErrOk)
      {
        ++target->num_netints;
      }
      else
      {
        RDMNET_LOG_WARNING("Failed to intiailize LLRP target for listening on network interface index %d: '%s'",
                           netint_id->index, etcpal_strerror(res));
        res = kEtcPalErrOk;
      }
    }
  }

  return res;
}

etcpal_error_t create_new_target(const LlrpTargetConfig* config, LlrpTarget** new_target)
{
  etcpal_error_t res = kEtcPalErrNoMem;

  llrp_target_t new_handle = get_next_int_handle(&state.handle_mgr);
  if (new_handle == LLRP_TARGET_INVALID)
    return res;

  LlrpTarget* target = LLRP_TARGET_ALLOC();
  if (target)
  {
    res = setup_target_netints(&config->optional, target);

    if (res == kEtcPalErrOk)
    {
      target->keys.handle = new_handle;
      target->keys.cid = config->cid;
      res = etcpal_rbtree_insert(&state.targets, target);
      if (res == kEtcPalErrOk)
      {
        res = etcpal_rbtree_insert(&state.targets_by_cid, target);
        if (res == kEtcPalErrOk)
        {
          if (RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&config->optional.uid))
          {
            // This is a hack around a hole in the standard. TODO add a more explanatory comment once
            // this has been further explored.
            target->uid.manu = config->optional.uid.manu;
            target->uid.id = (uint32_t)rand();
          }
          else
          {
            target->uid = config->optional.uid;
          }
          target->component_type = config->component_type;
          target->connected_to_broker = false;
          target->callbacks = config->callbacks;
          target->callback_context = config->callback_context;
          target->marked_for_destruction = false;
          target->next_to_destroy = NULL;

          *new_target = target;
        }
        else
        {
          etcpal_rbtree_remove(&state.targets, target);
          cleanup_target_netints(target);
          LLRP_TARGET_DEALLOC(target);
        }
      }
      else
      {
        cleanup_target_netints(target);
        LLRP_TARGET_DEALLOC(target);
      }
    }
    else
    {
      LLRP_TARGET_DEALLOC(target);
    }
  }

  return res;
}

etcpal_error_t get_target(llrp_target_t handle, LlrpTarget** target)
{
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;
  if (!rdmnet_readlock())
    return kEtcPalErrSys;

  LlrpTarget* found_target = (LlrpTarget*)etcpal_rbtree_find(&state.targets, &handle);
  if (!found_target || found_target->marked_for_destruction)
  {
    rdmnet_readunlock();
    return kEtcPalErrNotFound;
  }
  *target = found_target;
  return kEtcPalErrOk;
}

LlrpTargetNetintInfo* get_target_netint(LlrpTarget* target, const RdmnetMcastNetintId* id)
{
  RDMNET_ASSERT(target);
  RDMNET_ASSERT(id);

  for (LlrpTargetNetintInfo* netint = target->netints; netint < target->netints + target->num_netints; ++netint)
  {
    if (netint->id.index == id->index && netint->id.ip_type == id->ip_type)
      return netint;
  }
  return NULL;
}

void release_target(LlrpTarget* target)
{
  ETCPAL_UNUSED_ARG(target);
  rdmnet_readunlock();
}

void destroy_target(LlrpTarget* target, bool remove_from_tree)
{
  if (target)
  {
    cleanup_target_netints(target);
    if (remove_from_tree)
      etcpal_rbtree_remove(&state.targets, target);
    LLRP_TARGET_DEALLOC(target);
  }
}

/* Callback for IntHandleManager to determine whether a handle is in use */
bool target_handle_in_use(int handle_val)
{
  return etcpal_rbtree_find(&state.targets, &handle_val);
}

int target_compare_by_handle(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const LlrpTarget* a = (const LlrpTarget*)value_a;
  const LlrpTarget* b = (const LlrpTarget*)value_b;
  return (a->keys.handle > b->keys.handle) - (a->keys.handle < b->keys.handle);
}

int target_compare_by_cid(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const LlrpTarget* a = (const LlrpTarget*)value_a;
  const LlrpTarget* b = (const LlrpTarget*)value_b;
  return ETCPAL_UUID_CMP(&a->keys.cid, &b->keys.cid);
}

EtcPalRbNode* target_node_alloc(void)
{
#if RDMNET_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(llrp_target_rb_nodes);
#endif
}

void target_node_free(EtcPalRbNode* node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(llrp_target_rb_nodes, node);
#endif
}
