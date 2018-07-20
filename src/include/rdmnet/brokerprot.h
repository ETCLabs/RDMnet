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
  /*! The remote Component no longer has the ability to support this connection. */
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

/*! The Client Connect message in the Broker protocol. */
typedef struct ClientConnectMsg
{
#if RDMNET_DYNAMIC_MEM && !defined(__DOXYGEN__)
  /*! The Client's configured scope. Maximum length #E133_SCOPE_STRING_PADDED_LENGTH,
   *  including null terminator. */
  const char *scope;
#else
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
#endif
  /*! The maximum version of the standard supported by the Client. */
  uint16_t e133_version;
#if RDMNET_DYNAMIC_MEM && !defined(__DOXYGEN__)
  /*! The search domain of the Client. Maximum length #E133_DOMAIN_STRING_PADDED_LENGTH,
   *  including null terminator. */
  const char *search_domain;
#else
  char search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];
#endif
  /*! Configurable options for the connection. See CONNECTFLAG_*. */
  uint8_t connect_flags;
  /*! The Client's Client Entry. */
  ClientEntryData client_entry;
} ClientConnectMsg;

/*! The Connect Reply message in the Broker protocol. */
typedef struct ConnectReplyMsg
{
  /*! The connection status - kRdmnetConnectOk is the only one that indicates a
   *  successful connection. */
  rdmnet_connect_status_t connect_status;
  /*! The maximum version of the standard supported by the Broker. */
  uint16_t e133_version;
  /*! The Broker's UID for use in RPT and LLRP. */
  LwpaUid broker_uid;
} ConnectReplyMsg;

/*! The Client Entry Update message in the Broker protocol. */
typedef struct ClientEntryUpdateMsg
{
  /*! Configurable options for the connection. See CONNECTFLAG_*. */
  uint8_t connect_flags;
  /*! The new Client Entry. The standard says that it must have the same values
   *  for client_protocol and client_cid as the entry sent on initial connection -
   *  only the data section can be different.
   */
  ClientEntryData client_entry;
} ClientEntryUpdateMsg;

/*! The Client Redirect message in the Broker protocol. This struture is used
 *  to represent both CLIENT_REDIRECT_IPV4 and CLIENT_REDIRECT_IPV6. */
typedef struct ClientRedirectMsg
{
  /*! The new IPv4 or IPv6 address to which to connect. */
  LwpaSockaddr new_addr;
} ClientRedirectMsg;

/*! A structure that represents a list of Client Entries. Represents the data for
 *  multiple Broker Protocol messages: Connected Client List, Client Incremental
 *  Addition, Client Incremental Deletion, and Client Entry Change. */
typedef struct ClientList
{
  /*! This message contains a partial list. This can be set when the library runs out
   *  of static memory in which to store Client Entries and must deliver the partial
   *  list before continuing. The application should store the entries in the list
   *  but should not act on the list until another ClientList is received with
   *  partial set to false. */
  bool partial;
  /*! The head of a linked list of Client Entries. */
  ClientEntryData *client_entry_list;
} ClientList;

/*! The Disconnect message in the Broker protocol. */
typedef struct DisconnectMsg
{
  /*! The reason for the disconnect event. */
  rdmnet_disconnect_reason_t disconnect_reason;
} DisconnectMsg;

/*! A Broker message. */
typedef struct BrokerMessage
{
  /*! The vector indicates which type of message is present in the data section.
   *  Valid values are indicated by VECTOR_BROKER_* in estardmnet.h. */
  uint16_t vector;
  /*! The encapsulated message; use the helper macros to access it. */
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

/*! \brief Determine whether a BrokerMessage contains a Client Connect message.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return (true or false) whether the message contains a Client Connect message. */
#define is_client_connect_msg(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CONNECT)
/*! \brief Get the encapsulated Client Connect message from a BrokerMessage.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return Pointer to encapsulated Client Connect message (ClientConnectMsg *). */
#define get_client_connect_msg(brokermsgptr) (&(brokermsgptr)->data.client_connect)
/*! \brief Determine whether a BrokerMessage contains a Connect Reply message.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return (true or false) whether the message contains a Connect Reply message. */
#define is_connect_reply_msg(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CONNECT_REPLY)
/*! \brief Get the encapsulated Connect Reply message from a BrokerMessage.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return Pointer to encapsulated Connect Reply message (ConnectReplyMsg *). */
#define get_connect_reply_msg(brokermsgptr) (&(brokermsgptr)->data.connect_reply)
/*! \brief Determine whether a BrokerMessage contains a Client Entry Update message.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return (true or false) whether the message contains a Client Entry Update message. */
#define is_client_entry_update_msg(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ENTRY_UPDATE)
/*! \brief Get the encapsulated Client Entry Update message from a BrokerMessage.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return Pointer to encapsulated Client Entry Update message (ClientEntryUpdateMsg *). */
#define get_client_entry_update_msg(brokermsgptr) (&(brokermsgptr)->data.client_entry_update)
/*! \brief Determine whether a BrokerMessage contains a Client Redirect message.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return (true or false) whether the message contains a Client Redirect message. */
#define is_client_redirect_msg(brokermsgptr) \
  ((brokermsgptr)->vector == VECTOR_BROKER_REDIRECT_V4 || (brokermsgptr)->vector == VECTOR_BROKER_REDIRECT_V6)
/*! \brief Get the encapsulated Client Redirect message from a BrokerMessage.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return Pointer to encapsulated Client Redirect message (ClientRedirectMsg *). */
#define get_client_redirect_msg(brokermsgptr) (&(brokermsgptr)->data.client_redirect)
/*! \brief Determine whether a BrokerMessage contains a Client List. Multiple types
 *         of Broker messages can contain Client Lists.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return (true or false) whether the message contains a Client List. */
#define is_client_list(brokermsgptr)                                                                                \
  (&(brokermsgptr)->vector == VECTOR_BROKER_CONNECTED_CLIENT_LIST ||                                                \
   &(brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ADD || &(brokermsgptr)->vector == VECTOR_BROKER_CLIENT_REMOVE || \
   &(brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ENTRY_CHANGE)
/*! \brief Get the encapsulated Client List from a BrokerMessage.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return Pointer to encapsulated Client List (ClientList *). */
#define get_client_list(brokermsgptr) (&(brokermsgptr)->data.client_list)
/*! \brief Determine whether a BrokerMessage contains a Disconnect message.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return (true or false) whether the message contains a Disconnect message. */
#define is_disconnect(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_DISCONNECT)
/*! \brief Get the encapsulated Disconnect message from a BrokerMessage.
 *  \param msgptr Pointer to BrokerMessage.
 *  \return Pointer to encapsulated Disconnect message (ClientConnectMsg *). */
#define get_disconnect_msg(brokermsgptr) (&(brokermsgptr)->data.disconnect)

#ifdef __cplusplus
extern "C" {
#endif

size_t bufsize_client_list(const ClientEntryData *client_entry_list);

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
