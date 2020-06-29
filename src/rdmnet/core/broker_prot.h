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

/*
 * rdmnet/core/broker_prot.h
 * Functions to pack, send, and parse Broker PDUs and their encapsulated messages.
 */

#ifndef RDMNET_CORE_BROKER_PROT_H_
#define RDMNET_CORE_BROKER_PROT_H_

#include <stdint.h>
#include <string.h>
#include "etcpal/acn_rlp.h"
#include "etcpal/error.h"
#include "etcpal/uuid.h"
#include "rdmnet/defs.h"
#include "rdmnet/common.h"
#include "rdmnet/core/broker_message.h"
#include "rdmnet/core/connection.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BROKER_PDU_HEADER_SIZE 5
#define BROKER_PDU_FULL_HEADER_SIZE (BROKER_PDU_HEADER_SIZE + ACN_RLP_HEADER_SIZE_EXT_LEN + ACN_TCP_PREAMBLE_SIZE)

/***************************** Client Entry Sizes ****************************/

/*
 * Client Entry Header:
 * Flags + Length:  3
 * Vector:          4
 * CID:            16
 * ------------------
 * Total:          23
 */
#define CLIENT_ENTRY_HEADER_SIZE 23
/*
 * RPT Client Entry Data:
 * Client UID:   6
 * Client Type:  1
 * Binding CID: 16
 * ---------------
 * Total:       23
 */
#define RPT_CLIENT_ENTRY_DATA_SIZE 23
/*
 * EPT Protocol Entry:
 * Protocol Vector:  4
 * Protocol String: 32
 * -------------------
 * Total:           36
 */
#define EPT_PROTOCOL_ENTRY_SIZE 36
#define RPT_CLIENT_ENTRY_SIZE (CLIENT_ENTRY_HEADER_SIZE + RPT_CLIENT_ENTRY_DATA_SIZE)
#define CLIENT_ENTRY_MIN_SIZE RPT_CLIENT_ENTRY_SIZE

/**************************** Client Connect Sizes ***************************/

/*
 * Client Connect Common Fields:
 * Scope:         [Referenced]
 * E1.33 Version:            2
 * Search Domain: [Referenced]
 * Connect Flags:            1
 * ---------------------------
 * Total non-referenced:     3
 */
#define CLIENT_CONNECT_COMMON_FIELD_SIZE (3 + E133_SCOPE_STRING_PADDED_LENGTH + E133_DOMAIN_STRING_PADDED_LENGTH)
#define CLIENT_CONNECT_DATA_MIN_SIZE (CLIENT_CONNECT_COMMON_FIELD_SIZE + CLIENT_ENTRY_HEADER_SIZE)

/**************************** Connect Reply Sizes ****************************/

/*
 * Connect Reply Data:
 * Connection Code: 2
 * E1.33 Version:   2
 * Broker's UID:    6
 * Client's UID:    6
 * ------------------
 * Total:          16
 */
#define BROKER_CONNECT_REPLY_DATA_SIZE 16
#define BROKER_CONNECT_REPLY_FULL_MSG_SIZE (BROKER_PDU_FULL_HEADER_SIZE + BROKER_CONNECT_REPLY_DATA_SIZE)

/************************* Client Entry Update Sizes *************************/

#define CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE 1 /* One field: Connect Flags */
#define CLIENT_ENTRY_UPDATE_DATA_MIN_SIZE (CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE + CLIENT_ENTRY_HEADER_SIZE)

/*************************** Client Redirect Sizes ***************************/

/*
 * Client Redirect IPv4 Data:
 * IPv4 Address: 4
 * Port:         2
 * ---------------
 * Total:        6
 */
#define REDIRECT_V4_DATA_SIZE 6
/*
 * Client Redirect IPv6 Data:
 * IPv6 Address: 16
 * Port:          2
 * ----------------
 * Total:        18
 */
#define REDIRECT_V6_DATA_SIZE 18

/************************* Request Dynamic UIDs Sizes ************************/

/*
 * Dynamic UID Request Pair:
 * Dynamic UID Request:  6
 * RID:                 16
 * -----------------------
 * Total:               22
 */
#define DYNAMIC_UID_REQUEST_PAIR_SIZE 22

/********************* Dynamic UID Assignment List Sizes *********************/

// Dynamic UID Mapping:
// Dynamic UID:  6
// RID:         16
// Status Code:  2
// ---------------
// Total:       24
#define DYNAMIC_UID_MAPPING_SIZE 24

/****************************** Disconnect Sizes *****************************/

#define DISCONNECT_DATA_SIZE 2  // One field: Disconnect Reason
#define BROKER_DISCONNECT_MSG_SIZE (BROKER_PDU_HEADER_SIZE + DISCONNECT_DATA_SIZE)
#define BROKER_DISCONNECT_FULL_MSG_SIZE (BROKER_PDU_FULL_HEADER_SIZE + DISCONNECT_DATA_SIZE)

/********************************* Null Sizes ********************************/

#define BROKER_NULL_MSG_SIZE BROKER_PDU_HEADER_SIZE
#define BROKER_NULL_FULL_MSG_SIZE BROKER_PDU_FULL_HEADER_SIZE

// A flag to indicate whether a client would like to receive notifications when other clients
// connect and disconnect. Used in the connect_flags field of a BrokerClientConnectMsg or
// BrokerClientEntryUpdateMsg.
#define BROKER_CONNECT_FLAG_INCREMENTAL_UPDATES 0x01u

size_t rc_broker_get_rpt_client_list_buffer_size(size_t num_client_entries);
size_t rc_broker_get_ept_client_list_buffer_size(const RdmnetEptClientEntry* client_entries, size_t num_client_entries);
size_t rc_broker_get_uid_assignment_list_buffer_size(size_t num_mappings);

size_t rc_broker_pack_connect_reply(uint8_t*                     buf,
                                    size_t                       buflen,
                                    const EtcPalUuid*            local_cid,
                                    const BrokerConnectReplyMsg* data);
size_t rc_broker_pack_rpt_client_list(uint8_t*                    buf,
                                      size_t                      buflen,
                                      const EtcPalUuid*           local_cid,
                                      uint16_t                    vector,
                                      const RdmnetRptClientEntry* client_entries,
                                      size_t                      num_client_entries);
size_t rc_broker_pack_ept_client_list(uint8_t*                    buf,
                                      size_t                      buflen,
                                      const EtcPalUuid*           local_cid,
                                      uint16_t                    vector,
                                      const RdmnetEptClientEntry* client_entries,
                                      size_t                      num_client_entries);
size_t rc_broker_pack_uid_assignment_list(uint8_t*                       buf,
                                          size_t                         buflen,
                                          const EtcPalUuid*              local_cid,
                                          const RdmnetDynamicUidMapping* mappings,
                                          size_t                         num_mappings);
size_t rc_broker_pack_null(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid);

etcpal_error_t rc_broker_send_client_connect(RCConnection* conn, const BrokerClientConnectMsg* data);
etcpal_error_t rc_broker_send_fetch_client_list(RCConnection* conn, const EtcPalUuid* local_cid);
etcpal_error_t rc_broker_send_request_dynamic_uids(RCConnection*     conn,
                                                   const EtcPalUuid* local_cid,
                                                   uint16_t          manufacturer_id,
                                                   const EtcPalUuid* rids,
                                                   size_t            num_rids);
etcpal_error_t rc_broker_send_fetch_uid_assignment_list(RCConnection*     conn,
                                                        const EtcPalUuid* local_cid,
                                                        const RdmUid*     uids,
                                                        size_t            num_uids);
etcpal_error_t rc_broker_send_disconnect(RCConnection* conn, const BrokerDisconnectMsg* data);
etcpal_error_t rc_broker_send_null(RCConnection* conn);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_BROKER_PROT_H_ */
