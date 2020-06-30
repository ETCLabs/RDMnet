/******************************************************************************
 * Copyright 2020 ETC Inc.
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

#include "etcpal/common.h"
#include "rdm/uid.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/llrp.h"
#include "rdmnet/core/mcast.h"
#include "rdmnet/core/util.h"
#include "rdmnet/core/opts.h"

/***************************** Private types ********************************/

typedef struct DiscoveredTargetInternal DiscoveredTargetInternal;
struct DiscoveredTargetInternal
{
  RdmUid                    uid;
  EtcPalUuid                cid;
  DiscoveredTargetInternal* next;
};

typedef enum
{
  kRCLlrpManagerEventNone,
  kRCLlrpManagerEventTargetDiscovered,
  kRCLlrpManagerEventDiscoveryFinished,
  kRCLlrpManagerEventRdmRespReceived
} rc_llrp_manager_event_t;

typedef struct RCLlrpManagerEvent
{
  rc_llrp_manager_event_t which;

  union
  {
    const LlrpDiscoveredTarget* discovered_target;
    LlrpRdmResponse             rdm_response;
  } args;
} RCLlrpManagerEvent;

#define RC_LLRP_MANAGER_EVENT_INIT \
  {                                \
    kRCLlrpManagerEventNone        \
  }

typedef struct RCLlrpManagerKeys
{
  EtcPalUuid                 cid;
  const RdmnetMcastNetintId* netint;
} RCLlrpManagerKeys;

/***************************** Private macros ********************************/

#define MANAGER_LOCK(mgr_ptr) etcpal_mutex_lock((mgr_ptr)->lock)
#define MANAGER_UNLOCK(mgr_ptr) etcpal_mutex_unlock((mgr_ptr)->lock)

/**************************** Private variables ******************************/

RC_DECLARE_REF_LISTS(managers, 1);

/*********************** Private function prototypes *************************/

// Manager setup and cleanup
static etcpal_error_t get_manager_sockets(RCLlrpManager* manager);
static void           release_manager_sockets(RCLlrpManager* manager);
static void           cleanup_manager_resources(RCLlrpManager* manager, const void* context);

// Periodic state processing
static void process_manager_state(RCLlrpManager* manager, const void* context);
static bool send_next_probe(RCLlrpManager* manager);
static bool update_probe_range(RCLlrpManager* manager);

// Incoming message handling
static void handle_llrp_message(RCLlrpManager* manager, const LlrpMessage* msg, RCLlrpManagerEvent* event);
static void deliver_event_callback(RCLlrpManager* manager, RCLlrpManagerEvent* event);

// Utilities
static EtcPalRbNode*  discovered_target_node_alloc(void);
static void           discovered_target_node_dealloc(EtcPalRbNode* node);
static int            discovered_target_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static void           discovered_target_clear_cb(const EtcPalRbTree* self, EtcPalRbNode* node);
static RCLlrpManager* find_manager_by_message_keys(const RCRefList* list, const RCLlrpManagerKeys* keys);

/*************************** Function definitions ****************************/

etcpal_error_t rc_llrp_manager_module_init(void)
{
  if (!rc_ref_lists_init(&managers))
    return kEtcPalErrNoMem;
  return kEtcPalErrOk;
}

void rc_llrp_manager_module_deinit(void)
{
  rc_ref_lists_remove_all(&managers, (RCRefFunction)cleanup_manager_resources, NULL);
  rc_ref_lists_cleanup(&managers);
}

etcpal_error_t rc_llrp_manager_register(RCLlrpManager* manager)
{
  RDMNET_ASSERT(manager);

  if (!rc_initialized())
    return kEtcPalErrNotInit;

  if (!rc_ref_list_add_ref(&managers.pending, manager))
    return kEtcPalErrNoMem;

  etcpal_error_t res = get_manager_sockets(manager);
  if (res != kEtcPalErrOk)
  {
    rc_ref_list_remove_ref(&managers.pending, manager);
    return res;
  }

  manager->transaction_number = 0;
  manager->discovery_active = false;
  manager->num_clean_sends = 0;
  manager->disc_filter = 0;
  manager->num_known_uids = 0;
  etcpal_rbtree_init(&manager->discovered_targets, discovered_target_compare, discovered_target_node_alloc,
                     discovered_target_node_dealloc);
  return kEtcPalErrOk;
}

void rc_llrp_manager_unregister(RCLlrpManager* manager)
{
  RDMNET_ASSERT(manager);

  rc_ref_list_add_ref(&managers.to_remove, manager);
}

etcpal_error_t rc_llrp_manager_start_discovery(RCLlrpManager* manager, uint16_t filter)
{
  RDMNET_ASSERT(manager);

  if (!manager->discovery_active)
  {
    manager->cur_range_low.manu = 0;
    manager->cur_range_low.id = 0;
    manager->cur_range_high = kRdmBroadcastUid;
    manager->num_clean_sends = 0;
    manager->discovery_active = true;
    manager->disc_filter = filter;

    if (send_next_probe(manager))
    {
      return kEtcPalErrOk;
    }
    else
    {
      manager->discovery_active = false;
      return kEtcPalErrSys;
    }
  }
  else
  {
    return kEtcPalErrAlready;
  }
}

etcpal_error_t rc_llrp_manager_stop_discovery(RCLlrpManager* manager)
{
  RDMNET_ASSERT(manager);

  if (manager->discovery_active)
  {
    etcpal_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
    manager->discovery_active = false;
    return kEtcPalErrOk;
  }
  else
  {
    return kEtcPalErrInvalid;
  }
}

etcpal_error_t rc_llrp_manager_send_rdm_command(RCLlrpManager*             manager,
                                                const LlrpDestinationAddr* destination,
                                                rdmnet_command_class_t     command_class,
                                                uint16_t                   param_id,
                                                const uint8_t*             data,
                                                uint8_t                    data_len,
                                                uint32_t*                  seq_num)
{
  RdmCommandHeader rdm_header;
  rdm_header.source_uid = manager->uid;
  rdm_header.dest_uid = destination->dest_uid;
  rdm_header.transaction_num = (uint8_t)(manager->transaction_number & 0xffu);
  rdm_header.port_id = 1;
  rdm_header.subdevice = destination->subdevice;
  rdm_header.command_class = (rdm_command_class_t)command_class;
  rdm_header.param_id = param_id;

  RdmBuffer      cmd_buf;
  etcpal_error_t res = rdm_pack_command(&rdm_header, data, data_len, &cmd_buf);
  if (res == kEtcPalErrOk)
  {
    LlrpHeader header;
    header.dest_cid = destination->dest_cid;
    header.sender_cid = manager->cid;
    header.transaction_number = manager->transaction_number;

    res = rc_send_llrp_rdm_command(manager->send_sock, manager->send_buf, (manager->netint.ip_type == kEtcPalIpTypeV6),
                                   &header, &cmd_buf);
    if (res == kEtcPalErrOk && seq_num)
      *seq_num = manager->transaction_number++;
  }
  return res;
}

void rc_llrp_manager_module_tick(void)
{
  // Remove any managers marked for destruction.
  if (rdmnet_writelock())
  {
    rc_ref_lists_remove_marked(&managers, (RCRefFunction)cleanup_manager_resources, NULL);
    rc_ref_lists_add_pending(&managers);
    rdmnet_writeunlock();
  }

  rc_ref_list_for_each(&managers.active, (RCRefFunction)process_manager_state, NULL);
}

void rc_llrp_manager_data_received(const uint8_t* data, size_t data_len, const RdmnetMcastNetintId* netint)
{
  RCLlrpManagerEvent event = RC_LLRP_MANAGER_EVENT_INIT;

  RCLlrpManagerKeys keys;
  if (rc_get_llrp_destination_cid(data, data_len, &keys.cid))
  {
    keys.netint = netint;
    bool manager_found = false;

    RCLlrpManager* manager = find_manager_by_message_keys(&managers.active, &keys);
    if (manager)
    {
      manager_found = true;

      LlrpMessage         msg;
      LlrpMessageInterest interest;
      interest.my_cid = keys.cid;
      interest.interested_in_probe_reply = true;
      interest.interested_in_probe_request = false;

      if (rc_parse_llrp_message(data, data_len, &interest, &msg))
      {
        handle_llrp_message(manager, &msg, &event);
        deliver_event_callback(manager, &event);
      }
    }

    if (!manager_found && RDMNET_CAN_LOG(ETCPAL_LOG_DEBUG))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&keys.cid, cid_str);
      RDMNET_LOG_DEBUG("Ignoring LLRP message addressed to unknown LLRP Manager %s", cid_str);
    }
  }
}

etcpal_error_t get_manager_sockets(RCLlrpManager* manager)
{
  etcpal_error_t res = rc_mcast_get_send_socket(&manager->netint, 0, &manager->send_sock);
  if (res == kEtcPalErrOk)
  {
    res = rc_llrp_recv_netint_add(&manager->netint, kLlrpSocketTypeManager);
    if (res != kEtcPalErrOk)
      rc_mcast_release_send_socket(&manager->netint, 0);
  }
  return res;
}

void release_manager_sockets(RCLlrpManager* manager)
{
  rc_llrp_recv_netint_remove(&manager->netint, kLlrpSocketTypeManager);
  rc_mcast_release_send_socket(&manager->netint, 0);
}

void cleanup_manager_resources(RCLlrpManager* manager, const void* context)
{
  ETCPAL_UNUSED_ARG(context);
  RDMNET_ASSERT(manager);

  release_manager_sockets(manager);
  if (manager->discovery_active)
  {
    etcpal_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
  }
  if (manager->callbacks.destroyed)
    manager->callbacks.destroyed(manager);
}

void process_manager_state(RCLlrpManager* manager, const void* context)
{
  ETCPAL_UNUSED_ARG(context);
  if (MANAGER_LOCK(manager))
  {
    RCLlrpManagerEvent event = RC_LLRP_MANAGER_EVENT_INIT;

    if (manager->discovery_active)
    {
      if (etcpal_timer_is_expired(&manager->disc_timer))
      {
        if (!send_next_probe(manager))
        {
          event.which = kRCLlrpManagerEventDiscoveryFinished;
          etcpal_rbtree_clear_with_cb(&manager->discovered_targets, discovered_target_clear_cb);
          manager->discovery_active = false;
        }
      }
    }
    MANAGER_UNLOCK(manager);
    deliver_event_callback(manager, &event);
  }
}

bool send_next_probe(RCLlrpManager* manager)
{
  if (update_probe_range(manager))
  {
    LlrpHeader header;
    header.sender_cid = manager->cid;
    header.dest_cid = *kLlrpBroadcastCid;
    header.transaction_number = manager->transaction_number++;

    LocalProbeRequest request;
    request.filter = manager->disc_filter;
    request.lower_uid = manager->cur_range_low;
    request.upper_uid = manager->cur_range_high;
    request.known_uids = manager->known_uids;
    request.num_known_uids = manager->num_known_uids;

    etcpal_error_t send_res = rc_send_llrp_probe_request(
        manager->send_sock, manager->send_buf, (manager->netint.ip_type == kEtcPalIpTypeV6), &header, &request);
    if (send_res == kEtcPalErrOk)
    {
      etcpal_timer_start(&manager->disc_timer, LLRP_TIMEOUT_MS);
      ++manager->num_clean_sends;
      return true;
    }
    else
    {
      RDMNET_LOG_WARNING("Sending LLRP probe request failed with error: '%s'", etcpal_strerror(send_res));
      return false;
    }
  }
  else
  {
    // We are done with discovery.
    return false;
  }
}

bool update_probe_range(RCLlrpManager* manager)
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
      manager->cur_range_high = kRdmBroadcastUid;
      manager->num_clean_sends = 0;
    }
  }

  // Determine how many known UIDs are in the current range
  manager->num_known_uids = 0;

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  DiscoveredTargetInternal* cur_target =
      (DiscoveredTargetInternal*)etcpal_rbiter_first(&iter, &manager->discovered_targets);
  while (cur_target && (rdm_uid_compare(&cur_target->uid, &manager->cur_range_high) <= 0))
  {
    if (manager->num_known_uids != 0)
    {
      if (manager->num_known_uids + 1 <= LLRP_KNOWN_UID_SIZE)
      {
        manager->known_uids[manager->num_known_uids++] = cur_target->uid;
      }
      else
      {
        // Put the high point of the current range in the middle of the list of Known UIDs.
        manager->cur_range_high = manager->known_uids[(LLRP_KNOWN_UID_SIZE / 2) - 1];
        manager->num_known_uids = LLRP_KNOWN_UID_SIZE / 2;
        break;
      }
    }
    else if (rdm_uid_compare(&cur_target->uid, &manager->cur_range_low) >= 0)
    {
      manager->known_uids[manager->num_known_uids++] = cur_target->uid;
    }
    cur_target = (DiscoveredTargetInternal*)etcpal_rbiter_next(&iter);
  }
  return true;
}

void handle_llrp_message(RCLlrpManager* manager, const LlrpMessage* msg, RCLlrpManagerEvent* event)
{
  if (MANAGER_LOCK(manager))
  {
    switch (msg->vector)
    {
      case VECTOR_LLRP_PROBE_REPLY: {
        const LlrpDiscoveredTarget* target = LLRP_MSG_GET_PROBE_REPLY(msg);

        if (manager->discovery_active && (ETCPAL_UUID_CMP(&msg->header.dest_cid, &manager->cid) == 0))
        {
          DiscoveredTargetInternal* new_target = (DiscoveredTargetInternal*)malloc(sizeof(DiscoveredTargetInternal));
          if (new_target)
          {
            new_target->uid = target->uid;
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
              event->which = kRCLlrpManagerEventTargetDiscovered;
              event->args.discovered_target = &msg->data.probe_reply;
            }
          }
        }
        break;
      }
      case VECTOR_LLRP_RDM_CMD: {
        LlrpRdmResponse* resp = &event->args.rdm_response;
        if (kEtcPalErrOk ==
            rdm_unpack_response(LLRP_MSG_GET_RDM(msg), &resp->rdm_header, &resp->rdm_data, &resp->rdm_data_len))
        {
          resp->seq_num = msg->header.transaction_number;
          resp->source_cid = msg->header.sender_cid;
          event->which = kRCLlrpManagerEventRdmRespReceived;
        }
        break;
      }
    }
    MANAGER_UNLOCK(manager);
  }
}

void deliver_event_callback(RCLlrpManager* manager, RCLlrpManagerEvent* event)
{
  switch (event->which)
  {
    case kRCLlrpManagerEventTargetDiscovered:
      if (manager->callbacks.target_discovered)
        manager->callbacks.target_discovered(manager, event->args.discovered_target);
      break;
    case kRCLlrpManagerEventDiscoveryFinished:
      if (manager->callbacks.discovery_finished)
        manager->callbacks.discovery_finished(manager);
      break;
    case kRCLlrpManagerEventRdmRespReceived:
      if (manager->callbacks.rdm_response_received)
        manager->callbacks.rdm_response_received(manager, &event->args.rdm_response);
      break;
    case kRCLlrpManagerEventNone:
    default:
      break;
  }
}

EtcPalRbNode* discovered_target_node_alloc(void)
{
#if RDMNET_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  // TODO
  return NULL;
#endif
}

void discovered_target_node_dealloc(EtcPalRbNode* node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  // TODO
  ETCPAL_UNUSED_ARG(node);
#endif
}

int discovered_target_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);
  const DiscoveredTargetInternal* a = (const DiscoveredTargetInternal*)value_a;
  const DiscoveredTargetInternal* b = (const DiscoveredTargetInternal*)value_b;
  return rdm_uid_compare(&a->uid, &b->uid);
}

void discovered_target_clear_cb(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  DiscoveredTargetInternal* target = (DiscoveredTargetInternal*)node->value;
  while (target)
  {
    DiscoveredTargetInternal* next_target = target->next;
    free(target);
    target = next_target;
  }
  discovered_target_node_dealloc(node);
}

static bool cid_and_netint_equal_predicate(void* ref, const void* context)
{
  RCLlrpManager*           manager = (RCLlrpManager*)ref;
  const RCLlrpManagerKeys* keys = (const RCLlrpManagerKeys*)context;

  return ((manager->netint.ip_type == keys->netint->ip_type) && (manager->netint.index == keys->netint->index) &&
          (ETCPAL_UUID_CMP(&manager->cid, &keys->cid) == 0));
}

RCLlrpManager* find_manager_by_message_keys(const RCRefList* list, const RCLlrpManagerKeys* keys)
{
  return (RCLlrpManager*)rc_ref_list_find_ref(list, cid_and_netint_equal_predicate, keys);
}
