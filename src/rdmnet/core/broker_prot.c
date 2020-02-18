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
    ACN_PDU_PACK_EXT_LEN(buf, length);          \
    etcpal_pack_u16b(&(buf)[3], vector);        \
  } while (0)

#define PACK_CLIENT_ENTRY_HEADER(length, vector, cidptr, buf) \
  do                                                          \
  {                                                           \
    (buf)[0] = 0xf0;                                          \
    ACN_PDU_PACK_EXT_LEN(buf, length);                        \
    etcpal_pack_u32b(&(buf)[3], vector);                      \
    memcpy(&(buf)[7], (cidptr)->data, ETCPAL_UUID_BYTES);     \
  } while (0)

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
static size_t calc_request_dynamic_uids_len(const DynamicUidRequestListEntry* request_list);
static size_t calc_requested_uids_len(const FetchUidAssignmentListEntry* uid_list);
static size_t calc_dynamic_uid_mapping_list_len(const DynamicUidMapping* mapping_list);
static size_t pack_broker_header_with_rlp(const AcnRootLayerPdu* rlp, uint8_t* buf, size_t buflen, uint16_t vector);
static etcpal_error_t send_broker_header(RdmnetConnection* conn, const AcnRootLayerPdu* rlp, uint8_t* buf,
                                         size_t buflen, uint16_t vector);

/*************************** Function definitions ****************************/

/***************************** Broker PDU Header *****************************/

size_t pack_broker_header_with_rlp(const AcnRootLayerPdu* rlp, uint8_t* buf, size_t buflen, uint16_t vector)
{
  uint8_t* cur_ptr = buf;
  size_t data_size = acn_root_layer_buf_size(rlp, 1);

  if (data_size == 0)
    return 0;

  data_size = acn_pack_tcp_preamble(cur_ptr, buflen, data_size);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;
  buflen -= data_size;

  data_size = acn_pack_root_layer_header(cur_ptr, buflen, rlp);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;
  buflen -= data_size;

  PACK_BROKER_HEADER(rlp->datalen, vector, cur_ptr);
  cur_ptr += BROKER_PDU_HEADER_SIZE;
  return (size_t)(cur_ptr - buf);
}

etcpal_error_t send_broker_header(RdmnetConnection* conn, const AcnRootLayerPdu* rlp, uint8_t* buf, size_t buflen,
                                  uint16_t vector)
{
  size_t data_size = acn_root_layer_buf_size(rlp, 1);
  if (data_size == 0)
    return kEtcPalErrProtocol;

  // Pack and send the TCP preamble.
  data_size = acn_pack_tcp_preamble(buf, buflen, data_size);
  if (data_size == 0)
    return kEtcPalErrProtocol;
  int send_res = etcpal_send(conn->sock, buf, data_size, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  // Pack and send the Root Layer PDU header.
  data_size = acn_pack_root_layer_header(buf, buflen, rlp);
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
    EptSubProtocol* prot = GET_EPT_CLIENT_ENTRY_DATA(&data->client_entry)->protocol_list;
    for (; prot; prot = prot->next)
      res += EPT_PROTOCOL_ENTRY_SIZE;
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
  AcnRootLayerPdu rlp;
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
  etcpal_pack_u16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  rdmnet_safe_strncpy((char*)cur_ptr, data->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  cur_ptr += E133_DOMAIN_STRING_PADDED_LENGTH;
  *cur_ptr++ = data->connect_flags;
  int send_res = etcpal_send(conn->sock, buf, (size_t)(cur_ptr - buf), 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  // Pack and send the beginning of the Client Entry PDU
  PACK_CLIENT_ENTRY_HEADER(rlp.datalen - (BROKER_PDU_HEADER_SIZE + CLIENT_CONNECT_COMMON_FIELD_SIZE),
                           data->client_entry.client_protocol, &data->client_entry.client_cid, buf);
  send_res = etcpal_send(conn->sock, buf, CLIENT_ENTRY_HEADER_SIZE, 0);

  if (IS_RPT_CLIENT_ENTRY(&data->client_entry))
  {
    // Pack and send the RPT client entry
    const ClientEntryDataRpt* rpt_data = GET_RPT_CLIENT_ENTRY_DATA(&data->client_entry);
    cur_ptr = buf;
    etcpal_pack_u16b(cur_ptr, rpt_data->client_uid.manu);
    cur_ptr += 2;
    etcpal_pack_u32b(cur_ptr, rpt_data->client_uid.id);
    cur_ptr += 4;
    *cur_ptr++ = (uint8_t)(rpt_data->client_type);
    memcpy(cur_ptr, rpt_data->binding_cid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
    send_res = etcpal_send(conn->sock, buf, RPT_CLIENT_ENTRY_DATA_SIZE, 0);
    if (send_res < 0)
      return (etcpal_error_t)send_res;
  }
  else  // is EPT client entry
  {
    // Pack and send the EPT client entry
    const ClientEntryDataEpt* ept_data = GET_EPT_CLIENT_ENTRY_DATA(&data->client_entry);
    const EptSubProtocol* prot = ept_data->protocol_list;
    for (; prot; prot = prot->next)
    {
      cur_ptr = buf;
      etcpal_pack_u32b(cur_ptr, prot->protocol_vector);
      cur_ptr += 4;
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

  AcnRootLayerPdu rlp;
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
  etcpal_pack_u16b(cur_ptr, (uint16_t)(data->connect_status));
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, data->broker_uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, data->broker_uid.id);
  cur_ptr += 4;
  etcpal_pack_u16b(cur_ptr, data->client_uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, data->client_uid.id);
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

  AcnRootLayerPdu rlp;
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
  etcpal_pack_u16b(cur_ptr, (uint16_t)(data->connect_status));
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  etcpal_pack_u16b(cur_ptr, data->broker_uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, data->broker_uid.id);
  cur_ptr += 4;
  etcpal_pack_u16b(cur_ptr, data->client_uid.manu);
  cur_ptr += 2;
  etcpal_pack_u32b(cur_ptr, data->client_uid.id);
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

/*! \brief Send a Fetch Client List message on an RDMnet connection.
 *  \param[in] handle RDMnet connection handle on which to send the Fetch Client List message.
 *  \param[in] local_cid CID of the Component sending the Fetch Client List message.
 *  \return #kEtcPalErrOk: Send success.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t send_fetch_client_list(rdmnet_conn_t handle, const EtcPalUuid* local_cid)
{
  if (!local_cid)
    return kEtcPalErrInvalid;

  AcnRootLayerPdu rlp;
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

size_t calc_client_entry_buf_size(const ClientEntryData* client_entry_list)
{
  size_t res = 0;
  const ClientEntryData* cur_entry = client_entry_list;

  for (; cur_entry; cur_entry = cur_entry->next)
  {
    if (cur_entry->client_protocol == E133_CLIENT_PROTOCOL_RPT)
    {
      res += RPT_CLIENT_ENTRY_SIZE;
    }
    else
    {
      // TODO
      return 0;
    }
  }
  return res;
}

/*! \brief Get the packed buffer size for a given Client List.
 *  \param[in] client_entry_list Client List of which to calculate the packed size.
 *  \return Required buffer size, or 0 on error.
 */
size_t bufsize_client_list(const ClientEntryData* client_entry_list)
{
  return (client_entry_list ? (BROKER_PDU_FULL_HEADER_SIZE + calc_client_entry_buf_size(client_entry_list)) : 0);
}

/*! \brief Pack a Client List message into a buffer.
 *
 *  Multiple types of Broker messages can contain a Client List; indicate which type this should be
 *  with the vector field. Valid values are VECTOR_BROKER_CONNECTED_CLIENT_LIST,
 *  VECTOR_BROKER_CLIENT_ADD, VECTOR_BROKER_CLIENT_REMOVE and VECTOR_BROKER_CLIENT_ENTRY_CHANGE.
 *
 *  \param[out] buf Buffer into which to pack the Client List message.
 *  \param[in] buflen Length in bytes of buf.
 *  \param[in] local_cid CID of the Component sending the Client List message.
 *  \param[in] vector Which type of Client List message this is.
 *  \param[in] client_entry_list Client List to pack into the data segment.
 *  \return Number of bytes packed, or 0 on error.
 */
size_t pack_client_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, uint16_t vector,
                        const ClientEntryData* client_entry_list)
{
  if (!buf || buflen < BROKER_PDU_FULL_HEADER_SIZE || !local_cid || !client_entry_list ||
      (vector != VECTOR_BROKER_CONNECTED_CLIENT_LIST && vector != VECTOR_BROKER_CLIENT_ADD &&
       vector != VECTOR_BROKER_CLIENT_REMOVE && vector != VECTOR_BROKER_CLIENT_ENTRY_CHANGE))
  {
    return 0;
  }

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + calc_client_entry_buf_size(client_entry_list);

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = buf + buflen;

  // Try to pack all the header data
  size_t data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, vector);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  for (const ClientEntryData* cur_entry = client_entry_list; cur_entry; cur_entry = cur_entry->next)
  {
    // Check bounds
    if (cur_ptr + CLIENT_ENTRY_HEADER_SIZE > buf_end)
      return 0;

    // Pack the common client entry fields.
    *cur_ptr = 0xf0;
    ACN_PDU_PACK_EXT_LEN(cur_ptr, RPT_CLIENT_ENTRY_SIZE);
    cur_ptr += 3;
    etcpal_pack_u32b(cur_ptr, cur_entry->client_protocol);
    cur_ptr += 4;
    memcpy(cur_ptr, cur_entry->client_cid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;

    if (cur_entry->client_protocol == E133_CLIENT_PROTOCOL_RPT)
    {
      const ClientEntryDataRpt* rpt_data = GET_RPT_CLIENT_ENTRY_DATA(cur_entry);

      // Check bounds.
      if (cur_ptr + RPT_CLIENT_ENTRY_DATA_SIZE > buf_end)
        return 0;

      // Pack the RPT Client Entry data
      etcpal_pack_u16b(cur_ptr, rpt_data->client_uid.manu);
      cur_ptr += 2;
      etcpal_pack_u32b(cur_ptr, rpt_data->client_uid.id);
      cur_ptr += 4;
      *cur_ptr++ = (uint8_t)(rpt_data->client_type);
      memcpy(cur_ptr, rpt_data->binding_cid.data, ETCPAL_UUID_BYTES);
      cur_ptr += ETCPAL_UUID_BYTES;
    }
    else
    {
      // TODO EPT
      return 0;
    }
  }
  return (size_t)(cur_ptr - buf);
}

/**************************** Request Dynamic UIDs ***************************/

size_t calc_request_dynamic_uids_len(const DynamicUidRequestListEntry* request_list)
{
  size_t res = BROKER_PDU_HEADER_SIZE;

  for (const DynamicUidRequestListEntry* cur_request = request_list; cur_request; cur_request = cur_request->next)
  {
    res += DYNAMIC_UID_REQUEST_PAIR_SIZE;
  }
  return res;
}

/*! \brief Send a Request Dynamic UID Assignment message on an RDMnet connection.
 *  \param[in] handle RDMnet connection handle on which to send the Request Dynamic UID Assignment
 *                    message.
 *  \param[in] local_cid CID of the Component sending the Request Dynamic UID Assignment message.
 *  \param[in] request_list List of Dynamic UID Request Pairs, each indicating a request for a
 *                          newly-assigned Dynamic UID.
 *  \return #kEtcPalErrOk: Send success.
 *  \return #kEtcPalErrInvalid: Invalid argument provided.
 *  \return #kEtcPalErrSys: An internal library or system call error occurred.
 *  \return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t send_request_dynamic_uids(rdmnet_conn_t handle, const EtcPalUuid* local_cid,
                                         const DynamicUidRequestListEntry* request_list)
{
  const DynamicUidRequestListEntry* cur_request;

  if (!local_cid || !request_list)
    return kEtcPalErrInvalid;

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = calc_request_dynamic_uids_len(request_list);

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

  /* Pack and send each Dynamic UID Request Pair in turn */
  for (cur_request = request_list; cur_request; cur_request = cur_request->next)
  {
    /* Pack the Dynamic UID Request Pair */
    etcpal_pack_u16b(&buf[0], cur_request->manu_id | 0x8000);
    etcpal_pack_u32b(&buf[2], 0);
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

size_t calc_dynamic_uid_mapping_list_len(const DynamicUidMapping* mapping_list)
{
  size_t res = BROKER_PDU_HEADER_SIZE;

  for (const DynamicUidMapping* cur_mapping = mapping_list; cur_mapping; cur_mapping = cur_mapping->next)
  {
    res += DYNAMIC_UID_MAPPING_SIZE;
  }
  return res;
}

/*! \brief Get the packed buffer size for a Dynamic UID Assignment List message.
 *  \param[in] mapping_list The Dynamic UID Mapping List that will occupy the data segment of the
 *                          message.
 *  \return Required buffer size, or 0 on error.
 */
size_t bufsize_dynamic_uid_assignment_list(const DynamicUidMapping* mapping_list)
{
  return (mapping_list ? (BROKER_PDU_FULL_HEADER_SIZE + calc_dynamic_uid_mapping_list_len(mapping_list)) : 0);
}

/*! \brief Pack a Dynamic UID Assignment List message into a buffer.
 *
 *  \param[out] buf Buffer into which to pack the Dynamic UID Assignment List message.
 *  \param[in] buflen Length in bytes of buf.
 *  \param[in] local_cid CID of the Component sending the Dynamic UID Assignment List message.
 *  \param[in] mapping_list List of Dynamic UID Mappings to pack into the data segment.
 *  \return Number of bytes packed, or 0 on error.
 */
size_t pack_dynamic_uid_assignment_list(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid,
                                        const DynamicUidMapping* mapping_list)
{
  if (!buf || buflen < BROKER_PDU_FULL_HEADER_SIZE || !local_cid || !mapping_list)
  {
    return 0;
  }

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + calc_dynamic_uid_mapping_list_len(mapping_list);

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = buf + buflen;

  // Try to pack all the header data
  size_t data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  for (const DynamicUidMapping* cur_mapping = mapping_list; cur_mapping; cur_mapping = cur_mapping->next)
  {
    // Check bounds
    if (cur_ptr + DYNAMIC_UID_MAPPING_SIZE > buf_end)
      return 0;

    // Pack the Dynamic UID Mapping
    etcpal_pack_u16b(cur_ptr, cur_mapping->uid.manu);
    cur_ptr += 2;
    etcpal_pack_u32b(cur_ptr, cur_mapping->uid.id);
    cur_ptr += 4;
    memcpy(cur_ptr, cur_mapping->rid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
    etcpal_pack_u16b(cur_ptr, (uint16_t)cur_mapping->status_code);
    cur_ptr += 2;
  }
  return (size_t)(cur_ptr - buf);
}

/********************* Fetch Dynamic UID Assignment List *********************/

static size_t calc_requested_uids_len(const FetchUidAssignmentListEntry* uid_list)
{
  size_t res = BROKER_PDU_HEADER_SIZE;

  for (const FetchUidAssignmentListEntry* cur_uid = uid_list; cur_uid; cur_uid = cur_uid->next)
  {
    res += 6;  // The size of a packed UID
  }
  return res;
}

etcpal_error_t send_fetch_uid_assignment_list(rdmnet_conn_t handle, const EtcPalUuid* local_cid,
                                              const FetchUidAssignmentListEntry* uid_list)
{
  if (!local_cid || !uid_list)
    return kEtcPalErrInvalid;

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = calc_requested_uids_len(uid_list);

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
  for (const FetchUidAssignmentListEntry* cur_uid = uid_list; cur_uid; cur_uid = cur_uid->next)
  {
    // Pack the Requested UID
    etcpal_pack_u16b(&buf[0], cur_uid->uid.manu);
    etcpal_pack_u32b(&buf[2], cur_uid->uid.id);

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
  AcnRootLayerPdu rlp;
  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_DISCONNECT_MSG_SIZE;

  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  etcpal_error_t res = send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_DISCONNECT);
  if (res != kEtcPalErrOk)
    return res;

  etcpal_pack_u16b(buf, (uint16_t)(data->disconnect_reason));
  int send_res = etcpal_send(conn->sock, buf, 2, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  etcpal_timer_reset(&conn->send_timer);
  return kEtcPalErrOk;
}

/*********************************** Null ************************************/

etcpal_error_t send_null(RdmnetConnection* conn)
{
  AcnRootLayerPdu rlp;
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
