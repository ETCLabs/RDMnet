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

#include "rdmnetmsgbuf.h"

#include <assert.h>
#include "rdmnet/opts.h"
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif
#include "lwpa_pack.h"
#include "lwpa_rootlayerpdu.h"
#include "brokerprotpriv.h"
#include "rptprotpriv.h"
#include "rdmnetmessagepriv.h"

/* Suppress strncpy() warning on Windows/MSVC. */
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/*************************** Private constants *******************************/

#define MODULE_NAME "RDMnetMsgBuf"

/*********************** Private function prototypes *************************/

static lwpa_error_t run_parse_state_machine(RdmnetMsgBuf *msg_buf);
static size_t locate_tcp_preamble(RdmnetMsgBuf *msg_buf);
static size_t consume_bad_block(PduBlockState *block, size_t datalen, parse_result_t *parse_res);
static parse_result_t check_for_full_parse(parse_result_t prev_res, PduBlockState *block);

/* The parse functions are organized by protocol layer, and each one gets a
 * subset of the overall state structure. */

/* Root layer */
static size_t parse_rlp_block(RlpState *rlpstate, const uint8_t *data, size_t data_size, RdmnetMessage *msg,
                              parse_result_t *result, const LwpaLogParams *lparams);

/* RDMnet layer */
static void initialize_rdmnet_message(RlpState *rlpstate, RdmnetMessage *msg, size_t pdu_data_len,
                                      const LwpaLogParams *lparams);
static size_t parse_broker_block(BrokerState *bstate, const uint8_t *data, size_t datalen, BrokerMessage *bmsg,
                                 parse_result_t *result, const LwpaLogParams *lparams);
static size_t parse_rpt_block(RptState *rstate, const uint8_t *data, size_t datalen, RptMessage *rmsg,
                              parse_result_t *result, const LwpaLogParams *lparams);

/* RPT layer */
static void initialize_rpt_message(RptState *rstate, RptMessage *rmsg, size_t pdu_data_len,
                                   const LwpaLogParams *lparams);
static size_t parse_rdm_list(RdmListState *rlstate, const uint8_t *data, size_t datalen, RdmCmdList *cmd_list,
                             parse_result_t *result);
static size_t parse_rpt_status(RptStatusState *rsstate, const uint8_t *data, size_t datalen, RptStatusMsg *smsg,
                               parse_result_t *result, const LwpaLogParams *lparams);

/* Broker layer */
static void initialize_broker_message(BrokerState *bstate, BrokerMessage *bmsg, size_t pdu_data_len,
                                      const LwpaLogParams *lparams);
static void parse_client_connect_header(const uint8_t *data, ClientConnectMsg *ccmsg);
static size_t parse_client_connect(ClientConnectState *ccstate, const uint8_t *data, size_t datalen,
                                   ClientConnectMsg *ccmsg, parse_result_t *result, const LwpaLogParams *lparams);
static size_t parse_client_entry_update(ClientEntryUpdateState *ceustate, const uint8_t *data, size_t datalen,
                                        ClientEntryUpdateMsg *ceumsg, parse_result_t *result,
                                        const LwpaLogParams *lparams);
static size_t parse_client_entry_header(const uint8_t *data, ClientEntryData *entry);
static size_t parse_single_client_entry(ClientEntryState *cstate, const uint8_t *data, size_t datalen,
                                        ClientEntryData *entry, parse_result_t *result, const LwpaLogParams *lparams);
static size_t parse_client_list(ClientListState *clstate, const uint8_t *data, size_t datalen, ClientList *clist,
                                parse_result_t *result, const LwpaLogParams *lparams);

/*************************** Function definitions ****************************/

void rdmnet_msg_buf_init(RdmnetMsgBuf *msg_buf, const LwpaLogParams *lparams)
{
  if (msg_buf)
  {
    msg_buf->cur_data_size = 0;
    msg_buf->data_remaining = false;
    msg_buf->have_preamble = false;
    msg_buf->lparams = lparams;
  }
}

lwpa_error_t rdmnet_msg_buf_recv(lwpa_socket_t sock, RdmnetMsgBuf *msg_buf)
{
  lwpa_error_t res;

  if (msg_buf->data_remaining)
    res = run_parse_state_machine(msg_buf);
  else
  {
    int recv_res =
        lwpa_recv(sock, &msg_buf->buf[msg_buf->cur_data_size], RDMNET_RECV_BUF_SIZE - msg_buf->cur_data_size, 0);
    if (recv_res < 0)
      return recv_res;
    else if (recv_res == 0)
      /* 0 indicates graceful close of connection by peer */
      return LWPA_CONNCLOSED;
    else
    {
      msg_buf->cur_data_size += recv_res;
      res = run_parse_state_machine(msg_buf);
    }
  }
  return res;
}

lwpa_error_t run_parse_state_machine(RdmnetMsgBuf *msg_buf)
{
  /* Unless we finish parsing a message in this function, we will return
   * LWPA_NODATA to indicate that the parse is still in progress. */
  lwpa_error_t res = LWPA_NODATA;
  size_t consumed;
  parse_result_t parse_res;

  do
  {
    consumed = 0;

    if (!msg_buf->have_preamble)
    {
      size_t pdu_block_size = locate_tcp_preamble(msg_buf);
      if (pdu_block_size)
      {
        init_rlp_state(&msg_buf->rlp_state, pdu_block_size);
        msg_buf->have_preamble = true;
      }
      else
      {
        res = LWPA_NODATA;
        msg_buf->data_remaining = false;
        break;
      }
    }
    if (msg_buf->have_preamble)
    {
      consumed = parse_rlp_block(&msg_buf->rlp_state, msg_buf->buf, msg_buf->cur_data_size, &msg_buf->msg, &parse_res,
                                 msg_buf->lparams);
      switch (parse_res)
      {
        case kPSFullBlockParseOk:
        case kPSFullBlockProtErr:
          msg_buf->have_preamble = false;
          res = (parse_res == kPSFullBlockProtErr ? LWPA_PROTERR : LWPA_OK);
          msg_buf->data_remaining = (consumed < msg_buf->cur_data_size);
          break;
        case kPSPartialBlockParseOk:
        case kPSPartialBlockProtErr:
          res = (parse_res == kPSPartialBlockProtErr ? LWPA_PROTERR : LWPA_OK);
          msg_buf->data_remaining = (consumed < msg_buf->cur_data_size);
          break;
        case kPSNoData:
        default:
          res = LWPA_NODATA;
          msg_buf->data_remaining = false;
          break;
      }
    }

    if (consumed > 0)
    {
      /* Roll the buffer to discard the data we have already parsed. */
      assert(msg_buf->cur_data_size >= consumed);
      if (msg_buf->cur_data_size > consumed)
      {
        memmove(msg_buf->buf, &msg_buf->buf[consumed], msg_buf->cur_data_size - consumed);
      }
      msg_buf->cur_data_size -= consumed;
    }
  } while (res == LWPA_PROTERR);

  return res;
}

void initialize_rdmnet_message(RlpState *rlpstate, RdmnetMessage *msg, size_t pdu_data_len,
                               const LwpaLogParams *lparams)
{
  switch (msg->vector)
  {
    case VECTOR_ROOT_BROKER:
      init_broker_state(&rlpstate->data.broker, pdu_data_len, msg);
      break;
    case VECTOR_ROOT_RPT:
      init_rpt_state(&rlpstate->data.rpt, pdu_data_len);
      break;
    default:
      init_pdu_block_state(&rlpstate->data.unknown, pdu_data_len);
      lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Dropping Root Layer PDU with unknown vector %d.", msg->vector);
      break;
  }
}

size_t parse_rlp_block(RlpState *rlpstate, const uint8_t *data, size_t datalen, RdmnetMessage *msg,
                       parse_result_t *result, const LwpaLogParams *lparams)
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

    /* If the size remaining in the Root Layer PDU block is not enough for
     * another Root Layer PDU header, indicate a bad block condition. */
    if ((rlpstate->block.block_size - rlpstate->block.size_parsed) < RLP_HEADER_SIZE_EXT_LEN)
    {
      parse_err = true;
    }
    else if (datalen >= RLP_HEADER_SIZE_EXT_LEN)
    {
      RootLayerPdu rlp;

      /* Inheritance at the root layer is disallowed by E1.33. */
      if (parse_root_layer_header(data, datalen, &rlp, NULL))
      {
        /* Update the data pointers and sizes. */
        bytes_parsed += RLP_HEADER_SIZE_EXT_LEN;
        rlpstate->block.size_parsed += RLP_HEADER_SIZE_EXT_LEN;

        /* If this PDU indicates a length that takes it past the end of the
         * block size from the preamble, it is an error. */
        if (rlpstate->block.size_parsed + rlp.datalen <= rlpstate->block.block_size)
        {
          /* Fill in the root layer data in the overall rdmnet_message
           * struct. */
          msg->vector = rlp.vector;
          msg->sender_cid = rlp.sender_cid;
          rlpstate->block.parsed_header = true;
          initialize_rdmnet_message(rlpstate, msg, rlp.datalen, lparams);
        }
        else
          parse_err = true;
      }
      else
        parse_err = true;
    }
    /* No else for this block - if there is not enough data yet to parse an RLP
     * header, we simply indicate no data. */

    if (parse_err)
    {
      /* Parse error in the root layer header. We cannot keep parsing this
       * block. */
      bytes_parsed += consume_bad_block(&rlpstate->block, datalen, &res);
      lwpa_log(lparams, LWPA_LOG_WARNING,
               MODULE_NAME ": Protocol error encountered while parsing Root Layer PDU header.");
    }
  }
  if (rlpstate->block.parsed_header)
  {
    size_t next_layer_bytes_parsed;
    switch (msg->vector)
    {
      case VECTOR_ROOT_BROKER:
        next_layer_bytes_parsed = parse_broker_block(&rlpstate->data.broker, &data[bytes_parsed],
                                                     datalen - bytes_parsed, get_broker_msg(msg), &res, lparams);
        break;
      case VECTOR_ROOT_RPT:
        next_layer_bytes_parsed = parse_rpt_block(&rlpstate->data.rpt, &data[bytes_parsed], datalen - bytes_parsed,
                                                  get_rpt_msg(msg), &res, lparams);
        break;
      default:
        next_layer_bytes_parsed = consume_bad_block(&rlpstate->data.unknown, datalen - bytes_parsed, &res);
    }
    assert(next_layer_bytes_parsed <= (datalen - bytes_parsed));
    assert(rlpstate->block.size_parsed + next_layer_bytes_parsed <= rlpstate->block.block_size);
    rlpstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &rlpstate->block);
  }
  *result = res;
  return bytes_parsed;
}

void initialize_broker_message(BrokerState *bstate, BrokerMessage *bmsg, size_t pdu_data_len,
                               const LwpaLogParams *lparams)
{
  bool bad_length = false;

  switch (bmsg->vector)
  {
    case VECTOR_BROKER_CONNECT:
      if (pdu_data_len >= CLIENT_CONNECT_DATA_MIN_SIZE)
      {
        init_client_connect_state(&bstate->data.client_connect, pdu_data_len, bmsg);
      }
      else
        bad_length = true;
      break;
    case VECTOR_BROKER_CONNECT_REPLY:
      if (pdu_data_len != CONNECT_REPLY_DATA_SIZE)
        bad_length = true;
      break;
    case VECTOR_BROKER_CLIENT_ENTRY_UPDATE:
      if (pdu_data_len >= CLIENT_ENTRY_UPDATE_DATA_MIN_SIZE)
      {
        init_client_entry_update_state(&bstate->data.update, pdu_data_len, bmsg);
      }
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
      init_client_list_state(&bstate->data.client_list, pdu_data_len, bmsg);
      break;
    case VECTOR_BROKER_NULL:
    case VECTOR_BROKER_FETCH_CLIENT_LIST:
      /* Check the length. These messages have no data. */
      if (pdu_data_len != 0)
        bad_length = true;
      break;
    case VECTOR_BROKER_DISCONNECT:
      if (pdu_data_len != DISCONNECT_DATA_SIZE)
        bad_length = true;
      break;
    default:
      init_pdu_block_state(&bstate->data.unknown, pdu_data_len);
      lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Dropping Broker PDU with unknown vector %d.", bmsg->vector);
      break;
  }

  if (bad_length)
  {
    init_pdu_block_state(&bstate->data.unknown, pdu_data_len);
    /* An artificial "unknown" vector value to flag the data parsing
     * logic to consume the data section. */
    bmsg->vector = 0xffff;
    lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Dropping Broker PDU with vector %d and invalid length %zu",
             bmsg->vector, pdu_data_len + BROKER_PDU_HEADER_SIZE);
  }
}

size_t parse_broker_block(BrokerState *bstate, const uint8_t *data, size_t datalen, BrokerMessage *bmsg,
                          parse_result_t *result, const LwpaLogParams *lparams)
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

    /* If the size remaining in the Broker PDU block is not enough for another
     * Broker PDU header, indicate a bad block condition. */
    if ((bstate->block.block_size - bstate->block.size_parsed) < BROKER_PDU_HEADER_SIZE)
    {
      parse_err = true;
    }
    else if (datalen >= BROKER_PDU_HEADER_SIZE)
    {
      /* We can parse a Broker PDU header. */
      const uint8_t *cur_ptr = data;

      size_t pdu_len = pdu_length(cur_ptr);
      if (pdu_len >= BROKER_PDU_HEADER_SIZE && bstate->block.size_parsed + pdu_len <= bstate->block.block_size)
      {
        size_t pdu_data_len = pdu_len - BROKER_PDU_HEADER_SIZE;

        cur_ptr += 3;
        bmsg->vector = upack_16b(cur_ptr);
        cur_ptr += 2;
        bytes_parsed += BROKER_PDU_HEADER_SIZE;
        bstate->block.size_parsed += BROKER_PDU_HEADER_SIZE;
        bstate->block.parsed_header = true;
        initialize_broker_message(bstate, bmsg, pdu_data_len, lparams);
      }
      else
        parse_err = true;
    }
    /* Else we don't have enough data - return kPSNoData by default. */

    if (parse_err)
    {
      /* Parse error in the Broker PDU header. We cannot keep parsing this
       * block. */
      bytes_parsed += consume_bad_block(&bstate->block, datalen, &res);
      lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Protocol error encountered while parsing Broker PDU header.");
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
                                                       get_client_connect_msg(bmsg), &res, lparams);
        break;
      case VECTOR_BROKER_CONNECT_REPLY:
        if (remaining_len >= CONNECT_REPLY_DATA_SIZE)
        {
          ConnectReplyMsg *crmsg = get_connect_reply_msg(bmsg);
          const uint8_t *cur_ptr = &data[bytes_parsed];
          crmsg->connect_status = upack_16b(cur_ptr);
          cur_ptr += 2;
          crmsg->e133_version = upack_16b(cur_ptr);
          cur_ptr += 2;
          crmsg->broker_uid.manu = upack_16b(cur_ptr);
          cur_ptr += 2;
          crmsg->broker_uid.id = upack_32b(cur_ptr);
          cur_ptr += 4;
          next_layer_bytes_parsed = cur_ptr - &data[bytes_parsed];
          res = kPSFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_CLIENT_ENTRY_UPDATE:
        next_layer_bytes_parsed = parse_client_entry_update(&bstate->data.update, &data[bytes_parsed], remaining_len,
                                                            get_client_entry_update_msg(bmsg), &res, lparams);
        break;
      case VECTOR_BROKER_REDIRECT_V4:
        if (remaining_len >= REDIRECT_V4_DATA_SIZE)
        {
          ClientRedirectMsg *crmsg = get_client_redirect_msg(bmsg);
          const uint8_t *cur_ptr = &data[bytes_parsed];
          lwpaip_set_v4_address(&crmsg->new_addr.ip, upack_32b(cur_ptr));
          cur_ptr += 4;
          crmsg->new_addr.port = upack_16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = cur_ptr - &data[bytes_parsed];
          res = kPSFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_REDIRECT_V6:
        if (remaining_len >= REDIRECT_V6_DATA_SIZE)
        {
          ClientRedirectMsg *crmsg = get_client_redirect_msg(bmsg);
          const uint8_t *cur_ptr = &data[bytes_parsed];
          lwpaip_set_v6_address(&crmsg->new_addr.ip, cur_ptr);
          cur_ptr += 16;
          crmsg->new_addr.port = upack_16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = cur_ptr - &data[bytes_parsed];
          res = kPSFullBlockParseOk;
        }
        break;
      case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
      case VECTOR_BROKER_CLIENT_ADD:
      case VECTOR_BROKER_CLIENT_REMOVE:
      case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
        next_layer_bytes_parsed = parse_client_list(&bstate->data.client_list, &data[bytes_parsed], remaining_len,
                                                    get_client_list(bmsg), &res, lparams);
        break;
      case VECTOR_BROKER_NULL:
      case VECTOR_BROKER_FETCH_CLIENT_LIST:
        /* These messages have no data, so we are at the end of the
         * PDU. */
        res = kPSFullBlockParseOk;
        break;
      case VECTOR_BROKER_DISCONNECT:
        if (remaining_len >= DISCONNECT_DATA_SIZE)
        {
          const uint8_t *cur_ptr = &data[bytes_parsed];
          get_disconnect_msg(bmsg)->disconnect_reason = upack_16b(cur_ptr);
          cur_ptr += 2;
          next_layer_bytes_parsed = cur_ptr - &data[bytes_parsed];
          res = kPSFullBlockParseOk;
        }
        break;
      default:
        /* Unknown Broker vector - discard this Broker PDU. */
        next_layer_bytes_parsed = consume_bad_block(&bstate->data.unknown, remaining_len, &res);
        break;
    }
    assert(next_layer_bytes_parsed <= remaining_len);
    assert(bstate->block.size_parsed + next_layer_bytes_parsed <= bstate->block.block_size);
    bstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &bstate->block);
  }
  *result = res;
  return bytes_parsed;
}

void parse_client_connect_header(const uint8_t *data, ClientConnectMsg *ccmsg)
{
  const uint8_t *cur_ptr = data;

#if RDMNET_DYNAMIC_MEM
  if (cur_ptr[E133_SCOPE_STRING_PADDED_LENGTH - 1] == '\0')
  {
    char *new_scope = (char *)malloc(strlen((char *)cur_ptr) + 1);
    assert(new_scope);
    strcpy(new_scope, (char *)cur_ptr);
    ccmsg->scope = new_scope;
  }
  else
  {
    char *new_scope = (char *)malloc(E133_SCOPE_STRING_PADDED_LENGTH);
    assert(new_scope);
    memcpy(new_scope, cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH - 1);
    new_scope[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
    ccmsg->scope = new_scope;
  }
#else
  strncpy(ccmsg->scope, (char *)cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH);
  ccmsg->scope[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
#endif
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  ccmsg->e133_version = upack_16b(cur_ptr);
  cur_ptr += 2;
#if RDMNET_DYNAMIC_MEM
  if (cur_ptr[E133_DOMAIN_STRING_PADDED_LENGTH - 1] == '\0')
  {
    char *new_search_domain = (char *)malloc(strlen((char *)cur_ptr) + 1);
    assert(new_search_domain);
    strcpy(new_search_domain, (char *)cur_ptr);
    ccmsg->search_domain = new_search_domain;
  }
  else
  {
    char *new_search_domain = (char *)malloc(E133_DOMAIN_STRING_PADDED_LENGTH);
    assert(new_search_domain);
    memcpy(new_search_domain, cur_ptr, E133_DOMAIN_STRING_PADDED_LENGTH - 1);
    new_search_domain[E133_DOMAIN_STRING_PADDED_LENGTH - 1] = '\0';
    ccmsg->search_domain = new_search_domain;
  }
#else
  strncpy(ccmsg->search_domain, (char *)cur_ptr, E133_DOMAIN_STRING_PADDED_LENGTH);
  ccmsg->search_domain[E133_DOMAIN_STRING_PADDED_LENGTH - 1] = '\0';
#endif
  cur_ptr += E133_DOMAIN_STRING_PADDED_LENGTH;
  ccmsg->connect_flags = *cur_ptr;
}

size_t parse_client_connect(ClientConnectState *ccstate, const uint8_t *data, size_t datalen, ClientConnectMsg *ccmsg,
                            parse_result_t *result, const LwpaLogParams *lparams)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (!ccstate->common_data_parsed)
  {
    /* We want to wait until we can parse all of the Client Connect common data
     * at once. */
    if (datalen < CLIENT_CONNECT_COMMON_FIELD_SIZE)
    {
      *result = kPSNoData;
      return 0;
    }

    parse_client_connect_header(data, ccmsg);
    bytes_parsed += CLIENT_CONNECT_COMMON_FIELD_SIZE;
    ccstate->common_data_parsed = true;
    init_client_entry_state(&ccstate->entry, ccstate->pdu_data_size - CLIENT_CONNECT_COMMON_FIELD_SIZE,
                            &ccmsg->client_entry);
  }
  if (ccstate->common_data_parsed)
  {
    size_t next_layer_bytes_parsed = parse_single_client_entry(
        &ccstate->entry, &data[bytes_parsed], datalen - bytes_parsed, &ccmsg->client_entry, &res, lparams);
    assert(next_layer_bytes_parsed <= (datalen - bytes_parsed));
    bytes_parsed += next_layer_bytes_parsed;
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_client_entry_update(ClientEntryUpdateState *ceustate, const uint8_t *data, size_t datalen,
                                 ClientEntryUpdateMsg *ceumsg, parse_result_t *result, const LwpaLogParams *lparams)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (!ceustate->common_data_parsed)
  {
    /* We want to wait until we can parse all of the Client Entry Update common
     * data at once. */
    if (datalen < CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE)
    {
      *result = kPSNoData;
      return 0;
    }

    ceumsg->connect_flags = *data;
    bytes_parsed += CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE;
    ceustate->common_data_parsed = true;
    init_client_entry_state(&ceustate->entry, ceustate->pdu_data_size - CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE,
                            &ceumsg->client_entry);
  }
  if (ceustate->common_data_parsed)
  {
    size_t next_layer_bytes_parsed = parse_single_client_entry(
        &ceustate->entry, &data[bytes_parsed], datalen - bytes_parsed, &ceumsg->client_entry, &res, lparams);
    assert(next_layer_bytes_parsed <= (datalen - bytes_parsed));
    bytes_parsed += next_layer_bytes_parsed;
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_client_entry_header(const uint8_t *data, ClientEntryData *entry)
{
  const uint8_t *cur_ptr = data;
  size_t len = pdu_length(cur_ptr);
  cur_ptr += 3;
  entry->client_protocol = upack_32b(cur_ptr);
  cur_ptr += 4;
  memcpy(entry->client_cid.data, cur_ptr, CID_BYTES);
  entry->next = NULL;
  return len;
}

size_t parse_single_client_entry(ClientEntryState *cstate, const uint8_t *data, size_t datalen, ClientEntryData *entry,
                                 parse_result_t *result, const LwpaLogParams *lparams)
{
  size_t bytes_parsed = 0;
  parse_result_t res = kPSNoData;

  if (entry->client_protocol == kClientProtocolUnknown)
  {
    if (datalen >= CLIENT_ENTRY_HEADER_SIZE)
    {
      /* Parse the Client Entry header */
      size_t cli_entry_pdu_len = parse_client_entry_header(data, entry);
      bytes_parsed += CLIENT_ENTRY_HEADER_SIZE;
      init_pdu_block_state(&cstate->entry_data, cli_entry_pdu_len - CLIENT_ENTRY_HEADER_SIZE);
      if (cli_entry_pdu_len > cstate->enclosing_block_size)
      {
        bytes_parsed += consume_bad_block(&cstate->entry_data, datalen - bytes_parsed, &res);
      }
    }
    /* Else return no data */
  }
  if (entry->client_protocol != kClientProtocolUnknown)
  {
    size_t remaining_len = datalen - bytes_parsed;

    if (cstate->entry_data.consuming_bad_block)
    {
      bytes_parsed += consume_bad_block(&cstate->entry_data, remaining_len, &res);
    }
    else if (entry->client_protocol == kClientProtocolEPT)
    {
      /* Parse the EPT Client Entry data */
      /* TODO */
      bytes_parsed += consume_bad_block(&cstate->entry_data, remaining_len, &res);
    }
    else if (entry->client_protocol == kClientProtocolRPT)
    {
      if (cstate->entry_data.size_parsed + RPT_CLIENT_ENTRY_DATA_SIZE == cstate->entry_data.block_size)
      {
        if (remaining_len >= RPT_CLIENT_ENTRY_DATA_SIZE)
        {
          /* Parse the RPT Client Entry data */
          ClientEntryDataRpt *rpt_entry = get_rpt_client_entry_data(entry);
          const uint8_t *cur_ptr = &data[bytes_parsed];

          rpt_entry->client_uid.manu = upack_16b(cur_ptr);
          cur_ptr += 2;
          rpt_entry->client_uid.id = upack_32b(cur_ptr);
          cur_ptr += 4;
          rpt_entry->client_type = *cur_ptr++;
          memcpy(rpt_entry->binding_cid.data, cur_ptr, CID_BYTES);
          bytes_parsed += RPT_CLIENT_ENTRY_DATA_SIZE;
          cstate->entry_data.size_parsed += RPT_CLIENT_ENTRY_DATA_SIZE;
          res = kPSFullBlockParseOk;
        }
        /* Else return no data */
      }
      else
      {
        /* PDU length mismatch */
        bytes_parsed += consume_bad_block(&cstate->entry_data, remaining_len, &res);
        lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Dropping RPT Client Entry with invalid length %zu",
                 cstate->entry_data.block_size + CLIENT_ENTRY_HEADER_SIZE);
      }
    }
    else
    {
      /* Unknown Client Protocol */
      bytes_parsed += consume_bad_block(&cstate->entry_data, remaining_len, &res);
      lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Dropping Client Entry with invalid client protocol %d",
               entry->client_protocol);
    }
  }

  *result = res;
  return bytes_parsed;
}

size_t parse_client_list(ClientListState *clstate, const uint8_t *data, size_t datalen, ClientList *clist,
                         parse_result_t *result, const LwpaLogParams *lparams)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;
  ClientEntryData *centry = NULL;

  if (clstate->block.consuming_bad_block)
    bytes_parsed += consume_bad_block(&clstate->block, datalen, &res);
  else
  {
    ClientEntryData **centry_ptr;

    /* Navigate to the end of the client entry list */
    for (centry_ptr = &clist->client_entry_list; *centry_ptr && (*centry_ptr)->next; centry_ptr = &(*centry_ptr)->next)
      ;

    while (clstate->block.size_parsed < clstate->block.block_size)
    {
      if (!clstate->block.parsed_header)
      {
        /* We are starting at the beginning of a new Client Entry PDU. */
        /* Allocate a new struct at the end of the list */
        if (*centry_ptr)
          centry_ptr = &(*centry_ptr)->next;

        *centry_ptr = alloc_client_entry();
        if (!(*centry_ptr))
        {
          /* We've run out of space for client entries - send back up what we
           * have now. */
          if (clist->client_entry_list)
          {
            clist->partial = true;
            res = kPSPartialBlockParseOk;
          }
          else
            res = kPSNoData;
          break;
        }
        else
        {
          clstate->block.parsed_header = true;
          centry = *centry_ptr;
          init_client_entry_state(&clstate->entry, clstate->block.block_size, centry);
        }
      }
      else
        centry = *centry_ptr;

      if (clstate->block.parsed_header)
      {
        size_t next_layer_bytes_parsed = parse_single_client_entry(&clstate->entry, &data[bytes_parsed],
                                                                   datalen - bytes_parsed, centry, &res, lparams);
        assert(next_layer_bytes_parsed <= (datalen - bytes_parsed));
        assert(clstate->block.size_parsed + next_layer_bytes_parsed <= clstate->block.block_size);
        bytes_parsed += next_layer_bytes_parsed;
        clstate->block.size_parsed += next_layer_bytes_parsed;
        if (res == kPSFullBlockParseOk || res == kPSFullBlockProtErr)
          clstate->block.parsed_header = false;
        if (res != kPSFullBlockParseOk)
          break;
      }
    }
  }

  *result = res;
  return bytes_parsed;
}

void initialize_rpt_message(RptState *rstate, RptMessage *rmsg, size_t pdu_data_len, const LwpaLogParams *lparams)
{
  switch (rmsg->vector)
  {
    case VECTOR_RPT_REQUEST:
    case VECTOR_RPT_NOTIFICATION:
      if (pdu_data_len >= REQUEST_NOTIF_PDU_HEADER_SIZE)
        init_rdm_list_state(&rstate->data.rdm_list, pdu_data_len, rmsg);
      else
      {
        init_pdu_block_state(&rstate->data.unknown, pdu_data_len);
        /* An artificial "unknown" vector value to flag the data parsing
         * logic to consume the data section. */
        rmsg->vector = 0xffffffff;
        lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Dropping RPT PDU with invalid length %zu",
                 pdu_data_len + RPT_PDU_HEADER_SIZE);
      }
      break;
    case VECTOR_RPT_STATUS:
      if (pdu_data_len >= RPT_STATUS_HEADER_SIZE)
        init_rpt_status_state(&rstate->data.status, pdu_data_len);
      else
      {
        init_pdu_block_state(&rstate->data.unknown, pdu_data_len);
        /* An artificial "unknown" vector value to flag the data parsing
         * logic to consume the data section. */
        rmsg->vector = 0xffffffff;
        lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Dropping RPT PDU with invalid length %zu",
                 pdu_data_len + RPT_PDU_HEADER_SIZE);
      }
      break;
    default:
      init_pdu_block_state(&rstate->data.unknown, pdu_data_len);
      lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Dropping RPT PDU with invalid vector %u", rmsg->vector);
      break;
  }
}

size_t parse_rpt_block(RptState *rstate, const uint8_t *data, size_t datalen, RptMessage *rmsg, parse_result_t *result,
                       const LwpaLogParams *lparams)
{
  size_t bytes_parsed = 0;
  parse_result_t res = kPSNoData;

  if (rstate->block.consuming_bad_block)
    bytes_parsed += consume_bad_block(&rstate->block, datalen, &res);
  else if (!rstate->block.parsed_header)
  {
    bool parse_err = false;

    /* If the size remaining in the RPT PDU block is not enough for another
     * RPT PDU header, indicate a bad block condition. */
    if ((rstate->block.block_size - rstate->block.size_parsed) < RPT_PDU_HEADER_SIZE)
    {
      parse_err = true;
    }
    else if (datalen >= RPT_PDU_HEADER_SIZE)
    {
      /* We can parse an RPT PDU header. */
      const uint8_t *cur_ptr = data;
      size_t pdu_len = pdu_length(cur_ptr);
      if (pdu_len >= RPT_PDU_HEADER_SIZE && rstate->block.size_parsed + pdu_len <= rstate->block.block_size)
      {
        size_t pdu_data_len = pdu_len - RPT_PDU_HEADER_SIZE;
        cur_ptr += 3;
        rmsg->vector = upack_32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.source_uid.manu = upack_16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.source_uid.id = upack_32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.source_endpoint_id = upack_16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.dest_uid.manu = upack_16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.dest_uid.id = upack_32b(cur_ptr);
        cur_ptr += 4;
        rmsg->header.dest_endpoint_id = upack_16b(cur_ptr);
        cur_ptr += 2;
        rmsg->header.seqnum = upack_32b(cur_ptr);
        cur_ptr += 4;
        ++cur_ptr; /* 1-byte reserved field */

        bytes_parsed += RPT_PDU_HEADER_SIZE;
        rstate->block.size_parsed += RPT_PDU_HEADER_SIZE;
        initialize_rpt_message(rstate, rmsg, pdu_data_len, lparams);
        rstate->block.parsed_header = true;
      }
      else
        parse_err = true;
    }
    /* Else we don't have enough data - return kPSNoData by default. */

    if (parse_err)
    {
      bytes_parsed += consume_bad_block(&rstate->block, datalen, &res);
      lwpa_log(lparams, LWPA_LOG_WARNING, MODULE_NAME ": Protocol error encountered while parsing RPT PDU header.");
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
            parse_rdm_list(&rstate->data.rdm_list, &data[bytes_parsed], remaining_len, get_rdm_cmd_list(rmsg), &res);
        break;
      case VECTOR_RPT_STATUS:
        next_layer_bytes_parsed = parse_rpt_status(&rstate->data.status, &data[bytes_parsed], remaining_len,
                                                   get_status_msg(rmsg), &res, lparams);
        break;
      default:
        /* Unknown RPT vector - discard this RPT PDU. */
        next_layer_bytes_parsed = consume_bad_block(&rstate->data.unknown, remaining_len, &res);
    }
    assert(next_layer_bytes_parsed <= remaining_len);
    assert(rstate->block.size_parsed + next_layer_bytes_parsed <= rstate->block.block_size);
    rstate->block.size_parsed += next_layer_bytes_parsed;
    bytes_parsed += next_layer_bytes_parsed;
    res = check_for_full_parse(res, &rstate->block);
  }
  *result = res;
  return bytes_parsed;
}

size_t parse_rdm_list(RdmListState *rlstate, const uint8_t *data, size_t datalen, RdmCmdList *cmd_list,
                      parse_result_t *result)
{
  parse_result_t res = kPSNoData;
  size_t bytes_parsed = 0;

  if (!rlstate->parsed_request_notif_header && datalen >= REQUEST_NOTIF_PDU_HEADER_SIZE)
  {
    const uint8_t *cur_ptr = data;
    size_t pdu_len = pdu_length(cur_ptr);
    uint32_t vect;

    cur_ptr += 3;
    vect = upack_32b(cur_ptr);
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
      RdmCmdListEntry **rdmcmd_ptr;

      /* Navigate to the end of the RDM Command list */
      for (rdmcmd_ptr = &cmd_list->list; *rdmcmd_ptr; rdmcmd_ptr = &(*rdmcmd_ptr)->next)
        ;

      while (rlstate->block.size_parsed < rlstate->block.block_size)
      {
        size_t remaining_len = datalen - bytes_parsed;

        /* We want to parse an entire RDM Command PDU at once. */
        if (remaining_len >= RDM_CMD_PDU_MIN_SIZE)
        {
          const uint8_t *cur_ptr = &data[bytes_parsed];
          size_t rdm_cmd_pdu_len = pdu_length(cur_ptr);

          if (rdm_cmd_pdu_len > rlstate->block.block_size || rdm_cmd_pdu_len > RDM_CMD_PDU_MAX_SIZE)
          {
            bytes_parsed += consume_bad_block(&rlstate->block, remaining_len, &res);
          }
          else if (remaining_len >= rdm_cmd_pdu_len)
          {
            /* Allocate a new struct at the end of the list */
            *rdmcmd_ptr = alloc_rdm_command();
            if (!(*rdmcmd_ptr))
            {
              /* We've run out of space for RDM commands - send back up what
               * we have now. */
              if (cmd_list->list)
              {
                cmd_list->partial = true;
                res = kPSPartialBlockParseOk;
              }
              else
                res = kPSNoData;
              break;
            }
            else
            {
              /* Unpack the RDM Command PDU. */
              RdmCmdListEntry *rdmcmd = *rdmcmd_ptr;
              rdmcmd->next = NULL;
              cur_ptr += 3;
              memcpy(rdmcmd->msg.data, cur_ptr, rdm_cmd_pdu_len - 3);
              rdmcmd->msg.datalen = rdm_cmd_pdu_len - 3;
              bytes_parsed += rdm_cmd_pdu_len;
              rlstate->block.size_parsed += rdm_cmd_pdu_len;
              if (rlstate->block.size_parsed >= rlstate->block.block_size)
                res = kPSFullBlockParseOk;
              else
                rdmcmd_ptr = &rdmcmd->next;
            }
          }
          else
            break;
        }
        else
          break;
      }
    }
  }
  *result = res;
  return bytes_parsed;
}

size_t parse_rpt_status(RptStatusState *rsstate, const uint8_t *data, size_t datalen, RptStatusMsg *smsg,
                        parse_result_t *result, const LwpaLogParams *lparams)
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

    /* If the size remaining in the Broker PDU block is not enough for another
     * RPT Status PDU header, indicate a bad block condition. */
    if ((rsstate->block.block_size - rsstate->block.size_parsed) < RPT_STATUS_HEADER_SIZE)
    {
      parse_err = true;
    }
    else if (datalen >= RPT_STATUS_HEADER_SIZE)
    {
      /* We can parse an RPT Status PDU header. */
      const uint8_t *cur_ptr = data;

      size_t pdu_len = pdu_length(cur_ptr);
      if (pdu_len >= RPT_STATUS_HEADER_SIZE && pdu_len >= rsstate->block.block_size)
      {
        cur_ptr += 3;
        smsg->status_code = upack_16b(cur_ptr);
        cur_ptr += 2;
        bytes_parsed += RPT_STATUS_HEADER_SIZE;
        rsstate->block.size_parsed += RPT_STATUS_HEADER_SIZE;
        rsstate->block.parsed_header = true;
      }
      else
        parse_err = true;
    }
    /* Else we don't have enough data - return kPSNoData by default. */

    if (parse_err)
    {
      /* Parse error in the RPT Status PDU header. We cannot keep parsing this
       * block. */
      bytes_parsed += consume_bad_block(&rsstate->block, datalen, &res);
      lwpa_log(lparams, LWPA_LOG_WARNING,
               MODULE_NAME ": Protocol error encountered while parsing RPT Status PDU header.");
    }
  }
  if (rsstate->block.parsed_header)
  {
    size_t remaining_len = datalen - bytes_parsed;
    switch (smsg->status_code)
    {
      case VECTOR_RPT_STATUS_INVALID_MESSAGE:
      case VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS:
        /* These status codes have no additional data. */
        if (rsstate->block.size_parsed == rsstate->block.block_size)
          res = kPSFullBlockParseOk;
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

        /* These status codes contain an optional status string */
        if (str_len == 0)
        {
#if RDMNET_DYNAMIC_MEM
          smsg->status_string = NULL;
#endif
          res = kPSFullBlockParseOk;
        }
        else if (str_len > RPT_STATUS_STRING_MAXLEN)
        {
          bytes_parsed += consume_bad_block(&rsstate->block, remaining_len, &res);
        }
        else if (remaining_len >= str_len)
        {
#if RDMNET_DYNAMIC_MEM
          char *new_status_string = (char *)malloc(str_len + 1);
          assert(new_status_string);
          memcpy(new_status_string, &data[bytes_parsed], str_len);
          new_status_string[str_len] = '\0';
          smsg->status_string = new_status_string;
#else
          memcpy(smsg->status_string, &data[bytes_parsed], str_len);
          smsg->status_string[str_len] = '\0';
#endif
          bytes_parsed += str_len;
          rsstate->block.size_parsed += str_len;
          res = kPSFullBlockParseOk;
        }
        /* Else return no data */
        break;
      }
      default:
        /* Unknown RPT Status code - discard this RPT Status PDU. */
        bytes_parsed += consume_bad_block(&rsstate->block, remaining_len, &res);
        break;
    }
  }
  *result = res;
  return bytes_parsed;
}

size_t locate_tcp_preamble(RdmnetMsgBuf *msg_buf)
{
  size_t i;
  TcpPreamble preamble;

  if (msg_buf->cur_data_size < ACN_TCP_PREAMBLE_SIZE)
    return 0;

  for (i = 0; i < (msg_buf->cur_data_size - ACN_TCP_PREAMBLE_SIZE); ++i)
  {
    if (parse_tcp_preamble(&msg_buf->buf[i], msg_buf->cur_data_size - i, &preamble))
    {
      /* Discard the data before and including the TCP preamble. */
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
    /* Discard data from the range that has been determined definitively
     * to not contain a TCP preamble. */
    memmove(msg_buf->buf, &msg_buf->buf[i], msg_buf->cur_data_size - i);
    msg_buf->cur_data_size -= i;
  }
  return 0;
}

size_t consume_bad_block(PduBlockState *block, size_t datalen, parse_result_t *parse_res)
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

parse_result_t check_for_full_parse(parse_result_t prev_res, PduBlockState *block)
{
  parse_result_t res = prev_res;
  switch (prev_res)
  {
    case kPSFullBlockParseOk:
    case kPSFullBlockProtErr:
      /* If we're not through the PDU block, need to indicate that to the
       * higher layer. */
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
