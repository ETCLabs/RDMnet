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

/*! \file rdmnet/brokerprot.h
 *  \brief Functions to pack, send, and parse %Broker PDUs and their
 *         encapsulated messages.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_BROKERPROT_H_
#define _RDMNET_BROKERPROT_H_

#include "lwpa_int.h"
#include "lwpa_bool.h"
#include "lwpa_error.h"
#include "lwpa_uid.h"
#include "lwpa_inet.h"
#include "lwpa_rootlayerpdu.h"
#include "estardmnet.h"
#include "rdmnet/opts.h"
#include "rdmnet/client.h"

/*! \addtogroup rdmnet_message
 *  @{
 */

#define BROKER_PDU_HEADER_SIZE 5
#define BROKER_PDU_FULL_HEADER_SIZE (BROKER_PDU_HEADER_SIZE + RLP_HEADER_SIZE_EXT_LEN + ACN_TCP_PREAMBLE_SIZE)

#define CONNECT_REPLY_DATA_SIZE (2 /* Connection Code */ + 2 /* E1.33 Version */ + 6 /* Broker's UID */)
#define CONNECT_REPLY_FULL_MSG_SIZE (BROKER_PDU_FULL_HEADER_SIZE + CONNECT_REPLY_DATA_SIZE)

/*! A flag to indicate whether a client would like to receive notifications
 *  when other clients connect and disconnect. Used in the connect_flags field
 *  of a ClientConnectMsg or ClientEntryUpdateMsg.
 */
#define CONNECTFLAG_INCREMENTAL_UPDATES 0x01u

/*! Connect status defines for the ConnectReplyMsg. */
typedef enum
{
  /*! Connection completed successfully. */
  kRDMnetConnectOk = E133_CONNECT_OK,
  /*! The Client's scope does not match the %Broker's scope. */
  kRDMnetConnectScopeMismatch = E133_CONNECT_SCOPE_MISMATCH,
  /*! The %Broker has no further capacity for new Clients. */
  kRDMnetConnectCapacityExceeded = E133_CONNECT_CAPACITY_EXCEEDED,
  /*! The Client's Dynamic UID matches another connected Client's Dynamic
   *  UID. */
  kRDMnetConnectDuplicateUID = E133_CONNECT_DUPLICATE_UID,
  /*! The Client's Client Entry is invalid. */
  kRDMnetConnectInvalidClientEntry = E133_CONNECT_INVALID_CLIENT_ENTRY
} rdmnet_connect_status_t;

/*! Disconnect reason defines for the DisconnectMsg. */
typedef enum
{
  /*! The remote Component is shutting down. */
  kRDMnetDisconnectShutdown = E133_DISCONNECT_SHUTDOWN,
  /*! The remote Component no longer has the ability to support this
   *  connection. */
  kRDMnetDisconnectCapacityExhausted = E133_DISCONNECT_CAPACITY_EXHAUSTED,
  /*! Not a valid reason, removed from next revision. */
  kRDMnetDisconnectIncorrectClientType = E133_DISCONNECT_INCORRECT_CLIENT_TYPE,
  /*! The Component must disconnect due to an internal hardware fault. */
  kRDMnetDisconnectHardwareFault = E133_DISCONNECT_HARDWARE_FAULT,
  /*! The Component must disconnect due to a software fault. */
  kRDMnetDisconnectSoftwareFault = E133_DISCONNECT_SOFTWARE_FAULT,
  /*! The Component must terminated because of a software reset. */
  kRDMnetDisconnectSoftwareReset = E133_DISCONNECT_SOFTWARE_RESET,
  /*! Send by %Brokers that are not on the desired Scope. */
  kRDMnetDisconnectIncorrectScope = E133_DISCONNECT_INCORRECT_SCOPE,
  /*! The Component was reconfigured using LLRP, and the new configuration
   *  requires connection termination. */
  kRDMnetDisconnectLLRPReconfigure = E133_DISCONNECT_LLRP_RECONFIGURE,
  /*! The Component was reconfigured via some other means, and the new
   *  configuration requires connection termination. */
  kRDMnetDisconnectUserReconfigure = E133_DISCONNECT_USER_RECONFIGURE
} rdmnet_disconnect_reason_t;

typedef struct ClientConnectMsg
{
#if RDMNET_DYNAMIC_MEM && !defined(__DOXYGEN__)
  char *scope;
#else
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
#endif
  uint16_t e133_version;
#if RDMNET_DYNAMIC_MEM && !defined(__DOXYGEN__)
  char *search_domain;
#else
  char search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];
#endif
  uint8_t connect_flags;
  ClientEntryData client_entry;
} ClientConnectMsg;

typedef struct ConnectReplyMsg
{
  rdmnet_connect_status_t connect_status;
  uint16_t e133_version;
  LwpaUid broker_uid;
} ConnectReplyMsg;

typedef struct ClientEntryUpdateMsg
{
  uint8_t connect_flags;
  ClientEntryData client_entry;
} ClientEntryUpdateMsg;

typedef struct ClientRedirectMsg
{
  LwpaSockaddr new_addr;
} ClientRedirectMsg;

typedef struct ClientList
{
  bool partial;
  ClientEntryData *client_entry_list;
} ClientList;

typedef struct DisconnectMsg
{
  rdmnet_disconnect_reason_t disconnect_reason;
} DisconnectMsg;

typedef struct BrokerMessage
{
  uint16_t vector;
  union
  {
    ClientConnectMsg client_connect;
    ConnectReplyMsg connect_reply;
    ClientEntryUpdateMsg client_entry_update;
    ClientRedirectMsg client_redirect;
    ClientList client_list;
    DisconnectMsg disconnect;
  } data;
} BrokerMessage;

#define is_client_connect_msg(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CONNECT)
#define get_client_connect_msg(brokermsgptr) (&(brokermsgptr)->data.client_connect)
#define is_connect_reply_msg(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CONNECT_REPLY)
#define get_connect_reply_msg(brokermsgptr) (&(brokermsgptr)->data.connect_reply)
#define is_client_entry_update_msg(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ENTRY_UPDATE)
#define get_client_entry_update_msg(brokermsgptr) (&(brokermsgptr)->data.client_entry_update)
#define is_client_redirect_msg(brokermsgptr) \
  ((brokermsgptr)->vector == VECTOR_BROKER_REDIRECT_V4 || (brokermsgptr)->vector == VECTOR_BROKER_REDIRECT_V6)
#define get_client_redirect_msg(brokermsgptr) (&(brokermsgptr)->data.client_redirect)
#define is_client_list(brokermsgptr)                                                                                \
  (&(brokermsgptr)->vector == VECTOR_BROKER_CONNECTED_CLIENT_LIST ||                                                \
   &(brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ADD || &(brokermsgptr)->vector == VECTOR_BROKER_CLIENT_REMOVE || \
   &(brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ENTRY_CHANGE)
#define get_client_list(brokermsgptr) (&(brokermsgptr)->data.client_list)
#define is_disconnect(brokermsgptr) (&(brokermsgptr)->vector == VECTOR_BROKER_DISCONNECT)
#define get_disconnect_msg(brokermsgptr) (&(brokermsgptr)->data.disconnect)

#ifdef __cplusplus
extern "C" {
#endif

size_t bufsize_client_list(ClientEntryData *client_entry_list);

size_t pack_connect_reply(uint8_t *buf, size_t buflen, const LwpaCid *local_cid, const ConnectReplyMsg *data);
size_t pack_client_list(uint8_t *buf, size_t buflen, const LwpaCid *local_cid, uint16_t vector,
                        ClientEntryData *client_entry_list);

lwpa_error_t send_connect_reply(int handle, const LwpaCid *local_cid, const ConnectReplyMsg *data);

lwpa_error_t send_fetch_client_list(int handle, const LwpaCid *local_cid);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _RDMNET_BROKERPROT_H_ */
