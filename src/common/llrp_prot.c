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
#include "llrp_prot_priv.h"

#include <string.h>
#include "lwpa/pack.h"
#include "lwpa/socket.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"
#include "llrp_priv.h"

/**************************** Global variables *******************************/

LwpaUuid kLLRPBroadcastCID;

/*************************** Private constants *******************************/

#define PROBE_REPLY_PDU_SIZE \
  (3 /* Flags + Length */ + 1 /* Vector */ + 6 /* UID */ + 6 /* Hardware Address */ + 1 /* Component Type */)
#define LLRP_MIN_PDU_SIZE (LLRP_HEADER_SIZE + PROBE_REPLY_PDU_SIZE)
#define LLRP_MIN_TOTAL_MESSAGE_SIZE (ACN_UDP_PREAMBLE_SIZE + RLP_HEADER_SIZE_EXT_LEN + LLRP_MIN_PDU_SIZE)
#define LLRP_RDM_CMD_PDU_MIN_SIZE (3 /* Flags + Length */ + RDM_MIN_BYTES)
#define LLRP_RDM_CMD_PDU_MAX_SIZE (3 /* Flags + Length */ + RDM_MAX_BYTES)

/*********************** Private function prototypes *************************/

static bool parse_llrp_pdu(const uint8_t *buf, size_t buflen, const LlrpMessageInterest *interest, LlrpMessage *msg);
static bool parse_llrp_probe_request(const uint8_t *buf, size_t buflen, const LlrpMessageInterest *interest,
                                     ProbeRequestRecv *request);
static bool parse_llrp_probe_reply(const uint8_t *buf, size_t buflen, LlrpTarget *reply);
static bool parse_llrp_rdm_command(const uint8_t *buf, size_t buflen, RdmBuffer *cmd);

/*************************** Function definitions ****************************/

void llrp_prot_init()
{
  string_to_uuid(&kLLRPBroadcastCID, LLRP_BROADCAST_CID, sizeof(LLRP_BROADCAST_CID));
}

bool parse_llrp_message(const uint8_t *buf, size_t buflen, const LlrpMessageInterest *interest, LlrpMessage *msg)
{
  UdpPreamble preamble;
  RootLayerPdu rlp;
  LwpaPdu last_pdu = PDU_INIT;

  if (!buf || !msg || buflen < LLRP_MIN_TOTAL_MESSAGE_SIZE)
    return false;

  /* Try to parse the UDP preamble. */
  if (!parse_udp_preamble(buf, buflen, &preamble))
    return false;

  /* Try to parse the Root Layer PDU header. */
  if (!parse_root_layer_pdu(preamble.rlp_block, preamble.rlp_block_len, &rlp, &last_pdu))
  {
    return false;
  }

  /* Fill in the data we have and try to parse the LLRP PDU. */
  msg->header.sender_cid = rlp.sender_cid;
  return parse_llrp_pdu(rlp.pdata, rlp.datalen, interest, msg);
}

bool parse_llrp_pdu(const uint8_t *buf, size_t buflen, const LlrpMessageInterest *interest, LlrpMessage *msg)
{
  const uint8_t *cur_ptr = buf;
  size_t llrp_pdu_len;

  if (buflen < LLRP_MIN_PDU_SIZE)
    return false;

  /* Check the PDU length */
  llrp_pdu_len = pdu_length(cur_ptr);
  if (llrp_pdu_len > buflen || llrp_pdu_len < LLRP_MIN_PDU_SIZE)
    return false;

  /* Fill in the LLRP PDU header data */
  cur_ptr += 3;
  msg->vector = upack_32b(cur_ptr);
  cur_ptr += 4;
  memcpy(msg->header.dest_cid.data, cur_ptr, UUID_BYTES);
  cur_ptr += UUID_BYTES;
  msg->header.transaction_number = upack_32b(cur_ptr);
  cur_ptr += 4;

  /* Parse the next layer, based on the vector value and what the caller has registered interest in */
  if (0 == uuidcmp(&msg->header.dest_cid, &kLLRPBroadcastCID) || 0 == uuidcmp(&msg->header.dest_cid, &interest->my_cid))
  {
    switch (msg->vector)
    {
      case VECTOR_LLRP_PROBE_REQUEST:
        if (interest->interested_in_probe_request)
        {
          return parse_llrp_probe_request(cur_ptr, llrp_pdu_len - LLRP_HEADER_SIZE, interest, &msg->data.probe_request);
        }
        else
          return false;
      case VECTOR_LLRP_PROBE_REPLY:
        if (interest->interested_in_probe_reply)
        {
          msg->data.probe_reply.target_cid = msg->header.sender_cid;
          return parse_llrp_probe_reply(cur_ptr, llrp_pdu_len - LLRP_HEADER_SIZE, &msg->data.probe_reply);
        }
        else
          return false;
      case VECTOR_LLRP_RDM_CMD:
        return parse_llrp_rdm_command(cur_ptr, llrp_pdu_len - LLRP_HEADER_SIZE, &msg->data.rdm_cmd);
      default:
        return false;
    }
  }
  return false;
}

bool parse_llrp_probe_request(const uint8_t *buf, size_t buflen, const LlrpMessageInterest *interest,
                              ProbeRequestRecv *request)
{
  const uint8_t *cur_ptr = buf;
  const uint8_t *buf_end;
  size_t pdu_len;
  uint8_t vector;
  RdmUid lower_uid_bound;
  RdmUid upper_uid_bound;

  if (buflen < PROBE_REQUEST_PDU_MIN_SIZE)
    return false;

  /* Check the PDU length */
  pdu_len = pdu_length(cur_ptr);
  if (pdu_len > buflen || pdu_len < PROBE_REQUEST_PDU_MIN_SIZE)
    return false;
  buf_end = cur_ptr + pdu_len;

  /* Fill in the rest of the Probe Request data */
  cur_ptr += 3;
  vector = *cur_ptr++;
  if (vector != VECTOR_LLRP_PROBE_REQUEST)
    return false;
  lower_uid_bound.manu = upack_16b(cur_ptr);
  cur_ptr += 2;
  lower_uid_bound.id = upack_32b(cur_ptr);
  cur_ptr += 4;
  upper_uid_bound.manu = upack_16b(cur_ptr);
  cur_ptr += 2;
  upper_uid_bound.id = upack_32b(cur_ptr);
  cur_ptr += 4;
  request->filter = *cur_ptr++;

  /* If our UID is not in the range, there is no need to check the Known UIDs. */
  if (rdm_uid_cmp(&interest->my_uid, &lower_uid_bound) >= 0 && rdm_uid_cmp(&interest->my_uid, &upper_uid_bound) <= 0)
  {
    request->contains_my_uid = true;

    /* Check the Known UIDs to see if the registered UID is in it. */
    while (cur_ptr + 6 <= buf_end)
    {
      RdmUid cur_uid;
      cur_uid.manu = upack_16b(cur_ptr);
      cur_ptr += 2;
      cur_uid.id = upack_32b(cur_ptr);
      cur_ptr += 4;

      if (rdm_uid_equal(&interest->my_uid, &cur_uid))
      {
        /* The registered uid is suppressed. */
        request->contains_my_uid = false;
        break;
      }
    }
  }
  else
    request->contains_my_uid = false;

  return true;
}

bool parse_llrp_probe_reply(const uint8_t *buf, size_t buflen, LlrpTarget *reply)
{
  const uint8_t *cur_ptr = buf;
  size_t pdu_len;
  uint8_t vector;

  if (buflen < PROBE_REPLY_PDU_SIZE)
    return false;

  pdu_len = pdu_length(buf);
  if (pdu_len != PROBE_REPLY_PDU_SIZE)
    return false;
  cur_ptr += 3;

  vector = *cur_ptr++;
  if (vector != VECTOR_LLRP_PROBE_REPLY)
    return false;

  reply->target_uid.manu = upack_16b(cur_ptr);
  cur_ptr += 2;
  reply->target_uid.id = upack_32b(cur_ptr);
  cur_ptr += 4;
  memcpy(reply->hardware_address, cur_ptr, 6);
  cur_ptr += 6;
  reply->component_type = (llrp_component_t)*cur_ptr;
  return true;
}

bool parse_llrp_rdm_command(const uint8_t *buf, size_t buflen, RdmBuffer *cmd)
{
  const uint8_t *cur_ptr = buf;
  size_t pdu_len;

  if (buflen < LLRP_RDM_CMD_PDU_MIN_SIZE)
    return false;

  pdu_len = pdu_length(buf);
  if (pdu_len > buflen || pdu_len > LLRP_RDM_CMD_PDU_MAX_SIZE || pdu_len < LLRP_RDM_CMD_PDU_MIN_SIZE)
  {
    return false;
  }
  cur_ptr += 3;

  if (*cur_ptr != VECTOR_RDM_CMD_RDM_DATA)
    return false;

  memcpy(cmd->data, cur_ptr, pdu_len - 3);
  cmd->datalen = pdu_len - 3;
  return true;
}

size_t pack_llrp_header(uint8_t *buf, size_t pdu_len, uint32_t vector, const LlrpHeader *header)
{
  uint8_t *cur_ptr = buf;

  *cur_ptr = 0xf0;
  pdu_pack_ext_len(cur_ptr, pdu_len);
  cur_ptr += 3;
  pack_32b(cur_ptr, vector);
  cur_ptr += 4;
  memcpy(cur_ptr, header->dest_cid.data, UUID_BYTES);
  cur_ptr += UUID_BYTES;
  pack_32b(cur_ptr, header->transaction_number);
  cur_ptr += 4;
  return cur_ptr - buf;
}

#define PROBE_REQUEST_RLP_DATA_MIN_SIZE (LLRP_HEADER_SIZE + PROBE_REQUEST_PDU_MIN_SIZE)
#define PROBE_REQUEST_RLP_DATA_MAX_SIZE (PROBE_REQUEST_RLP_DATA_MIN_SIZE + (6 * LLRP_KNOWN_UID_SIZE))

lwpa_error_t send_llrp_probe_request(llrp_socket_t handle, const LwpaSockaddr *dest_addr, const LlrpHeader *header,
                                     const ProbeRequestSend *probe_request)
{
  uint8_t *cur_ptr = handle->send_buf;
  uint8_t *buf_end = cur_ptr + LLRP_MAX_MESSAGE_SIZE;
  RootLayerPdu rlp;
  const KnownUid *cur_uid;
  int send_res;

  /* Pack the UDP Preamble */
  cur_ptr += pack_udp_preamble(cur_ptr, buf_end - cur_ptr);

  rlp.vector = VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.datalen = PROBE_REQUEST_RLP_DATA_MIN_SIZE;

  /* Calculate the data length */
  cur_uid = probe_request->uid_list;
  while (cur_uid && rlp.datalen < PROBE_REQUEST_RLP_DATA_MAX_SIZE)
  {
    rlp.datalen += 6;
    cur_uid = cur_uid->next;
  }

  /* Pack the Root Layer PDU header */
  cur_ptr += pack_root_layer_header(cur_ptr, buf_end - cur_ptr, &rlp);

  /* Pack the LLRP header */
  cur_ptr += pack_llrp_header(cur_ptr, rlp.datalen, VECTOR_LLRP_PROBE_REQUEST, header);

  /* Pack the Probe Request PDU header fields */
  *cur_ptr = 0xf0;
  pdu_pack_ext_len(cur_ptr, rlp.datalen - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  /* TODO bad standard! Will be corrected in next public review draft. */
  *cur_ptr++ = (uint8_t)VECTOR_LLRP_PROBE_REQUEST;
  pack_16b(cur_ptr, probe_request->lower_uid.manu);
  cur_ptr += 2;
  pack_32b(cur_ptr, probe_request->lower_uid.id);
  cur_ptr += 4;
  pack_16b(cur_ptr, probe_request->upper_uid.manu);
  cur_ptr += 2;
  pack_32b(cur_ptr, probe_request->upper_uid.id);
  cur_ptr += 4;
  *cur_ptr++ = probe_request->filter;

  /* Pack the Known UIDs */
  cur_uid = probe_request->uid_list;
  while (cur_uid)
  {
    if (cur_ptr + 6 > buf_end)
      break;
    pack_16b(cur_ptr, cur_uid->uid.manu);
    cur_ptr += 2;
    pack_32b(cur_ptr, cur_uid->uid.id);
    cur_ptr += 4;
    cur_uid = cur_uid->next;
  }

  send_res = lwpa_sendto(handle->sys_sock, handle->send_buf, cur_ptr - handle->send_buf, 0, dest_addr);
  if (send_res >= 0)
    return LWPA_OK;
  else
    return (lwpa_error_t)send_res;
}

#define PROBE_REPLY_RLP_DATA_SIZE (LLRP_HEADER_SIZE + PROBE_REPLY_PDU_SIZE)

lwpa_error_t send_llrp_probe_reply(llrp_socket_t handle, const LwpaSockaddr *dest_addr, const LlrpHeader *header,
                                   const LlrpTarget *probe_reply)
{
  uint8_t *cur_ptr = handle->send_buf;
  uint8_t *buf_end = cur_ptr + LLRP_MAX_MESSAGE_SIZE;
  RootLayerPdu rlp;
  int send_res;

  /* Pack the UDP Preamble */
  cur_ptr += pack_udp_preamble(cur_ptr, buf_end - cur_ptr);

  rlp.vector = VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.datalen = PROBE_REPLY_RLP_DATA_SIZE;

  /* Pack the Root Layer PDU header */
  cur_ptr += pack_root_layer_header(cur_ptr, buf_end - cur_ptr, &rlp);

  /* Pack the LLRP header */
  cur_ptr += pack_llrp_header(cur_ptr, rlp.datalen, VECTOR_LLRP_PROBE_REPLY, header);

  /* Pack the Probe Reply PDU */
  *cur_ptr = 0xf0;
  pdu_pack_ext_len(cur_ptr, rlp.datalen - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  /* TODO bad standard! Will be corrected in next public review draft. */
  *cur_ptr++ = (uint8_t)VECTOR_LLRP_PROBE_REPLY;
  pack_16b(cur_ptr, probe_reply->target_uid.manu);
  cur_ptr += 2;
  pack_32b(cur_ptr, probe_reply->target_uid.id);
  cur_ptr += 4;
  memcpy(cur_ptr, probe_reply->hardware_address, 6);
  cur_ptr += 6;
  *cur_ptr++ = (uint8_t)probe_reply->component_type;

  send_res = lwpa_sendto(handle->sys_sock, handle->send_buf, cur_ptr - handle->send_buf, 0, dest_addr);
  if (send_res >= 0)
    return LWPA_OK;
  else
    return (lwpa_error_t)send_res;
}

#define RDM_CMD_RLP_DATA_MIN_SIZE (LLRP_HEADER_SIZE + 3 /* RDM cmd PDU Flags + Length */)

lwpa_error_t send_llrp_rdm(llrp_socket_t handle, const LwpaSockaddr *dest_addr, const LlrpHeader *header,
                           const RdmBuffer *rdm_msg)
{
  uint8_t *cur_ptr = handle->send_buf;
  uint8_t *buf_end = cur_ptr + LLRP_MAX_MESSAGE_SIZE;
  RootLayerPdu rlp;
  int send_res;

  /* Pack the UDP Preamble */
  cur_ptr += pack_udp_preamble(cur_ptr, buf_end - cur_ptr);

  rlp.vector = VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.datalen = RDM_CMD_RLP_DATA_MIN_SIZE + rdm_msg->datalen;

  /* Pack the Root Layer PDU header */
  cur_ptr += pack_root_layer_header(cur_ptr, buf_end - cur_ptr, &rlp);

  /* Pack the LLRP header */
  cur_ptr += pack_llrp_header(cur_ptr, rlp.datalen, VECTOR_LLRP_RDM_CMD, header);

  /* Pack the RDM Command PDU */
  *cur_ptr = 0xf0;
  pdu_pack_ext_len(cur_ptr, rlp.datalen - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  memcpy(cur_ptr, rdm_msg->data, rdm_msg->datalen);
  cur_ptr += rdm_msg->datalen;

  send_res = lwpa_sendto(handle->sys_sock, handle->send_buf, cur_ptr - handle->send_buf, 0, dest_addr);
  if (send_res >= 0)
    return LWPA_OK;
  else
    return (lwpa_error_t)send_res;
}
