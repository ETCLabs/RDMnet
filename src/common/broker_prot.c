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

#include "rdmnet/common/broker_prot.h"
#include "rdmnet/common/connection.h"
#include "rdmnet/common/util.h"
#include "broker_prot_priv.h"
#include "lwpa/pack.h"

/***************************** Private macros ********************************/

#define pack_broker_header(length, vector, buf) \
  do                                            \
  {                                             \
    (buf)[0] = 0xf0;                            \
    lwpa_pdu_pack_ext_len(buf, length);         \
    lwpa_pack_16b(&(buf)[3], vector);           \
  } while (0)

#define pack_client_entry_header(length, vector, cidptr, buf) \
  do                                                          \
  {                                                           \
    (buf)[0] = 0xf0;                                          \
    lwpa_pdu_pack_ext_len(buf, length);                       \
    lwpa_pack_32b(&(buf)[3], vector);                         \
    memcpy(&(buf)[7], (cidptr)->data, LWPA_UUID_BYTES);       \
  } while (0)

/*********************** Private function prototypes *************************/

static size_t calc_client_connect_len(const ClientConnectMsg *data);
static int do_send(int handle, RdmnetConnection *conn, const uint8_t *data, size_t datasize);
static size_t pack_broker_header_with_rlp(const LwpaRootLayerPdu *rlp, uint8_t *buf, size_t buflen, uint32_t vector);
static lwpa_error_t send_broker_header(int handle, RdmnetConnection *conn, const LwpaRootLayerPdu *rlp, uint8_t *buf,
                                       size_t buflen, uint32_t vector);

/*************************** Function definitions ****************************/

int do_send(int handle, RdmnetConnection *conn, const uint8_t *data, size_t datasize)
{
  if (conn)
    return lwpa_send(conn->sock, data, datasize, 0);
  else
    return rdmnet_send_partial_message(handle, data, datasize);
}

/***************************** Broker PDU Header *****************************/

size_t pack_broker_header_with_rlp(const LwpaRootLayerPdu *rlp, uint8_t *buf, size_t buflen, uint32_t vector)
{
  uint8_t *cur_ptr = buf;
  size_t data_size = lwpa_root_layer_buf_size(rlp, 1);

  if (data_size == 0)
    return 0;

  data_size = lwpa_pack_tcp_preamble(cur_ptr, buflen, data_size);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;
  buflen -= data_size;

  data_size = lwpa_pack_root_layer_header(cur_ptr, buflen, rlp);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;
  buflen -= data_size;

  pack_broker_header(rlp->datalen, vector, cur_ptr);
  cur_ptr += BROKER_PDU_HEADER_SIZE;
  return cur_ptr - buf;
}

lwpa_error_t send_broker_header(int handle, RdmnetConnection *conn, const LwpaRootLayerPdu *rlp, uint8_t *buf,
                                size_t buflen, uint32_t vector)
{
  int send_res;
  size_t data_size = lwpa_root_layer_buf_size(rlp, 1);

  if (data_size == 0)
  {
    if (!conn)
      rdmnet_end_message(handle);
    return LWPA_PROTERR;
  }

  /* Pack and send the TCP preamble. */
  data_size = lwpa_pack_tcp_preamble(buf, buflen, data_size);
  if (data_size == 0)
    return LWPA_PROTERR;
  send_res = do_send(handle, conn, buf, data_size);
  if (send_res < 0)
    return (lwpa_error_t)send_res;

  /* Pack and send the Root Layer PDU header */
  data_size = lwpa_pack_root_layer_header(buf, buflen, rlp);
  if (data_size == 0)
    return LWPA_PROTERR;
  send_res = do_send(handle, conn, buf, data_size);
  if (send_res < 0)
    return (lwpa_error_t)send_res;

  /* Pack and send the Broker PDU header */
  pack_broker_header(rlp->datalen, vector, buf);
  send_res = do_send(handle, conn, buf, BROKER_PDU_HEADER_SIZE);
  if (send_res < 0)
    return (lwpa_error_t)send_res;

  return LWPA_OK;
}

/******************************* Client Connect ******************************/

size_t calc_client_connect_len(const ClientConnectMsg *data)
{
  size_t res = BROKER_PDU_HEADER_SIZE + CLIENT_CONNECT_DATA_MIN_SIZE;

  if (is_rpt_client_entry(&data->client_entry))
  {
    res += RPT_CLIENT_ENTRY_DATA_SIZE;
    return res;
  }
  else if (is_ept_client_entry(&data->client_entry))
  {
    EptSubProtocol *prot = get_ept_client_entry_data(&data->client_entry)->protocol_list;
    for (; prot; prot = prot->next)
      res += EPT_PROTOCOL_ENTRY_SIZE;
    return res;
  }
  else
  {
    /* Should never happen */
    return 0;
  }
}

lwpa_error_t send_client_connect(RdmnetConnection *conn, const ClientConnectMsg *data)
{
  lwpa_error_t res;
  int send_res;
  LwpaRootLayerPdu rlp;
  uint8_t buf[CLIENT_CONNECT_COMMON_FIELD_SIZE];
  uint8_t *cur_ptr;

  if (!(is_rpt_client_entry(&data->client_entry) || is_ept_client_entry(&data->client_entry)))
  {
    return LWPA_PROTERR;
  }

  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = calc_client_connect_len(data);

  res = send_broker_header(-1, conn, &rlp, buf, CLIENT_CONNECT_COMMON_FIELD_SIZE, VECTOR_BROKER_CONNECT);
  if (res != LWPA_OK)
    return res;

  RDMNET_MSVC_BEGIN_NO_DEP_WARNINGS()

  /* Pack and send the common fields for the Client Connect message. */
  cur_ptr = buf;
  strncpy((char *)cur_ptr, data->scope, E133_SCOPE_STRING_PADDED_LENGTH);
  cur_ptr[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
  cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;
  lwpa_pack_16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  strncpy((char *)cur_ptr, data->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
  cur_ptr[E133_DOMAIN_STRING_PADDED_LENGTH - 1] = '\0';
  cur_ptr += E133_DOMAIN_STRING_PADDED_LENGTH;
  *cur_ptr++ = data->connect_flags;
  send_res = lwpa_send(conn->sock, buf, cur_ptr - buf, 0);
  if (send_res < 0)
    return (lwpa_error_t)send_res;

  RDMNET_MSVC_END_NO_DEP_WARNINGS()

  /* Pack and send the beginning of the Client Entry PDU */
  pack_client_entry_header(rlp.datalen - (BROKER_PDU_HEADER_SIZE + CLIENT_CONNECT_COMMON_FIELD_SIZE),
                           data->client_entry.client_protocol, &data->client_entry.client_cid, buf);
  send_res = lwpa_send(conn->sock, buf, CLIENT_ENTRY_HEADER_SIZE, 0);

  if (is_rpt_client_entry(&data->client_entry))
  {
    /* Pack and send the RPT client entry */
    const ClientEntryDataRpt *rpt_data = get_rpt_client_entry_data(&data->client_entry);
    cur_ptr = buf;
    lwpa_pack_16b(cur_ptr, rpt_data->client_uid.manu);
    cur_ptr += 2;
    lwpa_pack_32b(cur_ptr, rpt_data->client_uid.id);
    cur_ptr += 4;
    *cur_ptr++ = rpt_data->client_type;
    memcpy(cur_ptr, rpt_data->binding_cid.data, LWPA_UUID_BYTES);
    cur_ptr += LWPA_UUID_BYTES;
    send_res = lwpa_send(conn->sock, buf, RPT_CLIENT_ENTRY_DATA_SIZE, 0);
    if (send_res < 0)
      return (lwpa_error_t)send_res;
  }
  else /* is EPT client entry */
  {
    /* Pack and send the EPT client entry */
    const ClientEntryDataEpt *ept_data = get_ept_client_entry_data(&data->client_entry);
    const EptSubProtocol *prot = ept_data->protocol_list;
    for (; prot; prot = prot->next)
    {
      cur_ptr = buf;
      lwpa_pack_32b(cur_ptr, prot->protocol_vector);
      cur_ptr += 4;
      RDMNET_MSVC_NO_DEP_WRN strncpy((char *)cur_ptr, prot->protocol_string, EPT_PROTOCOL_STRING_PADDED_LENGTH);
      cur_ptr[EPT_PROTOCOL_STRING_PADDED_LENGTH - 1] = '\0';
      cur_ptr += EPT_PROTOCOL_STRING_PADDED_LENGTH;
      send_res = lwpa_send(conn->sock, buf, EPT_PROTOCOL_ENTRY_SIZE, 0);
      if (send_res < 0)
        return (lwpa_error_t)send_res;
    }
  }
  lwpa_timer_reset(&conn->send_timer);
  return LWPA_OK;
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
size_t lwpa_pack_connect_reply(uint8_t *buf, size_t buflen, const LwpaUuid *local_cid, const ConnectReplyMsg *data)
{
  LwpaRootLayerPdu rlp;
  uint8_t *cur_ptr = buf;
  size_t data_size;

  if (!buf || buflen < CONNECT_REPLY_FULL_MSG_SIZE || !local_cid || !data)
    return 0;

  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + CONNECT_REPLY_DATA_SIZE;

  /* Try to pack all the header data */
  data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, VECTOR_BROKER_CONNECT_REPLY);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  /* Pack the Connect Reply data fields */
  lwpa_pack_16b(cur_ptr, data->connect_status);
  cur_ptr += 2;
  lwpa_pack_16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  lwpa_pack_16b(cur_ptr, data->broker_uid.manu);
  cur_ptr += 2;
  lwpa_pack_32b(cur_ptr, data->broker_uid.id);
  cur_ptr += 4;
  lwpa_pack_16b(cur_ptr, data->client_uid.manu);
  cur_ptr += 2;
  lwpa_pack_32b(cur_ptr, data->client_uid.id);
  cur_ptr += 4;

  return cur_ptr - buf;
}

/*! \brief Send a Connect Reply message on an RDMnet connection.
 *  \param[in] handle RDMnet connection handle on which to send the Connect Reply message.
 *  \param[in] local_cid CID of the Component sending the Connect Reply message.
 *  \param[in] data Connect Reply data.
 *  \return #LWPA_OK: Send success.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t send_connect_reply(int handle, const LwpaUuid *local_cid, const ConnectReplyMsg *data)
{
  lwpa_error_t res;
  int send_res;
  LwpaRootLayerPdu rlp;
  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];
  uint8_t *cur_ptr;

  if (!local_cid || !data)
    return LWPA_INVALID;

  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + CONNECT_REPLY_DATA_SIZE;

  res = rdmnet_start_message(handle);
  if (res != LWPA_OK)
    return res;

  res = send_broker_header(handle, NULL, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_CONNECT_REPLY);
  if (res != LWPA_OK)
  {
    rdmnet_end_message(handle);
    return res;
  }

  /* Pack and send the Connect Reply data fields */
  cur_ptr = buf;
  lwpa_pack_16b(cur_ptr, data->connect_status);
  cur_ptr += 2;
  lwpa_pack_16b(cur_ptr, data->e133_version);
  cur_ptr += 2;
  lwpa_pack_16b(cur_ptr, data->broker_uid.manu);
  cur_ptr += 2;
  lwpa_pack_32b(cur_ptr, data->broker_uid.id);
  cur_ptr += 4;
  lwpa_pack_16b(cur_ptr, data->client_uid.manu);
  cur_ptr += 2;
  lwpa_pack_32b(cur_ptr, data->client_uid.id);
  cur_ptr += 4;

  send_res = do_send(handle, NULL, buf, cur_ptr - buf);
  if (send_res < 0)
  {
    rdmnet_end_message(handle);
    return (lwpa_error_t)send_res;
  }

  return rdmnet_end_message(handle);
}

/***************************** Fetch Client List *****************************/

/*! \brief Send a Fetch Client List message on an RDMnet connection.
 *  \param[in] handle RDMnet connection handle on which to send the Fetch Client List message.
 *  \param[in] local_cid CID of the Component sending the Fetch Client List message.
 *  \return #LWPA_OK: Send success.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_SYSERR: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
lwpa_error_t send_fetch_client_list(int handle, const LwpaUuid *local_cid)
{
  lwpa_error_t res;
  LwpaRootLayerPdu rlp;
  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];

  if (!local_cid)
    return LWPA_INVALID;

  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE;

  res = rdmnet_start_message(handle);
  if (res != LWPA_OK)
    return res;

  res = send_broker_header(handle, NULL, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_FETCH_CLIENT_LIST);
  if (res != LWPA_OK)
    return res;

  return rdmnet_end_message(handle);
}

/**************************** Client List Messages ***************************/

size_t calc_client_entry_buf_size(const ClientEntryData *client_entry_list)
{
  size_t res = 0;
  const ClientEntryData *cur_entry = client_entry_list;

  for (; cur_entry; cur_entry = cur_entry->next)
  {
    if (cur_entry->client_protocol == E133_CLIENT_PROTOCOL_RPT)
    {
      res += RPT_CLIENT_ENTRY_SIZE;
    }
    else
    {
      /* TODO */
      return 0;
    }
  }
  return res;
}

/*! \brief Get the packed buffer size for a given Client List.
 *  \param[in] client_entry_list Client List of which to calculate the packed size.
 *  \return Required buffer size, or 0 on error.
 */
size_t bufsize_client_list(const ClientEntryData *client_entry_list)
{
  return (client_entry_list ? (BROKER_PDU_FULL_HEADER_SIZE + calc_client_entry_buf_size(client_entry_list)) : 0);
}

/*! \brief Pack a Client List message into a buffer.
 *
 *  Multiple types of Broker messages can contain a Client List; indicate which type this should be
 *  with the vector field. Valid values are #VECTOR_BROKER_CONNECTED_CLIENT_LIST,
 *  #VECTOR_BROKER_CLIENT_ADD, #VECTOR_BROKER_CLIENT_REMOVE and #VECTOR_BROKER_CLIENT_ENTRY_CHANGE.
 *
 *  \param[out] buf Buffer into which to pack the Client List message.
 *  \param[in] buflen Length in bytes of buf.
 *  \param[in] local_cid CID of the Component sending the Client List message.
 *  \param[in] vector Which type of Client List message this is.
 *  \param[in] client_entry_list Client List to pack into the data segment.
 *  \return Number of bytes packed, or 0 on error.
 */
size_t pack_client_list(uint8_t *buf, size_t buflen, const LwpaUuid *local_cid, uint16_t vector,
                        const ClientEntryData *client_entry_list)
{
  LwpaRootLayerPdu rlp;
  uint8_t *cur_ptr = buf;
  uint8_t *buf_end = buf + buflen;
  size_t data_size;
  const ClientEntryData *cur_entry;

  if (!buf || buflen < BROKER_PDU_FULL_HEADER_SIZE || !local_cid || !client_entry_list ||
      (vector != VECTOR_BROKER_CONNECTED_CLIENT_LIST && vector != VECTOR_BROKER_CLIENT_ADD &&
       vector != VECTOR_BROKER_CLIENT_REMOVE && vector != VECTOR_BROKER_CLIENT_ENTRY_CHANGE))
  {
    return 0;
  }

  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_PDU_HEADER_SIZE + calc_client_entry_buf_size(client_entry_list);

  /* Try to pack all the header data */
  data_size = pack_broker_header_with_rlp(&rlp, buf, buflen, vector);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  for (cur_entry = client_entry_list; cur_entry; cur_entry = cur_entry->next)
  {
    /* Check bounds */
    if (cur_ptr + CLIENT_ENTRY_HEADER_SIZE > buf_end)
      return 0;

    /* Pack the common client entry fields. */
    *cur_ptr = 0xf0;
    lwpa_pdu_pack_ext_len(cur_ptr, RPT_CLIENT_ENTRY_SIZE);
    cur_ptr += 3;
    lwpa_pack_32b(cur_ptr, cur_entry->client_protocol);
    cur_ptr += 4;
    memcpy(cur_ptr, cur_entry->client_cid.data, LWPA_UUID_BYTES);
    cur_ptr += LWPA_UUID_BYTES;

    if (cur_entry->client_protocol == E133_CLIENT_PROTOCOL_RPT)
    {
      const ClientEntryDataRpt *rpt_data = get_rpt_client_entry_data(cur_entry);

      /* Check bounds. */
      if (cur_ptr + RPT_CLIENT_ENTRY_DATA_SIZE > buf_end)
        return 0;

      /* Pack the RPT Client Entry data */
      lwpa_pack_16b(cur_ptr, rpt_data->client_uid.manu);
      cur_ptr += 2;
      lwpa_pack_32b(cur_ptr, rpt_data->client_uid.id);
      cur_ptr += 4;
      *cur_ptr++ = rpt_data->client_type;
      memcpy(cur_ptr, rpt_data->binding_cid.data, LWPA_UUID_BYTES);
      cur_ptr += LWPA_UUID_BYTES;
    }
    else
    {
      /* TODO EPT */
      return 0;
    }
  }
  return cur_ptr - buf;
}

/******************************** Disconnect *********************************/

lwpa_error_t send_disconnect(RdmnetConnection *conn, const DisconnectMsg *data)
{
  lwpa_error_t res;
  int send_res;
  LwpaRootLayerPdu rlp;
  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];

  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_DISCONNECT_MSG_SIZE;

  res = send_broker_header(-1, conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_DISCONNECT);
  if (res != LWPA_OK)
    return res;

  lwpa_pack_16b(buf, data->disconnect_reason);
  send_res = lwpa_send(conn->sock, buf, 2, 0);
  if (send_res < 0)
    return (lwpa_error_t)send_res;

  lwpa_timer_reset(&conn->send_timer);
  return LWPA_OK;
}

/*********************************** Null ************************************/

lwpa_error_t send_null(RdmnetConnection *conn)
{
  lwpa_error_t res;
  LwpaRootLayerPdu rlp;
  uint8_t buf[ACN_RLP_HEADER_SIZE_EXT_LEN];

  rlp.sender_cid = conn->local_cid;
  rlp.vector = ACN_VECTOR_ROOT_BROKER;
  rlp.datalen = BROKER_NULL_MSG_SIZE;

  res = send_broker_header(-1, conn, &rlp, buf, ACN_RLP_HEADER_SIZE_EXT_LEN, VECTOR_BROKER_NULL);
  if (res != LWPA_OK)
    return res;

  lwpa_timer_reset(&conn->send_timer);
  return LWPA_OK;
}
