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

/*!
 * \file rdmnet/private/msg_buf.h
 * \brief Helper functions and definitions to do piece-wise parsing of an RDMnet message.
 * \author Sam Kearney
 */

#ifndef RDMNET_PRIVATE_MSG_BUF_H_
#define RDMNET_PRIVATE_MSG_BUF_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "etcpal/log.h"
#include "etcpal/uuid.h"
#include "etcpal/socket.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/message.h"
#include "rdmnet/private/opts.h"

typedef enum
{
  kPSNoData,
  kPSPartialBlockParseOk,
  kPSPartialBlockProtErr,
  kPSFullBlockParseOk,
  kPSFullBlockProtErr
} parse_result_t;

typedef struct PduBlockState
{
  size_t block_size;
  size_t size_parsed;
  bool consuming_bad_block;
  bool parsed_header;
} PduBlockState;

#define init_pdu_block_state(blockstateptr, blocksize) \
  do                                                   \
  {                                                    \
    (blockstateptr)->block_size = blocksize;           \
    (blockstateptr)->size_parsed = 0;                  \
    (blockstateptr)->consuming_bad_block = false;      \
    (blockstateptr)->parsed_header = false;            \
  } while (0)

typedef struct GenericListState
{
  size_t full_list_size;
  size_t size_parsed;
} GenericListState;

#define init_generic_list_state(liststateptr, list_size) \
  do                                                     \
  {                                                      \
    (liststateptr)->full_list_size = list_size;          \
    (liststateptr)->size_parsed = 0;                     \
  } while (0)

typedef struct RdmListState
{
  bool parsed_request_notif_header;
  PduBlockState block;
} RdmListState;

#define init_rdm_list_state(rlstateptr, blocksize, rmsgptr) \
  do                                                        \
  {                                                         \
    (rlstateptr)->parsed_request_notif_header = false;      \
    init_pdu_block_state(&(rlstateptr)->block, blocksize);  \
    GET_RDM_BUF_LIST(rmsgptr)->list = NULL;                 \
    GET_RDM_BUF_LIST(rmsgptr)->more_coming = false;         \
  } while (0)

typedef struct RptStatusState
{
  PduBlockState block;
} RptStatusState;

#define init_rpt_status_state(rsstateptr, blocksize)       \
  do                                                       \
  {                                                        \
    init_pdu_block_state(&(rsstateptr)->block, blocksize); \
  } while (0)

typedef struct RptState
{
  PduBlockState block;
  union
  {
    RdmListState rdm_list;
    RptStatusState status;
    PduBlockState unknown;
  } data;
} RptState;

#define init_rpt_state(rstateptr, blocksize)              \
  do                                                      \
  {                                                       \
    init_pdu_block_state(&(rstateptr)->block, blocksize); \
  } while (0)

typedef struct ClientEntryState
{
  size_t enclosing_block_size;
  PduBlockState entry_data;
} ClientEntryState;

#define init_client_entry_state(cstateptr, blocksize, centryptr) \
  do                                                             \
  {                                                              \
    (centryptr)->client_protocol = kClientProtocolUnknown;       \
    (cstateptr)->enclosing_block_size = blocksize;               \
  } while (0)

typedef struct ClientListState
{
  PduBlockState block;
  ClientEntryState entry;
} ClientListState;

#define init_client_list_state(clstateptr, blocksize, bmsgptr) \
  do                                                           \
  {                                                            \
    init_pdu_block_state(&(clstateptr)->block, blocksize);     \
    GET_CLIENT_LIST(bmsgptr)->client_entry_list = NULL;        \
    GET_CLIENT_LIST(bmsgptr)->more_coming = false;             \
  } while (0)

typedef struct ClientConnectState
{
  size_t pdu_data_size;
  bool common_data_parsed;
  ClientEntryState entry;
} ClientConnectState;

#define init_client_connect_state(cstateptr, blocksize, bmsgptr) \
  do                                                             \
  {                                                              \
    (cstateptr)->pdu_data_size = blocksize;                      \
    (cstateptr)->common_data_parsed = false;                     \
  } while (0)

typedef struct ClientEntryUpdateState
{
  size_t pdu_data_size;
  bool common_data_parsed;
  ClientEntryState entry;
} ClientEntryUpdateState;

#define init_client_entry_update_state(ceustateptr, blocksize, bmsgptr) \
  do                                                                    \
  {                                                                     \
    (ceustateptr)->pdu_data_size = blocksize;                           \
    (ceustateptr)->common_data_parsed = false;                          \
  } while (0)

typedef struct BrokerState
{
  PduBlockState block;
  union
  {
    GenericListState data_list;
    ClientListState client_list;
    ClientConnectState client_connect;
    ClientEntryUpdateState update;
    PduBlockState unknown;
  } data;
} BrokerState;

#define init_broker_state(bstateptr, blocksize, msgptr) init_pdu_block_state(&(bstateptr)->block, blocksize)

typedef struct RlpState
{
  PduBlockState block;
  union
  {
    BrokerState broker;
    RptState rpt;
    PduBlockState unknown;
  } data;
} RlpState;

#define init_rlp_state(rlpstateptr, blocksize) init_pdu_block_state(&(rlpstateptr)->block, blocksize)

typedef struct RdmnetMsgBuf
{
  uint8_t buf[RDMNET_RECV_DATA_MAX_SIZE * 2];
  size_t cur_data_size;
  RdmnetMessage msg;

  bool have_preamble;
  RlpState rlp_state;

  const EtcPalLogParams* lparams;
} RdmnetMsgBuf;

#ifdef __cplusplus
extern "C" {
#endif

void rdmnet_msg_buf_init(RdmnetMsgBuf* msg_buf);
etcpal_error_t rdmnet_msg_buf_recv(RdmnetMsgBuf* msg_buf, const uint8_t* data, size_t data_size);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_MSG_BUF_H_ */
