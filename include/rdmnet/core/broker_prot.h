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
 * \file rdmnet/core/broker_prot.h
 * \brief Functions to pack, send, and parse %Broker PDUs and their encapsulated messages.
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

/* Connect Reply Data:
 * Connection Code: 2
 * E1.33 Version:   2
 * Broker's UID:    6
 * Client's UID:    6
 * ------------------
 * Total:          16 */
#define BROKER_CONNECT_REPLY_DATA_SIZE 16
#define BROKER_CONNECT_REPLY_FULL_MSG_SIZE (BROKER_PDU_FULL_HEADER_SIZE + BROKER_CONNECT_REPLY_DATA_SIZE)

/*!
 * A flag to indicate whether a client would like to receive notifications when other clients
 * connect and disconnect. Used in the connect_flags field of a BrokerClientConnectMsg or
 * BrokerClientEntryUpdateMsg.
 */
#define BROKER_CONNECT_FLAG_INCREMENTAL_UPDATES 0x01u

/*! Connect status defines for the BrokerConnectReplyMsg. */
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

/*! Disconnect reason defines for the BrokerDisconnectMsg. */
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

/*! Dynamic UID Status Codes for the BrokerDynamicUidMapping struct. */
typedef enum
{
  /*! The Dynamic UID Mapping was fetched or assigned successfully. */
  kRdmnetDynamicUidStatusOk = E133_DYNAMIC_UID_STATUS_OK,
  /*! The corresponding request contained a malformed UID value. */
  kRdmnetDynamicUidStatusInvalidRequest = E133_DYNAMIC_UID_STATUS_INVALID_REQUEST,
  /*! The requested Dynamic UID was not found in the broker's Dynamic UID mapping table. */
  kRdmnetDynamicUidStatusUidNotFound = E133_DYNAMIC_UID_STATUS_UID_NOT_FOUND,
  /*! This RID has already been assigned a Dynamic UID by this broker. */
  kRdmnetDynamicUidStatusDuplicateRid = E133_DYNAMIC_UID_STATUS_DUPLICATE_RID,
  /*! The broker has exhausted its capacity to generate Dynamic UIDs. */
  kRdmnetDynamicUidStatusCapacityExhausted = E133_DYNAMIC_UID_STATUS_CAPACITY_EXHAUSTED
} rdmnet_dynamic_uid_status_t;

/*! The Client Connect message in the broker protocol. */
typedef struct BrokerClientConnectMsg
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
  ClientEntry client_entry;
} BrokerClientConnectMsg;

/*!
 * \brief Safely copy a scope string to a BrokerClientConnectMsg.
 * \param ccmsgptr Pointer to BrokerClientConnectMsg.
 * \param scope_str String to copy to the BrokerClientConnectMsg (const char *).
 */
#define BROKER_CLIENT_CONNECT_MSG_SET_SCOPE(ccmsgptr, scope_str) \
  rdmnet_safe_strncpy((ccmsgptr)->scope, scope_str, E133_SCOPE_STRING_PADDED_LENGTH)

/*!
 * \brief Copy the default scope string to a BrokerClientConnectMsg.
 * \param ccmsgptr Pointer to BrokerClientConnectMsg.
 */
#define BROKER_CLIENT_CONNECT_MSG_SET_DEFAULT_SCOPE(ccmsgptr) \
  RDMNET_MSVC_NO_DEP_WRN strncpy((ccmsgptr)->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH)

/*!
 * \brief Safely copy a search domain string to a BrokerClientConnectMsg.
 * \param ccmsgptr Pointer to BrokerClientConnectMsg.
 * \param search_domain_str String to copy to the BrokerClientConnectMsg (const char *).
 */
#define BROKER_CLIENT_CONNECT_MSG_SET_SEARCH_DOMAIN(ccmsgptr, search_domain_str) \
  rdmnet_safe_strncpy((ccmsgptr)->search_domain, search_domain_str, E133_DOMAIN_STRING_PADDED_LENGTH)

/*!
 * \brief Copy the default search domain string to a BrokerClientConnectMsg.
 * \param ccmsgptr Pointer to BrokerClientConnectMsg.
 */
#define BROKER_CLIENT_CONNECT_MSG_SET_DEFAULT_SEARCH_DOMAIN(ccmsgptr) \
  RDMNET_MSVC_NO_DEP_WRN strncpy((ccmsgptr)->search_domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH)

/*! The Connect Reply message in the broker protocol. */
typedef struct BrokerConnectReplyMsg
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
} BrokerConnectReplyMsg;

/*! The Client Entry Update message in the broker protocol. */
typedef struct BrokerClientEntryUpdateMsg
{
  /*! Configurable options for the connection. See CONNECTFLAG_*. */
  uint8_t connect_flags;
  /*! The new Client Entry. The standard says that it must have the same values for client_protocol
   *  and client_cid as the entry sent on initial connection - only the data section can be
   *  different. */
  ClientEntry client_entry;
} BrokerClientEntryUpdateMsg;

/*! The Client Redirect message in the broker protocol. This struture is used to represent both
 *  CLIENT_REDIRECT_IPV4 and CLIENT_REDIRECT_IPV6. */
typedef struct BrokerClientRedirectMsg
{
  /*! The new IPv4 or IPv6 address to which to connect. */
  EtcPalSockAddr new_addr;
} BrokerClientRedirectMsg;

/*!
 * A structure that represents a list of RPT Client Entries. Represents the data for multiple
 * Broker Protocol messages: Connected Client List, Client Incremental Addition, Client Incremental
 * Deletion, and Client Entry Change.
 */
typedef struct RptClientList
{
  /*! An array of RPT Client Entries. */
  RptClientEntry* client_entries;
  /*! The size of the client_entries array. */
  size_t num_client_entries;
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store Client Entries and must deliver the partial list before continuing.
   * The application should store the entries in the list but should not act on the list until
   * another BrokerClientList is received with more_coming set to false.
   */
  bool more_coming;
} RptClientList;

/*!
 * A structure that represents a list of EPT Client Entries. Represents the data for multiple
 * Broker Protocol messages: Connected Client List, Client Incremental Addition, Client Incremental
 * Deletion, and Client Entry Change.
 */
typedef struct EptClientList
{
  /*! An array of EPT Client Entries. */
  EptClientEntry* client_entries;
  /*! The size of the client_entries array. */
  size_t num_client_entries;
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store Client Entries and must deliver the partial list before continuing.
   * The application should store the entries in the list but should not act on the list until
   * another BrokerClientList is received with more_coming set to false.
   */
  bool more_coming;
} EptClientList;

typedef struct BrokerClientList
{
  client_protocol_t client_protocol;
  union
  {
    RptClientList rpt;
    EptClientList ept;
  } data;
} BrokerClientList;

/*! An entry in a list of Responder IDs (RIDs) which make up a Dynamic UID Request List. */
typedef struct BrokerDynamicUidRequest
{
  uint16_t manu_id;
  EtcPalUuid rid;
} BrokerDynamicUidRequest;

/*! A list of Responder IDs (RIDs) for which dynamic UID assignment is requested. */
typedef struct BrokerDynamicUidRequestList
{
  /*! An array of RIDs for which dynamic UIDs are requested. */
  BrokerDynamicUidRequest* requests;
  /*! The size of the requests array. */
  size_t num_requests;
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store BrokerDynamicUidRequests and must deliver the partial list before
   * continuing. The application should store the entries in the list but should not act on the
   * list until another BrokerDynamicUidRequestList is received with more_coming set to false.
   */
  bool more_coming;
} BrokerDynamicUidRequestList;

/*! A response from a broker to a Dynamic UID Request. */
typedef struct BrokerDynamicUidMapping
{
  /*! The response code - indicates whether the broker was able to assign this dynamic UID. */
  rdmnet_dynamic_uid_status_t status_code;
  /*! The dynamic UID - only valid if status_code is kRdmnetDynamicUidStatusOk. */
  RdmUid uid;
  /*! The corresponding RID for which the dynamic UID was requested. */
  EtcPalUuid rid;
} BrokerDynamicUidMapping;

typedef struct BrokerDynamicUidAssignmentList
{
  /*! An array of dynamic UID mappings. */
  BrokerDynamicUidMapping* mappings;
  /*! The size of the mappings array. */
  size_t num_mappings;
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store BrokerDynamicUidMappings and must deliver the partial list before
   * continuing. The application should store the entries in the list but should not act on the
   * list until another BrokerDynamicUidAssignmentList is received with more_coming set to false.
   */
  bool more_coming;
} BrokerDynamicUidAssignmentList;

/*! A list of Dynamic UIDs for which the currently assigned Responder IDs (RIDs) are being
 *  requested. */
typedef struct BrokerFetchUidAssignmentList
{
  /*! An array of Dynamic UIDs for which RIDs are requested. */
  RdmUid* uids;
  /*! The size of the uids array. */
  size_t num_uids;
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store RdmUids and must deliver the partial list before continuing. The
   * application should store the entries in the list but should not act on the list until another
   * BrokerFetchUidAssignmentList is received with more_coming set to false.
   */
  bool more_coming;
} BrokerFetchUidAssignmentList;

/*! The Disconnect message in the broker protocol. */
typedef struct BrokerDisconnectMsg
{
  /*! The reason for the disconnect event. */
  rdmnet_disconnect_reason_t disconnect_reason;
} BrokerDisconnectMsg;

/*! A broker message. */
typedef struct BrokerMessage
{
  /*! The vector indicates which type of message is present in the data section. Valid values are
   *  indicated by VECTOR_BROKER_* in rdmnet/defs.h. */
  uint16_t vector;
  /*! The encapsulated message; use the helper macros to access it. */
  union
  {
    BrokerClientConnectMsg client_connect;
    BrokerConnectReplyMsg connect_reply;
    BrokerClientEntryUpdateMsg client_entry_update;
    BrokerClientRedirectMsg client_redirect;
    BrokerClientList client_list;
    BrokerDynamicUidRequestList dynamic_uid_request_list;
    BrokerDynamicUidAssignmentList dynamic_uid_assignment_list;
    BrokerFetchUidAssignmentList fetch_uid_assignment_list;
    BrokerDisconnectMsg disconnect;
  } data;
} BrokerMessage;

/*!
 * \brief Determine whether a BrokerMessage contains a Client Connect message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Client Connect message.
 */
#define BROKER_IS_CLIENT_CONNECT_MSG(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CONNECT)

/*!
 * \brief Get the encapsulated Client Connect message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Client Connect message (BrokerClientConnectMsg*).
 */
#define BROKER_GET_CLIENT_CONNECT_MSG(brokermsgptr) (&(brokermsgptr)->data.client_connect)

/*!
 * \brief Determine whether a BrokerMessage contains a Connect Reply message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Connect Reply message.
 */
#define BROKER_IS_CONNECT_REPLY_MSG(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CONNECT_REPLY)

/*!
 * \brief Get the encapsulated Connect Reply message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Connect Reply message (BrokerConnectReplyMsg*).
 */
#define BROKER_GET_CONNECT_REPLY_MSG(brokermsgptr) (&(brokermsgptr)->data.connect_reply)

/*!
 * \brief Determine whether a BrokerMessage contains a Client Entry Update message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Client Entry Update message.
 */
#define BROKER_IS_CLIENT_ENTRY_UPDATE_MSG(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ENTRY_UPDATE)

/*!
 * \brief Get the encapsulated Client Entry Update message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Client Entry Update message (BrokerClientEntryUpdateMsg*).
 */
#define BROKER_GET_CLIENT_ENTRY_UPDATE_MSG(brokermsgptr) (&(brokermsgptr)->data.client_entry_update)

/*!
 * \brief Determine whether a BrokerMessage contains a Client Redirect message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Client Redirect message.
 */
#define BROKER_IS_CLIENT_REDIRECT_MSG(brokermsgptr) \
  ((brokermsgptr)->vector == VECTOR_BROKER_REDIRECT_V4 || (brokermsgptr)->vector == VECTOR_BROKER_REDIRECT_V6)

/*!
 * \brief Get the encapsulated Client Redirect message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Client Redirect message (BrokerClientRedirectMsg*).
 */
#define BROKER_GET_CLIENT_REDIRECT_MSG(brokermsgptr) (&(brokermsgptr)->data.client_redirect)

/*!
 * \brief Determine whether a BrokerMessage contains a Client List. Multiple types of broker
 *        message can contain Client Lists.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Client List.
 */
#define BROKER_IS_CLIENT_LIST(brokermsgptr)                                                                       \
  ((brokermsgptr)->vector == VECTOR_BROKER_CONNECTED_CLIENT_LIST ||                                               \
   (brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ADD || (brokermsgptr)->vector == VECTOR_BROKER_CLIENT_REMOVE || \
   (brokermsgptr)->vector == VECTOR_BROKER_CLIENT_ENTRY_CHANGE)

/*!
 * \brief Get the encapsulated Client List from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Client List (ClientList*).
 */
#define BROKER_GET_CLIENT_LIST(brokermsgptr) (&(brokermsgptr)->data.client_list)

/*!
 * \brief Determine whether a BrokerClientList contains an RPT Client List.
 * \param clientlistptr Pointer to BrokerClientList.
 * \return (bool) whether the message contains an RPT Client List.
 */
#define BROKER_IS_RPT_CLIENT_LIST(clientlistptr) ((clientlistptr)->client_protocol == E133_CLIENT_PROTOCOL_RPT)

/*!
 * \brief Get the encapsulated RPT Client List from a BrokerClientList.
 * \param clientlistptr Pointer to BrokerClientList.
 * \return Pointer to encapsulated RPT Client List (RptClientList*).
 */
#define BROKER_GET_RPT_CLIENT_LIST(clientlistptr) (&(clientlistptr)->data.rpt)

/*!
 * \brief Determine whether a BrokerClientList contains an EPT Client List.
 * \param clientlistptr Pointer to BrokerClientList.
 * \return (bool) whether the message contains an EPT Client List.
 */
#define BROKER_IS_EPT_CLIENT_LIST(clientlistptr) ((clientlistptr)->client_protocol == E133_CLIENT_PROTOCOL_EPT)

/*!
 * \brief Get the encapsulated EPT Client List from a BrokerClientList.
 * \param clientlistptr Pointer to BrokerClientList.
 * \return Pointer to encapsulated EPT Client List (EptClientList*).
 */
#define BROKER_GET_EPT_CLIENT_LIST(clientlistptr) (&(clientlistptr)->data.ept)

/*!
 * \brief Determine whether a BrokerMessage contains a Request Dynamic UID Assignment message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Request Dynamic UID Assignment message.
 */
#define BROKER_IS_REQUEST_DYNAMIC_UID_ASSIGNMENT(brokermsgptr) \
  ((brokermsgptr)->vector == VECTOR_BROKER_REQUEST_DYNAMIC_UIDS)

/*!
 * \brief Get the encapsulated Dynamic UID Request List from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Dynamic UID Request List (BrokerDynamicUidRequestList*).
 */
#define BROKER_GET_DYNAMIC_UID_REQUEST_LIST(brokermsgptr) (&(brokermsgptr)->data.dynamic_uid_request_list)

/*!
 * \brief Determine whether a BrokerMessage contains a Dynamic UID Assignment List message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Dynamic UID Assignment List message.
 */
#define BROKER_IS_DYNAMIC_UID_ASSIGNMENT_LIST(brokermsgptr) \
  ((brokermsgptr)->vector == VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS)

/*!
 * \brief Get the encapsulated Dynamic UID Assignment List from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Dynamic UID Assignment List (BrokerDynamicUidAssignmentList*).
 */
#define BROKER_GET_DYNAMIC_UID_ASSIGNMENT_LIST(brokermsgptr) (&(brokermsgptr)->data.dynamic_uid_assignment_list)

/*!
 * \brief Determine whether a BrokerMessage contains a Fetch Dynamic UID Assignment List message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Fetch Dynamic UID Assignment List message.
 */
#define BROKER_IS_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(brokermsgptr) \
  ((brokermsgptr)->vector == VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST)

/*!
 * \brief Get the encapsulated Fetch Dynamic UID Assignment List from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Fetch Dynamic UID Assignment List (BrokerFetchUidAssignmentList*).
 */
#define BROKER_GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(brokermsgptr) (&(brokermsgptr)->data.fetch_uid_assignment_list)

/*!
 * \brief Determine whether a BrokerMessage contains a Disconnect message.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return (bool) whether the message contains a Disconnect message.
 */
#define BROKER_IS_DISCONNECT_MSG(brokermsgptr) ((brokermsgptr)->vector == VECTOR_BROKER_DISCONNECT)

/*!
 * \brief Get the encapsulated Disconnect message from a BrokerMessage.
 * \param brokermsgptr Pointer to BrokerMessage.
 * \return Pointer to encapsulated Disconnect message (BrokerClientConnectMsg*).
 */
#define BROKER_GET_DISCONNECT_MSG(brokermsgptr) (&(brokermsgptr)->data.disconnect)

size_t broker_get_rpt_client_list_buffer_size(size_t num_client_entries);
size_t broker_get_ept_client_list_buffer_size(const EptClientEntry* client_entries, size_t num_client_entries);
size_t broker_get_uid_assignment_list_buffer_size(size_t num_mappings);

size_t broker_pack_connect_reply(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid,
                                 const BrokerConnectReplyMsg* data);
size_t broker_pack_rpt_client_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, uint16_t vector,
                                   const RptClientEntry* client_entries, size_t num_client_entries);
size_t broker_pack_ept_client_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, uint16_t vector,
                                   const EptClientEntry* client_entries, size_t num_client_entries);
size_t broker_pack_uid_assignment_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid,
                                       const BrokerDynamicUidMapping* mappings, size_t num_mappings);

etcpal_error_t broker_send_connect_reply(rdmnet_conn_t handle, const EtcPalUuid* local_cid,
                                         const BrokerConnectReplyMsg* data);
etcpal_error_t broker_send_fetch_client_list(rdmnet_conn_t handle, const EtcPalUuid* local_cid);
etcpal_error_t broker_send_request_dynamic_uids(rdmnet_conn_t handle, const EtcPalUuid* local_cid,
                                                const BrokerDynamicUidRequest* requests, size_t num_requests);
etcpal_error_t broker_send_fetch_uid_assignment_list(rdmnet_conn_t handle, const EtcPalUuid* local_cid,
                                                     const RdmUid* uids, size_t num_uids);

const char* rdmnet_connect_status_to_string(rdmnet_connect_status_t code);
const char* rdmnet_disconnect_reason_to_string(rdmnet_disconnect_reason_t code);
const char* rdmnet_rdmnet_dynamic_uid_status_to_string(rdmnet_dynamic_uid_status_t code);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* RDMNET_CORE_BROKER_PROT_H_ */
