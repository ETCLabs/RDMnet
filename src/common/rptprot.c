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

#include "rdmnet/rptprot.h"

#include "lwpa_pack.h"
#include "rdmnet/connection.h"
#include "estardmnet.h"
#include "rptprotpriv.h"

/* Suppress strncpy() warning on Windows/MSVC. */
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/***************************** Private macros ********************************/

#define rdm_cmd_pdu_len(rdmbufptr) ((rdmbufptr)->datalen + 3)
#define pack_rdm_cmd_pdu(rdmbufptr, buf)                                \
  do                                                                    \
  {                                                                     \
    (buf)[0] = 0xf0;                                                    \
    pdu_pack_ext_len(buf, rdm_cmd_pdu_len(rdmbufptr));                  \
    (buf)[3] = VECTOR_RDM_CMD_RDM_DATA;                                 \
    memcpy(&(buf)[4], &(rdmbufptr)->data[1], (rdmbufptr)->datalen - 1); \
  } while (0)

#define pack_request_header(length, buf)       \
  do                                           \
  {                                            \
    (buf)[0] = 0xf0;                           \
    pdu_pack_ext_len(buf, length);             \
    pack_32b(&buf[3], VECTOR_REQUEST_RDM_CMD); \
  } while (0)
#define pack_status_header(length, vector, buf) \
  do                                            \
  {                                             \
    (buf)[0] = 0xf0;                            \
    pdu_pack_ext_len(buf, length);              \
    pack_16b(&buf[3], vector);                  \
  } while (0)
#define pack_notification_header(length, buf)       \
  do                                                \
  {                                                 \
    (buf)[0] = 0xf0;                                \
    pdu_pack_ext_len(buf, length);                  \
    pack_32b(&buf[3], VECTOR_NOTIFICATION_RDM_CMD); \
  } while (0)

/*********************** Private function prototypes *************************/

static void pack_rpt_header(size_t length, uint32_t vector, const RptHeader *header, uint8_t *buf);
static lwpa_error_t send_rpt_header(int handle, const RootLayerPdu *rlp, uint32_t rpt_vector, const RptHeader *header,
                                    uint8_t *buf, size_t buflen);
static size_t calc_rpt_request_len(const RdmBuffer *cmd);
static size_t calc_status_pdu_size(const RptStatusMsg *status);

/*************************** Function definitions ****************************/

void pack_rpt_header(size_t length, uint32_t vector, const RptHeader *header, uint8_t *buf)
{
  buf[0] = 0xf0;
  pdu_pack_ext_len(buf, length);
  pack_32b(&buf[3], vector);
  pack_16b(&buf[7], header->source_uid.manu);
  pack_32b(&buf[9], header->source_uid.id);
  pack_16b(&buf[13], header->source_endpoint_id);
  pack_16b(&buf[15], header->dest_uid.manu);
  pack_32b(&buf[17], header->dest_uid.id);
  pack_16b(&buf[21], header->dest_endpoint_id);
  pack_32b(&buf[23], header->seqnum);
  buf[27] = 0;
}

size_t pack_rpt_header_with_rlp(const RootLayerPdu *rlp, uint8_t *buf, size_t buflen, uint32_t vector,
                                const RptHeader *header)
{
  uint8_t *cur_ptr = buf;
  size_t data_size = root_layer_buf_size(rlp, 1);

  if (data_size == 0)
    return 0;

  data_size = pack_tcp_preamble(cur_ptr, buflen, data_size);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;
  buflen -= data_size;

  data_size = pack_root_layer_header(cur_ptr, buflen, rlp);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;
  buflen -= data_size;

  pack_rpt_header(rlp->datalen, vector, header, cur_ptr);
  cur_ptr += RPT_PDU_HEADER_SIZE;
  return cur_ptr - buf;
}

lwpa_error_t send_rpt_header(int handle, const RootLayerPdu *rlp, uint32_t rpt_vector, const RptHeader *header,
                             uint8_t *buf, size_t buflen)
{
  int res;
  size_t data_size = root_layer_buf_size(rlp, 1);
  if (data_size == 0)
  {
    rdmnet_end_message(handle);
    return LWPA_PROTERR;
  }

  /* Pack and send the TCP preamble. */
  data_size = pack_tcp_preamble(buf, buflen, data_size);
  if (data_size == 0)
  {
    rdmnet_end_message(handle);
    return LWPA_PROTERR;
  }
  res = rdmnet_send_partial_message(handle, buf, data_size);
  if (res < 0)
    return res;

  /* Pack and send the Root Layer PDU header */
  data_size = pack_root_layer_header(buf, buflen, rlp);
  if (data_size == 0)
  {
    rdmnet_end_message(handle);
    return LWPA_PROTERR;
  }
  res = rdmnet_send_partial_message(handle, buf, data_size);
  if (res < 0)
    return res;

  /* Pack and send the RPT PDU header */
  pack_rpt_header(rlp->datalen, rpt_vector, header, buf);
  res = rdmnet_send_partial_message(handle, buf, RPT_PDU_HEADER_SIZE);
  if (res < 0)
    return res;

  return LWPA_OK;
}

size_t calc_request_pdu_size(const RdmBuffer *cmd)
{
  return REQUEST_NOTIF_PDU_HEADER_SIZE + rdm_cmd_pdu_len(cmd);
}

size_t bufsize_rpt_request(const RdmBuffer *cmd)
{
  return (cmd ? (RPT_PDU_FULL_HEADER_SIZE + calc_request_pdu_size(cmd)) : 0);
}

size_t pack_rpt_request(uint8_t *buf, size_t buflen, const LwpaCid *local_cid, const RptHeader *header,
                        const RdmBuffer *cmd)
{
  RootLayerPdu rlp;
  uint8_t *cur_ptr = buf;
  size_t data_size;
  size_t request_pdu_size;

  if (!buf || !local_cid || !header || !cmd || buflen < bufsize_rpt_request(cmd))
  {
    return 0;
  }

  request_pdu_size = calc_request_pdu_size(cmd);
  rlp.sender_cid = *local_cid;
  rlp.vector = VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + request_pdu_size;

  data_size = pack_rpt_header_with_rlp(&rlp, buf, buflen, VECTOR_RPT_REQUEST, header);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  pack_request_header(request_pdu_size, cur_ptr);
  cur_ptr += REQUEST_NOTIF_PDU_HEADER_SIZE;

  pack_rdm_cmd_pdu(cmd, cur_ptr);
  cur_ptr += request_pdu_size - REQUEST_NOTIF_PDU_HEADER_SIZE;
  return cur_ptr - buf;
}

lwpa_error_t send_rpt_request(int handle, const LwpaCid *local_cid, const RptHeader *header, const RdmBuffer *cmd)
{
  int res;
  RootLayerPdu rlp;
  uint8_t buf[RDM_CMD_PDU_MAX_SIZE];

  if (!local_cid || !header || !cmd)
    return LWPA_INVALID;

  rlp.sender_cid = *local_cid;
  rlp.vector = VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + calc_request_pdu_size(cmd);

  res = rdmnet_start_message(handle);
  if (res != LWPA_OK)
    return res;

  res = send_rpt_header(handle, &rlp, VECTOR_RPT_REQUEST, header, buf, RDM_CMD_PDU_MAX_SIZE);
  if (res != LWPA_OK)
  {
    rdmnet_end_message(handle);
    return res;
  }

  pack_request_header(rlp.datalen - RPT_PDU_HEADER_SIZE, buf);
  res = rdmnet_send_partial_message(handle, buf, REQUEST_NOTIF_PDU_HEADER_SIZE);
  if (res < 0)
  {
    rdmnet_end_message(handle);
    return res;
  }

  pack_rdm_cmd_pdu(cmd, buf);
  res = rdmnet_send_partial_message(handle, buf, rdm_cmd_pdu_len(cmd));
  if (res < 0)
  {
    rdmnet_end_message(handle);
    return res;
  }

  return rdmnet_end_message(handle);
}

size_t calc_status_pdu_size(const RptStatusMsg *status)
{
#if RDMNET_DYNAMIC_MEM
  return (RPT_STATUS_HEADER_SIZE +
          (status->status_string ? strnlen(status->status_string, RPT_STATUS_STRING_MAXLEN) : 0));
#else
  return (RPT_STATUS_HEADER_SIZE + strnlen(status->status_string, RPT_STATUS_STRING_MAXLEN));
#endif
}

size_t bufsize_rpt_status(const RptStatusMsg *status)
{
  return (status ? RPT_PDU_FULL_HEADER_SIZE + calc_status_pdu_size(status) : 0);
}

size_t pack_rpt_status(uint8_t *buf, size_t buflen, const LwpaCid *local_cid, const RptHeader *header,
                       const RptStatusMsg *status)
{
  RootLayerPdu rlp;
  uint8_t *cur_ptr = buf;
  size_t data_size;
  size_t status_pdu_size;

  if (!buf || !local_cid || !header || !status || buflen < bufsize_rpt_status(status))
  {
    return 0;
  }

  status_pdu_size = calc_status_pdu_size(status);
  rlp.sender_cid = *local_cid;
  rlp.vector = VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + status_pdu_size;

  data_size = pack_rpt_header_with_rlp(&rlp, buf, buflen, VECTOR_RPT_STATUS, header);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  pack_status_header(status_pdu_size, status->status_code, cur_ptr);
  cur_ptr += RPT_STATUS_HEADER_SIZE;
  if (status_pdu_size > RPT_STATUS_HEADER_SIZE)
  {
    strncpy((char *)cur_ptr, status->status_string, RPT_STATUS_STRING_MAXLEN);
    cur_ptr += (status_pdu_size - RPT_STATUS_HEADER_SIZE);
  }
  return cur_ptr - buf;
}

lwpa_error_t send_rpt_status(int handle, const LwpaCid *local_cid, const RptHeader *header, const RptStatusMsg *status)
{
  int res;
  RootLayerPdu rlp;
  uint8_t buf[RPT_PDU_HEADER_SIZE];
  size_t status_pdu_size;

  if (!local_cid || !header || !status)
    return LWPA_INVALID;

  status_pdu_size = calc_status_pdu_size(status);
  rlp.sender_cid = *local_cid;
  rlp.vector = VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + status_pdu_size;

  res = rdmnet_start_message(handle);
  if (res != LWPA_OK)
    return res;

  res = send_rpt_header(handle, &rlp, VECTOR_RPT_STATUS, header, buf, RPT_PDU_HEADER_SIZE);
  if (res != LWPA_OK)
  {
    rdmnet_end_message(handle);
    return res;
  }

  pack_status_header(status_pdu_size, status->status_code, buf);
  res = rdmnet_send_partial_message(handle, buf, RPT_STATUS_HEADER_SIZE);
  if (res < 0)
  {
    rdmnet_end_message(handle);
    return res;
  }

  if (status_pdu_size > RPT_STATUS_HEADER_SIZE)
  {
    res =
        rdmnet_send_partial_message(handle, (uint8_t *)status->status_string, status_pdu_size - RPT_STATUS_HEADER_SIZE);
    if (res < 0)
    {
      rdmnet_end_message(handle);
      return res;
    }
  }

  return rdmnet_end_message(handle);
}

size_t calc_notification_pdu_size(const RdmCmdListEntry *cmd_list)
{
  size_t res = REQUEST_NOTIF_PDU_HEADER_SIZE;
  const RdmCmdListEntry *cur_cmd = cmd_list;

  for (; cur_cmd; cur_cmd = cur_cmd->next)
  {
    res += rdm_cmd_pdu_len(&cur_cmd->msg);
  }
  return res;
}

size_t bufsize_rpt_notification(const RdmCmdListEntry *cmd_list)
{
  return (cmd_list ? (RPT_PDU_FULL_HEADER_SIZE + calc_notification_pdu_size(cmd_list)) : 0);
}

size_t pack_rpt_notification(uint8_t *buf, size_t buflen, const LwpaCid *local_cid, const RptHeader *header,
                             const RdmCmdListEntry *cmd_list)
{
  RootLayerPdu rlp;
  uint8_t *cur_ptr = buf;
  size_t data_size;
  size_t notif_pdu_size;
  const RdmCmdListEntry *cur_cmd;

  if (!buf || !local_cid || !header || !cmd_list || buflen < bufsize_rpt_notification(cmd_list))
  {
    return 0;
  }

  notif_pdu_size = calc_notification_pdu_size(cmd_list);
  rlp.sender_cid = *local_cid;
  rlp.vector = VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + notif_pdu_size;

  data_size = pack_rpt_header_with_rlp(&rlp, buf, buflen, VECTOR_RPT_NOTIFICATION, header);
  if (data_size == 0)
    return 0;
  cur_ptr += data_size;

  pack_notification_header(notif_pdu_size, cur_ptr);
  cur_ptr += REQUEST_NOTIF_PDU_HEADER_SIZE;

  for (cur_cmd = cmd_list; cur_cmd; cur_cmd = cur_cmd->next)
  {
    pack_rdm_cmd_pdu(&cur_cmd->msg, cur_ptr);
    cur_ptr += rdm_cmd_pdu_len(&cur_cmd->msg);
  }
  return cur_ptr - buf;
}

lwpa_error_t send_rpt_notification(int handle, const LwpaCid *local_cid, const RptHeader *header,
                                   const RdmCmdListEntry *cmd_list)
{
  int res;
  RootLayerPdu rlp;
  uint8_t buf[RDM_CMD_PDU_MAX_SIZE];
  size_t notif_pdu_size;
  const RdmCmdListEntry *cur_cmd;

  if (!local_cid || !header || !cmd_list)
    return LWPA_INVALID;

  notif_pdu_size = calc_notification_pdu_size(cmd_list);
  rlp.sender_cid = *local_cid;
  rlp.vector = VECTOR_ROOT_RPT;
  rlp.datalen = RPT_PDU_HEADER_SIZE + notif_pdu_size;

  res = rdmnet_start_message(handle);
  if (res != LWPA_OK)
    return res;

  res = send_rpt_header(handle, &rlp, VECTOR_RPT_NOTIFICATION, header, buf, RDM_CMD_PDU_MAX_SIZE);
  if (res != LWPA_OK)
  {
    rdmnet_end_message(handle);
    return res;
  }

  pack_notification_header(notif_pdu_size, buf);
  res = rdmnet_send_partial_message(handle, buf, REQUEST_NOTIF_PDU_HEADER_SIZE);
  if (res < 0)
  {
    rdmnet_end_message(handle);
    return res;
  }

  for (cur_cmd = cmd_list; cur_cmd; cur_cmd = cur_cmd->next)
  {
    pack_rdm_cmd_pdu(&cur_cmd->msg, buf);
    res = rdmnet_send_partial_message(handle, buf, rdm_cmd_pdu_len(&cur_cmd->msg));
  }

  return rdmnet_end_message(handle);
}
