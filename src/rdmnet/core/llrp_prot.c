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
#include "rdmnet/private/llrp_prot.h"

#include <string.h>
#include "etcpal/pack.h"
#include "etcpal/socket.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"
#include "rdmnet/private/llrp.h"

/**************************** Global variables *******************************/

EtcPalUuid kLlrpBroadcastCid;

/*************************** Private constants *******************************/

#define PROBE_REPLY_PDU_SIZE \
  (3 /* Flags + Length */ + 1 /* Vector */ + 6 /* UID */ + 6 /* Hardware Address */ + 1 /* Component Type */)
#define LLRP_MIN_PDU_SIZE (LLRP_HEADER_SIZE + PROBE_REPLY_PDU_SIZE)
#define LLRP_MIN_TOTAL_MESSAGE_SIZE (ACN_UDP_PREAMBLE_SIZE + ACN_RLP_HEADER_SIZE_EXT_LEN + LLRP_MIN_PDU_SIZE)
#define LLRP_RDM_CMD_PDU_MIN_SIZE (3 /* Flags + Length */ + RDM_MIN_BYTES)

/*********************** Private function prototypes *************************/

static bool parse_llrp_pdu(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest, LlrpMessage* msg);
static bool parse_llrp_probe_request(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest,
                                     RemoteProbeRequest* request);
static bool parse_llrp_probe_reply(const uint8_t* buf, size_t buflen, DiscoveredLlrpTarget* reply);
static bool parse_llrp_rdm_command(const uint8_t* buf, size_t buflen, RdmBuffer* cmd);

static etcpal_error_t send_llrp_rdm(etcpal_socket_t sock, uint8_t* buf, const EtcPalSockaddr* dest_addr,
                                    const LlrpHeader* header, const RdmBuffer* rdm_msg);

/*************************** Function definitions ****************************/

void llrp_prot_init()
{
  etcpal_string_to_uuid(LLRP_BROADCAST_CID, &kLlrpBroadcastCid);
}

bool get_llrp_destination_cid(const uint8_t* buf, size_t buflen, EtcPalUuid* dest_cid)
{
  if (!buf || !dest_cid || buflen < LLRP_MIN_TOTAL_MESSAGE_SIZE)
    return false;

  // Try to parse the UDP preamble.
  EtcPalUdpPreamble preamble;
  if (!etcpal_parse_udp_preamble(buf, buflen, &preamble))
    return false;

  // Try to parse the Root Layer PDU header.
  EtcPalRootLayerPdu rlp;
  EtcPalPdu last_pdu = ETCPAL_PDU_INIT;
  if (!etcpal_parse_root_layer_pdu(preamble.rlp_block, preamble.rlp_block_len, &rlp, &last_pdu))
    return false;

  // Check the PDU length
  const uint8_t* cur_ptr = rlp.pdata;
  size_t llrp_pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
  if (llrp_pdu_len > rlp.datalen || llrp_pdu_len < LLRP_MIN_PDU_SIZE)
    return false;

  // Jump to the position of the LLRP destination CID and fill it in
  cur_ptr += 7;
  memcpy(dest_cid->data, cur_ptr, ETCPAL_UUID_BYTES);
  return true;
}

bool parse_llrp_message(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest, LlrpMessage* msg)
{
  if (!buf || !msg || buflen < LLRP_MIN_TOTAL_MESSAGE_SIZE)
    return false;

  // Try to parse the UDP preamble.
  EtcPalUdpPreamble preamble;
  if (!etcpal_parse_udp_preamble(buf, buflen, &preamble))
    return false;

  // Try to parse the Root Layer PDU header.
  EtcPalRootLayerPdu rlp;
  EtcPalPdu last_pdu = ETCPAL_PDU_INIT;
  if (!etcpal_parse_root_layer_pdu(preamble.rlp_block, preamble.rlp_block_len, &rlp, &last_pdu))
    return false;

  // Fill in the data we have and try to parse the LLRP PDU.
  msg->header.sender_cid = rlp.sender_cid;
  return parse_llrp_pdu(rlp.pdata, rlp.datalen, interest, msg);
}

bool parse_llrp_pdu(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest, LlrpMessage* msg)
{
  if (buflen < LLRP_MIN_PDU_SIZE)
    return false;

  // Check the PDU length
  const uint8_t* cur_ptr = buf;
  size_t llrp_pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
  if (llrp_pdu_len > buflen || llrp_pdu_len < LLRP_MIN_PDU_SIZE)
    return false;

  // Fill in the LLRP PDU header data
  cur_ptr += 3;
  msg->vector = etcpal_upack_32b(cur_ptr);
  cur_ptr += 4;
  memcpy(msg->header.dest_cid.data, cur_ptr, ETCPAL_UUID_BYTES);
  cur_ptr += ETCPAL_UUID_BYTES;
  msg->header.transaction_number = etcpal_upack_32b(cur_ptr);
  cur_ptr += 4;

  // Parse the next layer, based on the vector value and what the caller has registered interest in
  if (0 == ETCPAL_UUID_CMP(&msg->header.dest_cid, &kLlrpBroadcastCid) ||
      0 == ETCPAL_UUID_CMP(&msg->header.dest_cid, &interest->my_cid))
  {
    switch (msg->vector)
    {
      case VECTOR_LLRP_PROBE_REQUEST:
        if (interest->interested_in_probe_request)
        {
          return parse_llrp_probe_request(cur_ptr, llrp_pdu_len - LLRP_HEADER_SIZE, interest, &msg->data.probe_request);
        }
        else
        {
          return false;
        }
      case VECTOR_LLRP_PROBE_REPLY:
        if (interest->interested_in_probe_reply)
        {
          msg->data.probe_reply.cid = msg->header.sender_cid;
          return parse_llrp_probe_reply(cur_ptr, llrp_pdu_len - LLRP_HEADER_SIZE, &msg->data.probe_reply);
        }
        else
        {
          return false;
        }
      case VECTOR_LLRP_RDM_CMD:
        return parse_llrp_rdm_command(cur_ptr, llrp_pdu_len - LLRP_HEADER_SIZE, &msg->data.rdm);
      default:
        return false;
    }
  }
  return false;
}

bool parse_llrp_probe_request(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest,
                              RemoteProbeRequest* request)
{
  if (buflen < PROBE_REQUEST_PDU_MIN_SIZE)
    return false;

  // Check the PDU length
  const uint8_t* cur_ptr = buf;
  size_t pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
  if (pdu_len > buflen || pdu_len < PROBE_REQUEST_PDU_MIN_SIZE)
    return false;
  const uint8_t* buf_end = cur_ptr + pdu_len;

  // Fill in the rest of the Probe Request data
  cur_ptr += 3;
  uint8_t vector = *cur_ptr++;
  if (vector != VECTOR_PROBE_REQUEST_DATA)
    return false;

  RdmUid lower_uid_bound;
  lower_uid_bound.manu = etcpal_upack_16b(cur_ptr);
  cur_ptr += 2;
  lower_uid_bound.id = etcpal_upack_32b(cur_ptr);
  cur_ptr += 4;

  RdmUid upper_uid_bound;
  upper_uid_bound.manu = etcpal_upack_16b(cur_ptr);
  cur_ptr += 2;
  upper_uid_bound.id = etcpal_upack_32b(cur_ptr);
  cur_ptr += 4;
  request->filter = etcpal_upack_16b(cur_ptr);
  cur_ptr += 2;

  // If our UID is not in the range, there is no need to check the Known UIDs.
  if (RDM_UID_CMP(&interest->my_uid, &lower_uid_bound) >= 0 && RDM_UID_CMP(&interest->my_uid, &upper_uid_bound) <= 0)
  {
    request->contains_my_uid = true;

    // Check the Known UIDs to see if the registered UID is in it.
    while (cur_ptr + 6 <= buf_end)
    {
      RdmUid cur_uid;
      cur_uid.manu = etcpal_upack_16b(cur_ptr);
      cur_ptr += 2;
      cur_uid.id = etcpal_upack_32b(cur_ptr);
      cur_ptr += 4;

      if (RDM_UID_EQUAL(&interest->my_uid, &cur_uid))
      {
        // The registered uid is suppressed.
        request->contains_my_uid = false;
        break;
      }
    }
  }
  else
  {
    request->contains_my_uid = false;
  }

  return true;
}

bool parse_llrp_probe_reply(const uint8_t* buf, size_t buflen, DiscoveredLlrpTarget* reply)
{
  if (buflen < PROBE_REPLY_PDU_SIZE)
    return false;

  const uint8_t* cur_ptr = buf;
  size_t pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
  if (pdu_len != PROBE_REPLY_PDU_SIZE)
    return false;
  cur_ptr += 3;

  uint8_t vector = *cur_ptr++;
  if (vector != VECTOR_PROBE_REPLY_DATA)
    return false;

  reply->uid.manu = etcpal_upack_16b(cur_ptr);
  cur_ptr += 2;
  reply->uid.id = etcpal_upack_32b(cur_ptr);
  cur_ptr += 4;
  memcpy(reply->hardware_address, cur_ptr, 6);
  cur_ptr += 6;
  reply->component_type = (llrp_component_t)*cur_ptr;
  return true;
}

bool parse_llrp_rdm_command(const uint8_t* buf, size_t buflen, RdmBuffer* cmd)
{
  if (buflen < LLRP_RDM_CMD_PDU_MIN_SIZE)
    return false;

  const uint8_t* cur_ptr = buf;
  size_t pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
  if (pdu_len > buflen || pdu_len > LLRP_RDM_CMD_PDU_MAX_SIZE || pdu_len < LLRP_RDM_CMD_PDU_MIN_SIZE)
    return false;
  cur_ptr += 3;

  if (*cur_ptr != VECTOR_RDM_CMD_RDM_DATA)
    return false;

  memcpy(cmd->data, cur_ptr, pdu_len - 3);
  cmd->datalen = pdu_len - 3;
  return true;
}

size_t etcpal_pack_llrp_header(uint8_t* buf, size_t pdu_len, uint32_t vector, const LlrpHeader* header)
{
  uint8_t* cur_ptr = buf;

  *cur_ptr = 0xf0;
  ETCPAL_PDU_PACK_EXT_LEN(cur_ptr, pdu_len);
  cur_ptr += 3;
  etcpal_pack_32b(cur_ptr, vector);
  cur_ptr += 4;
  memcpy(cur_ptr, header->dest_cid.data, ETCPAL_UUID_BYTES);
  cur_ptr += ETCPAL_UUID_BYTES;
  etcpal_pack_32b(cur_ptr, header->transaction_number);
  cur_ptr += 4;
  return (size_t)(cur_ptr - buf);
}

#define PROBE_REQUEST_RLP_DATA_MIN_SIZE (LLRP_HEADER_SIZE + PROBE_REQUEST_PDU_MIN_SIZE)
#define PROBE_REQUEST_RLP_DATA_MAX_SIZE (PROBE_REQUEST_RLP_DATA_MIN_SIZE + (6 * LLRP_KNOWN_UID_SIZE))

etcpal_error_t send_llrp_probe_request(etcpal_socket_t sock, uint8_t* buf, bool ipv6, const LlrpHeader* header,
                                       const LocalProbeRequest* probe_request)
{
  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = cur_ptr + LLRP_MANAGER_MAX_MESSAGE_SIZE;

  // Pack the UDP Preamble
  cur_ptr += etcpal_pack_udp_preamble(cur_ptr, (size_t)(buf_end - cur_ptr));

  EtcPalRootLayerPdu rlp;
  rlp.vector = ACN_VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.datalen = PROBE_REQUEST_RLP_DATA_MIN_SIZE;

  // Calculate the data length
  const KnownUid* cur_uid = probe_request->uid_list;
  while (cur_uid && rlp.datalen < PROBE_REQUEST_RLP_DATA_MAX_SIZE)
  {
    rlp.datalen += 6;
    cur_uid = cur_uid->next;
  }

  // Pack the Root Layer PDU header0
  cur_ptr += etcpal_pack_root_layer_header(cur_ptr, (size_t)(buf_end - cur_ptr), &rlp);

  // Pack the LLRP header
  cur_ptr += etcpal_pack_llrp_header(cur_ptr, rlp.datalen, VECTOR_LLRP_PROBE_REQUEST, header);

  // Pack the Probe Request PDU header fields
  *cur_ptr = 0xf0;
  ETCPAL_PDU_PACK_EXT_LEN(cur_ptr, rlp.datalen - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  *cur_ptr++ = VECTOR_PROBE_REQUEST_DATA;
  etcpal_pack_16b(cur_ptr, probe_request->lower_uid.manu);
  cur_ptr += 2;
  etcpal_pack_32b(cur_ptr, probe_request->lower_uid.id);
  cur_ptr += 4;
  etcpal_pack_16b(cur_ptr, probe_request->upper_uid.manu);
  cur_ptr += 2;
  etcpal_pack_32b(cur_ptr, probe_request->upper_uid.id);
  cur_ptr += 4;
  etcpal_pack_16b(cur_ptr, probe_request->filter);
  cur_ptr += 2;

  // Pack the Known UIDs
  cur_uid = probe_request->uid_list;
  while (cur_uid)
  {
    if (cur_ptr + 6 > buf_end)
      break;
    etcpal_pack_16b(cur_ptr, cur_uid->uid.manu);
    cur_ptr += 2;
    etcpal_pack_32b(cur_ptr, cur_uid->uid.id);
    cur_ptr += 4;
    cur_uid = cur_uid->next;
  }

  int send_res =
      etcpal_sendto(sock, buf, (size_t)(cur_ptr - buf), 0, ipv6 ? kLlrpIpv6RequestAddr : kLlrpIpv4RequestAddr);
  if (send_res >= 0)
    return kEtcPalErrOk;
  else
    return (etcpal_error_t)send_res;
}

#define PROBE_REPLY_RLP_DATA_SIZE (LLRP_HEADER_SIZE + PROBE_REPLY_PDU_SIZE)

etcpal_error_t send_llrp_probe_reply(etcpal_socket_t sock, uint8_t* buf, bool ipv6, const LlrpHeader* header,
                                     const DiscoveredLlrpTarget* target_info)
{
  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = cur_ptr + LLRP_TARGET_MAX_MESSAGE_SIZE;

  // Pack the UDP Preamble
  cur_ptr += etcpal_pack_udp_preamble(cur_ptr, (size_t)(buf_end - cur_ptr));

  EtcPalRootLayerPdu rlp;
  rlp.vector = ACN_VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.datalen = PROBE_REPLY_RLP_DATA_SIZE;

  // Pack the Root Layer PDU header
  cur_ptr += etcpal_pack_root_layer_header(cur_ptr, (size_t)(buf_end - cur_ptr), &rlp);

  // Pack the LLRP header
  cur_ptr += etcpal_pack_llrp_header(cur_ptr, rlp.datalen, VECTOR_LLRP_PROBE_REPLY, header);

  // Pack the Probe Reply PDU
  *cur_ptr = 0xf0;
  ETCPAL_PDU_PACK_EXT_LEN(cur_ptr, rlp.datalen - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  *cur_ptr++ = VECTOR_PROBE_REPLY_DATA;
  etcpal_pack_16b(cur_ptr, target_info->uid.manu);
  cur_ptr += 2;
  etcpal_pack_32b(cur_ptr, target_info->uid.id);
  cur_ptr += 4;
  memcpy(cur_ptr, target_info->hardware_address, 6);
  cur_ptr += 6;
  *cur_ptr++ = (uint8_t)target_info->component_type;

  int send_res = etcpal_sendto(sock, buf, (size_t)(cur_ptr - buf), 0, ipv6 ? kLlrpIpv6RespAddr : kLlrpIpv4RespAddr);
  if (send_res >= 0)
    return kEtcPalErrOk;
  else
    return (etcpal_error_t)send_res;
}

#define RDM_CMD_RLP_DATA_MIN_SIZE (LLRP_HEADER_SIZE + 3 /* RDM cmd PDU Flags + Length */)

etcpal_error_t send_llrp_rdm(etcpal_socket_t sock, uint8_t* buf, const EtcPalSockaddr* dest_addr,
                             const LlrpHeader* header, const RdmBuffer* rdm_msg)
{
  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = cur_ptr + LLRP_MAX_MESSAGE_SIZE;

  // Pack the UDP Preamble
  cur_ptr += etcpal_pack_udp_preamble(cur_ptr, (size_t)(buf_end - cur_ptr));

  EtcPalRootLayerPdu rlp;
  rlp.vector = ACN_VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.datalen = RDM_CMD_RLP_DATA_MIN_SIZE + rdm_msg->datalen;

  // Pack the Root Layer PDU header
  cur_ptr += etcpal_pack_root_layer_header(cur_ptr, (size_t)(buf_end - cur_ptr), &rlp);

  // Pack the LLRP header
  cur_ptr += etcpal_pack_llrp_header(cur_ptr, rlp.datalen, VECTOR_LLRP_RDM_CMD, header);

  // Pack the RDM Command PDU
  *cur_ptr = 0xf0;
  ETCPAL_PDU_PACK_EXT_LEN(cur_ptr, rlp.datalen - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  memcpy(cur_ptr, rdm_msg->data, rdm_msg->datalen);
  cur_ptr += rdm_msg->datalen;

  int send_res = etcpal_sendto(sock, buf, (size_t)(cur_ptr - buf), 0, dest_addr);
  if (send_res >= 0)
    return kEtcPalErrOk;
  else
    return (etcpal_error_t)send_res;
}

etcpal_error_t send_llrp_rdm_command(etcpal_socket_t sock, uint8_t* buf, bool ipv6, const LlrpHeader* header,
                                     const RdmBuffer* cmd)
{
  return send_llrp_rdm(sock, buf, ipv6 ? kLlrpIpv6RequestAddr : kLlrpIpv4RequestAddr, header, cmd);
}

etcpal_error_t send_llrp_rdm_response(etcpal_socket_t sock, uint8_t* buf, bool ipv6, const LlrpHeader* header,
                                      const RdmBuffer* resp)
{
  return send_llrp_rdm(sock, buf, ipv6 ? kLlrpIpv6RespAddr : kLlrpIpv4RespAddr, header, resp);
}
