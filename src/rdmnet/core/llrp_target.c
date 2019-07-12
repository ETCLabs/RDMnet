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

#define LLRP_TARGET_MAX_RB_NODES                              \
  (RDMNET_LLRP_MAX_TARGETS + RDMNET_LLRP_MAX_TARGET_NETINTS + \
   (RDMNET_LLRP_MAX_TARGETS + RDMNET_LLRP_MAX_TARGET_NETINTS))

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define llrp_target_alloc() malloc(sizeof(LlrpTarget))
#define llrp_target_dealloc(ptr) free(ptr)
#else
#define llrp_target_alloc() lwpa_mempool_alloc(llrp_targets)
#define llrp_target_dealloc(ptr) lwpa_mempool_free(llrp_targets, ptr)
#endif

#define INIT_CALLBACK_INFO(cbptr) ((cbptr)->which = kTargetCallbackNone)

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(llrp_targets, LlrpTarget, RDMNET_LLRP_MAX_TARGETS);
LWPA_MEMPOOL_DEFINE(llrp_target_rb_nodes, LwpaRbNode, RDMNET_LLRP_MAX_RB_NODES);
#endif

static struct LlrpTargetState
{
  LwpaRbTree targets;
  IntHandleManager handle_mgr;

  LwpaRbTree sys_netints;

  uint8_t lowest_hardware_addr[6];
} state;

/*********************** Private function prototypes *************************/

static lwpa_error_t init_sys_netints();

// Creating, destroying, and finding targets
static lwpa_error_t create_new_target(const LlrpTargetConfig* config, LlrpTarget** new_target);
static lwpa_error_t get_target(llrp_target_t handle, LlrpTarget** target);
static void release_target(LlrpTarget* target);
static void destroy_target(LlrpTarget* target, bool remove_from_tree);
static bool target_handle_in_use(int handle_val);

// Target state processing
static void target_socket_activity(const LwpaPollEvent* event, PolledSocketOpaqueData data);
static void target_socket_error(LlrpTargetNetintInfo* target_netint, lwpa_error_t err);
static void target_data_received(LlrpTargetNetintInfo* target_netint, const uint8_t* data, size_t size);
static void handle_llrp_message(LlrpTarget* target, LlrpTargetNetintInfo* netint, const LlrpMessage* msg,
                                TargetCallbackDispatchInfo* cb);
static void fill_callback_info(const LlrpTarget* target, TargetCallbackDispatchInfo* info);
static void deliver_callback(TargetCallbackDispatchInfo* info);
static void process_target_state(LlrpTarget* target);

// Target tracking using LwpaRbTrees
static LwpaRbNode* target_node_alloc();
static void target_node_free(LwpaRbNode* node);
static int target_cmp(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b);
static int netint_cmp(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_llrp_target_init()
{
  lwpa_error_t res = kLwpaErrOk;

#if !RDMNET_DYNAMIC_MEM
  /* Init memory pool */
  res = lwpa_mempool_init(llrp_sockets);
#endif

  if (res == kLwpaErrOk)
    res = init_sys_netints();

  if (res == kLwpaErrOk)
  {
    lwpa_rbtree_init(&state.targets, target_cmp, target_node_alloc, target_node_free);
    lwpa_rbtree_init(&state.sys_netints, netint_cmp, target_node_alloc, target_node_free);
    init_int_handle_manager(&state.handle_mgr, target_handle_in_use);
  }
  return res;
}

lwpa_error_t init_sys_netints()
{
  size_t num_sys_netints = lwpa_netint_get_num_interfaces();
  if (num_sys_netints == 0)
    return kLwpaErrNoNetints;

  const LwpaNetintInfo* netint_list = lwpa_netint_get_interfaces();

  uint8_t null_mac[6];
  memset(null_mac, 0, sizeof null_mac);

  lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, RDMNET_LOG_MSG("Initializing network interfaces for LLRP..."));
  size_t i = 0;
  while (i < num_sys_netints)
  {
    const LwpaNetintInfo* netint = &netint_list[i];
    char addr_str[LWPA_INET6_ADDRSTRLEN];
    addr_str[0] = '\0';
    if (LWPA_CAN_LOG(rdmnet_log_params, LWPA_LOG_WARNING))
    {
      lwpa_inet_ntop(&netint->addr, addr_str, LWPA_INET6_ADDRSTRLEN);
    }

    // Create a test socket on each network interface. If the socket create fails, we remove that
    // interface from the set that LLRP targets listen on.
    lwpa_socket_t test_socket;
    lwpa_error_t test_res = create_llrp_socket(netint->addr.type, netint->index, false, &test_socket);
    if (test_res == kLwpaErrOk)
    {
      lwpa_close(test_socket);

      lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, RDMNET_LOG_MSG("  Set up LLRP network interface %s for listening."),
               addr_str);

      if (memcmp(netint->mac, null_mac, 6) != 0)
      {
        if (netint == state.sys_netints)
        {
          memcpy(state.lowest_hardware_addr, netint->mac, 6);
        }
        else
        {
          if (memcmp(netint->mac, state.lowest_hardware_addr, 6) < 0)
          {
            memcpy(state.lowest_hardware_addr, netint->mac, 6);
          }
        }
      }

      ++i;
    }
    else
    {
      // Remove the network interface from the array.
      if (i < state.num_sys_netints - 1)
      {
        memmove(&state.sys_netints[i], &state.sys_netints[i + 1],
                sizeof(LwpaNetintInfo) * (state.num_sys_netints - (i + 1)));
      }
      --state.num_sys_netints;
      lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
               RDMNET_LOG_MSG("  Error creating test socket on LLRP network interface %s: '%s'. Skipping!"), addr_str,
               lwpa_strerror(test_res));
    }
  }

  if (state.num_sys_netints == 0)
  {
    lwpa_log(rdmnet_log_params, LWPA_LOG_ERR, RDMNET_LOG_MSG("No usable LLRP interfaces found."));
    return kLwpaErrNoNetints;
  }
  return kLwpaErrOk;
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
#if RDMNET_DYNAMIC_MEM
  free(state.sys_netints);
#endif
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
      *handle = target->handle;
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
    if (resp->interface_id < target->num_netints)
    {
      LlrpTargetNetintInfo* netint = &target->netints[resp->interface_id];
      LlrpHeader header;

      header.dest_cid = resp->dest_cid;
      header.sender_cid = target->cid;
      header.transaction_number = resp->seq_num;

      res = send_llrp_rdm_response(netint->sys_sock, netint->send_buf, (netint->ip_type == kLwpaIpTypeV6), &header,
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
  for (LlrpTargetNetintInfo* netint = target->netints; netint < target->netints + target->num_netints; ++netint)
  {
    if (netint->reply_pending)
    {
      if (lwpa_timer_is_expired(&netint->reply_backoff))
      {
        LlrpHeader header;
        header.sender_cid = target->cid;
        header.dest_cid = netint->pending_reply_cid;
        header.transaction_number = netint->pending_reply_trans_num;

        DiscoveredLlrpTarget target_info;
        target_info.cid = target->cid;
        target_info.uid = target->uid;
        memcpy(target_info.hardware_address, state.lowest_hardware_addr, 6);
        target_info.component_type = target->component_type;

        lwpa_error_t send_res = send_llrp_probe_reply(netint->sys_sock, netint->send_buf,
                                                      (netint->ip_type == kLwpaIpTypeV6), &header, &target_info);
        if (send_res != kLwpaErrOk && LWPA_CAN_LOG(rdmnet_log_params, LWPA_LOG_WARNING))
        {
          char cid_str[LWPA_UUID_STRING_BYTES];
          lwpa_uuid_to_string(cid_str, &header.dest_cid);
          lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
                   RDMNET_LOG_MSG("Error sending probe reply to manager CID %s on interface index %u"), cid_str,
                   netint->index);
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

void target_socket_activity(const LwpaPollEvent* event, PolledSocketOpaqueData data)
{
  static uint8_t llrp_target_recv_buf[LLRP_MAX_MESSAGE_SIZE];

  if (event->events & LWPA_POLL_ERR)
  {
    target_socket_error((LlrpTargetNetintInfo*)data.ptr, event->err);
  }
  else if (event->events & LWPA_POLL_IN)
  {
    int recv_res = lwpa_recv(event->socket, llrp_target_recv_buf, LLRP_MAX_MESSAGE_SIZE, 0);
    if (recv_res <= 0)
    {
      if (recv_res != kLwpaErrMsgSize)
      {
        target_socket_error((LlrpTargetNetintInfo*)data.ptr, event->err);
      }
    }
    else
    {
      target_data_received((LlrpTargetNetintInfo*)data.ptr, llrp_target_recv_buf, recv_res);
    }
  }
}

void target_socket_error(LlrpTargetNetintInfo* target_netint, lwpa_error_t err)
{
  (void)target_netint;
  (void)err;
  // TODO
}

void target_data_received(LlrpTargetNetintInfo* target_netint, const uint8_t* data, size_t size)
{
  TargetCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  if (rdmnet_readlock())
  {
    LlrpMessage msg;
    LlrpMessageInterest interest;
    interest.my_cid = target_netint->target->cid;
    interest.interested_in_probe_reply = false;
    interest.interested_in_probe_request = true;
    interest.my_uid = target_netint->target->uid;

    if (parse_llrp_message(data, size, &interest, &msg))
    {
      handle_llrp_message(target_netint->target, target_netint, &msg, &cb);
    }
    rdmnet_readunlock();
  }

  deliver_callback(&cb);
}

void handle_llrp_message(LlrpTarget* target, LlrpTargetNetintInfo* netint, const LlrpMessage* msg,
                         TargetCallbackDispatchInfo* cb)
{
  switch (msg->vector)
  {
    case VECTOR_LLRP_PROBE_REQUEST:
    {
      const RemoteProbeRequest* request = LLRP_MSG_GET_PROBE_REQUEST(msg);
      // TODO allow multiple probe replies to be queued
      if (request->contains_my_uid && !netint->reply_pending)
      {
        int backoff_ms;

        // Check the filter values.
        if (!((request->filter & LLRP_FILTERVAL_BROKERS_ONLY) && target->component_type != kLlrpCompBroker) &&
            !(request->filter & LLRP_FILTERVAL_CLIENT_CONN_INACTIVE && target->connected_to_broker))
        {
          netint->reply_pending = true;
          netint->pending_reply_cid = msg->header.sender_cid;
          netint->pending_reply_trans_num = msg->header.transaction_number;
          backoff_ms = (rand() * LLRP_MAX_BACKOFF_MS / RAND_MAX);
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
      if (kLwpaErrOk == rdmresp_unpack_command(LLRP_MSG_GET_RDM(msg), &remote_cmd->rdm))
      {
        remote_cmd->src_cid = msg->header.sender_cid;
        remote_cmd->seq_num = msg->header.transaction_number;
        remote_cmd->interface_id = netint - target->netints;

        cb->which = kTargetCallbackRdmCmdReceived;
        fill_callback_info(target, cb);
      }
    }
    default:
      break;
  }
}

void fill_callback_info(const LlrpTarget* target, TargetCallbackDispatchInfo* info)
{
  info->handle = target->handle;
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

lwpa_error_t setup_target_netint(lwpa_iptype_t ip_type, unsigned int netint_index, LlrpTarget* target,
                                 LlrpTargetNetintInfo* netint_info)
{
  lwpa_error_t res = create_llrp_socket(ip_type, netint_index, false, &netint_info->sys_sock);
  if (res != kLwpaErrOk)
    return res;

  netint_info->poll_info.callback = target_socket_activity;
  netint_info->poll_info.data.ptr = netint_info;
  res = rdmnet_core_add_polled_socket(netint_info->sys_sock, LWPA_POLL_IN, &netint_info->poll_info);

  if (res == kLwpaErrOk)
  {
    netint_info->ip_type = ip_type;
    netint_info->index = netint_index;
    netint_info->reply_pending = false;
    netint_info->target = target;
  }
  else
  {
    lwpa_close(netint_info->sys_sock);
  }

  return res;
}

void cleanup_target_netint(LlrpTargetNetintInfo* netint_info)
{
  rdmnet_core_remove_polled_socket(netint_info->sys_sock);
  lwpa_close(netint_info->sys_sock);
}

lwpa_error_t setup_target_netints(const LlrpTargetOptionalConfig* config, LlrpTarget* target)
{
  lwpa_error_t res = kLwpaErrOk;
  target->num_netints = 0;

  if (config->netint_arr && config->num_netints > 0)
  {
#if RDMNET_DYNAMIC_MEM
    target->netints = (LlrpTargetNetintInfo*)calloc(config->num_netints, sizeof(LlrpTargetNetintInfo));
    if (!target->netints)
      res = kLwpaErrNoMem;
#else
    if (config->num_netints > RDMNET_LLRP_MAX_TARGET_NETINTS)
      res = kLwpaErrNoMem;
#endif
    if (res == kLwpaErrOk)
    {
      for (size_t i = 0; i < config->num_netints; ++i)
      {
        res = setup_target_netint(config->netint_arr[i].ip_type, config->netint_arr[i].index, target,
                                  &target->netints[i]);
        if (res == kLwpaErrOk)
          ++target->num_netints;
        else
          break;
      }
    }
  }
  else
  {
#if RDMNET_DYNAMIC_MEM
    target->netints = (LlrpTargetNetintInfo*)calloc(state.num_sys_netints, sizeof(LlrpTargetNetintInfo));
    if (!target->netints)
      res = kLwpaErrNoMem;
#endif
    if (res == kLwpaErrOk)
    {
      for (size_t i = 0; i < state.num_sys_netints; ++i)
      {
        res = setup_target_netint(state.sys_netints[i].addr.type, state.sys_netints[i].index, target,
                                  &target->netints[i]);
        if (res == kLwpaErrOk)
          ++target->num_netints;
        else
          break;
      }
    }
  }

  if (res != kLwpaErrOk)
  {
#if RDMNET_DYNAMIC_MEM
    if (target->netints)
    {
#endif
      for (LlrpTargetNetintInfo* netint = target->netints; netint < target->netints + target->num_netints; ++netint)
      {
        cleanup_target_netint(netint);
      }
#if RDMNET_DYNAMIC_MEM
      free(target->netints);
    }
#endif
  }
  return res;
}

lwpa_error_t create_new_target(const LlrpTargetConfig* config, LlrpTarget** new_target)
{
  lwpa_error_t res = kLwpaErrNoMem;

  llrp_target_t new_handle = get_next_int_handle(&state.handle_mgr);
  if (new_handle == LLRP_TARGET_INVALID)
    return res;

  LlrpTarget* target = (LlrpTarget*)llrp_target_alloc();
  if (target)
  {
    res = setup_target_netints(&config->optional, target);

    if (res == kLwpaErrOk)
    {
      if (0 != lwpa_rbtree_insert(&state.targets, target))
      {
        target->cid = config->cid;
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
        llrp_target_dealloc(target);
        res = kLwpaErrNoMem;
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
    for (LlrpTargetNetintInfo* netint = target->netints; netint < target->netints + target->num_netints; ++netint)
    {
      cleanup_target_netint(netint);
    }
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

int target_cmp(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b)
{
  (void)self;

  const LlrpTarget* a = (const LlrpTarget*)node_a->value;
  const LlrpTarget* b = (const LlrpTarget*)node_b->value;

  return a->handle - b->handle;
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
