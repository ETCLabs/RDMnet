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

#include "rdmnet/core/llrp_prot.h"

#include <string.h>
#include "etcpal/pack.h"
#include "etcpal/socket.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/core/llrp.h"

/**************************** Global variables *******************************/

/*************************** Private constants *******************************/

#define PROBE_REPLY_PDU_SIZE \
  (3 /* Flags + Length */ + 1 /* Vector */ + 6 /* UID */ + 6 /* Hardware Address */ + 1 /* Component Type */)
#define LLRP_MIN_PDU_SIZE (LLRP_HEADER_SIZE + PROBE_REPLY_PDU_SIZE)
#define LLRP_MIN_TOTAL_MESSAGE_SIZE (ACN_UDP_PREAMBLE_SIZE + ACN_RLP_HEADER_SIZE_EXT_LEN + LLRP_MIN_PDU_SIZE)
#define LLRP_RDM_CMD_PDU_MIN_SIZE (3 /* Flags + Length */ + RDM_MIN_BYTES)

/*********************** Private function prototypes *************************/

static bool parse_llrp_pdu(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest, LlrpMessage* msg);
static bool parse_llrp_probe_request(const uint8_t*             buf,
                                     size_t                     buflen,
                                     const LlrpMessageInterest* interest,
                                     RemoteProbeRequest*        request);
static bool parse_llrp_probe_reply(const uint8_t* buf, size_t buflen, LlrpDiscoveredTarget* reply);
static bool parse_llrp_rdm_command(const uint8_t* buf, size_t buflen, RdmBuffer* cmd);

static size_t pack_llrp_header(uint8_t* buf, size_t pdu_len, uint32_t vector, const LlrpHeader* header);

static etcpal_error_t send_llrp_rdm(etcpal_socket_t       sock,
                                    uint8_t*              buf,
                                    const EtcPalSockAddr* dest_addr,
                                    const LlrpHeader*     header,
                                    const RdmBuffer*      rdm_msg);

/*************************** Function definitions ****************************/

bool rc_get_llrp_destination_cid(const uint8_t* buf, size_t buflen, EtcPalUuid* dest_cid)
{
  if (!buf || !dest_cid || buflen < LLRP_MIN_TOTAL_MESSAGE_SIZE)
    return false;

  // Try to parse the UDP preamble.
  AcnUdpPreamble preamble;
  if (!acn_parse_udp_preamble(buf, buflen, &preamble))
    return false;

  // Try to parse the Root Layer PDU header.
  AcnRootLayerPdu rlp;
  AcnPdu          last_pdu = ACN_PDU_INIT;
  if (!acn_parse_root_layer_pdu(preamble.rlp_block, preamble.rlp_block_len, &rlp, &last_pdu))
    return false;

  // Check the PDU length
  const uint8_t* cur_ptr = rlp.pdata;
  size_t         llrp_pdu_len = ACN_PDU_LENGTH(cur_ptr);
  if (llrp_pdu_len > rlp.data_len || llrp_pdu_len < LLRP_MIN_PDU_SIZE)
    return false;

  // Jump to the position of the LLRP destination CID and fill it in
  cur_ptr += 7;
  memcpy(dest_cid->data, cur_ptr, ETCPAL_UUID_BYTES);
  return true;
}

bool rc_parse_llrp_message(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest, LlrpMessage* msg)
{
  if (!RDMNET_ASSERT_VERIFY(interest))
    return false;

  if (!buf || !msg || buflen < LLRP_MIN_TOTAL_MESSAGE_SIZE)
    return false;

  // Try to parse the UDP preamble.
  AcnUdpPreamble preamble;
  if (!acn_parse_udp_preamble(buf, buflen, &preamble))
    return false;

  // Try to parse the Root Layer PDU header.
  AcnRootLayerPdu rlp;
  AcnPdu          last_pdu = ACN_PDU_INIT;
  if (!acn_parse_root_layer_pdu(preamble.rlp_block, preamble.rlp_block_len, &rlp, &last_pdu))
    return false;

  // Fill in the data we have and try to parse the LLRP PDU.
  msg->header.sender_cid = rlp.sender_cid;
  return parse_llrp_pdu(rlp.pdata, rlp.data_len, interest, msg);
}

bool parse_llrp_pdu(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest, LlrpMessage* msg)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(interest) || !RDMNET_ASSERT_VERIFY(msg) ||
      !RDMNET_ASSERT_VERIFY(kLlrpBroadcastCid))
  {
    return false;
  }

  if (buflen < LLRP_MIN_PDU_SIZE)
    return false;

  // Check the PDU length
  const uint8_t* cur_ptr = buf;
  size_t         llrp_pdu_len = ACN_PDU_LENGTH(cur_ptr);
  if (llrp_pdu_len > buflen || llrp_pdu_len < LLRP_MIN_PDU_SIZE)
    return false;

  // Fill in the LLRP PDU header data
  cur_ptr += 3;
  msg->vector = etcpal_unpack_u32b(cur_ptr);
  cur_ptr += 4;
  memcpy(msg->header.dest_cid.data, cur_ptr, ETCPAL_UUID_BYTES);
  cur_ptr += ETCPAL_UUID_BYTES;
  msg->header.transaction_number = etcpal_unpack_u32b(cur_ptr);
  cur_ptr += 4;

  // Parse the next layer, based on the vector value and what the caller has registered interest in
  if (0 == ETCPAL_UUID_CMP(&msg->header.dest_cid, kLlrpBroadcastCid) ||
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

bool parse_llrp_probe_request(const uint8_t*             buf,
                              size_t                     buflen,
                              const LlrpMessageInterest* interest,
                              RemoteProbeRequest*        request)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(interest) || !RDMNET_ASSERT_VERIFY(request))
    return false;

  if (buflen < PROBE_REQUEST_PDU_MIN_SIZE)
    return false;

  // Check the PDU length
  const uint8_t* cur_ptr = buf;
  size_t         pdu_len = ACN_PDU_LENGTH(cur_ptr);
  if (pdu_len > buflen || pdu_len < PROBE_REQUEST_PDU_MIN_SIZE)
    return false;
  const uint8_t* buf_end = cur_ptr + pdu_len;

  // Fill in the rest of the Probe Request data
  cur_ptr += 3;
  uint8_t vector = *cur_ptr++;
  if (vector != VECTOR_PROBE_REQUEST_DATA)
    return false;

  RdmUid lower_uid_bound;
  lower_uid_bound.manu = etcpal_unpack_u16b(cur_ptr);
  cur_ptr += 2;
  lower_uid_bound.id = etcpal_unpack_u32b(cur_ptr);
  cur_ptr += 4;

  RdmUid upper_uid_bound;
  upper_uid_bound.manu = etcpal_unpack_u16b(cur_ptr);
  cur_ptr += 2;
  upper_uid_bound.id = etcpal_unpack_u32b(cur_ptr);
  cur_ptr += 4;
  request->filter = etcpal_unpack_u16b(cur_ptr);
  cur_ptr += 2;

  // If our UID is not in the range, there is no need to check the Known UIDs.
  if (rdm_uid_compare(&interest->my_uid, &lower_uid_bound) >= 0 &&
      rdm_uid_compare(&interest->my_uid, &upper_uid_bound) <= 0)
  {
    request->contains_my_uid = true;

    // Check the Known UIDs to see if the registered UID is in it.
    while (cur_ptr + 6 <= buf_end)
    {
      RdmUid cur_uid;
      cur_uid.manu = etcpal_unpack_u16b(cur_ptr);
      cur_ptr += 2;
      cur_uid.id = etcpal_unpack_u32b(cur_ptr);
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

bool parse_llrp_probe_reply(const uint8_t* buf, size_t buflen, LlrpDiscoveredTarget* reply)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(reply))
    return false;

  if (buflen < PROBE_REPLY_PDU_SIZE)
    return false;

  const uint8_t* cur_ptr = buf;
  size_t         pdu_len = ACN_PDU_LENGTH(cur_ptr);
  if (pdu_len != PROBE_REPLY_PDU_SIZE)
    return false;
  cur_ptr += 3;

  uint8_t vector = *cur_ptr++;
  if (vector != VECTOR_PROBE_REPLY_DATA)
    return false;

  reply->uid.manu = etcpal_unpack_u16b(cur_ptr);
  cur_ptr += 2;
  reply->uid.id = etcpal_unpack_u32b(cur_ptr);
  cur_ptr += 4;
  memcpy(reply->hardware_address.data, cur_ptr, 6);
  cur_ptr += 6;
  reply->component_type = (llrp_component_t)*cur_ptr;
  return true;
}

bool parse_llrp_rdm_command(const uint8_t* buf, size_t buflen, RdmBuffer* cmd)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(cmd))
    return false;

  if (buflen < LLRP_RDM_CMD_PDU_MIN_SIZE)
    return false;

  const uint8_t* cur_ptr = buf;
  size_t         pdu_len = ACN_PDU_LENGTH(cur_ptr);
  if (pdu_len > buflen || pdu_len > LLRP_RDM_CMD_PDU_MAX_SIZE || pdu_len < LLRP_RDM_CMD_PDU_MIN_SIZE)
    return false;
  cur_ptr += 3;

  if (*cur_ptr != VECTOR_RDM_CMD_RDM_DATA)
    return false;

  memcpy(cmd->data, cur_ptr, pdu_len - 3);
  cmd->data_len = pdu_len - 3;
  return true;
}

size_t pack_llrp_header(uint8_t* buf, size_t pdu_len, uint32_t vector, const LlrpHeader* header)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(header))
    return 0;

  uint8_t* cur_ptr = buf;

  *cur_ptr = 0xf0;
  ACN_PDU_PACK_EXT_LEN(cur_ptr, pdu_len);
  cur_ptr += 3;
  etcpal_pack_u32b(cur_ptr, vector);
  cur_ptr += 4;
  memcpy(cur_ptr, header->dest_cid.data, ETCPAL_UUID_BYTES);
  cur_ptr += ETCPAL_UUID_BYTES;
  etcpal_pack_u32b(cur_ptr, header->transaction_number);
  cur_ptr += 4;
  return (size_t)(cur_ptr - buf);
}

#define PROBE_REQUEST_RLP_DATA_MIN_SIZE (LLRP_HEADER_SIZE + PROBE_REQUEST_PDU_MIN_SIZE)
#define PROBE_REQUEST_RLP_DATA_MAX_SIZE (PROBE_REQUEST_RLP_DATA_MIN_SIZE + (6 * LLRP_KNOWN_UID_SIZE))

etcpal_error_t rc_send_llrp_probe_request(etcpal_socket_t          sock,
                                          uint8_t*                 buf,
                                          bool                     ipv6,
                                          const LlrpHeader*        header,
                                          const LocalProbeRequest* probe_request)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(header) || !RDMNET_ASSERT_VERIFY(probe_request) ||
      !RDMNET_ASSERT_VERIFY(probe_request->known_uids))
  {
    return kEtcPalErrSys;
  }

  if (probe_request->num_known_uids > LLRP_KNOWN_UID_SIZE)
    return kEtcPalErrInvalid;

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = cur_ptr + LLRP_MANAGER_MAX_MESSAGE_SIZE;

  // Pack the UDP Preamble
  cur_ptr += acn_pack_udp_preamble(cur_ptr, (size_t)(buf_end - cur_ptr));

  AcnRootLayerPdu rlp;
  rlp.vector = ACN_VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.data_len = PROBE_REQUEST_RLP_DATA_MIN_SIZE + (probe_request->num_known_uids * 6);

  // Pack the Root Layer PDU header0
  cur_ptr += acn_pack_root_layer_header(cur_ptr, (size_t)(buf_end - cur_ptr), &rlp);

  // Pack the LLRP header
  cur_ptr += pack_llrp_header(cur_ptr, rlp.data_len, VECTOR_LLRP_PROBE_REQUEST, header);

  // Pack the Probe Request PDU header fields
  *cur_ptr = 0xf0;
  ACN_PDU_PACK_EXT_LEN(cur_ptr, rlp.data_len - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  *cur_ptr++ = VECTOR_PROBE_REQUEST_DATA;
  etcpal_pack_u16b(cur_ptr, probe_request->lower_uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, probe_request->lower_uid.id);
  cur_ptr += 4;
  etcpal_pack_u16b(cur_ptr, probe_request->upper_uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, probe_request->upper_uid.id);
  cur_ptr += 4;
  etcpal_pack_u16b(cur_ptr, probe_request->filter);
  cur_ptr += 2;

  // Pack the Known UIDs
  for (const RdmUid* cur_uid = probe_request->known_uids;
       cur_uid < probe_request->known_uids + probe_request->num_known_uids; ++cur_uid)
  {
    etcpal_pack_u16b(cur_ptr, cur_uid->manu);
    cur_ptr += 2;
    etcpal_pack_u32b(cur_ptr, cur_uid->id);
    cur_ptr += 4;
  }

  const EtcPalSockAddr* dest_addr = ipv6 ? kLlrpIpv6RequestAddr : kLlrpIpv4RequestAddr;
  if (!RDMNET_ASSERT_VERIFY(dest_addr))
    return kEtcPalErrSys;

  int send_res = etcpal_sendto(sock, buf, (size_t)(cur_ptr - buf), 0, dest_addr);
  if (send_res >= 0)
    return kEtcPalErrOk;

  return (etcpal_error_t)send_res;
}

#define PROBE_REPLY_RLP_DATA_SIZE (LLRP_HEADER_SIZE + PROBE_REPLY_PDU_SIZE)

etcpal_error_t rc_send_llrp_probe_reply(etcpal_socket_t             sock,
                                        uint8_t*                    buf,
                                        bool                        ipv6,
                                        const LlrpHeader*           header,
                                        const LlrpDiscoveredTarget* target_info)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(header) || !RDMNET_ASSERT_VERIFY(target_info))
    return kEtcPalErrSys;

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = cur_ptr + LLRP_TARGET_MAX_MESSAGE_SIZE;

  // Pack the UDP Preamble
  cur_ptr += acn_pack_udp_preamble(cur_ptr, (size_t)(buf_end - cur_ptr));

  AcnRootLayerPdu rlp;
  rlp.vector = ACN_VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.data_len = PROBE_REPLY_RLP_DATA_SIZE;

  // Pack the Root Layer PDU header
  cur_ptr += acn_pack_root_layer_header(cur_ptr, (size_t)(buf_end - cur_ptr), &rlp);

  // Pack the LLRP header
  cur_ptr += pack_llrp_header(cur_ptr, rlp.data_len, VECTOR_LLRP_PROBE_REPLY, header);

  // Pack the Probe Reply PDU
  *cur_ptr = 0xf0;
  ACN_PDU_PACK_EXT_LEN(cur_ptr, rlp.data_len - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  *cur_ptr++ = VECTOR_PROBE_REPLY_DATA;
  etcpal_pack_u16b(cur_ptr, target_info->uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, target_info->uid.id);
  cur_ptr += 4;
  memcpy(cur_ptr, target_info->hardware_address.data, 6);
  cur_ptr += 6;
  *cur_ptr++ = (uint8_t)target_info->component_type;

  const EtcPalSockAddr* dest_addr = ipv6 ? kLlrpIpv6RespAddr : kLlrpIpv4RespAddr;
  if (!RDMNET_ASSERT_VERIFY(dest_addr))
    return kEtcPalErrSys;

  int send_res = etcpal_sendto(sock, buf, (size_t)(cur_ptr - buf), 0, dest_addr);
  if (send_res >= 0)
    return kEtcPalErrOk;

  return (etcpal_error_t)send_res;
}

#define RDM_CMD_RLP_DATA_MIN_SIZE (LLRP_HEADER_SIZE + 3 /* RDM cmd PDU Flags + Length */)

etcpal_error_t send_llrp_rdm(etcpal_socket_t       sock,
                             uint8_t*              buf,
                             const EtcPalSockAddr* dest_addr,
                             const LlrpHeader*     header,
                             const RdmBuffer*      rdm_msg)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(dest_addr) || !RDMNET_ASSERT_VERIFY(header) ||
      !RDMNET_ASSERT_VERIFY(rdm_msg))
  {
    return kEtcPalErrSys;
  }

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = cur_ptr + LLRP_MAX_MESSAGE_SIZE;

  // Pack the UDP Preamble
  cur_ptr += acn_pack_udp_preamble(cur_ptr, (size_t)(buf_end - cur_ptr));

  AcnRootLayerPdu rlp;
  rlp.vector = ACN_VECTOR_ROOT_LLRP;
  rlp.sender_cid = header->sender_cid;
  rlp.data_len = RDM_CMD_RLP_DATA_MIN_SIZE + rdm_msg->data_len;

  // Pack the Root Layer PDU header
  cur_ptr += acn_pack_root_layer_header(cur_ptr, (size_t)(buf_end - cur_ptr), &rlp);

  // Pack the LLRP header
  cur_ptr += pack_llrp_header(cur_ptr, rlp.data_len, VECTOR_LLRP_RDM_CMD, header);

  // Pack the RDM Command PDU
  *cur_ptr = 0xf0;
  ACN_PDU_PACK_EXT_LEN(cur_ptr, rlp.data_len - LLRP_HEADER_SIZE);
  cur_ptr += 3;
  memcpy(cur_ptr, rdm_msg->data, rdm_msg->data_len);
  cur_ptr += rdm_msg->data_len;

  int send_res = etcpal_sendto(sock, buf, (size_t)(cur_ptr - buf), 0, dest_addr);
  if (send_res >= 0)
    return kEtcPalErrOk;

  return (etcpal_error_t)send_res;
}

etcpal_error_t rc_send_llrp_rdm_command(etcpal_socket_t   sock,
                                        uint8_t*          buf,
                                        bool              ipv6,
                                        const LlrpHeader* header,
                                        const RdmBuffer*  cmd)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(header) || !RDMNET_ASSERT_VERIFY(cmd))
    return kEtcPalErrSys;

  const EtcPalSockAddr* dest_addr = ipv6 ? kLlrpIpv6RequestAddr : kLlrpIpv4RequestAddr;
  if (!RDMNET_ASSERT_VERIFY(dest_addr))
    return kEtcPalErrSys;

  return send_llrp_rdm(sock, buf, dest_addr, header, cmd);
}

etcpal_error_t rc_send_llrp_rdm_response(etcpal_socket_t   sock,
                                         uint8_t*          buf,
                                         bool              ipv6,
                                         const LlrpHeader* header,
                                         const RdmBuffer*  resp)
{
  if (!RDMNET_ASSERT_VERIFY(buf) || !RDMNET_ASSERT_VERIFY(header) || !RDMNET_ASSERT_VERIFY(resp))
    return kEtcPalErrSys;

  const EtcPalSockAddr* dest_addr = ipv6 ? kLlrpIpv6RespAddr : kLlrpIpv4RespAddr;
  if (!RDMNET_ASSERT_VERIFY(dest_addr))
    return kEtcPalErrSys;

  return send_llrp_rdm(sock, buf, dest_addr, header, resp);
}
