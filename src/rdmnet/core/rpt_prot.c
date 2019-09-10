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

#include "rdmnet/core/rpt_prot.h"

#include "etcpal/pack.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/util.h"
#include "rdmnet/private/connection.h"
#include "rdmnet/private/rpt_prot.h"

/***************************** Private macros ********************************/

/* Helper macros for RDM Command PDUs */
#define RDM_CMD_PDU_LEN(rdmbufptr) ((rdmbufptr)->datalen + 3)
#define PACK_RDM_CMD_PDU(rdmbufptr, buf)                                \
  do                                                                    \
  {                                                                     \
    (buf)[0] = 0xf0;                                                    \
    ETCPAL_PDU_PACK_EXT_LEN(buf, RDM_CMD_PDU_LEN(rdmbufptr));           \
    (buf)[3] = VECTOR_RDM_CMD_RDM_DATA;                                 \
    memcpy(&(buf)[4], &(rdmbufptr)->data[1], (rdmbufptr)->datalen - 1); \
  } while (0)

/* Helper macros to pack the various RPT headers */
#define PACK_REQUEST_HEADER(length, buf)              \
  do                                                  \
  {                                                   \
    (buf)[0] = 0xf0;                                  \
    ETCPAL_PDU_PACK_EXT_LEN(buf, length);             \
    etcpal_pack_32b(&buf[3], VECTOR_REQUEST_RDM_CMD); \
  } while (0)
#define PACK_STATUS_HEADER(length, vector, buf) \
  do                                            \
  {                                             \
    (buf)[0] = 0xf0;                            \
    ETCPAL_PDU_PACK_EXT_LEN(buf, length);       \
    etcpal_pack_16b(&buf[3], vector);           \
  } while (0)
#define PACK_NOTIFICATION_HEADER(length, buf)              \
  do                                                       \
  {                                                        \
    (buf)[0] = 0xf0;                                       \
    ETCPAL_PDU_PACK_EXT_LEN(buf, length);                  \
    etcpal_pack_32b(&buf[3], VECTOR_NOTIFICATION_RDM_CMD); \
  } while (0)

/*********************** Private function prototypes *************************/

static void pack_rpt_header(size_t length, uint32_t vector, const RptHeader* header, uint8_t* buf);
static etcpal_error_t send_rpt_header(RdmnetConnection* conn, const EtcPalRootLayerPdu* rlp, uint32_t rpt_vector,
                                      const RptHeader* header, uint8_t* buf, size_t buflen);
static size_t calc_request_pdu_size(const RdmBuffer* cmd);
static size_t calc_status_pdu_size(const RptStatusMsg* status);
static size_t calc_notification_pdu_size(const RdmBuffer* cmd_arr, size_t num_cmds);

/*************************** Function definitions ****************************/

void pack_rpt_header(size_t length, uint32_t vector, const RptHeader* header, uint8_t* buf)
{
  buf[0] = 0xf0;
  ETCPAL_PDU_PACK_EXT_LEN(buf, length);
  etcpal_pack_32b(&buf[3], vector);
  etcpal_pack_16b(&buf[7], header->source_uid.manu);
  etcpal_pack_32b(&buf[9], header->source_uid.id);
  etcpal_pack_16b(&buf[13], header->source_endpoint_id);
  etcpal_pack_16b(&buf[15], header->dest_uid.manu);
  etcpal_pack_32b(&buf[17], header->dest_uid.id);
  etcpal_pack_16b(&buf[21], header->dest_endpoint_id);
  etcpal_pack_32b(&buf[23], header->seqnum);
  buf[27] = 0;
}

size_t pack_rpt_header_with_rlp(const EtcPalRootLayerPdu* rlp, uint8_t* buf, size_t buflen, uint32_t vector,
                                const RptHeader* header)
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

  pack_rpt_header(rlp->datalen, vector, header, cur_ptr);
  cur_ptr += RPT_PDU_HEADER_SIZE;
  return (size_t)(cur_ptr - buf);
}

etcpal_error_t send_rpt_header(RdmnetConnection* conn, const EtcPalRootLayerPdu* rlp, uint32_t rpt_vector,
                               const RptHeader* header, uint8_t* buf, size_t buflen)
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

  // Pack and send the Root Layer PDU header
  data_size = etcpal_pack_root_layer_header(buf, buflen, rlp);
  if (data_size == 0)
    return kEtcPalErrProtocol;

  send_res = etcpal_send(conn->sock, buf, data_size, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  // Pack and send the RPT PDU header
  pack_rpt_header(rlp->datalen, rpt_vector, header, buf);
  send_res = etcpal_send(conn->sock, buf, RPT_PDU_HEADER_SIZE, 0);
  if (send_res < 0)
    return (etcpal_error_t)send_res;

  return kEtcPalErrOk;
}

size_t calc_request_pdu_size(const RdmBuffer* cmd)
{
  return REQUEST_NOTIF_PDU_HEADER_SIZE + RDM_CMD_PDU_LEN(cmd);
}

/*! \brief Get the packed buffer size for an RPT Request message.
 *  \param[in] cmd Encapsulated RDM Command that will occupy the RPT Request message.
 *  \return Required buffer size, or 0 on error.
 */
size_t bufsize_rpt_request(const RdmBuffer* cmd)
{
  return (cmd ? (RPT_PDU_FULL_HEADER_SIZE + calc_request_pdu_size(cmd)) : 0);
}

/*! \brief Pack an RPT Request message into a buffer.
 *  \param[out] buf Buffer into which to pack the RPT Request message.
 *  \param[in] buflen Length in bytes of buf.
 *  \param[in] local_cid CID of the Component sending the RPT Request message.
 *  \param[in] header Header data for the RPT PDU that encapsulates this Request message.
 *  \param[in] cmd Encapsulated RDM Command that will occupy the RPT Request message.
 *  \return Number of bytes packed, or 0 on error.
 */
size_t pack_rpt_request(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, const RptHeader* header,
                        const RdmBuffer* cmd)
{
  if (!buf || !local_cid || !header || !cmd || buflen < bufsize_rpt_request(cmd))
  {
    return 0;
  }

  size_t request_pdu_size = calc_request_pdu_size(cmd);

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + request_pdu_size;

  uint8_t* cur_ptr = buf;
  size_t data_size = pack_rpt_header_with_rlp(&rlp, buf, buflen, VECTOR_RPT_REQUEST, header);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  PACK_REQUEST_HEADER(request_pdu_size, cur_ptr);
  cur_ptr += REQUEST_NOTIF_PDU_HEADER_SIZE;

  PACK_RDM_CMD_PDU(cmd, cur_ptr);
  cur_ptr += request_pdu_size - REQUEST_NOTIF_PDU_HEADER_SIZE;
  return (size_t)(cur_ptr - buf);
}

/*! \brief Send an RPT Request message on an RDMnet connection.
 *  \param[in] handle RDMnet connection handle on which to send the RPT Request message.
 *  \param[in] local_cid CID of the Component sending the RPT Request message.
 *  \param[in] header Header data for the RPT PDU that encapsulates this RPT Request message.
 *  \param[in] cmd Encapsulated RDM Command that will occupy the RPT Request message.
 *  \return #kEtcPalErrOk: Send success.\n
 *          #kEtcPalErrInvalid: Invalid argument provided.\n
 *          #kEtcPalErrSys: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
etcpal_error_t send_rpt_request(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const RptHeader* header,
                                const RdmBuffer* cmd)
{
  if (!local_cid || !header || !cmd)
    return kEtcPalErrInvalid;

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + calc_request_pdu_size(cmd);

  RdmnetConnection* conn;
  etcpal_error_t res = rdmnet_start_message(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  uint8_t buf[RDM_CMD_PDU_MAX_SIZE];
  res = send_rpt_header(conn, &rlp, VECTOR_RPT_REQUEST, header, buf, RDM_CMD_PDU_MAX_SIZE);
  if (res != kEtcPalErrOk)
  {
    rdmnet_end_message(conn);
    return res;
  }

  PACK_REQUEST_HEADER(rlp.datalen - RPT_PDU_HEADER_SIZE, buf);
  int send_res = etcpal_send(conn->sock, buf, REQUEST_NOTIF_PDU_HEADER_SIZE, 0);
  if (send_res < 0)
  {
    rdmnet_end_message(conn);
    return (etcpal_error_t)send_res;
  }

  PACK_RDM_CMD_PDU(cmd, buf);
  send_res = etcpal_send(conn->sock, buf, RDM_CMD_PDU_LEN(cmd), 0);
  if (send_res < 0)
  {
    rdmnet_end_message(conn);
    return (etcpal_error_t)send_res;
  }

  return rdmnet_end_message(conn);
}

size_t calc_status_pdu_size(const RptStatusMsg* status)
{
#if RDMNET_DYNAMIC_MEM
  return (RPT_STATUS_HEADER_SIZE +
          (status->status_string ? strnlen(status->status_string, RPT_STATUS_STRING_MAXLEN) : 0));
#else
  return (RPT_STATUS_HEADER_SIZE + strnlen(status->status_string, RPT_STATUS_STRING_MAXLEN));
#endif
}

/*! \brief Get the packed buffer size for an RPT Status message.
 *  \param[in] status RPT Status message data.
 *  \return Required buffer size, or 0 on error.
 */
size_t bufsize_rpt_status(const RptStatusMsg* status)
{
  return (status ? RPT_PDU_FULL_HEADER_SIZE + calc_status_pdu_size(status) : 0);
}

/*! \brief Pack an RPT Status message into a buffer.
 *  \param[out] buf Buffer into which to pack the RPT Status message.
 *  \param[in] buflen Length in bytes of buf.
 *  \param[in] local_cid CID of the Component sending the RPT Status message.
 *  \param[in] header Header data for the RPT PDU that encapsulates this Status message.
 *  \param[in] status RPT Status message data.
 *  \return Number of bytes packed, or 0 on error.
 */
size_t pack_rpt_status(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, const RptHeader* header,
                       const RptStatusMsg* status)
{
  if (!buf || !local_cid || !header || !status || buflen < bufsize_rpt_status(status))
  {
    return 0;
  }

  size_t status_pdu_size = calc_status_pdu_size(status);

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + status_pdu_size;

  uint8_t* cur_ptr = buf;
  size_t data_size = pack_rpt_header_with_rlp(&rlp, buf, buflen, VECTOR_RPT_STATUS, header);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  PACK_STATUS_HEADER(status_pdu_size, status->status_code, cur_ptr);
  cur_ptr += RPT_STATUS_HEADER_SIZE;
  if (status_pdu_size > RPT_STATUS_HEADER_SIZE)
  {
    RDMNET_MSVC_NO_DEP_WRN strncpy((char*)cur_ptr, status->status_string, RPT_STATUS_STRING_MAXLEN);
    cur_ptr += (status_pdu_size - RPT_STATUS_HEADER_SIZE);
  }
  return (size_t)(cur_ptr - buf);
}

/*! \brief Send an RPT Status message on an RDMnet connection.
 *  \param[in] handle RDMnet connection handle on which to send the RPT Status message.
 *  \param[in] local_cid CID of the Component sending the RPT Status message.
 *  \param[in] header Header data for the RPT PDU that encapsulates this Status message.
 *  \param[in] status RPT Status message data.
 *  \return #kEtcPalErrOk: Send success.\n
 *          #kEtcPalErrInvalid: Invalid argument provided.\n
 *          #kEtcPalErrSys: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
etcpal_error_t send_rpt_status(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const RptHeader* header,
                               const RptStatusMsg* status)
{
  if (!local_cid || !header || !status)
    return kEtcPalErrInvalid;

  size_t status_pdu_size = calc_status_pdu_size(status);

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + status_pdu_size;

  RdmnetConnection* conn;
  etcpal_error_t res = rdmnet_start_message(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  uint8_t buf[RPT_PDU_HEADER_SIZE];
  res = send_rpt_header(conn, &rlp, VECTOR_RPT_STATUS, header, buf, RPT_PDU_HEADER_SIZE);
  if (res != kEtcPalErrOk)
  {
    rdmnet_end_message(conn);
    return res;
  }

  PACK_STATUS_HEADER(status_pdu_size, status->status_code, buf);
  int send_res = etcpal_send(conn->sock, buf, RPT_STATUS_HEADER_SIZE, 0);
  if (send_res < 0)
  {
    rdmnet_end_message(conn);
    return (etcpal_error_t)send_res;
  }

  if (status_pdu_size > RPT_STATUS_HEADER_SIZE)
  {
    send_res = etcpal_send(conn->sock, (uint8_t*)status->status_string, status_pdu_size - RPT_STATUS_HEADER_SIZE, 0);
    if (send_res < 0)
    {
      rdmnet_end_message(conn);
      return (etcpal_error_t)send_res;
    }
  }

  return rdmnet_end_message(conn);
}

size_t calc_notification_pdu_size(const RdmBuffer* cmd_arr, size_t cmd_arr_size)
{
  size_t res = REQUEST_NOTIF_PDU_HEADER_SIZE;

  for (const RdmBuffer* cur_cmd = cmd_arr; cur_cmd < cmd_arr + cmd_arr_size; ++cur_cmd)
  {
    res += RDM_CMD_PDU_LEN(cur_cmd);
  }
  return res;
}

/*! \brief Get the packed buffer size for an RPT Notification message.
 *  \param[in] cmd_arr Array of packed RDM Commands that will occupy the RPT Notification message.
 *  \param[in] cmd_arr_size Size of packed RDM Command array.
 *  \return Required buffer size, or 0 on error.
 */
size_t bufsize_rpt_notification(const RdmBuffer* cmd_arr, size_t cmd_arr_size)
{
  return (cmd_arr ? (RPT_PDU_FULL_HEADER_SIZE + calc_notification_pdu_size(cmd_arr, cmd_arr_size)) : 0);
}

/*! \brief Pack an RPT Notification message into a buffer.
 *  \param[out] buf Buffer into which to pack the RPT Notification message.
 *  \param[in] buflen Length in bytes of buf.
 *  \param[in] local_cid CID of the Component sending the RPT Notification message.
 *  \param[in] header Header data for the RPT PDU that encapsulates this RPT Notification message.
 *  \param[in] cmd_arr Array of packed RDM Commands contained in this RPT Notification.
 *  \param[in] cmd_arr_size Size of packed RDM Command array.
 *  \return Number of bytes packed, or 0 on error.
 */
size_t pack_rpt_notification(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, const RptHeader* header,
                             const RdmBuffer* cmd_arr, size_t cmd_arr_size)
{
  if (!buf || !local_cid || !header || !cmd_arr || cmd_arr_size == 0 ||
      buflen < bufsize_rpt_notification(cmd_arr, cmd_arr_size))
  {
    return 0;
  }

  size_t notif_pdu_size = calc_notification_pdu_size(cmd_arr, cmd_arr_size);

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + notif_pdu_size;

  uint8_t* cur_ptr = buf;
  size_t data_size = pack_rpt_header_with_rlp(&rlp, buf, buflen, VECTOR_RPT_NOTIFICATION, header);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  PACK_NOTIFICATION_HEADER(notif_pdu_size, cur_ptr);
  cur_ptr += REQUEST_NOTIF_PDU_HEADER_SIZE;

  for (const RdmBuffer* cur_cmd = cmd_arr; cur_cmd < cmd_arr + cmd_arr_size; ++cur_cmd)
  {
    PACK_RDM_CMD_PDU(cur_cmd, cur_ptr);
    cur_ptr += RDM_CMD_PDU_LEN(cur_cmd);
  }
  return (size_t)(cur_ptr - buf);
}

/*! \brief Send an RPT Notification message on an RDMnet connection.
 *  \param[in] handle RDMnet connection handle on which to send the RPT Notification message.
 *  \param[in] local_cid CID of the Component sending the RPT Notification message.
 *  \param[in] header Header data for the RPT PDU that encapsulates this RPT Notification message.
 *  \param[in] cmd_arr Array of packed RDM Commands contained in this RPT Notification.
 *  \param[in] cmd_arr_size Size of packed RDM Command array.
 *  \return #kEtcPalErrOk: Send success.\n
 *          #kEtcPalErrInvalid: Invalid argument provided.\n
 *          #kEtcPalErrSys: An internal library or system call error occurred.\n
 *          Note: Other error codes might be propagated from underlying socket calls.\n
 */
etcpal_error_t send_rpt_notification(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const RptHeader* header,
                                     const RdmBuffer* cmd_arr, size_t cmd_arr_size)
{
  if (!local_cid || !header || !cmd_arr || cmd_arr_size == 0)
    return kEtcPalErrInvalid;

  size_t notif_pdu_size = calc_notification_pdu_size(cmd_arr, cmd_arr_size);

  EtcPalRootLayerPdu rlp;
  rlp.sender_cid = *local_cid;
  rlp.vector = ACN_VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + notif_pdu_size;

  RdmnetConnection* conn;
  etcpal_error_t res = rdmnet_start_message(handle, &conn);
  if (res != kEtcPalErrOk)
    return res;

  uint8_t buf[RDM_CMD_PDU_MAX_SIZE];
  res = send_rpt_header(conn, &rlp, VECTOR_RPT_NOTIFICATION, header, buf, RDM_CMD_PDU_MAX_SIZE);
  if (res != kEtcPalErrOk)
  {
    rdmnet_end_message(conn);
    return res;
  }

  PACK_NOTIFICATION_HEADER(notif_pdu_size, buf);
  int send_res = etcpal_send(conn->sock, buf, REQUEST_NOTIF_PDU_HEADER_SIZE, 0);
  if (send_res < 0)
  {
    rdmnet_end_message(conn);
    return (etcpal_error_t)send_res;
  }

  for (const RdmBuffer* cur_cmd = cmd_arr; cur_cmd < cmd_arr + cmd_arr_size; ++cur_cmd)
  {
    PACK_RDM_CMD_PDU(cur_cmd, buf);
    send_res = etcpal_send(conn->sock, buf, RDM_CMD_PDU_LEN(cur_cmd), 0);
    if (send_res < 0)
    {
      rdmnet_end_message(conn);
      return (etcpal_error_t)send_res;
    }
  }

  return rdmnet_end_message(conn);
}
