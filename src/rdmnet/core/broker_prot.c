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

#include "rdmnet/core/broker_prot.h"

#include <string.h>
#include "etcpal/pack.h"
#include "rdmnet/core/util.h"
#include "rdmnet/private/connection.h"
#include "rdmnet/private/broker_prot.h"

/***************************** Private macros ********************************/

#define PACK_BROKER_HEADER(length, vector, buf) \
  do                                            \
  {                                             \
    (buf)[0] = 0xf0;                            \
    ETCPAL_PDU_PACK_EXT_LEN(buf, length);       \
    etcpal_pack_16b(&(buf)[3], vector);         \
  } while (0)

#define PACK_CLIENT_ENTRY_HEADER(length, vector, cidptr, buf) \
  do                                                          \
  {                                                           \
    (buf)[0] = 0xf0;                                          \
    ETCPAL_PDU_PACK_EXT_LEN(buf, length);                     \
    etcpal_pack_32b(&(buf)[3], vector);                       \
    memcpy(&(buf)[7], (cidptr)->data, ETCPAL_UUID_BYTES);     \
  } while (0)

#define RPT_CLIENT_LIST_SIZE(num_client_entries) (num_client_entries * RPT_CLIENT_ENTRY_SIZE)
#define REQUEST_DYNAMIC_UIDS_DATA_SIZE(num_requests) (num_requests * DYNAMIC_UID_REQUEST_PAIR_SIZE)
#define FETCH_UID_ASSIGNMENT_LIST_DATA_SIZE(num_uids) (num_uids * 6)
#define DYNAMIC_UID_ASSIGNMENT_LIST_DATA_SIZE(num_mappings) (num_mappings * DYNAMIC_UID_MAPPING_SIZE)

/**************************** Private variables ******************************/

// clang-format off
static const char* kRdmnetConnectStatusStrings[] =
{
  "Successful connection",
  "Broker/Client scope mismatch",
  "Broker connection capacity exceeded",
  "Duplicate UID detected",
  "Invalid client entry",
  "Invalid UID"
};
#define NUM_CONNECT_STATUS_STRINGS (sizeof(kRdmnetConnectStatusStrings) / sizeof(const char*))

static const char* kRdmnetDisconnectReasonStrings[] =
{
  "Component shutting down",
  "Component can no longer support this connection",
  "Hardware fault",
  "Software fault",
  "Software reset",
  "Incorrect scope",
  "Component reconfigured via RPT",
  "Component reconfigured via LLRP",
  "Component reconfigured by non-RDMnet method"
};
#define NUM_DISCONNECT_REASON_STRINGS (sizeof(kRdmnetDisconnectReasonStrings) / sizeof(const char*))

static const char* kRdmnetDynamicUidStatusStrings[] =
{
  "Dynamic UID fetched or assigned successfully",
  "The Dynamic UID request was malformed",
  "The requested Dynamic UID was not found",
  "This RID has already been assigned a Dynamic UID",
  "Dynamic UID capacity exhausted"
};
#define NUM_DYNAMIC_UID_STATUS_STRINGS (sizeof(kRdmnetDynamicUidStatusStrings) / sizeof(const char*))
// clang-format on

/*********************** Private function prototypes *************************/

static size_t calc_client_connect_len(const ClientConnectMsg* data);
static size_t pack_broker_header_with_rlp(const EtcPalRootLayerPdu* rlp, uint8_t* buf, size_t buflen, uint16_t vector);
static etcpal_error_t send_broker_header(RdmnetConnection* conn, const EtcPalRootLayerPdu* rlp, uint8_t* buf,
                                         size_t buflen, uint16_t vector);

/*************************** Function definitions ****************************/

/***************************** Broker PDU Header *****************************/

size_t pack_broker_header_with_rlp(const EtcPalRootLayerPdu* rlp, uint8_t* buf, size_t buflen, uint16_t vector)
{
  uint8_t* cur_ptr = buf;
  size_t data_size = etcpal_root_layer_buf_size(rlp, 1);

  if (data_size == 0)
    return 0;

  data_size = etcpal_pack_tcp_preamble(cur_ptr, buflen, data_size);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;
  buflen -= data_size;

  data_size = etcpal_pack_root_layer_header(cur_ptr, buflen, rlp);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;
  buflen -= data_size;

  PACK_BROKER_HEADER(rlp->datalen, vector, cur_ptr);
  cur_ptr += BROKER_PDU_HEADER_SIZE;
  return (size_t)(cur_ptr - buf);
}

etcpal_error_t send_broker_header(RdmnetConnection* conn, const EtcPalRootLayerPdu* rlp, uint8_t* buf, size_t buflen,
                                  uint16_t vector)
{
  size_t data_size = etcpal_root_layer_buf_size(rlp, 1);
  if (data_size == 0)
    return kEtcPalErrProtocol;

  // Pack and send the TCP preamble.
  data_size = etcpal_pack_tcp_preamble(buf, buflen, data_size);
  if (data_size == 0)
    return kEtcPalErrProtocol;
  int send_res = etcpal_send(conn->sock, buf, data_size, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  // Pack and send the Root Layer PDU header.
  data_size = etcpal_pack_root_layer_header(buf, buflen, rlp);
  if (data_size == 0)
    return kEtcPalErrProtocol;
  send_res = etcpal_send(conn->sock, buf, data_size, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  // Pack and send the Broker PDU header
  PACK_BROKER_HEADER(rlp->datalen, vector, buf);
  send_res = etcpal_send(conn->sock, buf, BROKER_PDU_HEADER_SIZE, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  return kEtcPalErrOk;
}

/******************************* Client Connect ******************************/

size_t calc_client_connect_len(const ClientConnectMsg* data)
{
  size_t res = BROKER_PDU_HEADER_SIZE + CLIENT_CONNECT_DATA_MIN_SIZE;

  if (IS_RPT_CLIENT_ENTRY(&data->client_entry))
  {
    res += RPT_CLIENT_ENTRY_DATA_SIZE;
    return res;
  }
  else if (IS_EPT_CLIENT_ENTRY(&data->client_entry))
  {
    res += (EPT_PROTOCOL_ENTRY_SIZE * GET_EPT_CLIENT_ENTRY(&data->client_entry)->num_protocols);
    return res;
  }
  else
  {
    // Should never happen
    return 0;
  }
}

etcpal_error_t send_client_connect(RdmnetConnection* conn, const ClientConnectMsg* data)
{
  if (!(IS_RPT_CLIENT_ENTRY(&data->client_entry) || IS_EPT_CLIENT_ENTRY(&data->client_entry)))
  {
    return kEtcPalErrProtocol;
  }

  uint8_t buf[CLIENT_CONNECT_COMMON_FIELD_SIZE];
  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = calc_client_connect_len(data);

  etcpal_error_t res = send_broker_header(conn, &rlp, buf, CLIENT_CONNECT_COMMON_FIELD_SIZE, VECTOR_BROKER_CONNECT);
  if (res != kEtcPalErrOk)
    return res;

  // Pack and send the common fields for the Client Connect message
  uint8_t* cur_ptr = buf;
  rdmnet_safe_strncpy((char*)cur_ptr, data->scope, E133_SCOPE_STRING_PADDED_LENGTH);
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  etcpal_pack_16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  rdmnet_safe_strncpy((char*)cur_ptr, data->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  cur_ptr += E133_DOMAIN_STRING_PADDED_LENGTH;
  *cur_ptr++ = data->connect_flags;
  int send_res = etcpal_send(conn->sock, buf, (size_t)(cur_ptr - buf), 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  // Pack and send the beginning of the Client Entry PDU
  const EtcPalUuid* cid =
      (IS_RPT_CLIENT_ENTRY(&data->client_entry) ? &(GET_RPT_CLIENT_ENTRY(&data->client_entry)->cid)
                                                : &(GET_EPT_CLIENT_ENTRY(&data->client_entry)->cid));
  PACK_CLIENT_ENTRY_HEADER(rlp.datalen - (BROKER_PDU_HEADER_SIZE + CLIENT_CONNECT_COMMON_FIELD_SIZE),
                           data->client_entry.client_protocol, cid, buf);
  send_res = etcpal_send(conn->sock, buf, CLIENT_ENTRY_HEADER_SIZE, 0);

  if (IS_RPT_CLIENT_ENTRY(&data->client_entry))
  {
    // Pack and send the RPT client entry
    const RptClientEntry* rpt_entry = GET_RPT_CLIENT_ENTRY(&data->client_entry);
    cur_ptr = buf;
    memcpy(cur_ptr, rpt_entry->cid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
    etcpal_pack_16b(cur_ptr, rpt_entry->uid.manu);
    cur_ptr += 2;
    etcpal_pack_32b(cur_ptr, rpt_entry->uid.id);
    cur_ptr += 4;
    *cur_ptr++ = (uint8_t)(rpt_entry->type);
    memcpy(cur_ptr, rpt_entry->binding_cid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
    send_res = etcpal_send(conn->sock, buf, RPT_CLIENT_ENTRY_DATA_SIZE, 0);
    if (send_res < 0)
      return (etcpal_error_t)send_res;
  }
  else  // is EPT client entry
  {
    // Pack and send the EPT client entry
    const EptClientEntry* ept_entry = GET_EPT_CLIENT_ENTRY(&data->client_entry);
    for (const EptSubProtocol* prot = ept_entry->protocols;
         prot < ept_entry->protocols + ept_entry->num_protocols; ++prot)
    {
      cur_ptr = buf;
      etcpal_pack_16b(cur_ptr, prot->manufacturer_id);
      cur_ptr += 2;
      etcpal_pack_16b(cur_ptr, prot->protocol_id);
      cur_ptr += 2;
      rdmnet_safe_strncpy((char*)cur_ptr, prot->protocol_string, EPT_PROTOCOL_STRING_PADDED_LENGTH);
      cur_ptr += EPT_PROTOCOL_STRING_PADDED_LENGTH;
      send_res = etcpal_send(conn->sock, buf, EPT_PROTOCOL_ENTRY_SIZE, 0);
      if (send_res < 0)
        return (etcpal_error_t)send_res;
    }
  }
  etcpal_timer_reset(&conn->send_timer);
  return kEtcPalErrOk;
}

/******************************* Connect Reply *******************************/

/*! \brief Pack a Connect Reply message into a buffer.
 *
 *  \param[out] buf Buffer into which to pack the Connect Reply message.
 *  \param[in] buflen Length in bytes of buf.
 *  \param[in] local_cid CID of the Component sending the Connect Reply message.
 *  \param[in] data Connect Reply data to pack into the data segment.
 *  \return Number of bytes packed, or 0 on error.
 */
size_t pack_connect_reply(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, const ConnectReplyMsg* data)
{
  if (!buf || buflen < CONNECT_REPLY_FULL_MSG_SIZE || !local_cid || !data)
    return 0;

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + CONNECT_REPLY_DATA_SIZE;

  // Try to pack all the header data
  uint8_t* cur_ptr = buf;
  size_t data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, VECTOR_BROKER_CONNECT_REPLY);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  /* Pack the Connect Reply data fields */
  etcpal_pack_16b(cur_ptr, (uint16_t)(data->connect_status));
  cur_ptr += 2;
  etcpal_pack_16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  etcpal_pack_16b(cur_ptr, data->broker_uid.manu);
  cur_ptr += 2;
  etcpal_pack_32b(cur_ptr, data->broker_uid.id);
  cur_ptr += 4;
  etcpal_pack_16b(cur_ptr, data->client_uid.manu);
  cur_ptr += 2;
  etcpal_pack_32b(cur_ptr, data->client_uid.id);
  cur_ptr += 4;

  return (size_t)(cur_ptr - buf);
}

/*! \brief Send a Connect Reply message on an RDMnet connection.
 *  \param[in] handle RDMnet connection handle on which to send the Connect Reply message.
 *  \param[in] local_cid CID of the Component sending the Connect Reply message.
 *  \param[in] data Connect Reply data.
 *  \return #kEtcPalErrOk: Send success.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t send_connect_reply(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const ConnectReplyMsg* data)
{
  if (!local_cid || !data)
    return kEtcPalErrInvalid;

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + CONNECT_REPLY_DATA_SIZE;

  RdmnetConnection* conn;
  etcpal_error_t res = rdmnet_start_message(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  res = send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_CONNECT_REPLY);
  if (res != kEtcPalErrOk)
  {
    rdmnet_end_message(conn);
    return res;
  }

  // Pack and send the Connect Reply data fields
  uint8_t* cur_ptr = buf;
  etcpal_pack_16b(cur_ptr, (uint16_t)(data->connect_status));
  cur_ptr += 2;
  etcpal_pack_16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  etcpal_pack_16b(cur_ptr, data->broker_uid.manu);
  cur_ptr += 2;
  etcpal_pack_32b(cur_ptr, data->broker_uid.id);
  cur_ptr += 4;
  etcpal_pack_16b(cur_ptr, data->client_uid.manu);
  cur_ptr += 2;
  etcpal_pack_32b(cur_ptr, data->client_uid.id);
  cur_ptr += 4;

  int send_res = etcpal_send(conn->sock, buf, (size_t)(cur_ptr - buf), 0);
  if (send_res < 0)
  {
    rdmnet_end_message(conn);
    return (etcpal_error_t)send_res;
  }

  return rdmnet_end_message(conn);
}

/***************************** Fetch Client List *****************************/

/*!
 * \brief Send a Fetch Client List message on an RDMnet connection.
 * \param[in] handle RDMnet connection handle on which to send the Fetch Client List message.
 * \param[in] local_cid CID of the Component sending the Fetch Client List message.
 * \return #kEtcPalErrOk: Send success.
 * \return #kEtcPalErrInvalid: Invalid argument provided.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 * \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t send_fetch_client_list(rdmnet_conn_t handle, const EtcPalUuid* local_cid)
{
  if (!local_cid)
    return kEtcPalErrInvalid;

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE;

  RdmnetConnection* conn;
  etcpal_error_t res = rdmnet_start_message(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  res = send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_FETCH_CLIENT_LIST);
  if (res != kEtcPalErrOk)
    return res;

  return rdmnet_end_message(conn);
}

/**************************** Client List Messages ***************************/

/*!
 * \brief Get the packed buffer size for a given RPT Client List.
 * \param[in] num_client_entries Number of entries in the RPT Client List.
 * \return Required buffer size.
 */
size_t bufsize_rpt_client_list(size_t num_client_entries)
{
  return (BROKER_PDU_FULL_HEADER_SIZE + RPT_CLIENT_LIST_SIZE(num_client_entries));
}

/*!
 * \brief Pack a Client List message containing RPT Client Entries into a buffer.
 *
 * Multiple types of Broker messages can contain an RPT Client List; indicate which type this
 * should be with the vector field. Valid values are VECTOR_BROKER_CONNECTED_CLIENT_LIST,
 * VECTOR_BROKER_CLIENT_ADD, VECTOR_BROKER_CLIENT_REMOVE and VECTOR_BROKER_CLIENT_ENTRY_CHANGE.
 *
 * \param[out] buf Buffer into which to pack the Client List message.
 * \param[in] buflen Length in bytes of buf.
 * \param[in] local_cid CID of the Component sending the Client List message.
 * \param[in] vector Which type of Client List message this is.
 * \param[in] client_entries Array of RPT Client Entries to pack into the data segment.
 * \param[in] num_client_entries Size of client_entries array.
 * \return Number of bytes packed, or 0 on error.
 */
size_t pack_rpt_client_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, uint16_t vector,
                            const RptClientEntry* client_entries, size_t num_client_entries)
{
  if (!buf || buflen < BROKER_PDU_FULL_HEADER_SIZE || !local_cid || !client_entries || num_client_entries == 0 ||
      (vector != VECTOR_BROKER_CONNECTED_CLIENT_LIST && vector != VECTOR_BROKER_CLIENT_ADD &&
       vector != VECTOR_BROKER_CLIENT_REMOVE && vector != VECTOR_BROKER_CLIENT_ENTRY_CHANGE))
  {
    return 0;
  }

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + RPT_CLIENT_LIST_SIZE(num_client_entries);

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = buf + buflen;

  // Try to pack all the header data
  size_t data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, vector);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  for (const RptClientEntry* cur_entry = client_entries; cur_entry < client_entries + num_client_entries; ++cur_entry)
  {
    // Check bounds
    if (cur_ptr + RPT_CLIENT_ENTRY_SIZE > buf_end)
      return 0;

    // Pack the common client entry fields.
    *cur_ptr = 0xf0;
    ETCPAL_PDU_PACK_EXT_LEN(cur_ptr, RPT_CLIENT_ENTRY_SIZE);
    cur_ptr += 3;
    etcpal_pack_32b(cur_ptr, E133_CLIENT_PROTOCOL_RPT);
    cur_ptr += 4;
    memcpy(cur_ptr, cur_entry->cid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;

    // Pack the RPT Client Entry data
    etcpal_pack_16b(cur_ptr, cur_entry->uid.manu);
    cur_ptr += 2;
    etcpal_pack_32b(cur_ptr, cur_entry->uid.id);
    cur_ptr += 4;
    *cur_ptr++ = (uint8_t)(cur_entry->type);
    memcpy(cur_ptr, cur_entry->binding_cid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
  }
  return (size_t)(cur_ptr - buf);
}

/*!
 * \brief Pack a Client List message containing EPT Client Entries into a buffer.
 *
 * Multiple types of Broker messages can contain an EPT Client List; indicate which type this
 * should be with the vector field. Valid values are VECTOR_BROKER_CONNECTED_CLIENT_LIST,
 * VECTOR_BROKER_CLIENT_ADD, VECTOR_BROKER_CLIENT_REMOVE and VECTOR_BROKER_CLIENT_ENTRY_CHANGE.
 *
 * \param[out] buf Buffer into which to pack the Client List message.
 * \param[in] buflen Length in bytes of buf.
 * \param[in] local_cid CID of the Component sending the Client List message.
 * \param[in] vector Which type of Client List message this is.
 * \param[in] client_entries Array of EPT Client Entries to pack into the data segment.
 * \param[in] num_client_entries Size of client_entries array.
 * \return Number of bytes packed, or 0 on error.
 */
size_t pack_ept_client_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, uint16_t vector,
                            const EptClientEntry* client_entries, size_t num_client_entries)
{
  RDMNET_UNUSED_ARG(buf);
  RDMNET_UNUSED_ARG(buflen);
  RDMNET_UNUSED_ARG(local_cid);
  RDMNET_UNUSED_ARG(vector);
  RDMNET_UNUSED_ARG(client_entries);
  RDMNET_UNUSED_ARG(num_client_entries);
  // TODO
  return 0;
}

/**************************** Request Dynamic UIDs ***************************/

/*!
 * \brief Send a Request Dynamic UID Assignment message on an RDMnet connection.
 * \param[in] handle RDMnet connection handle on which to send the Request Dynamic UID Assignment
 *                   message.
 * \param[in] local_cid CID of the Component sending the Request Dynamic UID Assignment message.
 * \param[in] requests Array of Dynamic UID Request Pairs, each indicating a request for a
 *                     newly-assigned Dynamic UID.
 * \param[in] num_requests Size of requests array.
 * \return #kEtcPalErrOk: Send success.
 * \return #kEtcPalErrInvalid: Invalid argument provided.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 * \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t send_request_dynamic_uids(rdmnet_conn_t handle, const EtcPalUuid* local_cid,
                                         const DynamicUidRequest* requests, size_t num_requests)
{
  if (!local_cid || !requests || num_requests == 0)
    return kEtcPalErrInvalid;

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + REQUEST_DYNAMIC_UIDS_DATA_SIZE(num_requests);

  RdmnetConnection* conn;
  etcpal_error_t res = rdmnet_start_message(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  res = send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_REQUEST_DYNAMIC_UIDS);
  if (res != kEtcPalErrOk)
  {
    rdmnet_end_message(conn);
    return res;
  }

  // Pack and send each Dynamic UID Request Pair in turn
  for (const DynamicUidRequest* cur_request = requests; cur_request < requests + num_requests; ++cur_request)
  {
    // Pack the Dynamic UID Request Pair
    etcpal_pack_16b(&buf[0], cur_request->manu_id | 0x8000);
    etcpal_pack_32b(&buf[2], 0);
    memcpy(&buf[6], cur_request->rid.data, ETCPAL_UUID_BYTES);

    // Send the segment
    int send_res = etcpal_send(conn->sock, buf, DYNAMIC_UID_REQUEST_PAIR_SIZE, 0);
    if (send_res < 0)
    {
      rdmnet_end_message(conn);
      return (etcpal_error_t)send_res;
    }
  }

  return rdmnet_end_message(conn);
}

/************************ Dynamic UID Assignment List ************************/

/*!
 * \brief Get the packed buffer size for a Dynamic UID Assignment List message.
 * \param[in] num_mappings The number of DynamicUidMappings that will occupy the data segment of
 *                         the message.
 * \return Required buffer size, or 0 on error.
 */
size_t bufsize_dynamic_uid_assignment_list(size_t num_mappings)
{
  return BROKER_PDU_FULL_HEADER_SIZE + DYNAMIC_UID_ASSIGNMENT_LIST_DATA_SIZE(num_mappings);
}

/*!
 * \brief Pack a Dynamic UID Assignment List message into a buffer.
 *
 * \param[out] buf Buffer into which to pack the Dynamic UID Assignment List message.
 * \param[in] buflen Length in bytes of buf.
 * \param[in] local_cid CID of the Component sending the Dynamic UID Assignment List message.
 * \param[in] mappings Array of Dynamic UID Mappings to pack into the data segment.
 * \param[in] num_mappings Size of mappings array.
 * \return Number of bytes packed, or 0 on error.
 */
size_t pack_dynamic_uid_assignment_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid,
                                        const DynamicUidMapping* mappings, size_t num_mappings)
{
  if (!buf || buflen < BROKER_PDU_FULL_HEADER_SIZE || !local_cid || !mappings || num_mappings == 0)
  {
    return 0;
  }

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + DYNAMIC_UID_ASSIGNMENT_LIST_DATA_SIZE(num_mappings);

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = buf + buflen;

  // Try to pack all the header data
  size_t data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  for (const DynamicUidMapping* cur_mapping = mappings; cur_mapping < mappings + num_mappings; ++cur_mapping)
  {
    // Check bounds
    if (cur_ptr + DYNAMIC_UID_MAPPING_SIZE > buf_end)
      return 0;

    // Pack the Dynamic UID Mapping
    etcpal_pack_16b(cur_ptr, cur_mapping->uid.manu);
    cur_ptr += 2;
    etcpal_pack_32b(cur_ptr, cur_mapping->uid.id);
    cur_ptr += 4;
    memcpy(cur_ptr, cur_mapping->rid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
    etcpal_pack_16b(cur_ptr, (uint16_t)cur_mapping->status_code);
    cur_ptr += 2;
  }
  return (size_t)(cur_ptr - buf);
}

/********************* Fetch Dynamic UID Assignment List *********************/

/*!
 * \brief Send a Fetch Dynamic UID Assignment List message on an RDMnet connection.
 * \param[in] handle RDMnet connection handle on which to send the Fetch Dynamic UID Assignment
 *                   List message.
 * \param[in] local_cid CID of the Component sending the Fetch Dynamic UID Assignment List message.
 * \param[in] uids Array of UIDs, each indicating a request for a corresponding RID.
 * \param[in] num_uids Size of uids array.
 * \return #kEtcPalErrOk: Send success.
 * \return #kEtcPalErrInvalid: Invalid argument provided.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 * \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t send_fetch_uid_assignment_list(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const RdmUid* uids,
                                              size_t num_uids)
{
  if (!local_cid || !uids || num_uids == 0)
    return kEtcPalErrInvalid;

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + FETCH_UID_ASSIGNMENT_LIST_DATA_SIZE(num_uids);

  RdmnetConnection* conn;
  etcpal_error_t res = rdmnet_start_message(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  res = send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST);
  if (res != kEtcPalErrOk)
  {
    rdmnet_end_message(conn);
    return res;
  }

  // Pack and send each Dynamic UID Request Pair in turn
  for (const RdmUid* cur_uid = uids; cur_uid < uids + num_uids; ++cur_uid)
  {
    // Pack the Requested UID
    etcpal_pack_16b(&buf[0], cur_uid->manu);
    etcpal_pack_32b(&buf[2], cur_uid->id);

    // Send the segment
    int send_res = etcpal_send(conn->sock, buf, 6, 0);
    if (send_res < 0)
    {
      rdmnet_end_message(conn);
      return (etcpal_error_t)send_res;
    }
  }

  return rdmnet_end_message(conn);
}

/******************************** Disconnect *********************************/

etcpal_error_t send_disconnect(RdmnetConnection* conn, const DisconnectMsg* data)
{
  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_DISCONNECT_MSG_SIZE;

  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  etcpal_error_t res = send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_DISCONNECT);
  if (res != kEtcPalErrOk)
    return res;

  etcpal_pack_16b(buf, (uint16_t)(data->disconnect_reason));
  int send_res = etcpal_send(conn->sock, buf, 2, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  etcpal_timer_reset(&conn->send_timer);
  return kEtcPalErrOk;
}

/*********************************** Null ************************************/

etcpal_error_t send_null(RdmnetConnection* conn)
{
  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_NULL_MSG_SIZE;

  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  etcpal_error_t res = send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_NULL);

  if (res == kEtcPalErrOk)
    etcpal_timer_reset(&conn->send_timer);

  return res;
}

/*!
 * \brief Get a string description of an RDMnet connect status code.
 *
 * Connect status codes are returned by a broker in a connect reply message after a client attempts
 * to connect.
 *
 * \param[in] code Connect status code.
 * \return String, or NULL if code is invalid.
 */
const char* rdmnet_connect_status_to_string(rdmnet_connect_status_t code)
{
  if (code >= 0 && code < NUM_CONNECT_STATUS_STRINGS)
    return kRdmnetConnectStatusStrings[code];
  return NULL;
}

/*!
 * \brief Get a string description of an RDMnet disconnect reason code.
 *
 * Disconnect reason codes are sent by a broker or client that is disconnecting.
 *
 * \param[in] code Disconnect reason code.
 * \return String, or NULL if code is invalid.
 */
const char* rdmnet_disconnect_reason_to_string(rdmnet_disconnect_reason_t code)
{
  if (code >= 0 && code < NUM_DISCONNECT_REASON_STRINGS)
    return kRdmnetDisconnectReasonStrings[code];
  return NULL;
}

/*!
 * \brief Get a string description of an RDMnet Dynamic UID status code.
 *
 * Dynamic UID status codes are returned by a broker in response to a request for dynamic UIDs by a
 * client.
 *
 * \param[in] code Dynamic UID status code.
 * \return String, or NULL if code is invalid.
 */
const char* rdmnet_dynamic_uid_status_to_string(dynamic_uid_status_t code)
{
  if (code >= 0 && code < NUM_DYNAMIC_UID_STATUS_STRINGS)
    return kRdmnetDynamicUidStatusStrings[code];
  return NULL;
}
