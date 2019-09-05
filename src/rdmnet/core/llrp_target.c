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

#include "rdmnet/core/llrp_target.h"

#include <string.h>
#include <stdlib.h>
#include "rdmnet/private/opts.h"
#if !RDMNET_DYNAMIC_MEM
#include "lwpa/mempool.h"
#endif
#include "lwpa/rbtree.h"
#include "lwpa/socket.h"
#include "lwpa/netint.h"
#include "rdm/responder.h"
#include "rdmnet/defs.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/llrp_target.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/util.h"

/**************************** Private constants ******************************/

#define LLRP_MAX_TARGET_NETINTS (RDMNET_LLRP_MAX_TARGETS * RDMNET_LLRP_MAX_NETINTS_PER_TARGET)
#define LLRP_TARGET_MAX_RB_NODES ((RDMNET_LLRP_MAX_TARGETS * 2) + LLRP_TOTAL_MAX_TARGET_NETINTS)

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define llrp_target_alloc() (LlrpTarget*)malloc(sizeof(LlrpTarget))
#define llrp_target_netint_alloc() (LlrpTargetNetintInfo*)malloc(sizeof(LlrpTargetNetintInfo))
#define llrp_target_dealloc(ptr) free(ptr)
#define llrp_target_netint_dealloc(ptr) free(ptr)
#else
#define llrp_target_alloc() (LlrpTarget*)lwpa_mempool_alloc(llrp_targets)
#define llrp_target_netint_alloc() (LlrpTargetNetintInfo*)lwpa_mempool_alloc(llrp_targets_netints)
#define llrp_target_dealloc(ptr) lwpa_mempool_free(llrp_targets, ptr)
#define llrp_target_netint_dealloc(ptr) lwpa_mempool_free(llrp_target_netints, ptr)
#endif

#define INIT_CALLBACK_INFO(cbptr) ((cbptr)->which = kTargetCallbackNone)

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(llrp_targets, LlrpTarget, RDMNET_LLRP_MAX_TARGETS);
LWPA_MEMPOOL_DEFINE(llrp_target_netints, LlrpTargetNetintInfo, LLRP_MAX_TARGET_NETINTS);
LWPA_MEMPOOL_DEFINE(llrp_target_rb_nodes, LwpaRbNode, LLRP_TARGET_MAX_RB_NODES);
#endif

static struct LlrpTargetState
{
  LwpaRbTree targets;
  LwpaRbTree targets_by_cid;
  IntHandleManager handle_mgr;
} state;

/*********************** Private function prototypes *************************/

// Creating, destroying, and finding targets
static lwpa_error_t create_new_target(const LlrpTargetConfig* config, LlrpTarget** new_target);
static lwpa_error_t get_target(llrp_target_t handle, LlrpTarget** target);
static void release_target(LlrpTarget* target);
static void destroy_target(LlrpTarget* target, bool remove_from_tree);
static bool target_handle_in_use(int handle_val);

// Target state processing
static void handle_llrp_message(const uint8_t* data, size_t data_size, LlrpTarget* target, LlrpTargetNetintInfo* netint,
                                TargetCallbackDispatchInfo* cb);
static void fill_callback_info(const LlrpTarget* target, TargetCallbackDispatchInfo* info);
static void deliver_callback(TargetCallbackDispatchInfo* info);
static void process_target_state(LlrpTarget* target);

// Target tracking using LwpaRbTrees
static LwpaRbNode* target_node_alloc();
static void target_node_free(LwpaRbNode* node);
static int target_cmp_by_handle(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b);
static int target_cmp_by_cid(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b);
static int target_netint_cmp(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_llrp_target_init()
{
  lwpa_error_t res = kLwpaErrOk;

#if !RDMNET_DYNAMIC_MEM
  /* Init memory pool */
  res |= lwpa_mempool_init(llrp_targets);
  res |= lwpa_mempool_init(llrp_target_netints);
  res |= lwpa_mempool_init(llrp_target_rb_nodes);
#endif

  if (res == kLwpaErrOk)
  {
    lwpa_rbtree_init(&state.targets, target_cmp_by_handle, target_node_alloc, target_node_free);
    lwpa_rbtree_init(&state.targets_by_cid, target_cmp_by_cid, target_node_alloc, target_node_free);
    init_int_handle_manager(&state.handle_mgr, target_handle_in_use);
  }
  return res;
}

static void target_dealloc(const LwpaRbTree* self, LwpaRbNode* node)
{
  (void)self;

  LlrpTarget* target = (LlrpTarget*)node->value;
  if (target)
    destroy_target(target, false);
  target_node_free(node);
}

void rdmnet_llrp_target_deinit()
{
  lwpa_rbtree_clear_with_cb(&state.targets, target_dealloc);
  memset(&state, 0, sizeof state);
}

/*! \brief Create a new LLRP target instance.
 *
 *  \param[in] config Configuration parameters for the LLRP target to be created.
 *  \param[out] handle Handle to the newly-created target instance.
 *  \return #kLwpaErrOk: Target created successfully.
 *  \return #kLwpaErrInvalid: Invalid argument provided.
 *  \return #kLwpaErrNotInit: Module not initialized.
 *  \return #kLwpaErrNoMem: No memory to allocate additional target instance.
 *  \return #kLwpaErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
lwpa_error_t rdmnet_llrp_target_create(const LlrpTargetConfig* config, llrp_target_t* handle)
{
  if (!config || !handle)
    return kLwpaErrInvalid;
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;

  lwpa_error_t res = kLwpaErrSys;
  if (rdmnet_writelock())
  {
    // Attempt to create the LLRP target, give it a unique handle and add it to the map.
    LlrpTarget* target;
    res = create_new_target(config, &target);

    if (res == kLwpaErrOk)
    {
      *handle = target->keys.handle;
    }
    rdmnet_writeunlock();
  }

  return res;
}

/*! \brief Destroy an LLRP target instance.
 *
 *  The handle will be invalidated for any future calls to API functions.
 *
 *  \param[in] handle Handle to target to destroy.
 */
void rdmnet_llrp_target_destroy(llrp_target_t handle)
{
  LlrpTarget* target;
  lwpa_error_t res = get_target(handle, &target);
  if (res != kLwpaErrOk)
    return;

  target->marked_for_destruction = true;
  release_target(target);
}

/*! \brief Update the Broker connection state of an LLRP target.
 *
 *  If an LLRP target is associated with an RPT client, this should be called each time the client
 *  connects or disconnects from a broker. Controllers are considered not connected when they are
 *  not connected to any broker. This affects whether the LLRP target responds to filtered LLRP
 *  probe requests.
 *
 *  \param[in] handle Handle to LLRP target for which to update the connection state.
 *  \param[in] connected_to_broker Whether the LLRP target is currently connected to a broker.
 */
void rdmnet_llrp_target_update_connection_state(llrp_target_t handle, bool connected_to_broker)
{
  LlrpTarget* target;
  lwpa_error_t res = get_target(handle, &target);
  if (res == kLwpaErrOk)
  {
    target->connected_to_broker = connected_to_broker;
    release_target(target);
  }
}

/*! \brief Send an RDM response from an LLRP target.
 *
 *  \param[in] handle Handle to LLRP target from which to send an RDM response.
 *  \param[in] resp Response to send.
 *  \return #kLwpaErrOk: Response sent successfully.
 *  \return #kLwpaErrInvalid: Invalid argument provided.
 *  \return #kLwpaErrNotInit: Module not initialized.
 *  \return #kLwpaErrNotFound: Handle is not associated with a valid LLRP target instance.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
lwpa_error_t rdmnet_llrp_send_rdm_response(llrp_target_t handle, const LlrpLocalRdmResponse* resp)
{
  if (!resp)
    return kLwpaErrInvalid;

  RdmBuffer resp_buf;
  lwpa_error_t res = rdmresp_pack_response(&resp->rdm, &resp_buf);
  if (res != kLwpaErrOk)
    return res;

  LlrpTarget* target;
  res = get_target(handle, &target);
  if (res == kLwpaErrOk)
  {
    LlrpTargetNetintInfo* netint = (LlrpTargetNetintInfo*)lwpa_rbtree_find(&target->netints, (void*)&resp->netint_id);
    if (netint)
    {
      LlrpHeader header;

      header.dest_cid = resp->dest_cid;
      header.sender_cid = target->keys.cid;
      header.transaction_number = resp->seq_num;

      res = send_llrp_rdm_response(netint->send_sock, netint->send_buf, (netint->id.ip_type == kLwpaIpTypeV6), &header,
                                   &resp_buf);
    }
    else
    {
      // Something has changed about the system network interfaces since this command was received.
      res = kLwpaErrSys;
    }
    release_target(target);
  }
  return res;
}

void process_target_state(LlrpTarget* target)
{
  LwpaRbIter netint_iter;
  lwpa_rbiter_init(&netint_iter);

  for (LlrpTargetNetintInfo* netint = (LlrpTargetNetintInfo*)lwpa_rbiter_first(&netint_iter, &target->netints); netint;
       netint = (LlrpTargetNetintInfo*)lwpa_rbiter_next(&netint_iter))
  {
    if (netint->reply_pending)
    {
      if (lwpa_timer_is_expired(&netint->reply_backoff))
      {
        LlrpHeader header;
        header.sender_cid = target->keys.cid;
        header.dest_cid = netint->pending_reply_cid;
        header.transaction_number = netint->pending_reply_trans_num;

        DiscoveredLlrpTarget target_info;
        target_info.cid = target->keys.cid;
        target_info.uid = target->uid;
        memcpy(target_info.hardware_address, kLlrpLowestHardwareAddr, 6);
        target_info.component_type = target->component_type;

        lwpa_error_t send_res = send_llrp_probe_reply(netint->send_sock, netint->send_buf,
                                                      (netint->id.ip_type == kLwpaIpTypeV6), &header, &target_info);
        if (send_res != kLwpaErrOk && LWPA_CAN_LOG(rdmnet_log_params, LWPA_LOG_WARNING))
        {
          char cid_str[LWPA_UUID_STRING_BYTES];
          lwpa_uuid_to_string(cid_str, &header.dest_cid);
          lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
                   RDMNET_LOG_MSG("Error sending probe reply to manager CID %s on interface index %u"), cid_str,
                   netint->id.index);
        }

        netint->reply_pending = false;
      }
    }
  }
}

void rdmnet_llrp_target_tick()
{
  if (!rdmnet_core_initialized())
    return;

  // Remove any targets marked for destruction.
  if (rdmnet_writelock())
  {
    LlrpTarget* destroy_list = NULL;
    LlrpTarget** next_destroy_list_entry = &destroy_list;

    LwpaRbIter target_iter;
    lwpa_rbiter_init(&target_iter);

    LlrpTarget* target = (LlrpTarget*)lwpa_rbiter_first(&target_iter, &state.targets);
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
      target = (LlrpTarget*)lwpa_rbiter_next(&target_iter);
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
    LwpaRbIter target_iter;
    lwpa_rbiter_init(&target_iter);
    LlrpTarget* target = (LlrpTarget*)lwpa_rbiter_first(&target_iter, &state.targets);

    while (target)
    {
      process_target_state(target);
      target = (LlrpTarget*)lwpa_rbiter_next(&target_iter);
    }

    rdmnet_readunlock();
  }
}

void target_data_received(const uint8_t* data, size_t data_size, const LlrpNetintId* netint)
{
  TargetCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  LlrpTargetKeys keys;
  if (get_llrp_destination_cid(data, data_size, &keys.cid))
  {
    bool target_found = false;

    if (rdmnet_readlock())
    {
      if (0 == LWPA_UUID_CMP(&keys.cid, &kLlrpBroadcastCid))
      {
        // Broadcast LLRP message - handle with all targets
        target_found = true;

        LwpaRbIter target_iter;
        lwpa_rbiter_init(&target_iter);
        for (LlrpTarget* target = (LlrpTarget*)lwpa_rbiter_first(&target_iter, &state.targets); target;
             target = (LlrpTarget*)lwpa_rbiter_next(&target_iter))
        {
          LlrpTargetNetintInfo* target_netint =
              (LlrpTargetNetintInfo*)lwpa_rbtree_find(&target->netints, (void*)netint);
          if (target_netint)
          {
            handle_llrp_message(data, data_size, target, target_netint, &cb);
          }
        }
      }
      else
      {
        LlrpTarget* target = (LlrpTarget*)lwpa_rbtree_find(&state.targets_by_cid, &keys);
        if (target)
        {
          target_found = true;

          LlrpTargetNetintInfo* target_netint =
              (LlrpTargetNetintInfo*)lwpa_rbtree_find(&target->netints, (void*)netint);
          if (target_netint)
          {
            handle_llrp_message(data, data_size, target, target_netint, &cb);
          }
        }
      }
      rdmnet_readunlock();
    }

    if (!target_found && LWPA_CAN_LOG(rdmnet_log_params, LWPA_LOG_DEBUG))
    {
      char cid_str[LWPA_UUID_STRING_BYTES];
      lwpa_uuid_to_string(cid_str, &keys.cid);
      lwpa_log(rdmnet_log_params, LWPA_LOG_DEBUG,
               RDMNET_LOG_MSG("Ignoring LLRP message addressed to unknown LLRP Target %s"), cid_str);
    }
  }

  deliver_callback(&cb);
}

void handle_llrp_message(const uint8_t* data, size_t data_size, LlrpTarget* target, LlrpTargetNetintInfo* netint,
                         TargetCallbackDispatchInfo* cb)
{
  LlrpMessage msg;
  LlrpMessageInterest interest;
  interest.my_cid = target->keys.cid;
  interest.interested_in_probe_reply = false;
  interest.interested_in_probe_request = true;
  interest.my_uid = target->uid;

  if (parse_llrp_message(data, data_size, &interest, &msg))
  {
    switch (msg.vector)
    {
      case VECTOR_LLRP_PROBE_REQUEST:
      {
        const RemoteProbeRequest* request = LLRP_MSG_GET_PROBE_REQUEST(&msg);
        // TODO allow multiple probe replies to be queued
        if (request->contains_my_uid && !netint->reply_pending)
        {
          uint32_t backoff_ms;

          // Check the filter values.
          if (!((request->filter & LLRP_FILTERVAL_BROKERS_ONLY) && target->component_type != kLlrpCompBroker) &&
              !(request->filter & LLRP_FILTERVAL_CLIENT_CONN_INACTIVE && target->connected_to_broker))
          {
            netint->reply_pending = true;
            netint->pending_reply_cid = msg.header.sender_cid;
            netint->pending_reply_trans_num = msg.header.transaction_number;
            backoff_ms = (uint32_t)(rand() * LLRP_MAX_BACKOFF_MS / RAND_MAX);
            lwpa_timer_start(&netint->reply_backoff, backoff_ms);
          }
        }
        // Even if we got a valid probe request, we are starting a backoff timer, so there's nothing
        // else to do at this time.
        break;
      }
      case VECTOR_LLRP_RDM_CMD:
      {
        LlrpRemoteRdmCommand* remote_cmd = &cb->args.cmd_received.cmd;
        if (kLwpaErrOk == rdmresp_unpack_command(LLRP_MSG_GET_RDM(&msg), &remote_cmd->rdm))
        {
          remote_cmd->src_cid = msg.header.sender_cid;
          remote_cmd->seq_num = msg.header.transaction_number;
          remote_cmd->netint_id = netint->id;

          cb->which = kTargetCallbackRdmCmdReceived;
          fill_callback_info(target, cb);
        }
      }
      default:
        break;
    }
  }
}

void fill_callback_info(const LlrpTarget* target, TargetCallbackDispatchInfo* info)
{
  info->handle = target->keys.handle;
  info->cbs = target->callbacks;
  info->context = target->callback_context;
}

void deliver_callback(TargetCallbackDispatchInfo* info)
{
  switch (info->which)
  {
    case kTargetCallbackRdmCmdReceived:
      if (info->cbs.rdm_cmd_received)
        info->cbs.rdm_cmd_received(info->handle, &info->args.cmd_received.cmd, info->context);
      break;
    case kTargetCallbackNone:
    default:
      break;
  }
}

lwpa_error_t setup_target_netint(const LlrpNetintId* netint_id, LlrpTarget* target)
{
  lwpa_error_t res = kLwpaErrNoMem;

  LlrpTargetNetintInfo* new_netint_info = llrp_target_netint_alloc();
  if (new_netint_info)
  {
    res = get_llrp_send_socket(netint_id, &new_netint_info->send_sock);
    if (res != kLwpaErrOk)
    {
      llrp_target_netint_dealloc(new_netint_info);
      return res;
    }

    res = llrp_recv_netint_add(netint_id, kLlrpSocketTypeTarget);
    if (res != kLwpaErrOk)
    {
      release_llrp_send_socket(netint_id);
      llrp_target_netint_dealloc(new_netint_info);
      return res;
    }

    new_netint_info->id = *netint_id;
    res = lwpa_rbtree_insert(&target->netints, new_netint_info);

    if (res != kLwpaErrOk)
    {
      llrp_recv_netint_remove(netint_id, kLlrpSocketTypeTarget);
      release_llrp_send_socket(netint_id);
      llrp_target_netint_dealloc(new_netint_info);
      return res;
    }

    // Remaining initialization
    new_netint_info->reply_pending = false;
    new_netint_info->target = target;
  }

  return res;
}

void cleanup_target_netint(const LwpaRbTree* self, LwpaRbNode* node)
{
  (void)self;

  LlrpTargetNetintInfo* netint_info = (LlrpTargetNetintInfo*)node->value;
  if (netint_info)
  {
    release_llrp_send_socket(&netint_info->id);
    llrp_recv_netint_remove(&netint_info->id, kLlrpSocketTypeTarget);
    llrp_target_netint_dealloc(netint_info);
    target_node_free(node);
  }
}

lwpa_error_t setup_target_netints(const LlrpTargetOptionalConfig* config, LlrpTarget* target)
{
  lwpa_error_t res = kLwpaErrOk;
  lwpa_rbtree_init(&target->netints, target_netint_cmp, target_node_alloc, target_node_free);

  if (config->netint_arr && config->num_netints > 0)
  {
#if !RDMNET_DYNAMIC_MEM
    if (config->num_netints > RDMNET_LLRP_MAX_NETINTS_PER_TARGET)
      res = kLwpaErrNoMem;
#endif

    if (res == kLwpaErrOk)
    {
      for (const LlrpNetintId* netint_id = config->netint_arr; netint_id < config->netint_arr + config->num_netints;
           ++netint_id)
      {
        res = setup_target_netint(netint_id, target);
        if (res != kLwpaErrOk)
          break;
      }
    }

    if (res != kLwpaErrOk)
    {
      lwpa_rbtree_clear_with_cb(&target->netints, cleanup_target_netint);
    }
  }
  else
  {
    LwpaRbIter netint_iter;
    get_llrp_netint_list(&netint_iter);
    for (LlrpNetint* netint = (LlrpNetint*)netint_iter.node->value; netint;
         netint = (LlrpNetint*)lwpa_rbiter_next(&netint_iter))
    {
      // If the user hasn't provided a list of network interfaces to operate on, failing to
      // intialize on a network interface will be non-fatal and we will log it.
      res = setup_target_netint(&netint->id, target);
      if (res != kLwpaErrOk)
      {
        lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
                 RDMNET_LOG_MSG("Failed to intiailize LLRP target for listening on network interface index %d: '%s'"),
                 netint->id.index, lwpa_strerror(res));
        res = kLwpaErrOk;
      }
    }
  }

  return res;
}

lwpa_error_t create_new_target(const LlrpTargetConfig* config, LlrpTarget** new_target)
{
  lwpa_error_t res = kLwpaErrNoMem;

  llrp_target_t new_handle = get_next_int_handle(&state.handle_mgr);
  if (new_handle == LLRP_TARGET_INVALID)
    return res;

  LlrpTarget* target = llrp_target_alloc();
  if (target)
  {
    res = setup_target_netints(&config->optional, target);

    if (res == kLwpaErrOk)
    {
      target->keys.handle = new_handle;
      target->keys.cid = config->cid;
      res = lwpa_rbtree_insert(&state.targets, target);
      if (res == kLwpaErrOk)
      {
        res = lwpa_rbtree_insert(&state.targets_by_cid, target);
        if (res == kLwpaErrOk)
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
          lwpa_rbtree_remove(&state.targets, target);
          lwpa_rbtree_clear_with_cb(&target->netints, cleanup_target_netint);
          llrp_target_dealloc(target);
        }
      }
      else
      {
        lwpa_rbtree_clear_with_cb(&target->netints, cleanup_target_netint);
        llrp_target_dealloc(target);
      }
    }
    else
    {
      llrp_target_dealloc(target);
    }
  }

  return res;
}

lwpa_error_t get_target(llrp_target_t handle, LlrpTarget** target)
{
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;
  if (!rdmnet_readlock())
    return kLwpaErrSys;

  LlrpTarget* found_target = (LlrpTarget*)lwpa_rbtree_find(&state.targets, &handle);
  if (!found_target || found_target->marked_for_destruction)
  {
    rdmnet_readunlock();
    return kLwpaErrNotFound;
  }
  *target = found_target;
  return kLwpaErrOk;
}

void release_target(LlrpTarget* target)
{
  (void)target;
  rdmnet_readunlock();
}

void destroy_target(LlrpTarget* target, bool remove_from_tree)
{
  if (target)
  {
    lwpa_rbtree_clear_with_cb(&target->netints, cleanup_target_netint);
    if (remove_from_tree)
      lwpa_rbtree_remove(&state.targets, target);
    llrp_target_dealloc(target);
  }
}

/* Callback for IntHandleManager to determine whether a handle is in use */
bool target_handle_in_use(int handle_val)
{
  return lwpa_rbtree_find(&state.targets, &handle_val);
}

int target_cmp_by_handle(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b)
{
  (void)self;

  const LlrpTarget* a = (const LlrpTarget*)node_a->value;
  const LlrpTarget* b = (const LlrpTarget*)node_b->value;

  return (a->keys.handle > b->keys.handle) - (a->keys.handle < b->keys.handle);
}

int target_cmp_by_cid(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b)
{
  (void)self;

  const LlrpTarget* a = (const LlrpTarget*)node_a->value;
  const LlrpTarget* b = (const LlrpTarget*)node_b->value;

  return LWPA_UUID_CMP(&a->keys.cid, &b->keys.cid);
}

int target_netint_cmp(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b)
{
  (void)self;

  const LlrpTargetNetintInfo* a = (const LlrpTargetNetintInfo*)node_a->value;
  const LlrpTargetNetintInfo* b = (const LlrpTargetNetintInfo*)node_b->value;

  if (a->id.ip_type == b->id.ip_type)
  {
    return (a->id.index > b->id.index) - (a->id.index < b->id.index);
  }
  else
  {
    return (a->id.ip_type == kLwpaIpTypeV6) ? 1 : -1;
  }
}

LwpaRbNode* target_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode*)malloc(sizeof(LwpaRbNode));
#else
  return lwpa_mempool_alloc(llrp_target_rb_nodes);
#endif
}

void target_node_free(LwpaRbNode* node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  lwpa_mempool_free(llrp_target_rb_nodes, node);
#endif
}
