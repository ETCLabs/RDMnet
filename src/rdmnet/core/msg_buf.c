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

#include "rdmnet/core/msg_buf.h"

#include <assert.h>
#include <inttypes.h>
#include "etcpal/acn_rlp.h"
#include "etcpal/common.h"
#include "etcpal/pack.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/broker_prot.h"
#include "rdmnet/core/rpt_prot.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/opts.h"

/*********************** Private function prototypes *************************/

static size_t            locate_tcp_preamble(RCMsgBuf* msg_buf);
static size_t            consume_bad_block(PduBlockState* block, size_t data_len, rc_parse_result_t* parse_res);
static rc_parse_result_t check_for_full_parse(rc_parse_result_t prev_res, PduBlockState* block);

// The parse functions are organized by protocol layer, and each one gets a subset of the overall
// state structure.

// Root layer
static size_t parse_rlp_block(RlpState*          rlpstate,
                              const uint8_t*     data,
                              size_t             data_size,
                              RdmnetMessage*     msg,
                              rc_parse_result_t* result);

// RDMnet layer
static void   initialize_rdmnet_message(RlpState* rlpstate, RdmnetMessage* msg, size_t pdu_data_len);
static size_t parse_broker_block(BrokerState*       bstate,
                                 const uint8_t*     data,
                                 size_t             data_len,
                                 BrokerMessage*     bmsg,
                                 rc_parse_result_t* result);
static size_t parse_rpt_block(RptState*          rstate,
                              const uint8_t*     data,
                              size_t             data_len,
                              RptMessage*        rmsg,
                              rc_parse_result_t* result);

// RPT layer
static void   initialize_rpt_message(RptState* rstate, RptMessage* rmsg, size_t pdu_data_len);
static size_t parse_rdm_list(RdmListState*      rlstate,
                             const uint8_t*     data,
                             size_t             data_len,
                             RptRdmBufList*     cmd_list,
                             rc_parse_result_t* result);
static size_t parse_rpt_status(RptStatusState*    rsstate,
                               const uint8_t*     data,
                               size_t             data_len,
                               RptStatusMsg*      smsg,
                               rc_parse_result_t* result);

// Broker layer
static void   initialize_broker_message(BrokerState* bstate, BrokerMessage* bmsg, size_t pdu_data_len);
static void   parse_client_connect_header(const uint8_t* data, BrokerClientConnectMsg* ccmsg);
static size_t parse_client_connect(ClientConnectState*     ccstate,
                                   const uint8_t*          data,
                                   size_t                  data_len,
                                   BrokerClientConnectMsg* ccmsg,
                                   rc_parse_result_t*      result);
static size_t parse_client_entry_update(ClientEntryUpdateState*     ceustate,
                                        const uint8_t*              data,
                                        size_t                      data_len,
                                        BrokerClientEntryUpdateMsg* ceumsg,
                                        rc_parse_result_t*          result);
static size_t parse_single_client_entry(ClientEntryState*  cstate,
                                        const uint8_t*     data,
                                        size_t             data_len,
                                        client_protocol_t* client_protocol,
                                        ClientEntryUnion*  entry,
                                        rc_parse_result_t* result);
static size_t parse_client_list(ClientListState*   clstate,
                                const uint8_t*     data,
                                size_t             data_len,
                                BrokerClientList*  clist,
                                rc_parse_result_t* result);
static size_t parse_request_dynamic_uid_assignment(GenericListState*            lstate,
                                                   const uint8_t*               data,
                                                   size_t                       data_len,
                                                   BrokerDynamicUidRequestList* rlist,
                                                   rc_parse_result_t*           result);
static size_t parse_dynamic_uid_assignment_list(GenericListState*               lstate,
                                                const uint8_t*                  data,
                                                size_t                          data_len,
                                                RdmnetDynamicUidAssignmentList* alist,
                                                rc_parse_result_t*              result);
static size_t parse_fetch_dynamic_uid_assignment_list(GenericListState*             lstate,
                                                      const uint8_t*                data,
                                                      size_t                        data_len,
                                                      BrokerFetchUidAssignmentList* alist,
                                                      rc_parse_result_t*            result);

// Helpers for parsing client list messages
static size_t                parse_rpt_client_list(ClientListState*     clstate,
                                                   const uint8_t*       data,
                                                   size_t               data_len,
                                                   RdmnetRptClientList* clist,
                                                   rc_parse_result_t*   result);
static RdmnetRptClientEntry* alloc_next_rpt_client_entry(RdmnetRptClientList* clist);
#if 0
static RdmnetEptClientEntry* alloc_next_ept_client_entry(RdmnetEptClientList* clist);
#endif

/*************************** Function definitions ****************************/

void rc_msg_buf_init(RCMsgBuf* msg_buf)
{
  if (!RDMNET_ASSERT_VERIFY(msg_buf))
    return;

  msg_buf->cur_data_size = 0;
  msg_buf->have_preamble = false;
}

etcpal_error_t rc_msg_buf_recv(RCMsgBuf* msg_buf, etcpal_socket_t socket)
{
  if (!RDMNET_ASSERT_VERIFY(msg_buf) || !RDMNET_ASSERT_VERIFY(msg_buf->cur_data_size <= RC_MSG_BUF_SIZE))
    return kEtcPalErrSys;

  size_t original_data_size = msg_buf->cur_data_size;

  int recv_res = 0;
  do
  {
    size_t remaining_length = RC_MSG_BUF_SIZE - msg_buf->cur_data_size;
    if (remaining_length > 0)
      recv_res = etcpal_recv(socket, &msg_buf->buf[msg_buf->cur_data_size], remaining_length, 0);
    else
      recv_res = kEtcPalErrWouldBlock;

    if (recv_res > 0)
      msg_buf->cur_data_size += recv_res;
  } while (recv_res > 0);

  if (recv_res < 0)
  {
    if ((etcpal_error_t)recv_res != kEtcPalErrWouldBlock)
      return (etcpal_error_t)recv_res;
    if (msg_buf->cur_data_size == original_data_size)
      return kEtcPalErrWouldBlock;
  }
  else if (recv_res == 0)
  {
    return kEtcPalErrConnClosed;
  }

  return kEtcPalErrOk;
}

etcpal_error_t rc_msg_buf_parse_data(RCMsgBuf* msg_buf)
{
  if (!RDMNET_ASSERT_VERIFY(msg_buf))
    return kEtcPalErrSys;

  // Unless we finish parsing a message in this function, we will return kEtcPalErrNoData to indicate
  // that the parse is still in progress.
  etcpal_error_t res = kEtcPalErrNoData;

  do
  {
    size_t consumed = 0;

    if (!msg_buf->have_preamble)
    {
      size_t pdu_block_size = locate_tcp_preamble(msg_buf);
      if (pdu_block_size)
      {
        INIT_RLP_STATE(&msg_buf->rlp_state, pdu_block_size);
        msg_buf->have_preamble = true;
      }
      else
      {
        res = kEtcPalErrNoData;
        break;
      }
    }
    if (msg_buf->have_preamble)
    {
      rc_parse_result_t parse_res;
      consumed = parse_rlp_block(&msg_buf->rlp_state, msg_buf->buf, msg_buf->cur_data_size, &msg_buf->msg, &parse_res);
      switch (parse_res)
      {
        case kRCParseResFullBlockParseOk:
        case kRCParseResFullBlockProtErr:
          msg_buf->have_preamble = false;
          res = (parse_res == kRCParseResFullBlockProtErr ? kEtcPalErrProtocol : kEtcPalErrOk);
          break;
        case kRCParseResPartialBlockParseOk:
        case kRCParseResPartialBlockProtErr:
          res = (parse_res == kRCParseResPartialBlockProtErr ? kEtcPalErrProtocol : kEtcPalErrOk);
          break;
        case kRCParseResNoData:
        default:
          res = kEtcPalErrNoData;
          break;
      }
    }

    if (consumed > 0)
    {
      // Roll the buffer to discard the data we have already parsed.
      if (!RDMNET_ASSERT_VERIFY(msg_buf->cur_data_size >= consumed))
        return kEtcPalErrSys;

      if (msg_buf->cur_data_size > consumed)
      {
        memmove(msg_buf->buf, &msg_buf->buf[consumed], msg_buf->cur_data_size - consumed);
      }
      msg_buf->cur_data_size -= consumed;
    }
  } while (res == kEtcPalErrProtocol);

  return res;
}

void initialize_rdmnet_message(RlpState* rlpstate, RdmnetMessage* msg, size_t pdu_data_len)
{
  if (!RDMNET_ASSERT_VERIFY(rlpstate) || !RDMNET_ASSERT_VERIFY(msg))
    return;

  switch (msg->vector)
  {
    case ACN_VECTOR_ROOT_BROKER:
      INIT_BROKER_STATE(&rlpstate->data.broker, pdu_data_len, msg);
      break;
    case ACN_VECTOR_ROOT_RPT:
      INIT_RPT_STATE(&rlpstate->data.rpt, pdu_data_len);
      break;
    default:
      INIT_PDU_BLOCK_STATE(&rlpstate->data.unknown, pdu_data_len);
      RDMNET_LOG_WARNING("Dropping Root Layer PDU with unknown vector %" PRIu32 ".", msg->vector);
      break;
  }
}

size_t parse_rlp_block(RlpState*          rlpstate,
                       const uint8_t*     data,
                       size_t             data_len,
                       RdmnetMessage*     msg,
                       rc_parse_result_t* result)
{
  if (!RDMNET_ASSERT_VERIFY(rlpstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(msg) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  rc_parse_result_t res = kRCParseResNoData;
  size_t            bytes_parsed = 0;

  if (rlpstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&rlpstate->block, data_len, &res);
  }
  else if (!rlpstate->block.parsed_header)
  {
    bool parse_err = false;

    // If the size remaining in the Root Layer PDU block is not enough for another Root Layer PDU
    // header, indicate a bad block condition.
    if ((rlpstate->block.block_size - rlpstate->block.size_parsed) < ACN_RLP_HEADER_SIZE_EXT_LEN)
    {
      parse_err = true;
    }
    else if (data_len >= ACN_RLP_HEADER_SIZE_EXT_LEN)
    {
      AcnRootLayerPdu rlp;

      // Inheritance at the root layer is disallowed by E1.33.
      if (acn_parse_root_layer_header(data, data_len, &rlp, NULL))
      {
        // Update the data pointers and sizes.
        bytes_parsed += ACN_RLP_HEADER_SIZE_EXT_LEN;
        rlpstate->block.size_parsed += ACN_RLP_HEADER_SIZE_EXT_LEN;

        // If this PDU indicates a length that takes it past the end of the block size from the
        // preamble, it is an error.
        if (rlpstate->block.size_parsed + rlp.data_len <= rlpstate->block.block_size)
        {
          // Fill in the root layer data in the overall RdmnetMessage struct.
          msg->vector = rlp.vector;
          msg->sender_cid = rlp.sender_cid;
          rlpstate->block.parsed_header = true;
          initialize_rdmnet_message(rlpstate, msg, rlp.data_len);
        }
        else
        {
          parse_err = true;
        }
      }
      else
      {
        parse_err = true;
      }
    }
    // No else for this block - if there is not enough data yet to parse an RLP header, we simply
    // indicate no data.

    if (parse_err)
    {
      // Parse error in the root layer header. We cannot keep parsing this block.
      bytes_parsed += consume_bad_block(&rlpstate->block, data_len, &res);
      RDMNET_LOG_WARNING("Protocol error encountered while parsing Root Layer PDU header.");
    }
  }
  if (rlpstate->block.parsed_header)
  {
    size_t next_layer_bytes_parsed;
    switch (msg->vector)
    {
      case ACN_VECTOR_ROOT_BROKER:
        next_layer_bytes_parsed = parse_broker_block(&rlpstate->data.broker, &data[bytes_parsed],
                                                     data_len - bytes_parsed, RDMNET_GET_BROKER_MSG(msg), &res);
        break;
      case ACN_VECTOR_ROOT_RPT:
        next_layer_bytes_parsed = parse_rpt_block(&rlpstate->data.rpt, &data[bytes_parsed], data_len - bytes_parsed,
                                                  RDMNET_GET_RPT_MSG(msg), &res);
        break;
      default:
        next_layer_bytes_parsed = consume_bad_block(&rlpstate->data.unknown, data_len - bytes_parsed, &res);
        break;
    }

    if (!RDMNET_ASSERT_VERIFY(next_layer_bytes_parsed <= (data_len - bytes_parsed)) ||
        !RDMNET_ASSERT_VERIFY(rlpstate->block.size_parsed + next_layer_bytes_parsed <= rlpstate->block.block_size))
    {
      return 0;
    }

    rlpstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &rlpstate->block);
  }
  *result = res;
  return bytes_parsed;
}

void initialize_broker_message(BrokerState* bstate, BrokerMessage* bmsg, size_t pdu_data_len)
{
  if (!RDMNET_ASSERT_VERIFY(bstate) || !RDMNET_ASSERT_VERIFY(bmsg))
    return;

  bool bad_length = false;

  switch (bmsg->vector)
  {
    case VECTOR_BROKER_CONNECT:
      if (pdu_data_len >= CLIENT_CONNECT_DATA_MIN_SIZE)
      {
        INIT_CLIENT_CONNECT_STATE(&bstate->data.client_connect, pdu_data_len, bmsg);
      }
      else
      {
        bad_length = true;
      }
      break;
    case VECTOR_BROKER_CONNECT_REPLY:
      if (pdu_data_len != BROKER_CONNECT_REPLY_DATA_SIZE)
        bad_length = true;
      break;
    case VECTOR_BROKER_CLIENT_ENTRY_UPDATE:
      if (pdu_data_len >= CLIENT_ENTRY_UPDATE_DATA_MIN_SIZE)
      {
        INIT_CLIENT_ENTRY_UPDATE_STATE(&bstate->data.update, pdu_data_len, bmsg);
      }
      else
      {
        bad_length = true;
      }
      break;
    case VECTOR_BROKER_REDIRECT_V4:
      if (pdu_data_len != REDIRECT_V4_DATA_SIZE)
        bad_length = true;
      break;
    case VECTOR_BROKER_REDIRECT_V6:
      if (pdu_data_len != REDIRECT_V6_DATA_SIZE)
        bad_length = true;
      break;
    case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
    case VECTOR_BROKER_CLIENT_ADD:
    case VECTOR_BROKER_CLIENT_REMOVE:
    case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
      INIT_CLIENT_LIST_STATE(&bstate->data.client_list, pdu_data_len, bmsg);
      break;
    // For the generic list messages, the length must be a multiple of the list entry size.
    case VECTOR_BROKER_REQUEST_DYNAMIC_UIDS:
      if (pdu_data_len > 0 && pdu_data_len % DYNAMIC_UID_REQUEST_PAIR_SIZE == 0)
      {
        BrokerDynamicUidRequestList* rlist = BROKER_GET_DYNAMIC_UID_REQUEST_LIST(bmsg);
        if (!RDMNET_ASSERT_VERIFY(rlist))
          return;

        rlist->requests = NULL;
        rlist->num_requests = 0;
        rlist->more_coming = false;

        INIT_GENERIC_LIST_STATE(&bstate->data.data_list, pdu_data_len);
      }
      else
      {
        bad_length = true;
      }
      break;
    case VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS:
      if (pdu_data_len > 0 && pdu_data_len % DYNAMIC_UID_MAPPING_SIZE == 0)
      {
        RdmnetDynamicUidAssignmentList* alist = BROKER_GET_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg);
        if (!RDMNET_ASSERT_VERIFY(alist))
          return;

        alist->mappings = NULL;
        alist->num_mappings = 0;
        alist->more_coming = false;

        INIT_GENERIC_LIST_STATE(&bstate->data.data_list, pdu_data_len);
      }
      else
      {
        bad_length = true;
      }
      break;
    case VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST:
      if (pdu_data_len > 0 && pdu_data_len % 6 /* Size of one packed UID */ == 0)
      {
        BrokerFetchUidAssignmentList* ulist = BROKER_GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg);
        if (!RDMNET_ASSERT_VERIFY(ulist))
          return;

        ulist->uids = NULL;
        ulist->num_uids = 0;
        ulist->more_coming = false;

        INIT_GENERIC_LIST_STATE(&bstate->data.data_list, pdu_data_len);
      }
      else
      {
        bad_length = true;
      }
      break;
    case VECTOR_BROKER_NULL:
    case VECTOR_BROKER_FETCH_CLIENT_LIST:
      // Check the length. These messages have no data.
      if (pdu_data_len != 0)
        bad_length = true;
      break;
    case VECTOR_BROKER_DISCONNECT:
      if (pdu_data_len != DISCONNECT_DATA_SIZE)
        bad_length = true;
      break;
    default:
      INIT_PDU_BLOCK_STATE(&bstate->data.unknown, pdu_data_len);
      RDMNET_LOG_WARNING("Dropping Broker PDU with unknown vector %d.", bmsg->vector);
      break;
  }

  if (bad_length)
  {
    INIT_PDU_BLOCK_STATE(&bstate->data.unknown, pdu_data_len);
    // An artificial "unknown" vector value to flag the data parsing logic to consume the data
    // section.
    bmsg->vector = 0xffff;
    RDMNET_LOG_WARNING("Dropping Broker PDU with vector %d and invalid length %zu", bmsg->vector,
                       pdu_data_len + BROKER_PDU_HEADER_SIZE);
  }
}

size_t parse_broker_block(BrokerState*       bstate,
                          const uint8_t*     data,
                          size_t             data_len,
                          BrokerMessage*     bmsg,
                          rc_parse_result_t* result)
{
  if (!RDMNET_ASSERT_VERIFY(bstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(bmsg) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  rc_parse_result_t res = kRCParseResNoData;
  size_t            bytes_parsed = 0;

  if (bstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&bstate->block, data_len, &res);
  }
  else if (!bstate->block.parsed_header)
  {
    bool parse_err = false;

    // If the size remaining in the Broker PDU block is not enough for another Broker PDU header,
    // indicate a bad block condition.
    if ((bstate->block.block_size - bstate->block.size_parsed) < BROKER_PDU_HEADER_SIZE)
    {
      parse_err = true;
    }
    else if (data_len >= BROKER_PDU_HEADER_SIZE)
    {
      // We can parse a Broker PDU header.
      const uint8_t* cur_ptr = data;

      size_t pdu_len = ACN_PDU_LENGTH(cur_ptr);
      if (pdu_len >= BROKER_PDU_HEADER_SIZE && bstate->block.size_parsed + pdu_len <= bstate->block.block_size)
      {
        size_t pdu_data_len = pdu_len - BROKER_PDU_HEADER_SIZE;

        cur_ptr += 3;
        bmsg->vector = etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;
        bytes_parsed += BROKER_PDU_HEADER_SIZE;
        bstate->block.size_parsed += BROKER_PDU_HEADER_SIZE;
        bstate->block.parsed_header = true;
        initialize_broker_message(bstate, bmsg, pdu_data_len);
      }
      else
      {
        parse_err = true;
      }
    }
    // Else we don't have enough data - return kRCParseResNoData by default.

    if (parse_err)
    {
      // Parse error in the Broker PDU header. We cannot keep parsing this block.
      bytes_parsed += consume_bad_block(&bstate->block, data_len, &res);
      RDMNET_LOG_WARNING("Protocol error encountered while parsing Broker PDU header.");
    }
  }
  if (bstate->block.parsed_header)
  {
    size_t next_layer_bytes_parsed = 0;
    size_t remaining_len = data_len - bytes_parsed;
    switch (bmsg->vector)
    {
      case VECTOR_BROKER_CONNECT: {
        BrokerClientConnectMsg* ccmsg = BROKER_GET_CLIENT_CONNECT_MSG(bmsg);
        if (!RDMNET_ASSERT_VERIFY(ccmsg))
          return 0;

        next_layer_bytes_parsed =
            parse_client_connect(&bstate->data.client_connect, &data[bytes_parsed], remaining_len, ccmsg, &res);
      }
      break;
      case VECTOR_BROKER_CONNECT_REPLY:
        if (remaining_len >= BROKER_CONNECT_REPLY_DATA_SIZE)
        {
          BrokerConnectReplyMsg* crmsg = BROKER_GET_CONNECT_REPLY_MSG(bmsg);
          if (!RDMNET_ASSERT_VERIFY(crmsg))
            return 0;

          const uint8_t* cur_ptr = &data[bytes_parsed];
          crmsg->connect_status = (rdmnet_connect_status_t)etcpal_unpack_u16b(cur_ptr);
          cur_ptr += 2;
          crmsg->e133_version = etcpal_unpack_u16b(cur_ptr);
          cur_ptr += 2;
          crmsg->broker_uid.manu = etcpal_unpack_u16b(cur_ptr);
          cur_ptr += 2;
          crmsg->broker_uid.id = etcpal_unpack_u32b(cur_ptr);
          cur_ptr += 4;
          crmsg->client_uid.manu = etcpal_unpack_u16b(cur_ptr);
          cur_ptr += 2;
          crmsg->client_uid.id = etcpal_unpack_u32b(cur_ptr);
          cur_ptr += 4;
          next_layer_bytes_parsed = (size_t)(cur_ptr - &data[bytes_parsed]);
          res = kRCParseResFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_CLIENT_ENTRY_UPDATE: {
        BrokerClientEntryUpdateMsg* ceumsg = BROKER_GET_CLIENT_ENTRY_UPDATE_MSG(bmsg);
        if (!RDMNET_ASSERT_VERIFY(ceumsg))
          return 0;

        next_layer_bytes_parsed =
            parse_client_entry_update(&bstate->data.update, &data[bytes_parsed], remaining_len, ceumsg, &res);
      }
      break;
      case VECTOR_BROKER_REDIRECT_V4:
        if (remaining_len >= REDIRECT_V4_DATA_SIZE)
        {
          BrokerClientRedirectMsg* crmsg = BROKER_GET_CLIENT_REDIRECT_MSG(bmsg);
          if (!RDMNET_ASSERT_VERIFY(crmsg))
            return 0;

          const uint8_t* cur_ptr = &data[bytes_parsed];
          ETCPAL_IP_SET_V4_ADDRESS(&crmsg->new_addr.ip, etcpal_unpack_u32b(cur_ptr));
          cur_ptr += 4;
          crmsg->new_addr.port = etcpal_unpack_u16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = (size_t)(cur_ptr - &data[bytes_parsed]);
          res = kRCParseResFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_REDIRECT_V6:
        if (remaining_len >= REDIRECT_V6_DATA_SIZE)
        {
          BrokerClientRedirectMsg* crmsg = BROKER_GET_CLIENT_REDIRECT_MSG(bmsg);
          if (!RDMNET_ASSERT_VERIFY(crmsg))
            return 0;

          const uint8_t* cur_ptr = &data[bytes_parsed];
          ETCPAL_IP_SET_V6_ADDRESS(&crmsg->new_addr.ip, cur_ptr);
          cur_ptr += 16;
          crmsg->new_addr.port = etcpal_unpack_u16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = (size_t)(cur_ptr - &data[bytes_parsed]);
          res = kRCParseResFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
      case VECTOR_BROKER_CLIENT_ADD:
      case VECTOR_BROKER_CLIENT_REMOVE:
      case VECTOR_BROKER_CLIENT_ENTRY_CHANGE: {
        BrokerClientList* clist = BROKER_GET_CLIENT_LIST(bmsg);
        if (!RDMNET_ASSERT_VERIFY(clist))
          return 0;

        next_layer_bytes_parsed =
            parse_client_list(&bstate->data.client_list, &data[bytes_parsed], remaining_len, clist, &res);
      }
      break;
      case VECTOR_BROKER_REQUEST_DYNAMIC_UIDS: {
        BrokerDynamicUidRequestList* rlist = BROKER_GET_DYNAMIC_UID_REQUEST_LIST(bmsg);
        if (!RDMNET_ASSERT_VERIFY(rlist))
          return 0;

        next_layer_bytes_parsed = parse_request_dynamic_uid_assignment(&bstate->data.data_list, &data[bytes_parsed],
                                                                       remaining_len, rlist, &res);
      }
      break;
      case VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS: {
        RdmnetDynamicUidAssignmentList* dualist = BROKER_GET_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg);
        if (!RDMNET_ASSERT_VERIFY(dualist))
          return 0;

        next_layer_bytes_parsed = parse_dynamic_uid_assignment_list(&bstate->data.data_list, &data[bytes_parsed],
                                                                    remaining_len, dualist, &res);
      }
      break;
      case VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST: {
        BrokerFetchUidAssignmentList* ualist = BROKER_GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg);
        if (!RDMNET_ASSERT_VERIFY(ualist))
          return 0;

        next_layer_bytes_parsed = parse_fetch_dynamic_uid_assignment_list(&bstate->data.data_list, &data[bytes_parsed],
                                                                          remaining_len, ualist, &res);
      }
      break;
      case VECTOR_BROKER_NULL:
      case VECTOR_BROKER_FETCH_CLIENT_LIST:
        // These messages have no data, so we are at the end of the PDU.
        res = kRCParseResFullBlockParseOk;
        break;
      case VECTOR_BROKER_DISCONNECT:
        if (remaining_len >= DISCONNECT_DATA_SIZE)
        {
          BrokerDisconnectMsg* dmsg = BROKER_GET_DISCONNECT_MSG(bmsg);
          if (!RDMNET_ASSERT_VERIFY(dmsg))
            return 0;

          const uint8_t* cur_ptr = &data[bytes_parsed];
          dmsg->disconnect_reason = (rdmnet_disconnect_reason_t)etcpal_unpack_u16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = (size_t)(cur_ptr - &data[bytes_parsed]);
          res = kRCParseResFullBlockParseOk;
        }
        break;
      default:
        // Unknown Broker vector - discard this Broker PDU.
        next_layer_bytes_parsed = consume_bad_block(&bstate->data.unknown, remaining_len, &res);
        break;
    }

    if (!RDMNET_ASSERT_VERIFY(next_layer_bytes_parsed <= remaining_len) ||
        !RDMNET_ASSERT_VERIFY(bstate->block.size_parsed + next_layer_bytes_parsed <= bstate->block.block_size))
    {
      return 0;
    }

    bstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &bstate->block);
  }
  *result = res;
  return bytes_parsed;
}

void parse_client_connect_header(const uint8_t* data, BrokerClientConnectMsg* ccmsg)
{
  if (!RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(ccmsg))
    return;

  const uint8_t* cur_ptr = data;

  BROKER_CLIENT_CONNECT_MSG_SET_SCOPE(ccmsg, (const char*)cur_ptr);
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  ccmsg->e133_version = etcpal_unpack_u16b(cur_ptr);
  cur_ptr += 2;
  BROKER_CLIENT_CONNECT_MSG_SET_SEARCH_DOMAIN(ccmsg, (const char*)cur_ptr);
  cur_ptr += E133_DOMAIN_STRING_PADDED_LENGTH;
  ccmsg->connect_flags = *cur_ptr;
}

size_t parse_client_connect(ClientConnectState*     ccstate,
                            const uint8_t*          data,
                            size_t                  data_len,
                            BrokerClientConnectMsg* ccmsg,
                            rc_parse_result_t*      result)
{
  if (!RDMNET_ASSERT_VERIFY(ccstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(ccmsg) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  rc_parse_result_t res = kRCParseResNoData;
  size_t            bytes_parsed = 0;

  if (!ccstate->common_data_parsed)
  {
    // We want to wait until we can parse all of the Client Connect common data at once.
    if (data_len < CLIENT_CONNECT_COMMON_FIELD_SIZE)
    {
      *result = kRCParseResNoData;
      return 0;
    }

    parse_client_connect_header(data, ccmsg);
    bytes_parsed += CLIENT_CONNECT_COMMON_FIELD_SIZE;
    ccstate->common_data_parsed = true;
    INIT_CLIENT_ENTRY_STATE(&ccstate->entry, ccstate->pdu_data_size - CLIENT_CONNECT_COMMON_FIELD_SIZE);
  }
  if (ccstate->common_data_parsed)
  {
    size_t next_layer_bytes_parsed =
        parse_single_client_entry(&ccstate->entry, &data[bytes_parsed], data_len - bytes_parsed,
                                  &ccmsg->client_entry.client_protocol, &ccmsg->client_entry.data, &res);

    if (!RDMNET_ASSERT_VERIFY(next_layer_bytes_parsed <= (data_len - bytes_parsed)))
      return 0;

    bytes_parsed += next_layer_bytes_parsed;
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_client_entry_update(ClientEntryUpdateState*     ceustate,
                                 const uint8_t*              data,
                                 size_t                      data_len,
                                 BrokerClientEntryUpdateMsg* ceumsg,
                                 rc_parse_result_t*          result)
{
  if (!RDMNET_ASSERT_VERIFY(ceustate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(ceumsg) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  rc_parse_result_t res = kRCParseResNoData;
  size_t            bytes_parsed = 0;

  if (!ceustate->common_data_parsed)
  {
    // We want to wait until we can parse all of the Client Entry Update common data at once.
    if (data_len < CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE)
    {
      *result = kRCParseResNoData;
      return 0;
    }

    ceumsg->connect_flags = *data;
    bytes_parsed += CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE;
    ceustate->common_data_parsed = true;
    INIT_CLIENT_ENTRY_STATE(&ceustate->entry, ceustate->pdu_data_size - CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE);
  }
  if (ceustate->common_data_parsed)
  {
    size_t next_layer_bytes_parsed =
        parse_single_client_entry(&ceustate->entry, &data[bytes_parsed], data_len - bytes_parsed,
                                  &ceumsg->client_entry.client_protocol, &ceumsg->client_entry.data, &res);

    if (!RDMNET_ASSERT_VERIFY(next_layer_bytes_parsed <= (data_len - bytes_parsed)))
      return 0;

    bytes_parsed += next_layer_bytes_parsed;
  }

  *result = res;
  return bytes_parsed;
}

#define GET_LENGTH_FROM_CENTRY_HEADER(dataptr) (RDMNET_ASSERT_VERIFY(dataptr) ? ACN_PDU_LENGTH(dataptr) : 0)
#define GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(dataptr) \
  (RDMNET_ASSERT_VERIFY(dataptr) ? (client_protocol_t)(etcpal_unpack_u32b((dataptr) + 3)) : kClientProtocolUnknown)
#define COPY_CID_FROM_CENTRY_HEADER(dataptr, cid)               \
  ((RDMNET_ASSERT_VERIFY(dataptr) && RDMNET_ASSERT_VERIFY(cid)) \
       ? memcpy((cid)->data, (dataptr) + 7, ETCPAL_UUID_BYTES)  \
       : NULL)

size_t parse_single_client_entry(ClientEntryState*  cstate,
                                 const uint8_t*     data,
                                 size_t             data_len,
                                 client_protocol_t* client_protocol,
                                 ClientEntryUnion*  entry,
                                 rc_parse_result_t* result)
{
  if (!RDMNET_ASSERT_VERIFY(cstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(client_protocol) ||
      !RDMNET_ASSERT_VERIFY(entry) || !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  size_t            bytes_parsed = 0;
  rc_parse_result_t res = kRCParseResNoData;

  if (cstate->client_protocol == kClientProtocolUnknown)
  {
    if (data_len >= CLIENT_ENTRY_HEADER_SIZE)
    {
      // Parse the Client Entry header
      size_t cli_entry_pdu_len = GET_LENGTH_FROM_CENTRY_HEADER(data);
      cstate->client_protocol = GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(data);
      bytes_parsed += CLIENT_ENTRY_HEADER_SIZE;
      INIT_PDU_BLOCK_STATE(&cstate->entry_data, cli_entry_pdu_len - CLIENT_ENTRY_HEADER_SIZE);
      if (cli_entry_pdu_len > cstate->enclosing_block_size)
      {
        bytes_parsed += consume_bad_block(&cstate->entry_data, data_len - bytes_parsed, &res);
      }
      else
      {
        if (cstate->client_protocol == kClientProtocolRPT)
          COPY_CID_FROM_CENTRY_HEADER(data, &entry->rpt.cid);
        else
          COPY_CID_FROM_CENTRY_HEADER(data, &entry->ept.cid);
      }
    }
    // Else return no data
  }
  if (cstate->client_protocol != kClientProtocolUnknown)
  {
    size_t remaining_len = data_len - bytes_parsed;
    *client_protocol = cstate->client_protocol;

    if (cstate->entry_data.consuming_bad_block)
    {
      bytes_parsed += consume_bad_block(&cstate->entry_data, remaining_len, &res);
    }
    else if (cstate->client_protocol == kClientProtocolEPT)
    {
      // Parse the EPT Client Entry data
      // TODO
      bytes_parsed += consume_bad_block(&cstate->entry_data, remaining_len, &res);
    }
    else if (cstate->client_protocol == kClientProtocolRPT)
    {
      if (cstate->entry_data.size_parsed + RPT_CLIENT_ENTRY_DATA_SIZE == cstate->entry_data.block_size)
      {
        if (remaining_len >= RPT_CLIENT_ENTRY_DATA_SIZE)
        {
          // Parse the RPT Client Entry data
          RdmnetRptClientEntry* rpt_entry = (RdmnetRptClientEntry*)entry;
          const uint8_t*        cur_ptr = &data[bytes_parsed];

          rpt_entry->uid.manu = etcpal_unpack_u16b(cur_ptr);
          cur_ptr += 2;
          rpt_entry->uid.id = etcpal_unpack_u32b(cur_ptr);
          cur_ptr += 4;
          rpt_entry->type = (rpt_client_type_t)*cur_ptr++;
          memcpy(rpt_entry->binding_cid.data, cur_ptr, ETCPAL_UUID_BYTES);
          bytes_parsed += RPT_CLIENT_ENTRY_DATA_SIZE;
          cstate->entry_data.size_parsed += RPT_CLIENT_ENTRY_DATA_SIZE;
          res = kRCParseResFullBlockParseOk;
        }
        // Else return no data
      }
      else
      {
        // PDU length mismatch
        bytes_parsed += consume_bad_block(&cstate->entry_data, remaining_len, &res);
        RDMNET_LOG_WARNING("Dropping RPT Client Entry with invalid length %zu",
                           cstate->entry_data.block_size + CLIENT_ENTRY_HEADER_SIZE);
      }
    }
    else
    {
      // Unknown Client Protocol
      bytes_parsed += consume_bad_block(&cstate->entry_data, remaining_len, &res);
      RDMNET_LOG_WARNING("Dropping Client Entry with invalid client protocol %d", cstate->client_protocol);
    }
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_client_list(ClientListState*   clstate,
                         const uint8_t*     data,
                         size_t             data_len,
                         BrokerClientList*  clist,
                         rc_parse_result_t* result)
{
  if (!RDMNET_ASSERT_VERIFY(clstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(clist) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  rc_parse_result_t res = kRCParseResNoData;
  size_t            bytes_parsed = 0;

  if (clstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&clstate->block, data_len, &res);
  }
  else
  {
    if (clist->client_protocol == kClientProtocolUnknown)
    {
      if (data_len >= CLIENT_ENTRY_HEADER_SIZE)
      {
        clist->client_protocol = GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(data);
      }
    }

    if (clist->client_protocol == kClientProtocolRPT)
    {
      RdmnetRptClientList* rclist = BROKER_GET_RPT_CLIENT_LIST(clist);
      if (!RDMNET_ASSERT_VERIFY(rclist))
        return 0;

      bytes_parsed += parse_rpt_client_list(clstate, data, data_len, rclist, &res);
    }
    else if (clist->client_protocol == kClientProtocolEPT)
    {
      // TODO EPT
      bytes_parsed += consume_bad_block(&clstate->block, data_len, &res);
    }
    else if (clist->client_protocol != kClientProtocolUnknown)
    {
      RDMNET_LOG_WARNING("Dropping Client List message with unknown Client Protocol %d", clist->client_protocol);
      bytes_parsed += consume_bad_block(&clstate->block, data_len, &res);
    }
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_rpt_client_list(ClientListState*     clstate,
                             const uint8_t*       data,
                             size_t               data_len,
                             RdmnetRptClientList* clist,
                             rc_parse_result_t*   result)
{
  if (!RDMNET_ASSERT_VERIFY(clstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(clist) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  size_t            bytes_parsed = 0;
  rc_parse_result_t res = kRCParseResNoData;

  while (clstate->block.size_parsed < clstate->block.block_size)
  {
    size_t                remaining_len = data_len - bytes_parsed;
    const uint8_t*        cur_data_ptr = &data[bytes_parsed];
    RdmnetRptClientEntry* next_entry = NULL;

    if (!clstate->block.parsed_header)
    {
      if (remaining_len >= CLIENT_ENTRY_HEADER_SIZE)
      {
        if (GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(cur_data_ptr) != kClientProtocolRPT)
        {
          RDMNET_LOG_WARNING("Dropping invalid Client List - first entry was RPT, but also contains client protocol %d",
                             GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(cur_data_ptr));
          bytes_parsed += consume_bad_block(&clstate->block, data_len, &res);
          break;
        }

        next_entry = alloc_next_rpt_client_entry(clist);
        if (next_entry)
        {
          clstate->block.parsed_header = true;
          INIT_CLIENT_ENTRY_STATE(&clstate->entry, clstate->block.block_size);
        }
        else
        {
          // We've run out of space for RPT Client Entries - send back up what we have now
          clist->more_coming = true;
          res = kRCParseResPartialBlockParseOk;
        }
      }
      else
      {
        break;
      }
    }
    else
    {
      if (!RDMNET_ASSERT_VERIFY(clist->client_entries))
        return 0;

      next_entry = &clist->client_entries[clist->num_client_entries - 1];
    }

    if (clstate->block.parsed_header)
    {
      // We know the client protocol is correct because it's been validated above.
      client_protocol_t cp = kClientProtocolUnknown;
      size_t next_layer_bytes_parsed = parse_single_client_entry(&clstate->entry, cur_data_ptr, remaining_len, &cp,
                                                                 (ClientEntryUnion*)next_entry, &res);

      // Check and advance the buffer pointers
      if (!RDMNET_ASSERT_VERIFY(next_layer_bytes_parsed <= remaining_len) ||
          !RDMNET_ASSERT_VERIFY(clstate->block.size_parsed + next_layer_bytes_parsed <= clstate->block.block_size))
      {
        return 0;
      }

      bytes_parsed += next_layer_bytes_parsed;
      clstate->block.size_parsed += next_layer_bytes_parsed;

      // Determine what to do next in the list loop
      if (res == kRCParseResFullBlockParseOk)
      {
        clstate->block.parsed_header = false;
        if (clstate->block.size_parsed != clstate->block.block_size)
        {
          // This isn't the last entry in the list
          res = kRCParseResNoData;
        }
        // Iterate again
      }
      else if (res == kRCParseResFullBlockProtErr)
      {
        // Bail on the list
        clstate->block.parsed_header = false;
        bytes_parsed += consume_bad_block(&clstate->block, remaining_len - next_layer_bytes_parsed, &res);
        break;
      }
      else
      {
        // Couldn't parse a complete entry, wait for next time
        break;
      }
    }
  }

  *result = res;
  return bytes_parsed;
}

RdmnetRptClientEntry* alloc_next_rpt_client_entry(RdmnetRptClientList* clist)
{
  if (!RDMNET_ASSERT_VERIFY(clist))
    return NULL;

  if (clist->client_entries)
  {
    RdmnetRptClientEntry* new_arr = REALLOC_RPT_CLIENT_ENTRY(clist->client_entries, clist->num_client_entries + 1);
    if (new_arr)
    {
      clist->client_entries = new_arr;
      return &clist->client_entries[clist->num_client_entries++];
    }
    else
    {
      return NULL;
    }
  }
  else
  {
    clist->client_entries = ALLOC_RPT_CLIENT_ENTRY();
    if (clist->client_entries)
      clist->num_client_entries = 1;
    return clist->client_entries;
  }
}

#if 0
RdmnetEptClientEntry* alloc_next_ept_client_entry(RdmnetEptClientList* clist)
{
  if (!RDMNET_ASSERT_VERIFY(clist))
    return NULL;

  if (clist->client_entries)
  {
    RdmnetEptClientEntry* new_arr = REALLOC_EPT_CLIENT_ENTRY(clist->client_entries, clist->num_client_entries + 1);
    if (new_arr)
    {
      clist->client_entries = new_arr;
      return &clist->client_entries[clist->num_client_entries++];
    }
    else
    {
      return NULL;
    }
  }
  else
  {
    clist->client_entries = ALLOC_EPT_CLIENT_ENTRY();
    if (clist->client_entries)
      clist->num_client_entries = 1;
    return clist->client_entries;
  }
}
#endif

size_t parse_request_dynamic_uid_assignment(GenericListState*            lstate,
                                            const uint8_t*               data,
                                            size_t                       data_len,
                                            BrokerDynamicUidRequestList* rlist,
                                            rc_parse_result_t*           result)
{
  ETCPAL_UNUSED_ARG(rdmnet_log_params);

  if (!RDMNET_ASSERT_VERIFY(lstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(rlist) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  size_t            bytes_parsed = 0;
  rc_parse_result_t res = kRCParseResNoData;

  while (data_len - bytes_parsed >= DYNAMIC_UID_REQUEST_PAIR_SIZE)
  {
    // We are starting at the beginning of a new Request Dynamic UID Assignment PDU.
    // Make room for a new struct at the end of the current array.
    if (rlist->requests)
    {
      BrokerDynamicUidRequest* new_arr = REALLOC_DYNAMIC_UID_REQUEST_ENTRY(rlist->requests, rlist->num_requests + 1);
      if (new_arr)
      {
        rlist->requests = new_arr;
      }
      else
      {
        // We've run out of space for Dynamic UID Requests - send back up what we have now
        rlist->more_coming = true;
        res = kRCParseResPartialBlockParseOk;
        break;
      }
    }
    else
    {
      rlist->requests = ALLOC_DYNAMIC_UID_REQUEST_ENTRY();
      if (!rlist->requests)
      {
        res = kRCParseResNoData;
        break;
      }
    }

    // Gotten here - parse a new BrokerDynamicUidRequest
    BrokerDynamicUidRequest* request = &rlist->requests[rlist->num_requests++];
    request->manu_id = etcpal_unpack_u16b(&data[bytes_parsed]) & 0x7fff;
    memcpy(request->rid.data, &data[bytes_parsed + 6], ETCPAL_UUID_BYTES);
    bytes_parsed += DYNAMIC_UID_REQUEST_PAIR_SIZE;
    lstate->size_parsed += DYNAMIC_UID_REQUEST_PAIR_SIZE;

    if (lstate->size_parsed >= lstate->full_list_size)
    {
      res = kRCParseResFullBlockParseOk;
      break;
    }
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_dynamic_uid_assignment_list(GenericListState*               lstate,
                                         const uint8_t*                  data,
                                         size_t                          data_len,
                                         RdmnetDynamicUidAssignmentList* alist,
                                         rc_parse_result_t*              result)
{
  ETCPAL_UNUSED_ARG(rdmnet_log_params);

  if (!RDMNET_ASSERT_VERIFY(lstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(alist) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  size_t            bytes_parsed = 0;
  rc_parse_result_t res = kRCParseResNoData;

  while (data_len - bytes_parsed >= DYNAMIC_UID_MAPPING_SIZE)
  {
    // We are starting at the beginning of a new Dynamic UID Assignment PDU.
    // Make room for a new struct at the end of the current array.
    if (alist->mappings)
    {
      RdmnetDynamicUidMapping* new_arr = REALLOC_DYNAMIC_UID_MAPPING(alist->mappings, alist->num_mappings + 1);
      if (new_arr)
      {
        alist->mappings = new_arr;
      }
      else
      {
        // We've run out of space for Dynamic UID Mappings - send back up what we have now
        alist->more_coming = true;
        res = kRCParseResPartialBlockParseOk;
        break;
      }
    }
    else
    {
      alist->mappings = ALLOC_DYNAMIC_UID_MAPPING();
      if (!alist->mappings)
      {
        res = kRCParseResNoData;
        break;
      }
    }

    // Gotten here - parse a new RdmnetDynamicUidMapping
    RdmnetDynamicUidMapping* mapping = &alist->mappings[alist->num_mappings++];
    const uint8_t*           cur_ptr = &data[bytes_parsed];

    mapping->uid.manu = etcpal_unpack_u16b(cur_ptr);
    cur_ptr += 2;
    mapping->uid.id = etcpal_unpack_u32b(cur_ptr);
    cur_ptr += 4;
    memcpy(mapping->rid.data, cur_ptr, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
    mapping->status_code = (rdmnet_dynamic_uid_status_t)etcpal_unpack_u16b(cur_ptr);
    cur_ptr += 2;
    bytes_parsed += DYNAMIC_UID_MAPPING_SIZE;
    lstate->size_parsed += DYNAMIC_UID_MAPPING_SIZE;

    if (lstate->size_parsed >= lstate->full_list_size)
    {
      res = kRCParseResFullBlockParseOk;
      break;
    }
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_fetch_dynamic_uid_assignment_list(GenericListState*             lstate,
                                               const uint8_t*                data,
                                               size_t                        data_len,
                                               BrokerFetchUidAssignmentList* alist,
                                               rc_parse_result_t*            result)
{
  if (!RDMNET_ASSERT_VERIFY(lstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(alist) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  size_t            bytes_parsed = 0;
  rc_parse_result_t res = kRCParseResNoData;

  ETCPAL_UNUSED_ARG(rdmnet_log_params);

  while (data_len - bytes_parsed >= 6)
  {
    // We are starting at the beginning of a new Fetch Dynamic UID Assignment PDU.
    // Make room for a new struct at the end of the current array.
    if (alist->uids)
    {
      RdmUid* new_arr = REALLOC_FETCH_UID_ASSIGNMENT(alist->uids, alist->num_uids + 1);
      if (new_arr)
      {
        alist->uids = new_arr;
      }
      else
      {
        // We've run out of space for Fetch UID Assignments - send back up what we have now
        alist->more_coming = true;
        res = kRCParseResPartialBlockParseOk;
        break;
      }
    }
    else
    {
      alist->uids = ALLOC_FETCH_UID_ASSIGNMENT();
      if (!alist->uids)
      {
        res = kRCParseResNoData;
        break;
      }
    }

    // Gotten here - parse a new UID
    RdmUid* uid_entry = &alist->uids[alist->num_uids++];
    uid_entry->manu = etcpal_unpack_u16b(&data[bytes_parsed]);
    uid_entry->id = etcpal_unpack_u32b(&data[bytes_parsed + 2]);
    bytes_parsed += 6;
    lstate->size_parsed += 6;

    if (lstate->size_parsed >= lstate->full_list_size)
    {
      res = kRCParseResFullBlockParseOk;
      break;
    }
  }

  *result = res;
  return bytes_parsed;
}

void initialize_rpt_message(RptState* rstate, RptMessage* rmsg, size_t pdu_data_len)
{
  if (!RDMNET_ASSERT_VERIFY(rstate) || !RDMNET_ASSERT_VERIFY(rmsg))
    return;

  switch (rmsg->vector)
  {
    case VECTOR_RPT_REQUEST:
    case VECTOR_RPT_NOTIFICATION:
      if (pdu_data_len >= REQUEST_NOTIF_PDU_HEADER_SIZE)
      {
        INIT_RDM_LIST_STATE(&rstate->data.rdm_list, pdu_data_len, rmsg);
      }
      else
      {
        INIT_PDU_BLOCK_STATE(&rstate->data.unknown, pdu_data_len);
        // An artificial "unknown" vector value to flag the data parsing logic to consume the data
        // section.
        rmsg->vector = 0xffffffff;
        RDMNET_LOG_WARNING("Dropping RPT PDU with invalid length %zu", pdu_data_len + RPT_PDU_HEADER_SIZE);
      }
      break;
    case VECTOR_RPT_STATUS:
      if (pdu_data_len >= RPT_STATUS_HEADER_SIZE)
      {
        INIT_RPT_STATUS_STATE(&rstate->data.status, pdu_data_len);
      }
      else
      {
        INIT_PDU_BLOCK_STATE(&rstate->data.unknown, pdu_data_len);
        // An artificial "unknown" vector value to flag the data parsing logic to consume the data
        // section.
        rmsg->vector = 0xffffffff;
        RDMNET_LOG_WARNING("Dropping RPT PDU with invalid length %zu", pdu_data_len + RPT_PDU_HEADER_SIZE);
      }
      break;
    default:
      INIT_PDU_BLOCK_STATE(&rstate->data.unknown, pdu_data_len);
      RDMNET_LOG_WARNING("Dropping RPT PDU with invalid vector %" PRIu32, rmsg->vector);
      break;
  }
}

size_t parse_rpt_block(RptState*          rstate,
                       const uint8_t*     data,
                       size_t             data_len,
                       RptMessage*        rmsg,
                       rc_parse_result_t* result)
{
  if (!RDMNET_ASSERT_VERIFY(rstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(rmsg) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  size_t            bytes_parsed = 0;
  rc_parse_result_t res = kRCParseResNoData;

  if (rstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&rstate->block, data_len, &res);
  }
  else if (!rstate->block.parsed_header)
  {
    bool parse_err = false;

    // If the size remaining in the RPT PDU block is not enough for another RPT PDU header, indicate
    // a bad block condition.
    if ((rstate->block.block_size - rstate->block.size_parsed) < RPT_PDU_HEADER_SIZE)
    {
      parse_err = true;
    }
    else if (data_len >= RPT_PDU_HEADER_SIZE)
    {
      // We can parse an RPT PDU header.
      const uint8_t* cur_ptr = data;
      size_t         pdu_len = ACN_PDU_LENGTH(cur_ptr);
      if (pdu_len >= RPT_PDU_HEADER_SIZE && rstate->block.size_parsed + pdu_len <= rstate->block.block_size)
      {
        size_t pdu_data_len = pdu_len - RPT_PDU_HEADER_SIZE;
        cur_ptr += 3;
        rmsg->vector = etcpal_unpack_u32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.source_uid.manu = etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.source_uid.id = etcpal_unpack_u32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.source_endpoint_id = etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.dest_uid.manu = etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.dest_uid.id = etcpal_unpack_u32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.dest_endpoint_id = etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.seqnum = etcpal_unpack_u32b(cur_ptr);
        cur_ptr += 4;
        ++cur_ptr;  // 1-byte reserved field

        bytes_parsed += RPT_PDU_HEADER_SIZE;
        rstate->block.size_parsed += RPT_PDU_HEADER_SIZE;
        initialize_rpt_message(rstate, rmsg, pdu_data_len);
        rstate->block.parsed_header = true;
      }
      else
      {
        parse_err = true;
      }
    }
    // Else we don't have enough data - return kRCParseResNoData by default.

    if (parse_err)
    {
      bytes_parsed += consume_bad_block(&rstate->block, data_len, &res);
      RDMNET_LOG_WARNING("Protocol error encountered while parsing RPT PDU header.");
    }
  }
  if (rstate->block.parsed_header)
  {
    size_t next_layer_bytes_parsed;
    size_t remaining_len = data_len - bytes_parsed;
    switch (rmsg->vector)
    {
      case VECTOR_RPT_REQUEST:
      case VECTOR_RPT_NOTIFICATION: {
        RptRdmBufList* cmd_list = RPT_GET_RDM_BUF_LIST(rmsg);
        if (!RDMNET_ASSERT_VERIFY(cmd_list))
          return 0;

        next_layer_bytes_parsed =
            parse_rdm_list(&rstate->data.rdm_list, &data[bytes_parsed], remaining_len, cmd_list, &res);
      }
      break;
      case VECTOR_RPT_STATUS: {
        RptStatusMsg* smsg = RPT_GET_STATUS_MSG(rmsg);
        if (!RDMNET_ASSERT_VERIFY(smsg))
          return 0;

        next_layer_bytes_parsed =
            parse_rpt_status(&rstate->data.status, &data[bytes_parsed], remaining_len, smsg, &res);
      }
      break;
      default:
        // Unknown RPT vector - discard this RPT PDU.
        next_layer_bytes_parsed = consume_bad_block(&rstate->data.unknown, remaining_len, &res);
    }

    if (!RDMNET_ASSERT_VERIFY(next_layer_bytes_parsed <= remaining_len) ||
        !RDMNET_ASSERT_VERIFY(rstate->block.size_parsed + next_layer_bytes_parsed <= rstate->block.block_size))
    {
      return 0;
    }

    rstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &rstate->block);
  }
  *result = res;
  return bytes_parsed;
}

size_t parse_rdm_list(RdmListState*      rlstate,
                      const uint8_t*     data,
                      size_t             data_len,
                      RptRdmBufList*     cmd_list,
                      rc_parse_result_t* result)
{
  if (!RDMNET_ASSERT_VERIFY(rlstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(cmd_list) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  rc_parse_result_t res = kRCParseResNoData;
  size_t            bytes_parsed = 0;

  if (!rlstate->parsed_request_notif_header && data_len >= REQUEST_NOTIF_PDU_HEADER_SIZE)
  {
    const uint8_t* cur_ptr = data;
    size_t         pdu_len = ACN_PDU_LENGTH(cur_ptr);
    uint32_t       vect;

    cur_ptr += 3;
    vect = etcpal_unpack_u32b(cur_ptr);
    cur_ptr += 4;
    if (pdu_len != rlstate->block.block_size || (vect != VECTOR_REQUEST_RDM_CMD && vect != VECTOR_NOTIFICATION_RDM_CMD))
    {
      bytes_parsed += consume_bad_block(&rlstate->block, data_len, &res);
    }
    else
    {
      rlstate->parsed_request_notif_header = true;
      rlstate->block.block_size -= REQUEST_NOTIF_PDU_HEADER_SIZE;
      bytes_parsed += REQUEST_NOTIF_PDU_HEADER_SIZE;
    }
  }
  if (rlstate->parsed_request_notif_header)
  {
    if (rlstate->block.consuming_bad_block)
    {
      bytes_parsed += consume_bad_block(&rlstate->block, data_len - bytes_parsed, &res);
    }
    else
    {
      while (rlstate->block.size_parsed < rlstate->block.block_size)
      {
        size_t remaining_len = data_len - bytes_parsed;

        // We want to parse an entire RDM Command PDU at once.
        if (remaining_len >= RDM_CMD_PDU_MIN_SIZE)
        {
          const uint8_t* cur_ptr = &data[bytes_parsed];
          size_t         rdm_cmd_pdu_len = ACN_PDU_LENGTH(cur_ptr);

          if (rdm_cmd_pdu_len > rlstate->block.block_size || rdm_cmd_pdu_len > RDM_CMD_PDU_MAX_SIZE)
          {
            bytes_parsed += consume_bad_block(&rlstate->block, remaining_len, &res);
          }
          else if (remaining_len >= rdm_cmd_pdu_len)
          {
            // We are starting at the beginning of a new RDM Command PDU.
            // Make room for a new struct at the end of the current array.
            if (cmd_list->rdm_buffers)
            {
              RdmBuffer* new_arr = REALLOC_RDM_BUFFER(cmd_list->rdm_buffers, cmd_list->num_rdm_buffers + 1);
              if (new_arr)
              {
                cmd_list->rdm_buffers = new_arr;
              }
              else
              {
                // We've run out of space for RDM buffers - send back up what we have now
                cmd_list->more_coming = true;
                res = kRCParseResPartialBlockParseOk;
                break;
              }
            }
            else
            {
              cmd_list->rdm_buffers = ALLOC_RDM_BUFFER();
              if (!cmd_list->rdm_buffers)
              {
                res = kRCParseResNoData;
                break;
              }
            }

            // Gotten here - unpack the RDM command PDU
            RdmBuffer* rdm_buf = &cmd_list->rdm_buffers[cmd_list->num_rdm_buffers++];
            cur_ptr += 3;
            memcpy(rdm_buf->data, cur_ptr, rdm_cmd_pdu_len - 3);
            rdm_buf->data_len = rdm_cmd_pdu_len - 3;
            bytes_parsed += rdm_cmd_pdu_len;
            rlstate->block.size_parsed += rdm_cmd_pdu_len;
            if (rlstate->block.size_parsed >= rlstate->block.block_size)
              res = kRCParseResFullBlockParseOk;
          }
          else
          {
            break;
          }
        }
        else
        {
          break;
        }
      }
    }
  }
  *result = res;
  return bytes_parsed;
}

size_t parse_rpt_status(RptStatusState*    rsstate,
                        const uint8_t*     data,
                        size_t             data_len,
                        RptStatusMsg*      smsg,
                        rc_parse_result_t* result)
{
  if (!RDMNET_ASSERT_VERIFY(rsstate) || !RDMNET_ASSERT_VERIFY(data) || !RDMNET_ASSERT_VERIFY(smsg) ||
      !RDMNET_ASSERT_VERIFY(result))
  {
    return 0;
  }

  rc_parse_result_t res = kRCParseResNoData;
  size_t            bytes_parsed = 0;

  if (rsstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&rsstate->block, data_len, &res);
  }
  else if (!rsstate->block.parsed_header)
  {
    bool parse_err = false;

    // If the size remaining in the Broker PDU block is not enough for another RPT Status PDU
    // header, indicate a bad block condition.
    if ((rsstate->block.block_size - rsstate->block.size_parsed) < RPT_STATUS_HEADER_SIZE)
    {
      parse_err = true;
    }
    else if (data_len >= RPT_STATUS_HEADER_SIZE)
    {
      // We can parse an RPT Status PDU header.
      const uint8_t* cur_ptr = data;

      size_t pdu_len = ACN_PDU_LENGTH(cur_ptr);
      if (pdu_len >= RPT_STATUS_HEADER_SIZE && pdu_len >= rsstate->block.block_size)
      {
        cur_ptr += 3;
        smsg->status_code = (rpt_status_code_t)etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;
        bytes_parsed += RPT_STATUS_HEADER_SIZE;
        rsstate->block.size_parsed += RPT_STATUS_HEADER_SIZE;
        rsstate->block.parsed_header = true;
      }
      else
      {
        parse_err = true;
      }
    }
    // Else we don't have enough data - return kRCParseResNoData by default.

    if (parse_err)
    {
      // Parse error in the RPT Status PDU header. We cannot keep parsing this block.
      bytes_parsed += consume_bad_block(&rsstate->block, data_len, &res);
      RDMNET_LOG_WARNING("Protocol error encountered while parsing RPT Status PDU header.");
    }
  }
  if (rsstate->block.parsed_header)
  {
    size_t remaining_len = data_len - bytes_parsed;
    switch (smsg->status_code)
    {
      case VECTOR_RPT_STATUS_INVALID_MESSAGE:
      case VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS:
        // These status codes have no additional data.
        if (rsstate->block.size_parsed == rsstate->block.block_size)
        {
          smsg->status_string = NULL;
          res = kRCParseResFullBlockParseOk;
        }
        else
        {
          bytes_parsed += consume_bad_block(&rsstate->block, remaining_len, &res);
        }
        break;
      case VECTOR_RPT_STATUS_UNKNOWN_RPT_UID:
      case VECTOR_RPT_STATUS_RDM_TIMEOUT:
      case VECTOR_RPT_STATUS_RDM_INVALID_RESPONSE:
      case VECTOR_RPT_STATUS_UNKNOWN_RDM_UID:
      case VECTOR_RPT_STATUS_UNKNOWN_ENDPOINT:
      case VECTOR_RPT_STATUS_BROADCAST_COMPLETE:
      case VECTOR_RPT_STATUS_UNKNOWN_VECTOR: {
        size_t str_len = rsstate->block.block_size - rsstate->block.size_parsed;

        // These status codes contain an optional status string
        if (str_len == 0)
        {
          smsg->status_string = NULL;
          res = kRCParseResFullBlockParseOk;
        }
        else if (str_len > RPT_STATUS_STRING_MAXLEN)
        {
          bytes_parsed += consume_bad_block(&rsstate->block, remaining_len, &res);
        }
        else if (remaining_len >= str_len)
        {
          char* str_buf = ALLOC_RPT_STATUS_STR(str_len + 1);
          if (str_buf)
          {
            memcpy(str_buf, &data[bytes_parsed], str_len);
            str_buf[str_len] = '\0';
            smsg->status_string = str_buf;
          }
          else
          {
            smsg->status_string = NULL;
          }
          bytes_parsed += str_len;
          rsstate->block.size_parsed += str_len;
          res = kRCParseResFullBlockParseOk;
        }
        // Else return no data
        break;
      }
      default:
        // Unknown RPT Status code - discard this RPT Status PDU.
        bytes_parsed += consume_bad_block(&rsstate->block, remaining_len, &res);
        break;
    }
  }
  *result = res;
  return bytes_parsed;
}

size_t locate_tcp_preamble(RCMsgBuf* msg_buf)
{
  if (!RDMNET_ASSERT_VERIFY(msg_buf))
    return 0;

  if (msg_buf->cur_data_size < ACN_TCP_PREAMBLE_SIZE)
    return 0;

  size_t i = 0;
  for (; i < (msg_buf->cur_data_size - ACN_TCP_PREAMBLE_SIZE); ++i)
  {
    AcnTcpPreamble preamble;
    if (acn_parse_tcp_preamble(&msg_buf->buf[i], msg_buf->cur_data_size - i, &preamble))
    {
      // Discard the data before and including the TCP preamble.
      if (msg_buf->cur_data_size > i + ACN_TCP_PREAMBLE_SIZE)
      {
        memmove(msg_buf->buf, &msg_buf->buf[i + ACN_TCP_PREAMBLE_SIZE],
                msg_buf->cur_data_size - (i + ACN_TCP_PREAMBLE_SIZE));
      }
      msg_buf->cur_data_size -= (i + ACN_TCP_PREAMBLE_SIZE);
      return preamble.rlp_block_len;
    }
  }
  if (i > 0)
  {
    // Discard data from the range that has been determined definitively to not contain a TCP
    // preamble.
    memmove(msg_buf->buf, &msg_buf->buf[i], msg_buf->cur_data_size - i);
    msg_buf->cur_data_size -= i;
  }
  return 0;
}

size_t consume_bad_block(PduBlockState* block, size_t data_len, rc_parse_result_t* parse_res)
{
  if (!RDMNET_ASSERT_VERIFY(block) || !RDMNET_ASSERT_VERIFY(parse_res))
    return 0;

  size_t size_remaining = block->block_size - block->size_parsed;
  if (data_len >= size_remaining)
  {
    *parse_res = kRCParseResFullBlockProtErr;
    block->size_parsed = block->block_size;
    return size_remaining;
  }
  else
  {
    *parse_res = kRCParseResNoData;
    block->size_parsed += data_len;
    block->consuming_bad_block = true;
    return data_len;
  }
}

rc_parse_result_t check_for_full_parse(rc_parse_result_t prev_res, PduBlockState* block)
{
  if (!RDMNET_ASSERT_VERIFY(block))
    return prev_res;

  rc_parse_result_t res = prev_res;

  switch (prev_res)
  {
    case kRCParseResFullBlockParseOk:
    case kRCParseResFullBlockProtErr:
      // If we're not through the PDU block, need to indicate that to the higher layer.
      if (block->size_parsed < block->block_size)
      {
        res =
            (prev_res == kRCParseResFullBlockProtErr) ? kRCParseResPartialBlockProtErr : kRCParseResPartialBlockParseOk;
      }
      block->parsed_header = false;
      break;
    case kRCParseResPartialBlockParseOk:
    case kRCParseResPartialBlockProtErr:
    case kRCParseResNoData:
    default:
      break;
  }
  return res;
}
