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
 * \file rdmnet/core/rpt_prot.h
 * \brief Functions to pack, send and parse RPT PDUs and their encapsulated messages.
 */

#ifndef RDMNET_CORE_RPT_PROT_H_
#define RDMNET_CORE_RPT_PROT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "etcpal/acn_rlp.h"
#include "etcpal/error.h"
#include "etcpal/uuid.h"
#include "rdm/message.h"
#include "rdmnet/defs.h"
#include "rdmnet/common.h"
#include "rdmnet/core/util.h"

/*!
 * \addtogroup rdmnet_message
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/* RPT PDU Header:
 * Flags + Length:          3
 * Vector:                  4
 * Source UID:              6
 * Source Endpoint ID:      2
 * Destination UID:         6
 * Destination Endpoint ID: 2
 * Sequence Number:         4
 * Reserved:                1
 * --------------------------
 * Total:                  28 */
/*! The header size of an RPT PDU (not including encapsulating PDUs) */
#define RPT_PDU_HEADER_SIZE 28
/*! The header size of an RPT PDU, including encapsulating PDUs */
#define RPT_PDU_FULL_HEADER_SIZE (RPT_PDU_HEADER_SIZE + ACN_RLP_HEADER_SIZE_EXT_LEN + ACN_TCP_PREAMBLE_SIZE)

/* RPT Status Header:
 * Flags + Length: 3
 * Vector:         2
 * -----------------
 * Total:          5 */
/*! The header size of an RPT Status PDU (not including encapsulating PDUs) */
#define RPT_STATUS_HEADER_SIZE 5
/*! The maximum length of the Status String portion of an RPT Status message. */
#define RPT_STATUS_STRING_MAXLEN 1024
/*! The maximum length of an RPT Status message, including all encapsulating PDUs. */
#define RPT_STATUS_FULL_MSG_MAX_SIZE (RPT_PDU_FULL_HEADER_SIZE + RPT_STATUS_HEADER_SIZE + RPT_STATUS_STRING_MAXLEN)

/* Request or Notification PDU Header:
 * Flags + Length: 3
 * Vector:         4
 * -----------------
 * Total:          7 */
#define REQUEST_NOTIF_PDU_HEADER_SIZE 7

/* RDM Command PDU Minimum Size:
 * Flags + Length:                      3
 * Minimum RDM Command Size: [Referenced]
 * --------------------------------------
 * Total non-referenced:                3 */
#define RDM_CMD_PDU_MIN_SIZE (3 + RDM_MIN_BYTES)

/* RDM Command PDU Maximum Size:
 * Flags + Length:                      3
 * Maximum RDM Command Size: [Referenced]
 * --------------------------------------
 * Total non-referenced:                3 */
#define RDM_CMD_PDU_MAX_SIZE (3 + RDM_MAX_BYTES)

#define REQUEST_PDU_MAX_SIZE (REQUEST_NOTIF_PDU_HEADER_SIZE + RDM_CMD_PDU_MAX_SIZE)

/*! The header of an RPT message. */
typedef struct RptHeader
{
  /*! The UID of the RPT Component that originated this message. */
  RdmUid source_uid;
  /*! Identifier for the Endpoint from which this message originated. */
  uint16_t source_endpoint_id;
  /*! The UID of the RPT Component to which this message is addressed. */
  RdmUid dest_uid;
  /*! Identifier for the Endpoint to which this message is directed. */
  uint16_t dest_endpoint_id;
  /*! A sequence number that identifies this RPT Transaction. */
  uint32_t seqnum;
} RptHeader;

/*! The RPT Status message in the RPT protocol. */
typedef struct RptStatusMsg
{
  /*! A status code that indicates the specific error or status condition. */
  rpt_status_code_t status_code;
  /*! An optional implementation-defined status string to accompany this status message. */
  const char* status_string;
} RptStatusMsg;

/*! A list of packed RDM Commands. Two types of RPT messages contain an RptRdmBufList: Request and
 *  Notification. */
typedef struct RptRdmBufList
{
  /*! An array of packed RDM commands and/or responses. */
  RdmBuffer* rdm_buffers;
  /*! The size of the rdm_buffers array. */
  size_t num_rdm_buffers;
  /*!
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store RDM Commands and must deliver the partial list before continuing.
   * The application should store the entries in the list but should not act on the list until
   * another RptRdmBufList is received with more_coming set to false.
   */
  bool more_coming;
} RptRdmBufList;

/*! An RPT message. */
typedef struct RptMessage
{
  /*! The vector indicates which type of message is present in the data section.
   *  Valid values are indicated by VECTOR_RPT_* in rdmnet/defs.h. */
  uint32_t vector;
  /*! The header contains routing information and metadata for the RPT message. */
  RptHeader header;
  /*! The encapsulated message; use the helper macros to access it. */
  union
  {
    RptStatusMsg status;
    RptRdmBufList rdm;
  } data;
} RptMessage;

/*!
 * \brief Determine whether an RptMessage contains an RDM Buffer List. Multiple types of RPT
 *        Messages can contain RDM Buffer Lists.
 * \param rptmsgptr Pointer to RptMessage.
 * \return (bool) Whether the message contains an RDM Buffer List.
 */
#define RPT_IS_RDM_BUF_LIST(rptmsgptr) \
  ((rptmsgptr)->vector == VECTOR_RPT_REQUEST || (rptmsgptr)->vector == VECTOR_RPT_NOTIFICATION)

/*!
 * \brief Get the encapsulated RDM Buffer List from an RptMessage.
 * \param rptmsgptr Pointer to RptMessage.
 * \return Pointer to encapsulated RDM Buffer List (RptRdmBufList*).
 */
#define RPT_GET_RDM_BUF_LIST(rptmsgptr) (&(rptmsgptr)->data.rdm)

/*!
 * \brief Determine whether an RptMessage contains an RPT Status Message.
 * \param rptmsgptr Pointer to RptMessage.
 * \return (bool) Whether the message contains an RPT Status Message.
 */
#define RPT_IS_STATUS_MSG(rptmsgptr) ((rptmsgptr)->vector == VECTOR_RPT_STATUS)

/*!
 * \brief Get the encapsulated RPT Status message from an RptMessage.
 * \param rptmsgptr Pointer to RptMessage.
 * \return Pointer to encapsulated RPT Status Message (RptStatusMsg*).
 */
#define RPT_GET_STATUS_MSG(rptmsgptr) (&(rptmsgptr)->data.status)

size_t rpt_get_request_buffer_size(const RdmBuffer* cmd);
size_t rpt_get_status_buffer_size(const RptStatusMsg* status);
size_t rpt_get_notification_buffer_size(const RdmBuffer* cmd_arr, size_t cmd_arr_size);

size_t rpt_pack_request(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, const RptHeader* header,
                        const RdmBuffer* cmd);
size_t rpt_pack_status(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, const RptHeader* header,
                       const RptStatusMsg* status);
size_t rpt_pack_notification(uint8_t* buf, size_t buflen, const EtcPalUuid* local_cid, const RptHeader* header,
                             const RdmBuffer* cmd_arr, size_t cmd_arr_size);

etcpal_error_t rpt_send_request(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const RptHeader* header,
                                const RdmBuffer* cmd);
etcpal_error_t rpt_send_status(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const RptHeader* header,
                               const RptStatusMsg* status);
etcpal_error_t rpt_send_notification(rdmnet_conn_t handle, const EtcPalUuid* local_cid, const RptHeader* header,
                                     const RdmBuffer* cmd_arr, size_t cmd_arr_size);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* RDMNET_CORE_RPT_PROT_H_ */