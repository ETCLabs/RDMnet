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

#include "rdmnet/core/llrp_target.h"

#include "etcpal/inet.h"
#include "rdm/responder.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/mcast.h"
#include "rdmnet/core/util.h"

/***************************** Private types ********************************/

typedef enum
{
  kRCLlrpTargetEventNone,
  kRCLlrpTargetEventRdmCmdReceived
} rc_llrp_target_event_t;

typedef struct RCLlrpTargetEvent
{
  rc_llrp_target_event_t which;
  LlrpRdmCommand         rdm_cmd;
} RCLlrpTargetEvent;

#define RC_LLRP_TARGET_EVENT_INIT \
  {                               \
    kRCLlrpTargetEventNone        \
  }

typedef struct LlrpTargetIncomingMessage
{
  const uint8_t*             data;
  size_t                     data_len;
  const EtcPalMcastNetintId* netint;
} LlrpTargetIncomingMessage;

/***************************** Private macros ********************************/

#define TARGET_LOCK(target_ptr) (RDMNET_ASSERT_VERIFY(target_ptr) && etcpal_mutex_lock((target_ptr)->lock))
#define TARGET_UNLOCK(target_ptr)            \
  if (RDMNET_ASSERT_VERIFY(target_ptr))      \
  {                                          \
    etcpal_mutex_unlock((target_ptr)->lock); \
  }

/**************************** Private variables ******************************/

RC_DECLARE_REF_LISTS(targets, RC_MAX_LLRP_TARGETS);

/*********************** Private function prototypes *************************/

// Target setup and cleanup
static etcpal_error_t setup_target_netints(RCLlrpTarget*              target,
                                           const EtcPalMcastNetintId* netints,
                                           size_t                     num_netints);
static etcpal_error_t setup_target_netint(const EtcPalMcastNetintId* netint_id, RCLlrpTargetNetintInfo* netint);
static void           cleanup_target_netints(RCLlrpTarget* target);
static RCLlrpTargetNetintInfo* get_target_netint(RCLlrpTarget* target, const EtcPalMcastNetintId* id);
static void                    cleanup_target_resources(RCLlrpTarget* target, const void* context);

// Periodic state processing
static void process_target_state(RCLlrpTarget* target, const void* context);

// Incoming message handling
static void           target_handle_llrp_message(RCLlrpTarget* target, const LlrpTargetIncomingMessage* message);
static void           deliver_event_callback(RCLlrpTarget* target, RCLlrpTargetEvent* event);
static void           send_response_if_requested(RCLlrpTarget*                target,
                                                 const RCLlrpTargetEvent*     event,
                                                 RCLlrpTargetSyncRdmResponse* response);
static etcpal_error_t target_send_ack_internal(RCLlrpTarget*              target,
                                               const EtcPalUuid*          dest_cid,
                                               uint32_t                   received_seq_num,
                                               const RdmCommandHeader*    received_rdm_header,
                                               const EtcPalMcastNetintId* received_netint_id,
                                               const uint8_t*             response_data,
                                               uint8_t                    response_data_len);
static etcpal_error_t target_send_nack_internal(RCLlrpTarget*              target,
                                                const EtcPalUuid*          dest_cid,
                                                uint32_t                   received_seq_num,
                                                const RdmCommandHeader*    received_rdm_header,
                                                const EtcPalMcastNetintId* received_netint_id,
                                                rdm_nack_reason_t          nack_reason);

// Utilities
static RCLlrpTarget* find_target_by_cid(const RCRefList* list, const EtcPalUuid* cid);

/*************************** Function definitions ****************************/

/*
 * Initialize the RDMnet Core LLRP Target module. Do all necessary initialization before other
 * RDMnet Core LLRP Target API functions can be called. This function is called from rdmnet_init().
 */
etcpal_error_t rc_llrp_target_module_init(void)
{
  if (!rc_ref_lists_init(&targets))
    return kEtcPalErrNoMem;
  return kEtcPalErrOk;
}

/*
 * Deinitialize the RDMnet Core LLRP Target module, setting it back to an uninitialized state. All
 * existing connections will be closed/disconnected. This function is called from rdmnet_deinit()
 * after any threads that call rc_conn_module_tick() are joined.
 */
void rc_llrp_target_module_deinit(void)
{
  rc_ref_lists_remove_all(&targets, (RCRefFunction)cleanup_target_resources, NULL);
  rc_ref_lists_cleanup(&targets);
}

/*
 * Initialize and add an RCLlrpTarget structure to the list to be processed as LLRP Targets.
 */
etcpal_error_t rc_llrp_target_register(RCLlrpTarget* target, const EtcPalMcastNetintId* netints, size_t num_netints)
{
  if (!RDMNET_ASSERT_VERIFY(target))
    return kEtcPalErrSys;

  if (!rc_initialized())
    return kEtcPalErrNotInit;

  if (!rc_ref_list_add_ref(&targets.pending, target))
    return kEtcPalErrNoMem;

  etcpal_error_t res = setup_target_netints(target, netints, num_netints);
  if (res != kEtcPalErrOk)
  {
    rc_ref_list_remove_ref(&targets.pending, target);
    return res;
  }

  if (RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&target->uid))
  {
    // This is a hack around a hole in the standard. TODO add a more explanatory comment once
    // this has been further explored.
    target->uid.id = (uint32_t)rand();
  }
  target->connected_to_broker = false;
  return kEtcPalErrOk;
}

/*
 * Remove an RCLlrpTarget structure from internal processing by this module.
 */
void rc_llrp_target_unregister(RCLlrpTarget* target)
{
  if (!RDMNET_ASSERT_VERIFY(target))
    return;

  rc_ref_list_add_ref(&targets.to_remove, target);
}

/*
 * Update the Broker connection state of an LLRP target. This affects whether the LLRP target
 * responds to filtered LLRP probe requests.
 */
void rc_llrp_target_update_connection_state(RCLlrpTarget* target, bool connected_to_broker)
{
  if (!RDMNET_ASSERT_VERIFY(target))
    return;

  target->connected_to_broker = connected_to_broker;
}

etcpal_error_t rc_llrp_target_send_ack(RCLlrpTarget*              target,
                                       const LlrpSavedRdmCommand* received_cmd,
                                       const uint8_t*             response_data,
                                       uint8_t                    response_data_len)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(received_cmd))
    return kEtcPalErrSys;

  return target_send_ack_internal(target, &received_cmd->source_cid, received_cmd->seq_num, &received_cmd->rdm_header,
                                  &received_cmd->netint_id, response_data, response_data_len);
}

etcpal_error_t rc_llrp_target_send_nack(RCLlrpTarget*              target,
                                        const LlrpSavedRdmCommand* received_cmd,
                                        rdm_nack_reason_t          nack_reason)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(received_cmd))
    return kEtcPalErrSys;

  return target_send_nack_internal(target, &received_cmd->source_cid, received_cmd->seq_num, &received_cmd->rdm_header,
                                   &received_cmd->netint_id, nack_reason);
}

void rc_llrp_target_module_tick(void)
{
  if (rdmnet_writelock())
  {
    rc_ref_lists_remove_marked(&targets, (RCRefFunction)cleanup_target_resources, NULL);
    rc_ref_lists_add_pending(&targets);
    rdmnet_writeunlock();
  }

  rc_ref_list_for_each(&targets.active, (RCRefFunction)process_target_state, NULL);
}

void rc_llrp_target_data_received(const uint8_t* data, size_t data_len, const EtcPalMcastNetintId* netint)
{
  if (!RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(netint))
    return;

  EtcPalUuid dest_cid;
  if (rc_get_llrp_destination_cid(data, data_len, &dest_cid))
  {
    bool                      target_found = false;
    LlrpTargetIncomingMessage msg;
    msg.data = data;
    msg.data_len = data_len;
    msg.netint = netint;

    if (0 == ETCPAL_UUID_CMP(&dest_cid, kLlrpBroadcastCid))
    {
      // Broadcast LLRP message - handle with all targets
      target_found = true;
      rc_ref_list_for_each(&targets.active, (RCRefFunction)target_handle_llrp_message, &msg);
    }
    else
    {
      RCLlrpTarget* target = find_target_by_cid(&targets.active, &dest_cid);
      if (target)
      {
        target_found = true;
        target_handle_llrp_message(target, &msg);
      }
    }

    if (!target_found && RDMNET_CAN_LOG(ETCPAL_LOG_DEBUG))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&dest_cid, cid_str);
      RDMNET_LOG_DEBUG("Ignoring LLRP message addressed to unknown LLRP Target %s", cid_str);
    }
  }
}

etcpal_error_t setup_target_netints(RCLlrpTarget* target, const EtcPalMcastNetintId* netints, size_t num_netints)
{
  if (!RDMNET_ASSERT_VERIFY(target))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;

  if (netints && num_netints > 0)
  {
    // Check or initialize the target's network interface array.
#if RDMNET_DYNAMIC_MEM
    target->netints = (RCLlrpTargetNetintInfo*)calloc(num_netints, sizeof(RCLlrpTargetNetintInfo));
    if (!target->netints)
      return kEtcPalErrNoMem;
#else
    if (num_netints > RDMNET_MAX_MCAST_NETINTS)
      return kEtcPalErrNoMem;
#endif
    target->num_netints = 0;

    for (size_t i = 0; i < num_netints; ++i)
    {
      res = setup_target_netint(&netints[i], &target->netints[i]);
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
    const EtcPalMcastNetintId* mcast_netint_arr;
    size_t                     mcast_netint_arr_size = rc_mcast_get_netint_array(&mcast_netint_arr);

#if RDMNET_DYNAMIC_MEM
    target->netints = (RCLlrpTargetNetintInfo*)calloc(mcast_netint_arr_size, sizeof(RCLlrpTargetNetintInfo));
    if (!target->netints)
      return kEtcPalErrNoMem;
#endif

    target->num_netints = 0;
    for (const EtcPalMcastNetintId* netint_id = mcast_netint_arr; netint_id < mcast_netint_arr + mcast_netint_arr_size;
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

etcpal_error_t setup_target_netint(const EtcPalMcastNetintId* netint_id, RCLlrpTargetNetintInfo* netint)
{
  if (!RDMNET_ASSERT_VERIFY(netint_id) || !RDMNET_ASSERT_VERIFY(netint))
    return kEtcPalErrSys;

  netint->id = *netint_id;

  etcpal_error_t res = rc_mcast_get_send_socket(netint_id, 0, &netint->send_sock);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_llrp_recv_netint_add(netint_id, kLlrpSocketTypeTarget);
  if (res != kEtcPalErrOk)
  {
    rc_mcast_release_send_socket(netint_id, 0);
    return res;
  }

  // Remaining initialization
  netint->reply_pending = false;
  return res;
}

void cleanup_target_netints(RCLlrpTarget* target)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(target->netints))
    return;

  for (const RCLlrpTargetNetintInfo* netint_info = target->netints; netint_info < target->netints + target->num_netints;
       ++netint_info)
  {
    rc_mcast_release_send_socket(&netint_info->id, 0);
    rc_llrp_recv_netint_remove(&netint_info->id, kLlrpSocketTypeTarget);
  }
#if RDMNET_DYNAMIC_MEM
  free(target->netints);
#endif
}

RCLlrpTargetNetintInfo* get_target_netint(RCLlrpTarget* target, const EtcPalMcastNetintId* id)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(target->netints) || !RDMNET_ASSERT_VERIFY(id))
    return NULL;

  for (RCLlrpTargetNetintInfo* netint = target->netints; netint < target->netints + target->num_netints; ++netint)
  {
    if (netint->id.index == id->index && netint->id.ip_type == id->ip_type)
      return netint;
  }
  return NULL;
}

void cleanup_target_resources(RCLlrpTarget* target, const void* context)
{
  ETCPAL_UNUSED_ARG(context);

  if (!RDMNET_ASSERT_VERIFY(target))
    return;

  cleanup_target_netints(target);
  if (target->callbacks.destroyed)
    target->callbacks.destroyed(target);
}

void process_target_state(RCLlrpTarget* target, const void* context)
{
  ETCPAL_UNUSED_ARG(context);

  if (!RDMNET_ASSERT_VERIFY(target))
    return;

  if (TARGET_LOCK(target))
  {
    if (!RDMNET_ASSERT_VERIFY(target->netints))
      return;

    for (RCLlrpTargetNetintInfo* netint = target->netints; netint < target->netints + target->num_netints; ++netint)
    {
      if (netint->reply_pending)
      {
        if (etcpal_timer_is_expired(&netint->reply_backoff))
        {
          LlrpHeader header;
          header.sender_cid = target->cid;
          header.dest_cid = netint->pending_reply_cid;
          header.transaction_number = netint->pending_reply_trans_num;

          LlrpDiscoveredTarget target_info;
          target_info.cid = target->cid;
          target_info.uid = target->uid;
          target_info.hardware_address = *(rc_mcast_get_lowest_mac_addr());
          target_info.component_type = target->component_type;

          etcpal_error_t send_res = rc_send_llrp_probe_reply(
              netint->send_sock, netint->send_buf, (netint->id.ip_type == kEtcPalIpTypeV6), &header, &target_info);
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
    TARGET_UNLOCK(target);
  }
}

void target_handle_llrp_message(RCLlrpTarget* target, const LlrpTargetIncomingMessage* message)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(message))
    return;

  if (TARGET_LOCK(target))
  {
    RCLlrpTargetEvent event = RC_LLRP_TARGET_EVENT_INIT;

    RCLlrpTargetNetintInfo* target_netint = get_target_netint(target, message->netint);
    if (target_netint)
    {
      // msg being static is a stack-saving optimization. It's unlikely that the LLRP target code
      // will become multithreaded, but if it does this will need to be revisited.
      static LlrpMessage msg;

      LlrpMessageInterest interest;
      interest.my_cid = target->cid;
      interest.interested_in_probe_reply = false;
      interest.interested_in_probe_request = true;
      interest.my_uid = target->uid;

      if (rc_parse_llrp_message(message->data, message->data_len, &interest, &msg))
      {
        switch (msg.vector)
        {
          case VECTOR_LLRP_PROBE_REQUEST: {
            const RemoteProbeRequest* request = LLRP_MSG_GET_PROBE_REQUEST(&msg);
            if (!RDMNET_ASSERT_VERIFY(request))
              return;

            // TODO allow multiple probe replies to be queued
            if (request->contains_my_uid && !target_netint->reply_pending)
            {
              uint32_t backoff_ms;

              // Check the filter values.
              if (!((request->filter & LLRP_FILTERVAL_BROKERS_ONLY) && target->component_type != kLlrpCompBroker) &&
                  !(request->filter & LLRP_FILTERVAL_CLIENT_CONN_INACTIVE && target->connected_to_broker))
              {
                target_netint->reply_pending = true;
                target_netint->pending_reply_cid = msg.header.sender_cid;
                target_netint->pending_reply_trans_num = msg.header.transaction_number;
                backoff_ms = (uint32_t)(rand() * LLRP_MAX_BACKOFF_MS / RAND_MAX);
                etcpal_timer_start(&target_netint->reply_backoff, backoff_ms);
              }
            }
            // Even if we got a valid probe request, we are starting a backoff timer, so there's nothing
            // else to do at this time.
            break;
          }
          case VECTOR_LLRP_RDM_CMD: {
            LlrpRdmCommand* cmd = &event.rdm_cmd;
            if (kEtcPalErrOk ==
                rdm_unpack_command(LLRP_MSG_GET_RDM(&msg), &cmd->rdm_header, &cmd->data, &cmd->data_len))
            {
              cmd->source_cid = msg.header.sender_cid;
              cmd->seq_num = msg.header.transaction_number;
              cmd->netint_id = target_netint->id;

              event.which = kRCLlrpTargetEventRdmCmdReceived;
            }
          }
          default:
            break;
        }
      }
    }
    TARGET_UNLOCK(target);
    deliver_event_callback(target, &event);
  }
}

void deliver_event_callback(RCLlrpTarget* target, RCLlrpTargetEvent* event)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(event))
    return;

  switch (event->which)
  {
    case kRCLlrpTargetEventRdmCmdReceived:
      if (target->callbacks.rdm_command_received)
      {
        RCLlrpTargetSyncRdmResponse response = RC_LLRP_TARGET_SYNC_RDM_RESPONSE_INIT;
        target->callbacks.rdm_command_received(target, &event->rdm_cmd, &response);
        send_response_if_requested(target, event, &response);
      }
      break;
    case kRCLlrpTargetEventNone:
    default:
      break;
  }
}

void send_response_if_requested(RCLlrpTarget*                target,
                                const RCLlrpTargetEvent*     event,
                                RCLlrpTargetSyncRdmResponse* response)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(event) || !RDMNET_ASSERT_VERIFY(response))
    return;

  RdmnetSyncRdmResponse* external_resp = &response->resp;
  if (external_resp->response_action == kRdmnetRdmResponseActionSendAck)
  {
    // Check for errors in the sync response
    // No sync response buffer provided
    if (external_resp->response_data.response_data_len != 0 && !response->response_buf)
    {
      if (RDMNET_CAN_LOG(ETCPAL_LOG_ERR))
      {
        char target_cid[ETCPAL_UUID_BYTES] = {'\0'};
        etcpal_uuid_to_string(&target->cid, target_cid);
        RDMNET_LOG_ERR(
            "Error: local LLRP target %s specified a synchronous RDM response with data but no data buffer was "
            "provided. This is a bug in usage of the library.",
            target_cid);
      }
      return;
    }

    // Response data length too long
    if (external_resp->response_data.response_data_len > RDM_MAX_PDL)
    {
      if (RDMNET_CAN_LOG(ETCPAL_LOG_ERR))
      {
        char target_cid[ETCPAL_UUID_BYTES] = {'\0'};
        etcpal_uuid_to_string(&target->cid, target_cid);
        RDMNET_LOG_ERR(
            "Error: local LLRP target %s specified a synchronous RDM response with data length %zu, which is too long "
            "for LLRP. This is a bug in usage of the library.",
            target_cid, external_resp->response_data.response_data_len);
      }
      return;
    }

    const LlrpRdmCommand* received_cmd = &event->rdm_cmd;
    etcpal_error_t        send_res = target_send_ack_internal(
               target, &received_cmd->source_cid, received_cmd->seq_num, &received_cmd->rdm_header, &received_cmd->netint_id,
               response->response_buf, (uint8_t)external_resp->response_data.response_data_len);
    if (send_res != kEtcPalErrOk && RDMNET_CAN_LOG(ETCPAL_LOG_ERR))
    {
      char source_cid[ETCPAL_UUID_BYTES] = {'\0'};
      char dest_cid[ETCPAL_UUID_BYTES] = {'\0'};
      etcpal_uuid_to_string(&target->cid, source_cid);
      etcpal_uuid_to_string(&event->rdm_cmd.source_cid, dest_cid);
      RDMNET_LOG_ERR("Error sending synchronous LLRP ACK to manager %s from local target %s: '%s'", dest_cid,
                     source_cid, etcpal_strerror(send_res));
    }
  }
  else if (external_resp->response_action == kRdmnetRdmResponseActionSendNack)
  {
    const LlrpRdmCommand* received_cmd = &event->rdm_cmd;
    etcpal_error_t        send_res =
        target_send_nack_internal(target, &received_cmd->source_cid, received_cmd->seq_num, &received_cmd->rdm_header,
                                  &received_cmd->netint_id, external_resp->response_data.nack_reason);
    if (send_res != kEtcPalErrOk && RDMNET_CAN_LOG(ETCPAL_LOG_ERR))
    {
      char source_cid[ETCPAL_UUID_BYTES] = {'\0'};
      char dest_cid[ETCPAL_UUID_BYTES] = {'\0'};
      etcpal_uuid_to_string(&target->cid, source_cid);
      etcpal_uuid_to_string(&event->rdm_cmd.source_cid, dest_cid);
      RDMNET_LOG_ERR("Error sending synchronous LLRP NACK to manager %s from local target %s: '%s'", dest_cid,
                     source_cid, etcpal_strerror(send_res));
    }
  }
}

etcpal_error_t target_send_ack_internal(RCLlrpTarget*              target,
                                        const EtcPalUuid*          dest_cid,
                                        uint32_t                   received_seq_num,
                                        const RdmCommandHeader*    received_rdm_header,
                                        const EtcPalMcastNetintId* received_netint_id,
                                        const uint8_t*             response_data,
                                        uint8_t                    response_data_len)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(dest_cid) || !RDMNET_ASSERT_VERIFY(received_rdm_header) ||
      !RDMNET_ASSERT_VERIFY(received_netint_id))
  {
    return kEtcPalErrSys;
  }

  RdmBuffer      resp_buf;
  etcpal_error_t res = rdm_pack_response(received_rdm_header, 0, response_data, response_data_len, &resp_buf);
  if (res != kEtcPalErrOk)
    return res;

  RCLlrpTargetNetintInfo* netint = get_target_netint(target, received_netint_id);
  if (netint)
  {
    LlrpHeader header;
    header.dest_cid = *dest_cid;
    header.sender_cid = target->cid;
    header.transaction_number = received_seq_num;

    return rc_send_llrp_rdm_response(netint->send_sock, netint->send_buf, (netint->id.ip_type == kEtcPalIpTypeV6),
                                     &header, &resp_buf);
  }
  else
  {
    // Something has changed about the system network interfaces since this command was received.
    return kEtcPalErrSys;
  }
}

etcpal_error_t target_send_nack_internal(RCLlrpTarget*              target,
                                         const EtcPalUuid*          dest_cid,
                                         uint32_t                   received_seq_num,
                                         const RdmCommandHeader*    received_rdm_header,
                                         const EtcPalMcastNetintId* received_netint_id,
                                         rdm_nack_reason_t          nack_reason)
{
  if (!RDMNET_ASSERT_VERIFY(target) || !RDMNET_ASSERT_VERIFY(dest_cid) || !RDMNET_ASSERT_VERIFY(received_rdm_header) ||
      !RDMNET_ASSERT_VERIFY(received_netint_id))
  {
    return kEtcPalErrSys;
  }

  RCLlrpTargetNetintInfo* netint = get_target_netint(target, received_netint_id);
  if (netint)
  {
    RdmBuffer      nack_buf;
    etcpal_error_t res = rdm_pack_nack_response(received_rdm_header, 0, nack_reason, &nack_buf);
    if (res != kEtcPalErrOk)
      return res;

    LlrpHeader header;
    header.dest_cid = *dest_cid;
    header.sender_cid = target->cid;
    header.transaction_number = received_seq_num;

    return rc_send_llrp_rdm_response(netint->send_sock, netint->send_buf, (netint->id.ip_type == kEtcPalIpTypeV6),
                                     &header, &nack_buf);
  }
  else
  {
    // Something has changed about the system network interfaces since this command was received.
    return kEtcPalErrSys;
  }
}

static bool cid_is_equal_predicate(void* ref, const void* context)
{
  if (!RDMNET_ASSERT_VERIFY(ref) || !RDMNET_ASSERT_VERIFY(context))
    return false;

  RCLlrpTarget* target = (RCLlrpTarget*)ref;
  EtcPalUuid*   cid = (EtcPalUuid*)context;
  return (ETCPAL_UUID_CMP(&target->cid, cid) == 0);
}

RCLlrpTarget* find_target_by_cid(const RCRefList* list, const EtcPalUuid* cid)
{
  if (!RDMNET_ASSERT_VERIFY(list) || !RDMNET_ASSERT_VERIFY(cid))
    return NULL;

  return (RCLlrpTarget*)rc_ref_list_find_ref(list, cid_is_equal_predicate, cid);
}
