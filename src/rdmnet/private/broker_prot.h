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
 * \file rdmnet/private/broker_prot.h
 * \brief Functions and definitions for Broker PDU messages that are only used internally.
 */

#ifndef RDMNET_PRIVATE_BROKER_PROT_
#define RDMNET_PRIVATE_BROKER_PROT_

#include "etcpal/error.h"
#include "rdmnet/core/broker_prot.h"
#include "rdmnet/private/connection.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BROKER_NULL_MSG_SIZE BROKER_PDU_HEADER_SIZE

/***************************** Client Entry Sizes ****************************/

/* Client Entry Header:
 * Flags + Length:  3
 * Vector:          4
 * CID:            16
 * ------------------
 * Total:          23 */
#define CLIENT_ENTRY_HEADER_SIZE 23
/* RPT Client Entry Data:
 * Client UID:   6
 * Client Type:  1
 * Binding CID: 16
 * ---------------
 * Total:       23 */
#define RPT_CLIENT_ENTRY_DATA_SIZE 23
/* EPT Protocol Entry:
 * Protocol Vector:  4
 * Protocol String: 32
 * -------------------
 * Total:           36 */
#define EPT_PROTOCOL_ENTRY_SIZE 36
#define RPT_CLIENT_ENTRY_SIZE (CLIENT_ENTRY_HEADER_SIZE + RPT_CLIENT_ENTRY_DATA_SIZE)
#define CLIENT_ENTRY_MIN_SIZE RPT_CLIENT_ENTRY_SIZE

/**************************** Client Connect Sizes ***************************/

/* Client Connect Common Fields:
 * Scope:         [Referenced]
 * E1.33 Version:            2
 * Search Domain: [Referenced]
 * Connect Flags:            1
 * ---------------------------
 * Total non-referenced:     3 */
#define CLIENT_CONNECT_COMMON_FIELD_SIZE (3 + E133_SCOPE_STRING_PADDED_LENGTH + E133_DOMAIN_STRING_PADDED_LENGTH)
#define CLIENT_CONNECT_DATA_MIN_SIZE (CLIENT_CONNECT_COMMON_FIELD_SIZE + CLIENT_ENTRY_HEADER_SIZE)

/************************* Client Entry Update Sizes *************************/

#define CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE 1 /* One field: Connect Flags */
#define CLIENT_ENTRY_UPDATE_DATA_MIN_SIZE (CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE + CLIENT_ENTRY_HEADER_SIZE)

/*************************** Client Redirect Sizes ***************************/

/* Client Redirect IPv4 Data:
 * IPv4 Address: 4
 * Port:         2
 * ---------------
 * Total:        6 */
#define REDIRECT_V4_DATA_SIZE 6
/* Client Redirect IPv6 Data:
 * IPv6 Address: 16
 * Port:          2
 * ----------------
 * Total:        18 */
#define REDIRECT_V6_DATA_SIZE 18

/************************* Request Dynamic UIDs Sizes ************************/

/* Dynamic UID Request Pair:
 * Dynamic UID Request:  6
 * RID:                 16
 * -----------------------
 * Total:               22 */
#define DYNAMIC_UID_REQUEST_PAIR_SIZE 22

/********************* Dynamic UID Assignment List Sizes *********************/

/* Dynamic UID Mapping:
 * Dynamic UID:  6
 * RID:         16
 * Status Code:  2
 * ---------------
 * Total:       24 */
#define DYNAMIC_UID_MAPPING_SIZE 24

/****************************** Disconnect Sizes *****************************/

#define DISCONNECT_DATA_SIZE 2 /* One field: Disconnect Reason */
#define BROKER_DISCONNECT_MSG_SIZE (BROKER_PDU_HEADER_SIZE + DISCONNECT_DATA_SIZE)

/* All functions must be called from inside the relevant send locks. */
etcpal_error_t send_client_connect(RdmnetConnection* conn, const BrokerClientConnectMsg* data);
etcpal_error_t send_disconnect(RdmnetConnection* conn, const BrokerDisconnectMsg* data);
etcpal_error_t send_null(RdmnetConnection* conn);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_BROKER_PROT_H_ */
