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
LWPA_MEMPOOL_DEFINE(llrp_target_rb_nodes, LwpaRbNode, RDMNET_LLRP_MAX_TARGETS);
#endif

static struct LlrpTargetState
{
  LwpaRbTree targets;
  IntHandleManager handle_mgr;

#if RDMNET_DYNAMIC_MEM
  LwpaNetintInfo *sys_netints;
#else
  LwpaNetintInfo sys_netints[RDMNET_LLRP_MAX_NETWORK_INTERFACES];
#endif
  size_t num_sys_netints;
  uint8_t lowest_hardware_addr[6];
} state;

/*********************** Private function prototypes *************************/

static lwpa_error_t init_sys_netints();

// Helper functions for creating managers and targets
static bool target_handle_in_use(int handle_val);
static lwpa_error_t create_new_target(const LlrpTargetConfig *config, LlrpTarget **new_target);
static lwpa_error_t get_target(llrp_target_t handle, LlrpTarget **target);
static void release_target(LlrpTarget *target);
static void destroy_target(LlrpTarget *target, bool remove_from_tree);
static lwpa_socket_t create_sys_socket(const LwpaSockaddr *saddr, const LwpaIpAddr *netint);
static bool subscribe_multicast(lwpa_socket_t lwpa_sock, bool manager, const LwpaIpAddr *netint);

static void llrp_target_socket_activity(const LwpaPollEvent *event, PolledSocketOpaqueData data);
static void llrp_target_socket_error(LlrpTargetNetintInfo *target_netint, lwpa_error_t err);
static void llrp_target_data_received(LlrpTargetNetintInfo *target_netint, const uint8_t *data, size_t size);
static void fill_callback_info(const LlrpTarget *target, TargetCallbackDispatchInfo *info);
static void deliver_callback(TargetCallbackDispatchInfo *info);

static LwpaRbNode *target_node_alloc();
static void target_node_free(LwpaRbNode *node);
static int target_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b);

// Helper functions for rdmnet_llrp_udpate()
static void process_target_state(LlrpTarget *target);
static void register_message_interest(LlrpTarget *target, LlrpMessageInterest *interest);
static void handle_llrp_message(LlrpTarget *target, LlrpTargetNetintInfo *netint, const LlrpMessage *msg,
                                TargetCallbackDispatchInfo *cb);

/*************************** Function definitions ****************************/

/*! \brief Initialize the LLRP module.
 *
 *  Do all necessary initialization before other LLRP functions can be called.
 *
 *  \return #kLwpaErrOk: Initialization successful.\n
 *          #kLwpaErrSys: An internal library of system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
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
    init_int_handle_manager(&state.handle_mgr, target_handle_in_use);
  }
  return res;
}

lwpa_error_t init_sys_netints()
{
  state.num_sys_netints = lwpa_netint_get_num_interfaces();
  if (state.num_sys_netints == 0)
    return kLwpaErrNoNetints;

#if RDMNET_DYNAMIC_MEM
  state.sys_netints = calloc(state.num_sys_netints, sizeof(LwpaNetintInfo));
  if (!state.sys_netints)
  {
    state.num_sys_netints = 0;
    return kLwpaErrNoMem;
  }
#else
  if (state.num_sys_netints > RDMNET_LLRP_MAX_TARGET_NETINTS)
  {
    state.num_sys_netints = 0;
    return kLwpaErrNoMem;
  }
#endif
  state.num_sys_netints = lwpa_netint_get_interfaces(state.sys_netints, state.num_sys_netints);

  for (const LwpaNetintInfo *netint = state.sys_netints; netint < state.sys_netints + state.num_sys_netints; ++netint)
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
  return kLwpaErrOk;
}

static void target_dealloc(const LwpaRbTree *self, LwpaRbNode *node)
{
  (void)self;

  LlrpTarget *target = (LlrpTarget *)node->value;
  if (target)
    destroy_target(target, false);
  target_node_free(node);
}

/*! \brief Deinitialize the LLRP module.
 *
 *  Set the LLRP module back to an uninitialized state. All existing connections will be
 *  closed/disconnected. Calls to other LLRP API functions will fail until rdmnet_llrp_init() is called
 *  again.
 */
void rdmnet_llrp_target_deinit()
{
  lwpa_rbtree_clear_with_cb(&state.targets, target_dealloc);
#if RDMNET_DYNAMIC_MEM
  free(state.sys_netints);
#endif
  memset(&state, 0, sizeof state);
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
lwpa_error_t rdmnet_llrp_target_create(const LlrpTargetConfig *config, llrp_target_t *handle)
{
  if (!config || !handle)
    return kLwpaErrInvalid;
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;

  lwpa_error_t res = kLwpaErrSys;
  if (rdmnet_writelock())
  {
    // Attempt to create the LLRP target, give it a unique handle and add it to the map.
    LlrpTarget *target;
    res = create_new_target(config, &target);

    if (res == kLwpaErrOk)
    {
      *handle = target->handle;
    }
    rdmnet_writeunlock();
  }

  return res;
}

/*! \brief Close and deallocate an LLRP socket.
 *
 *  Also closes the underlying system sockets.
 *
 *  \param[in] handle LLRP socket to close.
 *  \return true (closed successfully) or false (not closed successfully).
 */
void rdmnet_llrp_target_destroy(llrp_target_t handle)
{
  LlrpTarget *target;
  lwpa_error_t res = get_target(handle, &target);
  if (res != kLwpaErrOk)
    return;

  target->marked_for_destruction = true;
  release_target(target);
}

void process_target_state(LlrpTarget *target)
{
  for (LlrpTargetNetintInfo *netint = target->netints; netint < target->netints + target->num_netints; ++netint)
  {
    if (netint->reply_pending)
    {
      if (lwpa_timer_isexpired(&netint->reply_backoff))
      {
        LlrpHeader header;
        header.sender_cid = target->cid;
        header.dest_cid = netint->pending_reply_cid;
        header.transaction_number = netint->pending_reply_trans_num;

        DiscoveredLlrpTarget target_info;
        target_info.target_cid = target->cid;
        target_info.target_uid = target->uid;
        memcpy(target_info.hardware_address, state.lowest_hardware_addr, 6);
        target_info.component_type = target->component_type;

        send_llrp_probe_reply(netint->sys_sock, netint->send_buf, lwpaip_is_v6(&netint->ip), &header, &target_info);

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
    LlrpTarget *destroy_list = NULL;
    LlrpTarget **next_destroy_list_entry = &destroy_list;

    LwpaRbIter target_iter;
    lwpa_rbiter_init(&target_iter);

    LlrpTarget *target = (LlrpTarget *)lwpa_rbiter_first(&target_iter, &state.targets);
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
      target = lwpa_rbiter_next(&target_iter);
    }

    // Now do the actual destruction
    if (destroy_list)
    {
      LlrpTarget *to_destroy = destroy_list;
      while (to_destroy)
      {
        LlrpTarget *next = to_destroy->next_to_destroy;
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
    LlrpTarget *target = (LlrpTarget *)lwpa_rbiter_first(&target_iter, &state.targets);

    while (target)
    {
      process_target_state(target);
      target = lwpa_rbiter_next(&target_iter);
    }

    rdmnet_readunlock();
  }
}

/*! \brief Update the Broker connection state of an LLRP Target socket.
 *
 *  If an LLRP Target is associated with an RPT Client, this should be called each time the Client
 *  connects or disconnects from the Broker. This affects whether the LLRP Target responds to
 *  filtered LLRP probe requests.
 *
 *  \param[in] handle Handle to LLRP Target for which to update the connection state.
 *  \param[in] connected_to_broker Whether the LLRP Target is currently connected to a Broker.
 */
void rdmnet_llrp_target_update_connection_state(llrp_target_t handle, bool connected_to_broker)
{
  (void)handle;
  (void)connected_to_broker;
  /* TODO */
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
lwpa_error_t rdmnet_llrp_send_rdm_response(llrp_target_t handle, const LlrpLocalRdmResponse *resp)
{
  if (!resp)
    return kLwpaErrInvalid;

  RdmBuffer resp_buf;
  lwpa_error_t res = rdmresp_create_response(&resp->rdm, &resp_buf);
  if (res != kLwpaErrOk)
    return res;

  LlrpTarget *target;
  res = get_target(handle, &target);
  if (res == kLwpaErrOk)
  {
    if (resp->interface_index < target->num_netints)
    {
      LlrpTargetNetintInfo *netint = &target->netints[resp->interface_index];
      LlrpHeader header;

      header.dest_cid = resp->dest_cid;
      header.sender_cid = target->cid;
      header.transaction_number = resp->seq_num;

      res = send_llrp_rdm_response(netint->sys_sock, netint->send_buf, lwpaip_is_v6(&netint->ip), &header, &resp_buf);
    }
    else
    {
      // Something has changed about the system network interfaces since this command was received.
      res = kLwpaErrSys;
    }
  }
  return res;
}

void llrp_target_socket_activity(const LwpaPollEvent *event, PolledSocketOpaqueData data)
{
  static uint8_t llrp_target_recv_buf[LLRP_MAX_MESSAGE_SIZE];

  if (event->events & LWPA_POLL_ERR)
  {
    llrp_target_socket_error((LlrpTargetNetintInfo *)data.ptr, event->err);
  }
  else if (event->events & LWPA_POLL_IN)
  {
    int recv_res = lwpa_recv(event->socket, llrp_target_recv_buf, LLRP_MAX_MESSAGE_SIZE, 0);
    if (recv_res <= 0)
    {
      if (recv_res != kLwpaErrMsgSize)
      {
        llrp_target_socket_error((LlrpTargetNetintInfo *)data.ptr, event->err);
      }
    }
    else
    {
      llrp_target_data_received((LlrpTargetNetintInfo *)data.ptr, llrp_target_recv_buf, recv_res);
    }
  }
}

void llrp_target_socket_error(LlrpTargetNetintInfo *target_netint, lwpa_error_t err)
{
  // TODO
}

void llrp_target_data_received(LlrpTargetNetintInfo *target_netint, const uint8_t *data, size_t size)
{
  TargetCallbackDispatchInfo cb;
  INIT_CALLBACK_INFO(&cb);

  if (rdmnet_readlock())
  {
    LlrpMessage msg;
    LlrpMessageInterest interest;
    register_message_interest(target_netint->target, &interest);

    if (parse_llrp_message(data, size, &interest, &msg))
    {
      handle_llrp_message(target_netint->target, target_netint, &msg, &cb);
    }
    rdmnet_readunlock();
  }

  deliver_callback(&cb);
}

void register_message_interest(LlrpTarget *target, LlrpMessageInterest *interest)
{
  interest->my_cid = target->cid;
  interest->interested_in_probe_reply = false;
  interest->interested_in_probe_request = true;
  interest->my_uid = target->uid;
}

void handle_llrp_message(LlrpTarget *target, LlrpTargetNetintInfo *netint, const LlrpMessage *msg,
                         TargetCallbackDispatchInfo *cb)
{
  switch (msg->vector)
  {
    case VECTOR_LLRP_PROBE_REQUEST:
    {
      const RemoteProbeRequest *request = LLRP_MSG_GET_PROBE_REQUEST(msg);
      // TODO allow multiple probe replies to be queued
      if (request->contains_my_uid && !netint->reply_pending)
      {
        int backoff_ms;

        /* Check the filter values. */
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
      // to report at this time.
      break;
    }
    case VECTOR_LLRP_RDM_CMD:
    {
      LlrpRemoteRdmCommand *remote_cmd = &cb->args.cmd_received.cmd;
      if (kLwpaErrOk == rdmresp_unpack_command(LLRP_MSG_GET_RDM(msg), &remote_cmd->rdm))
      {
        remote_cmd->src_cid = msg->header.sender_cid;
        remote_cmd->seq_num = msg->header.transaction_number;
        remote_cmd->interface_index = netint - target->netints;

        cb->which = kTargetCallbackRdmCmdReceived;
        fill_callback_info(target, cb);
      }
    }
    default:
      break;
  }
}

void fill_callback_info(const LlrpTarget *target, TargetCallbackDispatchInfo *info)
{
  info->handle = target->handle;
  info->cbs = target->callbacks;
  info->context = target->callback_context;
}

void deliver_callback(TargetCallbackDispatchInfo *info)
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

lwpa_error_t setup_target_netint(const LwpaIpAddr *netint_addr, LlrpTarget *target, LlrpTargetNetintInfo *netint_info)
{
  lwpa_error_t res = create_llrp_socket(netint_addr, false, &netint_info->sys_sock);
  if (res != kLwpaErrOk)
    return res;

  netint_info->poll_info.callback = llrp_target_socket_activity;
  netint_info->poll_info.data.ptr = netint_info;
  res = rdmnet_core_add_polled_socket(netint_info->sys_sock, LWPA_POLL_IN, &netint_info->poll_info);

  if (res == kLwpaErrOk)
  {
    netint_info->ip = *netint_addr;
    netint_info->reply_pending = false;
    netint_info->target = target;
  }
  else
  {
    lwpa_close(netint_info->sys_sock);
  }

  return res;
}

void cleanup_target_netint(LlrpTargetNetintInfo *netint_info)
{
  rdmnet_core_remove_polled_socket(netint_info->sys_sock);
  lwpa_close(netint_info->sys_sock);
}

lwpa_error_t setup_target_netints(const LlrpTargetOptionalConfig *config, LlrpTarget *target)
{
  lwpa_error_t res = kLwpaErrOk;
  target->num_netints = 0;

  if (config->netint_arr && config->num_netints > 0)
  {
#if RDMNET_DYNAMIC_MEM
    target->netints = calloc(config->num_netints, sizeof(LlrpTargetNetintInfo));
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
        res = setup_target_netint(&config->netint_arr[i], target, &target->netints[i]);
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
    target->netints = calloc(state.num_sys_netints, sizeof(LlrpTargetNetintInfo));
    if (!target->netints)
      res = kLwpaErrNoMem;
#endif
    if (res == kLwpaErrOk)
    {
      for (size_t i = 0; i < state.num_sys_netints; ++i)
      {
        res = setup_target_netint(&state.sys_netints[i].addr, target, &target->netints[i]);
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
      for (LlrpTargetNetintInfo *netint = target->netints; netint < target->netints + target->num_netints; ++netint)
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

/* Callback for IntHandleManager to determine whether a handle is in use */
bool target_handle_in_use(int handle_val)
{
  return lwpa_rbtree_find(&state.targets, &handle_val);
}

lwpa_error_t create_new_target(const LlrpTargetConfig *config, LlrpTarget **new_target)
{
  lwpa_error_t res = kLwpaErrNoMem;

  llrp_target_t new_handle = get_next_int_handle(&state.handle_mgr);
  if (new_handle == LLRP_TARGET_INVALID)
    return res;

  LlrpTarget *target = llrp_target_alloc();
  if (target)
  {
    res = setup_target_netints(&config->optional, target);

    if (res == kLwpaErrOk)
    {
      if (0 != lwpa_rbtree_insert(&state.targets, target))
      {
        target->cid = config->cid;
        if (rdmnet_uid_is_dynamic_uid_request(&config->optional.uid))
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

lwpa_error_t get_target(llrp_target_t handle, LlrpTarget **target)
{
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;
  if (!rdmnet_readlock())
    return kLwpaErrSys;

  LlrpTarget *found_target = (LlrpTarget *)lwpa_rbtree_find(&state.targets, &handle);
  if (!found_target || found_target->marked_for_destruction)
  {
    rdmnet_readunlock();
    return kLwpaErrNotFound;
  }
  *target = found_target;
  return kLwpaErrOk;
}

void release_target(LlrpTarget *target)
{
  (void)target;
  rdmnet_readunlock();
}

void destroy_target(LlrpTarget *target, bool remove_from_tree)
{
  if (target)
  {
    for (LlrpTargetNetintInfo *netint = target->netints; netint < target->netints + target->num_netints; ++netint)
    {
      rdmnet_core_remove_polled_socket(netint->sys_sock);
      lwpa_close(netint->sys_sock);
    }
    if (remove_from_tree)
      lwpa_rbtree_remove(&state.targets, target);
    llrp_target_dealloc(target);
  }
}

int target_cmp(const LwpaRbTree *self, const LwpaRbNode *node_a, const LwpaRbNode *node_b)
{
  (void)self;

  LlrpTarget *a = (LlrpTarget *)node_a->value;
  LlrpTarget *b = (LlrpTarget *)node_b->value;

  return a->handle - b->handle;
}

LwpaRbNode *target_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode *)malloc(sizeof(LwpaRbNode));
#else
  return lwpa_mempool_alloc(llrp_target_rb_nodes);
#endif
}

void target_node_free(LwpaRbNode *node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  lwpa_mempool_free(llrp_target_rb_nodes, node);
#endif
}
