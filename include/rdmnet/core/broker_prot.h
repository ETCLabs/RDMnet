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

/*! \file rdmnet/core/broker_prot.h
 *  \brief Functions to pack, send, and parse %Broker PDUs and their encapsulated messages.
 *  \author Sam Kearney
 */

#ifndef RDMNET_CORE_BROKER_PROT_H_
#define RDMNET_CORE_BROKER_PROT_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "etcpal/acn_rlp.h"
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"
#include "rdmnet/core.h"
#include "rdmnet/core/client_entry.h"
#include "rdmnet/core/util.h"

/*!
 * \addtogroup rdmnet_message
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#define BROKER_PDU_HEADER_SIZE 5
#define BROKER_PDU_FULL_HEADER_SIZE (BROKER_PDU_HEADER_SIZE + ACN_RLP_HEADER_SIZE_EXT_LEN + ACN_TCP_PREAMBLE_SIZE)

#define CONNECT_REPLY_DATA_SIZE \
  (2 /* Connection Code */ + 2 /* E1.33 Version */ + 6 /* broker's UID */ + 6 /* Client's UID */)
#define CONNECT_REPLY_FULL_MSG_SIZE (BROKER_PDU_FULL_HEADER_SIZE + CONNECT_REPLY_DATA_SIZE)

/*!
 * A flag to indicate whether a client would like to receive notifications when other clients
 * connect and disconnect. Used in the connect_flags field of a ClientConnectMsg or
 * ClientEntryUpdateMsg.
 */
#define CONNECTFLAG_INCREMENTAL_UPDATES 0x01u

/*! Connect status defines for the ConnectReplyMsg. */
typedef enum
{
  /*! Connection completed successfully. */
  kRdmnetConnectOk = E133_CONNECT_OK,
  /*! The client's scope does not match the broker's scope. */
  kRdmnetConnectScopeMismatch = E133_CONNECT_SCOPE_MISMATCH,
  /*! The broker has no further capacity for new clients. */
  kRdmnetConnectCapacityExceeded = E133_CONNECT_CAPACITY_EXCEEDED,
  /*! The client's static UID matches another connected client's static UID. */
  kRdmnetConnectDuplicateUid = E133_CONNECT_DUPLICATE_UID,
  /*! The client's Client Entry is invalid. */
  kRdmnetConnectInvalidClientEntry = E133_CONNECT_INVALID_CLIENT_ENTRY,
  /*! The UID sent in the Client Entry PDU is malformed. */
  kRdmnetConnectInvalidUid = E133_CONNECT_INVALID_UID
} rdmnet_connect_status_t;

/*! Disconnect reason defines for the DisconnectMsg. */
typedef enum
{
  /*! The remote component is shutting down. */
  kRdmnetDisconnectShutdown = E133_DISCONNECT_SHUTDOWN,
  /*! The remote component no longer has the ability to support this connection. */
  kRdmnetDisconnectCapacityExhausted = E133_DISCONNECT_CAPACITY_EXHAUSTED,
  /*! The component must disconnect due to an internal hardware fault. */
  kRdmnetDisconnectHardwareFault = E133_DISCONNECT_HARDWARE_FAULT,
  /*! The component must disconnect due to a software fault. */
  kRdmnetDisconnectSoftwareFault = E133_DISCONNECT_SOFTWARE_FAULT,
  /*! The component must terminated because of a software reset. */
  kRdmnetDisconnectSoftwareReset = E133_DISCONNECT_SOFTWARE_RESET,
  /*! Send by brokers that are not on the desired Scope. */
  kRdmnetDisconnectIncorrectScope = E133_DISCONNECT_INCORRECT_SCOPE,
  /*! The component was reconfigured using RPT, and the new configuration requires connection
   *  termination. */
  kRdmnetDisconnectRptReconfigure = E133_DISCONNECT_RPT_RECONFIGURE,
  /*! The component was reconfigured using LLRP, and the new configuration requires connection
   *  termination. */
  kRdmnetDisconnectLlrpReconfigure = E133_DISCONNECT_LLRP_RECONFIGURE,
  /*! The component was reconfigured via some other means, and the new configuration requires
   *  connection termination. */
  kRdmnetDisconnectUserReconfigure = E133_DISCONNECT_USER_RECONFIGURE
} rdmnet_disconnect_reason_t;

/*! Dynamic UID Status Codes for the DynamicUidMapping struct. */
typedef enum
{
  /*! The Dynamic UID Mapping was fetched or assigned successfully. */
  kDynamicUidStatusOk = E133_DYNAMIC_UID_STATUS_OK,
  /*! The corresponding request contained a malformed UID value. */
  kDynamicUidStatusInvalidRequest = E133_DYNAMIC_UID_STATUS_INVALID_REQUEST,
  /*! The requested Dynamic UID was not found in the broker's Dynamic UID mapping table. */
  kDynamicUidStatusUidNotFound = E133_DYNAMIC_UID_STATUS_UID_NOT_FOUND,
  /*! This RID has already been assigned a Dynamic UID by this broker. */
  kDynamicUidStatusDuplicateRid = E133_DYNAMIC_UID_STATUS_DUPLICATE_RID,
  /*! The broker has exhausted its capacity to generate Dynamic UIDs. */
  kDynamicUidStatusCapacityExhausted = E133_DYNAMIC_UID_STATUS_CAPACITY_EXHAUSTED
} dynamic_uid_status_t;

/*! The Client Connect message in the broker protocol. */
typedef struct ClientConnectMsg
{
  /*! The client's configured scope. Maximum length E133_SCOPE_STRING_PADDED_LENGTH, including null
   *  terminator. */
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  /*! The maximum version of the standard supported by the client. */
  uint16_t e133_version;
  /*! The search domain of the client. Maximum length E133_DOMAIN_STRING_PADDED_LENGTH, including
   *  null terminator. */
  char search_domain[E133_DOMAIN_STRING_PADDED_LENGTH];
  /*! Configurable options for the connection. See CONNECTFLAG_*. */
  uint8_t connect_flags;
  /*! The client's Client Entry. */
  ClientEntryData client_entry;
} ClientConnectMsg;

/*!
 * \brief Safely copy a scope string to a ClientConnectMsg.
 * \param ccmsgptr Pointer to ClientConnectMsg.
 * \param scope_str String to copy to the ClientConnectMsg (const char *).
 */
#define CLIENT_CONNECT_MSG_SET_SCOPE(ccmsgptr, scope_str) \
  rdmnet_safe_strncpy((ccmsgptr)->scope, scope_str, E133_SCOPE_STRING_PADDED_LENGTH)

/*!
 * \brief Copy the default scope string to a ClientConnectMsg.
 * \param ccmsgptr Pointer to ClientConnectMsg.
 */
#define CLIENT_CONNECT_MSG_SET_DEFAULT_SCOPE(ccmsgptr) \
  RDMNET_MSVC_NO_DEP_WRN strncpy((ccmsgptr)->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH)

/*!
 * \brief Safely copy a search domain string to a ClientConnectMsg.
 * \param ccmsgptr Pointer to ClientConnectMsg.
 * \param search_domain_str String to copy to the ClientConnectMsg (const char *).
 */
#define CLIENT_CONNECT_MSG_SET_SEARCH_DOMAIN(ccmsgptr, search_domain_str) \
  rdmnet_safe_strncpy((ccmsgptr)->search_domain, search_domain_str, E133_DOMAIN_STRING_PADDED_LENGTH)

/*!
 * \brief Copy the default search domain string to a ClientConnectMsg.
 * \param ccmsgptr Pointer to ClientConnectMsg.
 */
#define CLIENT_CONNECT_MSG_SET_DEFAULT_SEARCH_DOMAIN(ccmsgptr) \
  RDMNET_MSVC_NO_DEP_WRN strncpy((ccmsgptr)->search_domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH)

/*! The Connect Reply message in the broker protocol. */
typedef struct ConnectReplyMsg
{
  /*! The connection status - kRdmnetConnectOk is the only one that indicates a successful
   *  connection. */
  rdmnet_connect_status_t connect_status;
  /*! The maximum version of the standard supported by the broker. */
  uint16_t e133_version;
  /*! The broker's UID for use in RPT and LLRP. */
  RdmUid broker_uid;
  /*! The client's UID for use in RPT and LLRP, either echoed back (Static UID) or assigned by the
   *  broker (Dynamic UID). Set to 0 for a non-RPT Client. */
  RdmUid client_uid;
} ConnectReplyMsg;

/*! The Client Entry Update message in the broker protocol. */
typedef struct ClientEntryUpdateMsg
{
  /*! Configurable options for the connection. See CONNECTFLAG_*. */
  uint8_t connect_flags;
  /*! The new Client Entry. The standard says that it must have the same values for client_protocol
   *  and client_cid as the entry sent on initial connection - only the data section can be
   *  different. */
  ClientEntryData client_entry;
} ClientEntryUpdateMsg;

/*! The Client Redirect message in the broker protocol. This struture is used to represent both
 *  CLIENT_REDIRECT_IPV4 and CLIENT_REDIRECT_IPV6. */
typedef struct ClientRedirectMsg
{
  /*! The new IPv4 or IPv6 address to which to connect. */
  EtcPalSockAddr new_addr;
} ClientRedirectMsg;

/*!
 * A structure that represents a list of Client Entries. Represents the data for multiple broker
 * Protocol messages: Connected Client List, Client Incremental Addition, Client Incremental
 * Deletion, and Client Entry Change.
 */
typedef struct ClientList
{
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store Client Entries and must deliver the partial list before continuing.
   * The application should store the entries in the list but should not act on the list until
   * another ClientList is received with partial set to false.
   */
  bool more_coming;
  /*! The head of a linked list of Client Entries. */
  ClientEntryData* client_entry_list;
} ClientList;

typedef struct DynamicUidRequestListEntry DynamicUidRequestListEntry;
/*! An entry in a linked list of Responder IDs (RIDs) which make up a Dynamic UID Request List. */
struct DynamicUidRequestListEntry
{
  uint16_t manu_id;
  EtcPalUuid rid;
  DynamicUidRequestListEntry* next;
};

/*! A list of Responder IDs (RIDs) for which Dynamic UID assignment is requested. */
typedef struct DynamicUidRequestList
{
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store DynamicUidRequestListEntrys and must deliver the partial list before
   * continuing. The application should store the entries in the list but should not act on the
   * list until another DynamicUidRequestList is received with partial set to false.
   */
  bool more_coming;
  /*! The head of a linked list of RIDs for which Dynamic UIDs are requested. */
  DynamicUidRequestListEntry* request_list;
} DynamicUidRequestList;

typedef struct DynamicUidMapping DynamicUidMapping;

struct DynamicUidMapping
{
  dynamic_uid_status_t status_code;
  RdmUid uid;
  EtcPalUuid rid;
  DynamicUidMapping* next;
};

typedef struct DynamicUidAssignmentList
{
  bool more_coming;
  DynamicUidMapping* mapping_list;
} DynamicUidAssignmentList;

typedef struct FetchUidAssignmentListEntry FetchUidAssignmentListEntry;
/*! An entry in a linked list of UIDs which make up the data of a Fetch Dynamic UID Assignment List
 *  message. */
struct FetchUidAssignmentListEntry
{
  RdmUid uid;
  FetchUidAssignmentListEntry* next;
};

/*! A list of Dynamic UIDs for which the currently assigned Responder IDs (RIDs) are being
 *  requested. */
typedef struct FetchUidAssignmentList
{
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store FetchUidAssignmentListEntrys and must deliver the partial list before
   * continuing. The application should store the entries in the list but should not act on the
   * list until another DynamicUidRequestList is received with partial set to false.
   */
  bool more_coming;
  /*! The head of a linked list of Dynamic UIDs for which the currently assigned RIDs are being
   *  requested. */
  FetchUidAssignmentListEntry* assignment_list;
} FetchUidAssignmentList;

/*! The Disconnect message in the broker protocol. */
typedef struct DisconnectMsg
{
  /*! The reason for the disconnect event. */
  rdmnet_disconnect_reason_t disconnect_reason;
} DisconnectMsg;

/*! A broker message. */
typedef struct BrokerMessage
{
  /*! The vector indicates which type of message is present in the data section. Valid values are
   *  indicated by VECTOR_BROKER_* in rdmnet/defs.h. */
  uint16_t vector;
  /*! The encapsulated message; use the helper macros to access it. */
  union
  {
    ClientConnectMsg client_connect;
    ConnectReplyMsg connect_reply;
    ClientEntryUpdateMsg client_entry_update;
    ClientRedirectMsg client_redirect;
    ClientList client_list;
    DynamicUidRequestList dynamic_uid_request_list;
    DynamicUidAssignmentList dynamic_uid_assignment_list;
    FetchUidAssignmentList fetch_uid_assignment_list;
    DisconnectMsg disconnect;
  } data;
} BrokerMessage;

/*!
 * \brief Determine whether a BrokerMessage contains a Client Connect message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Client Connect message.
 */
#define IS_CLIENT_CONNECT_MSG(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CONNECT)

/*!
 * \brief Get the encapsulated Client Connect message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Client Connect message (ClientConnectMsg *).
 */
#define GET_CLIENT_CONNECT_MSG(brokermsgptr) (&(brokermsgptr)->data.client_connect)

/*!
 * \brief Determine whether a BrokerMessage contains a Connect Reply message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Connect Reply message.
 */
#define IS_CONNECT_REPLY_MSG(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CONNECT_REPLY)

/*!
 * \brief Get the encapsulated Connect Reply message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Connect Reply message (ConnectReplyMsg *).
 */
#define GET_CONNECT_REPLY_MSG(brokermsgptr) (&(brokermsgptr)->data.connect_reply)

/*!
 * \brief Determine whether a BrokerMessage contains a Client Entry Update message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Client Entry Update message.
 */
#define IS_CLIENT_ENTRY_UPDATE_MSG(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ENTRY_UPDATE)

/*!
 * \brief Get the encapsulated Client Entry Update message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Client Entry Update message (ClientEntryUpdateMsg *).
 */
#define GET_CLIENT_ENTRY_UPDATE_MSG(brokermsgptr) (&(brokermsgptr)->data.client_entry_update)

/*!
 * \brief Determine whether a BrokerMessage contains a Client Redirect message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Client Redirect message.
 */
#define IS_CLIENT_REDIRECT_MSG(brokermsgptr) \
  ((brokermsgptr)->vector == VECTOR_BROKER_REDIRECT_V4 || (brokermsgptr)->vector == VECTOR_BROKER_REDIRECT_V6)

/*!
 * \brief Get the encapsulated Client Redirect message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Client Redirect message (ClientRedirectMsg *).
 */
#define GET_CLIENT_REDIRECT_MSG(brokermsgptr) (&(brokermsgptr)->data.client_redirect)

/*!
 * \brief Determine whether a BrokerMessage contains a Client List. Multiple types of broker
 *        message can contain Client Lists.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Client List.
 */
#define IS_CLIENT_LIST(brokermsgptr)                                                                              \
  ((brokermsgptr)->vector == VECTOR_BROKER_CONNECTED_CLIENT_LIST ||                                               \
   (brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ADD || (brokermsgptr)->vector == VECTOR_BROKER_CLIENT_REMOVE || \
   (brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ENTRY_CHANGE)

/*!
 * \brief Get the encapsulated Client List from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Client List (ClientList *).
 */
#define GET_CLIENT_LIST(brokermsgptr) (&(brokermsgptr)->data.client_list)

/*!
 * \brief Determine whether a BrokerMessage contains a Request Dynamic UID Assignment message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Request Dynamic UID Assignment message.
 */
#define IS_REQUEST_DYNAMIC_UID_ASSIGNMENT(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_REQUEST_DYNAMIC_UIDS)

/*!
 * \brief Get the encapsulated Dynamic UID Request List from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Dynamic UID Request List (DynamicUidRequestList *).
 */
#define GET_DYNAMIC_UID_REQUEST_LIST(brokermsgptr) (&(brokermsgptr)->data.dynamic_uid_request_list)

/*!
 * \brief Determine whether a BrokerMessage contains a Dynamic UID Assignment List message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Dynamic UID Assignment List message.
 */
#define IS_DYNAMIC_UID_ASSIGNMENT_LIST(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS)

/*!
 * \brief Get the encapsulated Dynamic UID Assignment List from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Dynamic UID Assignment List (DynamicUidAssignmentList *).
 */
#define GET_DYNAMIC_UID_ASSIGNMENT_LIST(brokermsgptr) (&(brokermsgptr)->data.dynamic_uid_assignment_list)

/*!
 * \brief Determine whether a BrokerMessage contains a Fetch Dynamic UID Assignment List message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Fetch Dynamic UID Assignment List message.
 */
#define IS_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(brokermsgptr) \
  ((brokermsgptr)->vector == VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST)

/*!
 * \brief Get the encapsulated Fetch Dynamic UID Assignment List from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Fetch Dynamic UID Assignment List (FetchUidAssignmentList *).
 */
#define GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(brokermsgptr) (&(brokermsgptr)->data.fetch_uid_assignment_list)

/*!
 * \brief Determine whether a BrokerMessage contains a Disconnect message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (true or false) whether the message contains a Disconnect message.
 */
#define IS_DISCONNECT_MSG(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_DISCONNECT)

/*!
 * \brief Get the encapsulated Disconnect message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Disconnect message (ClientConnectMsg *).
 */
#define GET_DISCONNECT_MSG(brokermsgptr) (&(brokermsgptr)->data.disconnect)

size_t bufsize_client_list(const ClientEntryData* client_entry_list);
size_t bufsize_dynamic_uid_assignment_list(const DynamicUidMapping* mapping_list);

size_t pack_connect_reply(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, const ConnectReplyMsg* data);
size_t pack_client_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, uint16_t vector,
                        const ClientEntryData* client_entry_list);
size_t pack_dynamic_uid_assignment_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid,
                                        const DynamicUidMapping* mapping_list);

etcpal_error_t send_connect_reply(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const ConnectReplyMsg* data);
etcpal_error_t send_fetch_client_list(rdmnet_conn_t handle, const EtcPalUuid* local_cid);
etcpal_error_t send_request_dynamic_uids(rdmnet_conn_t handle, const EtcPalUuid* local_cid,
                                         const DynamicUidRequestListEntry* request_list);
etcpal_error_t send_fetch_uid_assignment_list(rdmnet_conn_t handle, const EtcPalUuid* local_cid,
                                              const FetchUidAssignmentListEntry* uid_list);

const char* rdmnet_connect_status_to_string(rdmnet_connect_status_t code);
const char* rdmnet_disconnect_reason_to_string(rdmnet_disconnect_reason_t code);
const char* rdmnet_dynamic_uid_status_to_string(dynamic_uid_status_t code);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* RDMNET_CORE_BROKER_PROT_H_ */
