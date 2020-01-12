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

#include "rdmnet/private/msg_buf.h"

#include <assert.h>
#include <inttypes.h>
#include "etcpal/pack.h"
#include "etcpal/root_layer_pdu.h"
#include "rdmnet/core.h"
#include "rdmnet/private/broker_prot.h"
#include "rdmnet/private/opts.h"
#include "rdmnet/private/rpt_prot.h"
#include "rdmnet/private/message.h"

/*********************** Private function prototypes *************************/

static etcpal_error_t run_parse_state_machine(RdmnetMsgBuf* msg_buf);
static size_t locate_tcp_preamble(RdmnetMsgBuf* msg_buf);
static size_t consume_bad_block(PduBlockState* block, size_t datalen, parse_result_t* parse_res);
static parse_result_t check_for_full_parse(parse_result_t prev_res, PduBlockState* block);

// The parse functions are organized by protocol layer, and each one gets a subset of the overall
// state structure.

// Root layer
static size_t parse_rlp_block(RlpState* rlpstate, const uint8_t* data, size_t data_size, RdmnetMessage* msg,
                              parse_result_t* result);

// RDMnet layer
static void initialize_rdmnet_message(RlpState* rlpstate, RdmnetMessage* msg, size_t pdu_data_len);
static size_t parse_broker_block(BrokerState* bstate, const uint8_t* data, size_t datalen, BrokerMessage* bmsg,
                                 parse_result_t* result);
static size_t parse_rpt_block(RptState* rstate, const uint8_t* data, size_t datalen, RptMessage* rmsg,
                              parse_result_t* result);

// RPT layer
static void initialize_rpt_message(RptState* rstate, RptMessage* rmsg, size_t pdu_data_len);
static size_t parse_rdm_list(RdmListState* rlstate, const uint8_t* data, size_t datalen, RdmBufList* cmd_list,
                             parse_result_t* result);
static size_t parse_rpt_status(RptStatusState* rsstate, const uint8_t* data, size_t datalen, RptStatusMsg* smsg,
                               parse_result_t* result);

// Broker layer
static void initialize_broker_message(BrokerState* bstate, BrokerMessage* bmsg, size_t pdu_data_len);
static void parse_client_connect_header(const uint8_t* data, ClientConnectMsg* ccmsg);
static size_t parse_client_connect(ClientConnectState* ccstate, const uint8_t* data, size_t datalen,
                                   ClientConnectMsg* ccmsg, parse_result_t* result);
static size_t parse_client_entry_update(ClientEntryUpdateState* ceustate, const uint8_t* data, size_t datalen,
                                        ClientEntryUpdateMsg* ceumsg, parse_result_t* result);
static size_t parse_single_client_entry(ClientEntryState* cstate, const uint8_t* data, size_t datalen,
                                        client_protocol_t* client_protocol, ClientEntryUnion* entry,
                                        parse_result_t* result);
static size_t parse_client_list(ClientListState* clstate, const uint8_t* data, size_t datalen, ClientList* clist,
                                parse_result_t* result);
static size_t parse_request_dynamic_uid_assignment(GenericListState* lstate, const uint8_t* data, size_t datalen,
                                                   DynamicUidRequestList* rlist, parse_result_t* result);
static size_t parse_dynamic_uid_assignment_list(GenericListState* lstate, const uint8_t* data, size_t datalen,
                                                DynamicUidAssignmentList* alist, parse_result_t* result);
static size_t parse_fetch_dynamic_uid_assignment_list(GenericListState* lstate, const uint8_t* data, size_t datalen,
                                                      FetchUidAssignmentList* alist, parse_result_t* result);

// Helpers for parsing client list messages
static size_t parse_rpt_client_list(ClientListState* clstate, const uint8_t* data, size_t datalen, RptClientList* clist,
                                    parse_result_t* result);
static RptClientEntry* alloc_next_rpt_client_entry(RptClientList* clist);
static EptClientEntry* alloc_next_ept_client_entry(EptClientList* clist);

/*************************** Function definitions ****************************/

void rdmnet_msg_buf_init(RdmnetMsgBuf* msg_buf)
{
  if (msg_buf)
  {
    msg_buf->cur_data_size = 0;
    msg_buf->have_preamble = false;
  }
}

etcpal_error_t rdmnet_msg_buf_recv(RdmnetMsgBuf* msg_buf, const uint8_t* data, size_t data_size)
{
  if (data && data_size)
  {
    if (msg_buf->cur_data_size)
    {
      RDMNET_ASSERT(msg_buf->cur_data_size + data_size < RDMNET_RECV_DATA_MAX_SIZE * 2);
    }
    memcpy(&msg_buf->buf[msg_buf->cur_data_size], data, data_size);
    msg_buf->cur_data_size += data_size;
  }
  return run_parse_state_machine(msg_buf);
}

etcpal_error_t run_parse_state_machine(RdmnetMsgBuf* msg_buf)
{
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
      parse_result_t parse_res;
      consumed = parse_rlp_block(&msg_buf->rlp_state, msg_buf->buf, msg_buf->cur_data_size, &msg_buf->msg, &parse_res);
      switch (parse_res)
      {
        case kPSFullBlockParseOk:
        case kPSFullBlockProtErr:
          msg_buf->have_preamble = false;
          res = (parse_res == kPSFullBlockProtErr ? kEtcPalErrProtocol : kEtcPalErrOk);
          break;
        case kPSPartialBlockParseOk:
        case kPSPartialBlockProtErr:
          res = (parse_res == kPSPartialBlockProtErr ? kEtcPalErrProtocol : kEtcPalErrOk);
          break;
        case kPSNoData:
        default:
          res = kEtcPalErrNoData;
          break;
      }
    }

    if (consumed > 0)
    {
      // Roll the buffer to discard the data we have already parsed.
      RDMNET_ASSERT(msg_buf->cur_data_size >= consumed);
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

size_t parse_rlp_block(RlpState* rlpstate, const uint8_t* data, size_t datalen, RdmnetMessage* msg,
                       parse_result_t* result)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (rlpstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&rlpstate->block, datalen, &res);
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
    else if (datalen >= ACN_RLP_HEADER_SIZE_EXT_LEN)
    {
      EtcPalRootLayerPdu rlp;

      // Inheritance at the root layer is disallowed by E1.33.
      if (etcpal_parse_root_layer_header(data, datalen, &rlp, NULL))
      {
        // Update the data pointers and sizes.
        bytes_parsed += ACN_RLP_HEADER_SIZE_EXT_LEN;
        rlpstate->block.size_parsed += ACN_RLP_HEADER_SIZE_EXT_LEN;

        // If this PDU indicates a length that takes it past the end of the block size from the
        // preamble, it is an error.
        if (rlpstate->block.size_parsed + rlp.datalen <= rlpstate->block.block_size)
        {
          // Fill in the root layer data in the overall RdmnetMessage struct.
          msg->vector = rlp.vector;
          msg->sender_cid = rlp.sender_cid;
          rlpstate->block.parsed_header = true;
          initialize_rdmnet_message(rlpstate, msg, rlp.datalen);
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
      bytes_parsed += consume_bad_block(&rlpstate->block, datalen, &res);
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
                                                     datalen - bytes_parsed, GET_BROKER_MSG(msg), &res);
        break;
      case ACN_VECTOR_ROOT_RPT:
        next_layer_bytes_parsed =
            parse_rpt_block(&rlpstate->data.rpt, &data[bytes_parsed], datalen - bytes_parsed, GET_RPT_MSG(msg), &res);
        break;
      default:
        next_layer_bytes_parsed = consume_bad_block(&rlpstate->data.unknown, datalen - bytes_parsed, &res);
        break;
    }
    RDMNET_ASSERT(next_layer_bytes_parsed <= (datalen - bytes_parsed));
    RDMNET_ASSERT(rlpstate->block.size_parsed + next_layer_bytes_parsed <= rlpstate->block.block_size);
    rlpstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &rlpstate->block);
  }
  *result = res;
  return bytes_parsed;
}

void initialize_broker_message(BrokerState* bstate, BrokerMessage* bmsg, size_t pdu_data_len)
{
  bool bad_length = false;

  switch (bmsg->vector)
  {
    case VECTOR_BROKER_CONNECT:
      if (pdu_data_len >= CLIENT_CONNECT_DATA_MIN_SIZE)
        INIT_CLIENT_CONNECT_STATE(&bstate->data.client_connect, pdu_data_len, bmsg);
      else
        bad_length = true;
      break;
    case VECTOR_BROKER_CONNECT_REPLY:
      if (pdu_data_len != CONNECT_REPLY_DATA_SIZE)
        bad_length = true;
      break;
    case VECTOR_BROKER_CLIENT_ENTRY_UPDATE:
      if (pdu_data_len >= CLIENT_ENTRY_UPDATE_DATA_MIN_SIZE)
        INIT_CLIENT_ENTRY_UPDATE_STATE(&bstate->data.update, pdu_data_len, bmsg);
      else
        bad_length = true;
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
        DynamicUidRequestList* rlist = GET_DYNAMIC_UID_REQUEST_LIST(bmsg);
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
        DynamicUidAssignmentList* alist = GET_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg);
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
        FetchUidAssignmentList* ulist = GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg);
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

size_t parse_broker_block(BrokerState* bstate, const uint8_t* data, size_t datalen, BrokerMessage* bmsg,
                          parse_result_t* result)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (bstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&bstate->block, datalen, &res);
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
    else if (datalen >= BROKER_PDU_HEADER_SIZE)
    {
      // We can parse a Broker PDU header.
      const uint8_t* cur_ptr = data;

      size_t pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
      if (pdu_len >= BROKER_PDU_HEADER_SIZE && bstate->block.size_parsed + pdu_len <= bstate->block.block_size)
      {
        size_t pdu_data_len = pdu_len - BROKER_PDU_HEADER_SIZE;

        cur_ptr += 3;
        bmsg->vector = etcpal_upack_16b(cur_ptr);
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
    // Else we don't have enough data - return kPSNoData by default.

    if (parse_err)
    {
      // Parse error in the Broker PDU header. We cannot keep parsing this block.
      bytes_parsed += consume_bad_block(&bstate->block, datalen, &res);
      RDMNET_LOG_WARNING("Protocol error encountered while parsing Broker PDU header.");
    }
  }
  if (bstate->block.parsed_header)
  {
    size_t next_layer_bytes_parsed = 0;
    size_t remaining_len = datalen - bytes_parsed;
    switch (bmsg->vector)
    {
      case VECTOR_BROKER_CONNECT:
        next_layer_bytes_parsed = parse_client_connect(&bstate->data.client_connect, &data[bytes_parsed], remaining_len,
                                                       GET_CLIENT_CONNECT_MSG(bmsg), &res);
        break;
      case VECTOR_BROKER_CONNECT_REPLY:
        if (remaining_len >= CONNECT_REPLY_DATA_SIZE)
        {
          ConnectReplyMsg* crmsg = GET_CONNECT_REPLY_MSG(bmsg);
          const uint8_t* cur_ptr = &data[bytes_parsed];
          crmsg->connect_status = (rdmnet_connect_status_t)etcpal_upack_16b(cur_ptr);
          cur_ptr += 2;
          crmsg->e133_version = etcpal_upack_16b(cur_ptr);
          cur_ptr += 2;
          crmsg->broker_uid.manu = etcpal_upack_16b(cur_ptr);
          cur_ptr += 2;
          crmsg->broker_uid.id = etcpal_upack_32b(cur_ptr);
          cur_ptr += 4;
          crmsg->client_uid.manu = etcpal_upack_16b(cur_ptr);
          cur_ptr += 2;
          crmsg->client_uid.id = etcpal_upack_32b(cur_ptr);
          cur_ptr += 4;
          next_layer_bytes_parsed = (size_t)(cur_ptr - &data[bytes_parsed]);
          res = kPSFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_CLIENT_ENTRY_UPDATE:
        next_layer_bytes_parsed = parse_client_entry_update(&bstate->data.update, &data[bytes_parsed], remaining_len,
                                                            GET_CLIENT_ENTRY_UPDATE_MSG(bmsg), &res);
        break;
      case VECTOR_BROKER_REDIRECT_V4:
        if (remaining_len >= REDIRECT_V4_DATA_SIZE)
        {
          ClientRedirectMsg* crmsg = GET_CLIENT_REDIRECT_MSG(bmsg);
          const uint8_t* cur_ptr = &data[bytes_parsed];
          ETCPAL_IP_SET_V4_ADDRESS(&crmsg->new_addr.ip, etcpal_upack_32b(cur_ptr));
          cur_ptr += 4;
          crmsg->new_addr.port = etcpal_upack_16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = (size_t)(cur_ptr - &data[bytes_parsed]);
          res = kPSFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_REDIRECT_V6:
        if (remaining_len >= REDIRECT_V6_DATA_SIZE)
        {
          ClientRedirectMsg* crmsg = GET_CLIENT_REDIRECT_MSG(bmsg);
          const uint8_t* cur_ptr = &data[bytes_parsed];
          ETCPAL_IP_SET_V6_ADDRESS(&crmsg->new_addr.ip, cur_ptr);
          cur_ptr += 16;
          crmsg->new_addr.port = etcpal_upack_16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = (size_t)(cur_ptr - &data[bytes_parsed]);
          res = kPSFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
      case VECTOR_BROKER_CLIENT_ADD:
      case VECTOR_BROKER_CLIENT_REMOVE:
      case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
        next_layer_bytes_parsed = parse_client_list(&bstate->data.client_list, &data[bytes_parsed], remaining_len,
                                                    GET_CLIENT_LIST(bmsg), &res);
        break;
      case VECTOR_BROKER_REQUEST_DYNAMIC_UIDS:
        next_layer_bytes_parsed = parse_request_dynamic_uid_assignment(
            &bstate->data.data_list, &data[bytes_parsed], remaining_len, GET_DYNAMIC_UID_REQUEST_LIST(bmsg), &res);
        break;
      case VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS:
        next_layer_bytes_parsed = parse_dynamic_uid_assignment_list(
            &bstate->data.data_list, &data[bytes_parsed], remaining_len, GET_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg), &res);
        break;
      case VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST:
        next_layer_bytes_parsed =
            parse_fetch_dynamic_uid_assignment_list(&bstate->data.data_list, &data[bytes_parsed], remaining_len,
                                                    GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(bmsg), &res);
        break;
      case VECTOR_BROKER_NULL:
      case VECTOR_BROKER_FETCH_CLIENT_LIST:
        // These messages have no data, so we are at the end of the PDU.
        res = kPSFullBlockParseOk;
        break;
      case VECTOR_BROKER_DISCONNECT:
        if (remaining_len >= DISCONNECT_DATA_SIZE)
        {
          const uint8_t* cur_ptr = &data[bytes_parsed];
          GET_DISCONNECT_MSG(bmsg)->disconnect_reason = (rdmnet_disconnect_reason_t)etcpal_upack_16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = (size_t)(cur_ptr - &data[bytes_parsed]);
          res = kPSFullBlockParseOk;
        }
        break;
      default:
        // Unknown Broker vector - discard this Broker PDU.
        next_layer_bytes_parsed = consume_bad_block(&bstate->data.unknown, remaining_len, &res);
        break;
    }
    RDMNET_ASSERT(next_layer_bytes_parsed <= remaining_len);
    RDMNET_ASSERT(bstate->block.size_parsed + next_layer_bytes_parsed <= bstate->block.block_size);
    bstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &bstate->block);
  }
  *result = res;
  return bytes_parsed;
}

void parse_client_connect_header(const uint8_t* data, ClientConnectMsg* ccmsg)
{
  const uint8_t* cur_ptr = data;

  CLIENT_CONNECT_MSG_SET_SCOPE(ccmsg, (const char*)cur_ptr);
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  ccmsg->e133_version = etcpal_upack_16b(cur_ptr);
  cur_ptr += 2;
  CLIENT_CONNECT_MSG_SET_SEARCH_DOMAIN(ccmsg, (const char*)cur_ptr);
  cur_ptr += E133_DOMAIN_STRING_PADDED_LENGTH;
  ccmsg->connect_flags = *cur_ptr;
}

size_t parse_client_connect(ClientConnectState* ccstate, const uint8_t* data, size_t datalen, ClientConnectMsg* ccmsg,
                            parse_result_t* result)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (!ccstate->common_data_parsed)
  {
    // We want to wait until we can parse all of the Client Connect common data at once.
    if (datalen < CLIENT_CONNECT_COMMON_FIELD_SIZE)
    {
      *result = kPSNoData;
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
        parse_single_client_entry(&ccstate->entry, &data[bytes_parsed], datalen - bytes_parsed,
                                  &ccmsg->client_entry.client_protocol, &ccmsg->client_entry.data, &res);
    RDMNET_ASSERT(next_layer_bytes_parsed <= (datalen - bytes_parsed));
    bytes_parsed += next_layer_bytes_parsed;
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_client_entry_update(ClientEntryUpdateState* ceustate, const uint8_t* data, size_t datalen,
                                 ClientEntryUpdateMsg* ceumsg, parse_result_t* result)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (!ceustate->common_data_parsed)
  {
    // We want to wait until we can parse all of the Client Entry Update common data at once.
    if (datalen < CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE)
    {
      *result = kPSNoData;
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
        parse_single_client_entry(&ceustate->entry, &data[bytes_parsed], datalen - bytes_parsed,
                                  &ceumsg->client_entry.client_protocol, &ceumsg->client_entry.data, &res);
    RDMNET_ASSERT(next_layer_bytes_parsed <= (datalen - bytes_parsed));
    bytes_parsed += next_layer_bytes_parsed;
  }

  *result = res;
  return bytes_parsed;
}

#define GET_LENGTH_FROM_CENTRY_HEADER(dataptr) ETCPAL_PDU_LENGTH(dataptr)
#define GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(dataptr) (client_protocol_t)(etcpal_upack_32b((dataptr) + 3))
#define COPY_CID_FROM_CENTRY_HEADER(dataptr, cid) memcpy((cid)->data, (dataptr) + 7, ETCPAL_UUID_BYTES)

size_t parse_single_client_entry(ClientEntryState* cstate, const uint8_t* data, size_t datalen,
                                 client_protocol_t* client_protocol, ClientEntryUnion* entry, parse_result_t* result)
{
  size_t bytes_parsed = 0;
  parse_result_t res = kPSNoData;

  if (cstate->client_protocol == kClientProtocolUnknown)
  {
    if (datalen >= CLIENT_ENTRY_HEADER_SIZE)
    {
      // Parse the Client Entry header
      size_t cli_entry_pdu_len = GET_LENGTH_FROM_CENTRY_HEADER(data);
      cstate->client_protocol = GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(data);
      bytes_parsed += CLIENT_ENTRY_HEADER_SIZE;
      INIT_PDU_BLOCK_STATE(&cstate->entry_data, cli_entry_pdu_len - CLIENT_ENTRY_HEADER_SIZE);
      if (cli_entry_pdu_len > cstate->enclosing_block_size)
      {
        bytes_parsed += consume_bad_block(&cstate->entry_data, datalen - bytes_parsed, &res);
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
    size_t remaining_len = datalen - bytes_parsed;
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
          RptClientEntry* rpt_entry = (RptClientEntry*)entry;
          const uint8_t* cur_ptr = &data[bytes_parsed];

          rpt_entry->uid.manu = etcpal_upack_16b(cur_ptr);
          cur_ptr += 2;
          rpt_entry->uid.id = etcpal_upack_32b(cur_ptr);
          cur_ptr += 4;
          rpt_entry->type = (rpt_client_type_t)*cur_ptr++;
          memcpy(rpt_entry->binding_cid.data, cur_ptr, ETCPAL_UUID_BYTES);
          bytes_parsed += RPT_CLIENT_ENTRY_DATA_SIZE;
          cstate->entry_data.size_parsed += RPT_CLIENT_ENTRY_DATA_SIZE;
          res = kPSFullBlockParseOk;
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

size_t parse_client_list(ClientListState* clstate, const uint8_t* data, size_t datalen, ClientList* clist,
                         parse_result_t* result)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (clstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&clstate->block, datalen, &res);
  }
  else
  {
    if (clist->client_protocol == kClientProtocolUnknown)
    {
      if (datalen >= CLIENT_ENTRY_HEADER_SIZE)
      {
        clist->client_protocol = GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(data);
      }
    }

    if (clist->client_protocol == kClientProtocolRPT)
    {
      bytes_parsed += parse_rpt_client_list(clstate, data, datalen, GET_RPT_CLIENT_LIST(clist), &res);
    }
    else if (clist->client_protocol == kClientProtocolEPT)
    {
      // TODO EPT
      bytes_parsed += consume_bad_block(&clstate->block, datalen, &res);
    }
    else if (clist->client_protocol != kClientProtocolUnknown)
    {
      RDMNET_LOG_WARNING("Dropping Client List message with unknown Client Protocol %d", clist->client_protocol);
      bytes_parsed += consume_bad_block(&clstate->block, datalen, &res);
    }
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_rpt_client_list(ClientListState* clstate, const uint8_t* data, size_t datalen, RptClientList* clist,
                             parse_result_t* result)
{
  size_t bytes_parsed = 0;
  parse_result_t res = kPSNoData;

  while (clstate->block.size_parsed < clstate->block.block_size)
  {
    size_t remaining_len = datalen - bytes_parsed;
    const uint8_t* cur_data_ptr = &data[bytes_parsed];
    RptClientEntry* next_entry = NULL;

    if (!clstate->block.parsed_header)
    {
      if (remaining_len >= CLIENT_ENTRY_HEADER_SIZE)
      {
        if (GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(cur_data_ptr) != kClientProtocolRPT)
        {
          RDMNET_LOG_WARNING("Dropping invalid Client List - first entry was RPT, but also contains client protocol %d",
                             GET_CLIENT_PROTOCOL_FROM_CENTRY_HEADER(cur_data_ptr));
          bytes_parsed += consume_bad_block(&clstate->block, datalen, &res);
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
          res = kPSPartialBlockParseOk;
        }
      }
      else
      {
        break;
      }
    }
    else
    {
      next_entry = &clist->client_entries[clist->num_client_entries - 1];
    }

    if (clstate->block.parsed_header)
    {
      // We know the client protocol is correct because it's been validated above.
      client_protocol_t cp = kClientProtocolUnknown;
      size_t next_layer_bytes_parsed = parse_single_client_entry(&clstate->entry, cur_data_ptr, remaining_len, &cp,
                                                                 (ClientEntryUnion*)next_entry, &res);

      // Check and advance the buffer pointers
      RDMNET_ASSERT(next_layer_bytes_parsed <= remaining_len);
      RDMNET_ASSERT(clstate->block.size_parsed + next_layer_bytes_parsed <= clstate->block.block_size);
      bytes_parsed += next_layer_bytes_parsed;
      clstate->block.size_parsed += next_layer_bytes_parsed;

      // Determine what to do next in the list loop
      if (res == kPSFullBlockParseOk)
      {
        clstate->block.parsed_header = false;
        if (clstate->block.size_parsed != clstate->block.block_size)
        {
          // This isn't the last entry in the list
          res = kPSNoData;
        }
        // Iterate again
      }
      else if (res == kPSFullBlockProtErr)
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

RptClientEntry* alloc_next_rpt_client_entry(RptClientList* clist)
{
  if (clist->client_entries)
  {
    RptClientEntry* new_arr = REALLOC_RPT_CLIENT_ENTRY(clist->client_entries, clist->num_client_entries + 1);
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

EptClientEntry* alloc_next_ept_client_entry(EptClientList* clist)
{
  if (clist->client_entries)
  {
    EptClientEntry* new_arr = REALLOC_EPT_CLIENT_ENTRY(clist->client_entries, clist->num_client_entries + 1);
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

size_t parse_request_dynamic_uid_assignment(GenericListState* lstate, const uint8_t* data, size_t datalen,
                                            DynamicUidRequestList* rlist, parse_result_t* result)
{
  RDMNET_UNUSED_ARG(rdmnet_log_params);

  size_t bytes_parsed = 0;
  parse_result_t res = kPSNoData;

  while (datalen - bytes_parsed >= DYNAMIC_UID_REQUEST_PAIR_SIZE)
  {
    // We are starting at the beginning of a new Request Dynamic UID Assignment PDU.
    // Make room for a new struct at the end of the current array.
    if (rlist->requests)
    {
      DynamicUidRequest* new_arr = REALLOC_DYNAMIC_UID_REQUEST_ENTRY(rlist->requests, rlist->num_requests + 1);
      if (new_arr)
      {
        rlist->requests = new_arr;
      }
      else
      {
        // We've run out of space for Dynamic UID Requests - send back up what we have now
        rlist->more_coming = true;
        res = kPSPartialBlockParseOk;
        break;
      }
    }
    else
    {
      rlist->requests = ALLOC_DYNAMIC_UID_REQUEST_ENTRY();
      if (!rlist->requests)
      {
        res = kPSNoData;
        break;
      }
    }

    // Gotten here - parse a new DynamicUidRequest
    DynamicUidRequest* request = &rlist->requests[rlist->num_requests++];
    request->manu_id = etcpal_upack_16b(&data[bytes_parsed]) & 0x7fff;
    memcpy(request->rid.data, &data[bytes_parsed + 6], ETCPAL_UUID_BYTES);
    bytes_parsed += DYNAMIC_UID_REQUEST_PAIR_SIZE;
    lstate->size_parsed += DYNAMIC_UID_REQUEST_PAIR_SIZE;

    if (lstate->size_parsed >= lstate->full_list_size)
    {
      res = kPSFullBlockParseOk;
      break;
    }
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_dynamic_uid_assignment_list(GenericListState* lstate, const uint8_t* data, size_t datalen,
                                         DynamicUidAssignmentList* alist, parse_result_t* result)
{
  RDMNET_UNUSED_ARG(rdmnet_log_params);

  size_t bytes_parsed = 0;
  parse_result_t res = kPSNoData;

  while (datalen - bytes_parsed >= DYNAMIC_UID_MAPPING_SIZE)
  {
    // We are starting at the beginning of a new Dynamic UID Assignment PDU.
    // Make room for a new struct at the end of the current array.
    if (alist->mappings)
    {
      DynamicUidMapping* new_arr = REALLOC_DYNAMIC_UID_MAPPING(alist->mappings, alist->num_mappings + 1);
      if (new_arr)
      {
        alist->mappings = new_arr;
      }
      else
      {
        // We've run out of space for Dynamic UID Mappings - send back up what we have now
        alist->more_coming = true;
        res = kPSPartialBlockParseOk;
        break;
      }
    }
    else
    {
      alist->mappings = ALLOC_DYNAMIC_UID_MAPPING();
      if (!alist->mappings)
      {
        res = kPSNoData;
        break;
      }
    }

    // Gotten here - parse a new DynamicUidMapping
    DynamicUidMapping* mapping = &alist->mappings[alist->num_mappings++];
    const uint8_t* cur_ptr = &data[bytes_parsed];

    mapping->uid.manu = etcpal_upack_16b(cur_ptr);
    cur_ptr += 2;
    mapping->uid.id = etcpal_upack_32b(cur_ptr);
    cur_ptr += 4;
    memcpy(mapping->rid.data, cur_ptr, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
    mapping->status_code = (dynamic_uid_status_t)etcpal_upack_16b(cur_ptr);
    cur_ptr += 2;
    bytes_parsed += DYNAMIC_UID_MAPPING_SIZE;
    lstate->size_parsed += DYNAMIC_UID_MAPPING_SIZE;

    if (lstate->size_parsed >= lstate->full_list_size)
    {
      res = kPSFullBlockParseOk;
      break;
    }
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_fetch_dynamic_uid_assignment_list(GenericListState* lstate, const uint8_t* data, size_t datalen,
                                               FetchUidAssignmentList* alist, parse_result_t* result)
{
  size_t bytes_parsed = 0;
  parse_result_t res = kPSNoData;

  RDMNET_UNUSED_ARG(rdmnet_log_params);

  while (datalen - bytes_parsed >= 6)
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
        res = kPSPartialBlockParseOk;
        break;
      }
    }
    else
    {
      alist->uids = ALLOC_FETCH_UID_ASSIGNMENT();
      if (!alist->uids)
      {
        res = kPSNoData;
        break;
      }
    }

    // Gotten here - parse a new UID
    RdmUid* uid_entry = &alist->uids[alist->num_uids++];
    uid_entry->manu = etcpal_upack_16b(&data[bytes_parsed]);
    uid_entry->id = etcpal_upack_32b(&data[bytes_parsed + 2]);
    bytes_parsed += 6;
    lstate->size_parsed += 6;

    if (lstate->size_parsed >= lstate->full_list_size)
    {
      res = kPSFullBlockParseOk;
      break;
    }
  }

  *result = res;
  return bytes_parsed;
}

void initialize_rpt_message(RptState* rstate, RptMessage* rmsg, size_t pdu_data_len)
{
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

size_t parse_rpt_block(RptState* rstate, const uint8_t* data, size_t datalen, RptMessage* rmsg, parse_result_t* result)
{
  size_t bytes_parsed = 0;
  parse_result_t res = kPSNoData;

  if (rstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&rstate->block, datalen, &res);
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
    else if (datalen >= RPT_PDU_HEADER_SIZE)
    {
      // We can parse an RPT PDU header.
      const uint8_t* cur_ptr = data;
      size_t pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
      if (pdu_len >= RPT_PDU_HEADER_SIZE && rstate->block.size_parsed + pdu_len <= rstate->block.block_size)
      {
        size_t pdu_data_len = pdu_len - RPT_PDU_HEADER_SIZE;
        cur_ptr += 3;
        rmsg->vector = etcpal_upack_32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.source_uid.manu = etcpal_upack_16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.source_uid.id = etcpal_upack_32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.source_endpoint_id = etcpal_upack_16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.dest_uid.manu = etcpal_upack_16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.dest_uid.id = etcpal_upack_32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.dest_endpoint_id = etcpal_upack_16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.seqnum = etcpal_upack_32b(cur_ptr);
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
    // Else we don't have enough data - return kPSNoData by default.

    if (parse_err)
    {
      bytes_parsed += consume_bad_block(&rstate->block, datalen, &res);
      RDMNET_LOG_WARNING("Protocol error encountered while parsing RPT PDU header.");
    }
  }
  if (rstate->block.parsed_header)
  {
    size_t next_layer_bytes_parsed;
    size_t remaining_len = datalen - bytes_parsed;
    switch (rmsg->vector)
    {
      case VECTOR_RPT_REQUEST:
      case VECTOR_RPT_NOTIFICATION:
        next_layer_bytes_parsed =
            parse_rdm_list(&rstate->data.rdm_list, &data[bytes_parsed], remaining_len, GET_RDM_BUF_LIST(rmsg), &res);
        break;
      case VECTOR_RPT_STATUS:
        next_layer_bytes_parsed =
            parse_rpt_status(&rstate->data.status, &data[bytes_parsed], remaining_len, GET_RPT_STATUS_MSG(rmsg), &res);
        break;
      default:
        // Unknown RPT vector - discard this RPT PDU.
        next_layer_bytes_parsed = consume_bad_block(&rstate->data.unknown, remaining_len, &res);
    }
    RDMNET_ASSERT(next_layer_bytes_parsed <= remaining_len);
    RDMNET_ASSERT(rstate->block.size_parsed + next_layer_bytes_parsed <= rstate->block.block_size);
    rstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &rstate->block);
  }
  *result = res;
  return bytes_parsed;
}

size_t parse_rdm_list(RdmListState* rlstate, const uint8_t* data, size_t datalen, RdmBufList* cmd_list,
                      parse_result_t* result)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (!rlstate->parsed_request_notif_header && datalen >= REQUEST_NOTIF_PDU_HEADER_SIZE)
  {
    const uint8_t* cur_ptr = data;
    size_t pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
    uint32_t vect;

    cur_ptr += 3;
    vect = etcpal_upack_32b(cur_ptr);
    cur_ptr += 4;
    if (pdu_len != rlstate->block.block_size || (vect != VECTOR_REQUEST_RDM_CMD && vect != VECTOR_NOTIFICATION_RDM_CMD))
    {
      bytes_parsed += consume_bad_block(&rlstate->block, datalen, &res);
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
      bytes_parsed += consume_bad_block(&rlstate->block, datalen - bytes_parsed, &res);
    }
    else
    {
      while (rlstate->block.size_parsed < rlstate->block.block_size)
      {
        size_t remaining_len = datalen - bytes_parsed;

        // We want to parse an entire RDM Command PDU at once.
        if (remaining_len >= RDM_CMD_PDU_MIN_SIZE)
        {
          const uint8_t* cur_ptr = &data[bytes_parsed];
          size_t rdm_cmd_pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);

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
                res = kPSPartialBlockParseOk;
                break;
              }
            }
            else
            {
              cmd_list->rdm_buffers = ALLOC_RDM_BUFFER();
              if (!cmd_list->rdm_buffers)
              {
                res = kPSNoData;
                break;
              }
            }

            // Gotten here - unpack the RDM command PDU
            RdmBuffer* rdm_buf = &cmd_list->rdm_buffers[cmd_list->num_rdm_buffers++];
            cur_ptr += 3;
            memcpy(rdm_buf->data, cur_ptr, rdm_cmd_pdu_len - 3);
            rdm_buf->datalen = rdm_cmd_pdu_len - 3;
            bytes_parsed += rdm_cmd_pdu_len;
            rlstate->block.size_parsed += rdm_cmd_pdu_len;
            if (rlstate->block.size_parsed >= rlstate->block.block_size)
              res = kPSFullBlockParseOk;
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

size_t parse_rpt_status(RptStatusState* rsstate, const uint8_t* data, size_t datalen, RptStatusMsg* smsg,
                        parse_result_t* result)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (rsstate->block.consuming_bad_block)
  {
    bytes_parsed += consume_bad_block(&rsstate->block, datalen, &res);
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
    else if (datalen >= RPT_STATUS_HEADER_SIZE)
    {
      // We can parse an RPT Status PDU header.
      const uint8_t* cur_ptr = data;

      size_t pdu_len = ETCPAL_PDU_LENGTH(cur_ptr);
      if (pdu_len >= RPT_STATUS_HEADER_SIZE && pdu_len >= rsstate->block.block_size)
      {
        cur_ptr += 3;
        smsg->status_code = (rpt_status_code_t)etcpal_upack_16b(cur_ptr);
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
    // Else we don't have enough data - return kPSNoData by default.

    if (parse_err)
    {
      // Parse error in the RPT Status PDU header. We cannot keep parsing this block.
      bytes_parsed += consume_bad_block(&rsstate->block, datalen, &res);
      RDMNET_LOG_WARNING("Protocol error encountered while parsing RPT Status PDU header.");
    }
  }
  if (rsstate->block.parsed_header)
  {
    size_t remaining_len = datalen - bytes_parsed;
    switch (smsg->status_code)
    {
      case VECTOR_RPT_STATUS_INVALID_MESSAGE:
      case VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS:
        // These status codes have no additional data.
        if (rsstate->block.size_parsed == rsstate->block.block_size)
        {
          res = kPSFullBlockParseOk;
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
      case VECTOR_RPT_STATUS_UNKNOWN_VECTOR:
      {
        size_t str_len = rsstate->block.block_size - rsstate->block.size_parsed;

        // These status codes contain an optional status string
        if (str_len == 0)
        {
          smsg->status_string = NULL;
          res = kPSFullBlockParseOk;
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
          res = kPSFullBlockParseOk;
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

size_t locate_tcp_preamble(RdmnetMsgBuf* msg_buf)
{
  if (msg_buf->cur_data_size < ACN_TCP_PREAMBLE_SIZE)
    return 0;

  size_t i = 0;
  for (; i < (msg_buf->cur_data_size - ACN_TCP_PREAMBLE_SIZE); ++i)
  {
    EtcPalTcpPreamble preamble;
    if (etcpal_parse_tcp_preamble(&msg_buf->buf[i], msg_buf->cur_data_size - i, &preamble))
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

size_t consume_bad_block(PduBlockState* block, size_t datalen, parse_result_t* parse_res)
{
  size_t size_remaining = block->block_size - block->size_parsed;
  if (datalen >= size_remaining)
  {
    *parse_res = kPSFullBlockProtErr;
    block->size_parsed = block->block_size;
    return size_remaining;
  }
  else
  {
    *parse_res = kPSNoData;
    block->size_parsed += datalen;
    block->consuming_bad_block = true;
    return datalen;
  }
}

parse_result_t check_for_full_parse(parse_result_t prev_res, PduBlockState* block)
{
  parse_result_t res = prev_res;
  switch (prev_res)
  {
    case kPSFullBlockParseOk:
    case kPSFullBlockProtErr:
      // If we're not through the PDU block, need to indicate that to the higher layer.
      if (block->size_parsed < block->block_size)
      {
        res = (prev_res == kPSFullBlockProtErr) ? kPSPartialBlockProtErr : kPSPartialBlockParseOk;
      }
      block->parsed_header = false;
      break;
    case kPSPartialBlockParseOk:
    case kPSPartialBlockProtErr:
    case kPSNoData:
    default:
      break;
  }
  return res;
}
