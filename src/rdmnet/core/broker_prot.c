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

#include "rdmnet/core/broker_prot.h"

#include <string.h>
#include "etcpal/common.h"
#include "etcpal/pack.h"
#include "rdmnet/core/util.h"
#include "rdmnet/core/connection.h"

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

#define RPT_CLIENT_LIST_SIZE(num_client_entries) (num_client_entries * RPT_CLIENT_ENTRY_SIZE)
#define REQUEST_DYNAMIC_UIDS_DATA_SIZE(num_requests) (num_requests * DYNAMIC_UID_REQUEST_PAIR_SIZE)
#define FETCH_UID_ASSIGNMENT_LIST_DATA_SIZE(num_uids) (num_uids * 6)
#define DYNAMIC_UID_ASSIGNMENT_LIST_DATA_SIZE(num_mappings) (num_mappings * DYNAMIC_UID_MAPPING_SIZE)

/*********************** Private function prototypes *************************/

static size_t calc_client_connect_len(const BrokerClientConnectMsg* data);
static size_t pack_broker_header_with_rlp(const AcnRootLayerPdu* rlp, uint8_t* buf, size_t buflen, uint16_t vector);
static etcpal_error_t send_broker_header(RCConnection*          conn,
                                         const AcnRootLayerPdu* rlp,
                                         uint8_t*               buf,
                                         size_t                 buflen,
                                         uint16_t               vector);

/*************************** Function definitions ****************************/

/***************************** Broker PDU Header *****************************/

size_t pack_broker_header_with_rlp(const AcnRootLayerPdu* rlp, uint8_t* buf, size_t buflen, uint16_t vector)
{
  uint8_t* cur_ptr = buf;
  size_t   data_size = acn_root_layer_buf_size(rlp, 1);

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

  PACK_BROKER_HEADER(rlp->data_len, vector, cur_ptr);
  cur_ptr += BROKER_PDU_HEADER_SIZE;
  return (size_t)(cur_ptr - buf);
}

etcpal_error_t send_broker_header(RCConnection*          conn,
                                  const AcnRootLayerPdu* rlp,
                                  uint8_t*               buf,
                                  size_t                 buflen,
                                  uint16_t               vector)
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
  PACK_BROKER_HEADER(rlp->data_len, vector, buf);
  send_res = etcpal_send(conn->sock, buf, BROKER_PDU_HEADER_SIZE, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  return kEtcPalErrOk;
}

/******************************* Client Connect ******************************/

size_t calc_client_connect_len(const BrokerClientConnectMsg* data)
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

etcpal_error_t rc_broker_send_client_connect(RCConnection* conn, const BrokerClientConnectMsg* data)
{
  if (!(IS_RPT_CLIENT_ENTRY(&data->client_entry) || IS_EPT_CLIENT_ENTRY(&data->client_entry)))
  {
    return kEtcPalErrProtocol;
  }

  uint8_t         buf[CLIENT_CONNECT_COMMON_FIELD_SIZE];
  AcnRootLayerPdu rlp;
  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = calc_client_connect_len(data);

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
  const EtcPalUuid* cid =
      (IS_RPT_CLIENT_ENTRY(&data->client_entry) ? &(GET_RPT_CLIENT_ENTRY(&data->client_entry)->cid)
                                                : &(GET_EPT_CLIENT_ENTRY(&data->client_entry)->cid));
  PACK_CLIENT_ENTRY_HEADER(rlp.data_len - (BROKER_PDU_HEADER_SIZE + CLIENT_CONNECT_COMMON_FIELD_SIZE),
                           data->client_entry.client_protocol, cid, buf);
  send_res = etcpal_send(conn->sock, buf, CLIENT_ENTRY_HEADER_SIZE, 0);

  if (IS_RPT_CLIENT_ENTRY(&data->client_entry))
  {
    // Pack and send the RPT client entry
    const RdmnetRptClientEntry* rpt_entry = GET_RPT_CLIENT_ENTRY(&data->client_entry);
    cur_ptr = buf;
    etcpal_pack_u16b(cur_ptr, rpt_entry->uid.manu);
    cur_ptr += 2;
    etcpal_pack_u32b(cur_ptr, rpt_entry->uid.id);
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
    const RdmnetEptClientEntry* ept_entry = GET_EPT_CLIENT_ENTRY(&data->client_entry);
    for (const RdmnetEptSubProtocol* prot = ept_entry->protocols;
         prot < ept_entry->protocols + ept_entry->num_protocols; ++prot)
    {
      cur_ptr = buf;
      etcpal_pack_u16b(cur_ptr, prot->manufacturer_id);
      cur_ptr += 2;
      etcpal_pack_u16b(cur_ptr, prot->protocol_id);
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

/** @brief Pack a Connect Reply message into a buffer.
 *
 *  @param[out] buf Buffer into which to pack the Connect Reply message.
 *  @param[in] buflen Length in bytes of buf.
 *  @param[in] local_cid CID of the Component sending the Connect Reply message.
 *  @param[in] data Connect Reply data to pack into the data segment.
 *  @return Number of bytes packed, or 0 on error.
 */
size_t rc_broker_pack_connect_reply(uint8_t*                     buf,
                                    size_t                       buflen,
                                    const EtcPalUuid*            local_cid,
                                    const BrokerConnectReplyMsg* data)
{
  if (!buf || buflen < BROKER_CONNECT_REPLY_FULL_MSG_SIZE || !local_cid || !data)
    return 0;

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_PDU_HEADER_SIZE + BROKER_CONNECT_REPLY_DATA_SIZE;

  // Try to pack all the header data
  uint8_t* cur_ptr = buf;
  size_t   data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, VECTOR_BROKER_CONNECT_REPLY);
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

/***************************** Fetch Client List *****************************/

/**
 * @brief Send a Fetch Client List message on an RDMnet connection.
 * @param[in] handle RDMnet connection handle on which to send the Fetch Client List message.
 * @param[in] local_cid CID of the Component sending the Fetch Client List message.
 * @return #kEtcPalErrOk: Send success.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t rc_broker_send_fetch_client_list(RCConnection* conn, const EtcPalUuid* local_cid)
{
  if (!local_cid)
    return kEtcPalErrInvalid;

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_PDU_HEADER_SIZE;

  uint8_t        buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  etcpal_error_t res =
      send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_FETCH_CLIENT_LIST);

  return res;
}

/**************************** Client List Messages ***************************/

/**
 * @brief Get the packed buffer size for a given RPT Client List.
 * @param[in] num_client_entries Number of entries in the RPT Client List.
 * @return Required buffer size.
 */
size_t rc_broker_get_rpt_client_list_buffer_size(size_t num_client_entries)
{
  return (BROKER_PDU_FULL_HEADER_SIZE + RPT_CLIENT_LIST_SIZE(num_client_entries));
}

/**
 * @brief Pack a Client List message containing RPT Client Entries into a buffer.
 *
 * Multiple types of Broker messages can contain an RPT Client List; indicate which type this
 * should be with the vector field. Valid values are VECTOR_BROKER_CONNECTED_CLIENT_LIST,
 * VECTOR_BROKER_CLIENT_ADD, VECTOR_BROKER_CLIENT_REMOVE and VECTOR_BROKER_CLIENT_ENTRY_CHANGE.
 *
 * @param[out] buf Buffer into which to pack the Client List message.
 * @param[in] buflen Length in bytes of buf.
 * @param[in] local_cid CID of the Component sending the Client List message.
 * @param[in] vector Which type of Client List message this is.
 * @param[in] client_entries Array of RPT Client Entries to pack into the data segment.
 * @param[in] num_client_entries Size of client_entries array.
 * @return Number of bytes packed, or 0 on error.
 */
size_t rc_broker_pack_rpt_client_list(uint8_t*                    buf,
                                      size_t                      buflen,
                                      const EtcPalUuid*           local_cid,
                                      uint16_t                    vector,
                                      const RdmnetRptClientEntry* client_entries,
                                      size_t                      num_client_entries)
{
  if (!buf || buflen < BROKER_PDU_FULL_HEADER_SIZE || !local_cid || !client_entries || num_client_entries == 0 ||
      (vector != VECTOR_BROKER_CONNECTED_CLIENT_LIST && vector != VECTOR_BROKER_CLIENT_ADD &&
       vector != VECTOR_BROKER_CLIENT_REMOVE && vector != VECTOR_BROKER_CLIENT_ENTRY_CHANGE))
  {
    return 0;
  }

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_PDU_HEADER_SIZE + RPT_CLIENT_LIST_SIZE(num_client_entries);

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = buf + buflen;

  // Try to pack all the header data
  size_t data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, vector);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  for (const RdmnetRptClientEntry* cur_entry = client_entries; cur_entry < client_entries + num_client_entries;
       ++cur_entry)
  {
    // Check bounds
    if (cur_ptr + RPT_CLIENT_ENTRY_SIZE > buf_end)
      return 0;

    // Pack the common client entry fields.
    *cur_ptr = 0xf0;
    ACN_PDU_PACK_EXT_LEN(cur_ptr, RPT_CLIENT_ENTRY_SIZE);
    cur_ptr += 3;
    etcpal_pack_u32b(cur_ptr, E133_CLIENT_PROTOCOL_RPT);
    cur_ptr += 4;
    memcpy(cur_ptr, cur_entry->cid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;

    // Pack the RPT Client Entry data
    etcpal_pack_u16b(cur_ptr, cur_entry->uid.manu);
    cur_ptr += 2;
    etcpal_pack_u32b(cur_ptr, cur_entry->uid.id);
    cur_ptr += 4;
    *cur_ptr++ = (uint8_t)(cur_entry->type);
    memcpy(cur_ptr, cur_entry->binding_cid.data, ETCPAL_UUID_BYTES);
    cur_ptr += ETCPAL_UUID_BYTES;
  }
  return (size_t)(cur_ptr - buf);
}

/**
 * @brief Pack a Client List message containing EPT Client Entries into a buffer.
 *
 * Multiple types of Broker messages can contain an EPT Client List; indicate which type this
 * should be with the vector field. Valid values are VECTOR_BROKER_CONNECTED_CLIENT_LIST,
 * VECTOR_BROKER_CLIENT_ADD, VECTOR_BROKER_CLIENT_REMOVE and VECTOR_BROKER_CLIENT_ENTRY_CHANGE.
 *
 * @param[out] buf Buffer into which to pack the Client List message.
 * @param[in] buflen Length in bytes of buf.
 * @param[in] local_cid CID of the Component sending the Client List message.
 * @param[in] vector Which type of Client List message this is.
 * @param[in] client_entries Array of EPT Client Entries to pack into the data segment.
 * @param[in] num_client_entries Size of client_entries array.
 * @return Number of bytes packed, or 0 on error.
 */
size_t rc_broker_pack_ept_client_list(uint8_t*                    buf,
                                      size_t                      buflen,
                                      const EtcPalUuid*           local_cid,
                                      uint16_t                    vector,
                                      const RdmnetEptClientEntry* client_entries,
                                      size_t                      num_client_entries)
{
  ETCPAL_UNUSED_ARG(buf);
  ETCPAL_UNUSED_ARG(buflen);
  ETCPAL_UNUSED_ARG(local_cid);
  ETCPAL_UNUSED_ARG(vector);
  ETCPAL_UNUSED_ARG(client_entries);
  ETCPAL_UNUSED_ARG(num_client_entries);
  // TODO
  return 0;
}

/**************************** Request Dynamic UIDs ***************************/

/**
 * @brief Send a Request Dynamic UID Assignment message on an RDMnet connection.
 * @param[in] handle RDMnet connection handle on which to send the Request Dynamic UID Assignment
 *                   message.
 * @param[in] local_cid CID of the Component sending the Request Dynamic UID Assignment message.
 * @param[in] requests Array of Dynamic UID Request Pairs, each indicating a request for a
 *                     newly-assigned Dynamic UID.
 * @param[in] num_requests Size of requests array.
 * @return #kEtcPalErrOk: Send success.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t rc_broker_send_request_dynamic_uids(RCConnection*     conn,
                                                   const EtcPalUuid* local_cid,
                                                   uint16_t          manufacturer_id,
                                                   const EtcPalUuid* rids,
                                                   size_t            num_rids)
{
  if (!local_cid || !rids || num_rids == 0)
    return kEtcPalErrInvalid;

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_PDU_HEADER_SIZE + REQUEST_DYNAMIC_UIDS_DATA_SIZE(num_rids);

  uint8_t        buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  etcpal_error_t res =
      send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_REQUEST_DYNAMIC_UIDS);
  if (res != kEtcPalErrOk)
    return res;

  // Pack and send each Dynamic UID Request Pair in turn
  for (const EtcPalUuid* cur_rid = rids; cur_rid < rids + num_rids; ++cur_rid)
  {
    // Pack the Dynamic UID Request Pair
    etcpal_pack_u16b(&buf[0], (manufacturer_id | 0x8000));
    etcpal_pack_u32b(&buf[2], 0);
    memcpy(&buf[6], cur_rid->data, ETCPAL_UUID_BYTES);

    // Send the segment
    int send_res = etcpal_send(conn->sock, buf, DYNAMIC_UID_REQUEST_PAIR_SIZE, 0);
    if (send_res < 0)
      return (etcpal_error_t)send_res;
  }

  return kEtcPalErrOk;
}

/************************ Dynamic UID Assignment List ************************/

/**
 * @brief Get the packed buffer size for a Dynamic UID Assignment List message.
 * @param[in] num_mappings The number of BrokerDynamicUidMappings that will occupy the data segment of
 *                         the message.
 * @return Required buffer size, or 0 on error.
 */
size_t rc_broker_get_uid_assignment_list_buffer_size(size_t num_mappings)
{
  return BROKER_PDU_FULL_HEADER_SIZE + DYNAMIC_UID_ASSIGNMENT_LIST_DATA_SIZE(num_mappings);
}

/**
 * @brief Pack a Dynamic UID Assignment List message into a buffer.
 *
 * @param[out] buf Buffer into which to pack the Dynamic UID Assignment List message.
 * @param[in] buflen Length in bytes of buf.
 * @param[in] local_cid CID of the Component sending the Dynamic UID Assignment List message.
 * @param[in] mappings Array of Dynamic UID Mappings to pack into the data segment.
 * @param[in] num_mappings Size of mappings array.
 * @return Number of bytes packed, or 0 on error.
 */
size_t rc_broker_pack_uid_assignment_list(uint8_t*                       buf,
                                          size_t                         buflen,
                                          const EtcPalUuid*              local_cid,
                                          const RdmnetDynamicUidMapping* mappings,
                                          size_t                         num_mappings)
{
  if (!buf || buflen < BROKER_PDU_FULL_HEADER_SIZE || !local_cid || !mappings || num_mappings == 0)
  {
    return 0;
  }

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_PDU_HEADER_SIZE + DYNAMIC_UID_ASSIGNMENT_LIST_DATA_SIZE(num_mappings);

  uint8_t* cur_ptr = buf;
  uint8_t* buf_end = buf + buflen;

  // Try to pack all the header data
  size_t data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  for (const RdmnetDynamicUidMapping* cur_mapping = mappings; cur_mapping < mappings + num_mappings; ++cur_mapping)
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

/**
 * @brief Send a Fetch Dynamic UID Assignment List message on an RDMnet connection.
 * @param[in] handle RDMnet connection handle on which to send the Fetch Dynamic UID Assignment
 *                   List message.
 * @param[in] local_cid CID of the Component sending the Fetch Dynamic UID Assignment List message.
 * @param[in] uids Array of UIDs, each indicating a request for a corresponding RID.
 * @param[in] num_uids Size of uids array.
 * @return #kEtcPalErrOk: Send success.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t rc_broker_send_fetch_uid_assignment_list(RCConnection*     conn,
                                                        const EtcPalUuid* local_cid,
                                                        const RdmUid*     uids,
                                                        size_t            num_uids)
{
  if (!local_cid || !uids || num_uids == 0)
    return kEtcPalErrInvalid;

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_PDU_HEADER_SIZE + FETCH_UID_ASSIGNMENT_LIST_DATA_SIZE(num_uids);

  uint8_t        buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  etcpal_error_t res =
      send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST);
  if (res != kEtcPalErrOk)
    return res;

  // Pack and send each Dynamic UID Request Pair in turn
  for (const RdmUid* cur_uid = uids; cur_uid < uids + num_uids; ++cur_uid)
  {
    // Pack the Requested UID
    etcpal_pack_u16b(&buf[0], cur_uid->manu);
    etcpal_pack_u32b(&buf[2], cur_uid->id);

    // Send the segment
    int send_res = etcpal_send(conn->sock, buf, 6, 0);
    if (send_res < 0)
      return (etcpal_error_t)send_res;
  }

  return kEtcPalErrOk;
}

/******************************** Disconnect *********************************/

size_t rc_broker_pack_disconnect(uint8_t*                   buf,
                                 size_t                     buflen,
                                 const EtcPalUuid*          local_cid,
                                 const BrokerDisconnectMsg* data)
{
  if (!buf || buflen < BROKER_DISCONNECT_FULL_MSG_SIZE || !local_cid || !data)
    return 0;

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_DISCONNECT_MSG_SIZE;

  // The null message is just the broker header by itself
  size_t data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, VECTOR_BROKER_DISCONNECT);
  if (data_size == 0)
    return 0;

  etcpal_pack_u16b(&buf[data_size], (uint16_t)data->disconnect_reason);
  return BROKER_DISCONNECT_FULL_MSG_SIZE;
}

etcpal_error_t rc_broker_send_disconnect(RCConnection* conn, const BrokerDisconnectMsg* data)
{
  AcnRootLayerPdu rlp;
  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_DISCONNECT_MSG_SIZE;

  uint8_t        buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
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

size_t rc_broker_pack_null(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid)
{
  if (!buf || buflen < BROKER_NULL_FULL_MSG_SIZE || !local_cid)
    return 0;

  AcnRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_NULL_MSG_SIZE;

  // The null message is just the broker header by itself
  return pack_broker_header_with_rlp(&rlp, buf, buflen, VECTOR_BROKER_NULL);
}

etcpal_error_t rc_broker_send_null(RCConnection* conn)
{
  AcnRootLayerPdu rlp;
  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.data_len = BROKER_NULL_MSG_SIZE;

  uint8_t        buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  etcpal_error_t res = send_broker_header(conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_NULL);

  if (res == kEtcPalErrOk)
    etcpal_timer_reset(&conn->send_timer);

  return res;
}
