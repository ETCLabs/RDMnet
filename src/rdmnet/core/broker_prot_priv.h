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

/*! \file broker_prot_priv.h
 *  \brief Functions and definitions for Broker PDU messages that are only used internally.
 *  \author Sam Kearney
 */
#ifndef _BROKER_PROT_PRIV_H_
#define _BROKER_PROT_PRIV_H_

#include "lwpa/error.h"
#include "rdmnet/core/broker_prot.h"
#include "connection_priv.h"

#define BROKER_NULL_MSG_SIZE BROKER_PDU_HEADER_SIZE

/***************************** Client Entry Sizes ****************************/

#define CLIENT_ENTRY_HEADER_SIZE (3 /* Flags + length */ + 4 /* Vector */ + 16 /* CID */)
#define RPT_CLIENT_ENTRY_DATA_SIZE (6 /* Client UID */ + 1 /* Client Type */ + 16 /* Binding CID */)
#define EPT_PROTOCOL_ENTRY_SIZE (4 /* Protocol Vector */ + 32 /* Protocol String */)
#define RPT_CLIENT_ENTRY_SIZE (CLIENT_ENTRY_HEADER_SIZE + RPT_CLIENT_ENTRY_DATA_SIZE)
#define CLIENT_ENTRY_MIN_SIZE RPT_CLIENT_ENTRY_SIZE

/**************************** Client Connect Sizes ***************************/

#define CLIENT_CONNECT_COMMON_FIELD_SIZE                                 \
  (E133_SCOPE_STRING_PADDED_LENGTH /* Scope */ + 2 /* E1.33 Version */ + \
   E133_DOMAIN_STRING_PADDED_LENGTH /* Search Domain */ + 1 /* Connect Flags */)
#define CLIENT_CONNECT_DATA_MIN_SIZE (CLIENT_CONNECT_COMMON_FIELD_SIZE + CLIENT_ENTRY_HEADER_SIZE)

/************************* Client Entry Update Sizes *************************/

#define CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE (1 /* Connect Flags */)
#define CLIENT_ENTRY_UPDATE_DATA_MIN_SIZE (CLIENT_ENTRY_UPDATE_COMMON_FIELD_SIZE + CLIENT_ENTRY_HEADER_SIZE)

/*************************** Client Redirect Sizes ***************************/

#define REDIRECT_V4_DATA_SIZE (4 /* IPv4 address */ + 2 /* Port */)
#define REDIRECT_V6_DATA_SIZE (16 /* IPv6 address */ + 2 /* Port */)

/************************* Request Dynamic UIDs Sizes ************************/

#define DYNAMIC_UID_REQUEST_PAIR_SIZE (6 /* Dynamic UID Request */ + 16 /* RID */)

/********************* Dynamic UID Assignment List Sizes *********************/

#define DYNAMIC_UID_MAPPING_SIZE (6 /* Dynamic UID */ + 16 /* RID */ + 2 /* Status Code */)

/****************************** Disconnect Sizes *****************************/

#define DISCONNECT_DATA_SIZE 2 /* Disconnect Reason */
#define BROKER_DISCONNECT_MSG_SIZE (BROKER_PDU_HEADER_SIZE + DISCONNECT_DATA_SIZE)

#ifdef __cplusplus
extern "C" {
#endif

/* All functions must be called from inside the relevant send locks. */
lwpa_error_t send_client_connect(RdmnetConnection *conn, const ClientConnectMsg *data);
lwpa_error_t send_disconnect(RdmnetConnection *conn, const DisconnectMsg *data);
lwpa_error_t send_null(RdmnetConnection *conn);

#ifdef __cplusplus
}
#endif

#endif /* _BROKERPROT_PRIV_H_ */
