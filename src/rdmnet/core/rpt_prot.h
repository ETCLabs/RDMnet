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

/*
 * rdmnet/core/rpt_prot.h
 * Functions to pack, send and parse RPT PDUs and their encapsulated messages.
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
#include "rdmnet/core/connection.h"
#include "rdmnet/core/rpt_message.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RPT PDU Header:
 * Flags + Length:          3
 * Vector:                  4
 * Source UID:              6
 * Source Endpoint ID:      2
 * Destination UID:         6
 * Destination Endpoint ID: 2
 * Sequence Number:         4
 * Reserved:                1
 * --------------------------
 * Total:                  28
 */
/* The header size of an RPT PDU (not including encapsulating PDUs) */
#define RPT_PDU_HEADER_SIZE 28
/* The header size of an RPT PDU, including encapsulating PDUs */
#define RPT_PDU_FULL_HEADER_SIZE (RPT_PDU_HEADER_SIZE + ACN_RLP_HEADER_SIZE_EXT_LEN + ACN_TCP_PREAMBLE_SIZE)

/*
 * RPT Status Header:
 * Flags + Length: 3
 * Vector:         2
 * -----------------
 * Total:          5
 */
/* The header size of an RPT Status PDU (not including encapsulating PDUs) */
#define RPT_STATUS_HEADER_SIZE 5
/* The maximum length of an RPT Status message, including all encapsulating PDUs. */
#define RPT_STATUS_FULL_MSG_MAX_SIZE (RPT_PDU_FULL_HEADER_SIZE + RPT_STATUS_HEADER_SIZE + RPT_STATUS_STRING_MAXLEN)

/*
 * Request or Notification PDU Header:
 * Flags + Length: 3
 * Vector:         4
 * -----------------
 * Total:          7
 */
#define REQUEST_NOTIF_PDU_HEADER_SIZE 7

/*
 * RDM Command PDU Minimum Size:
 * Flags + Length:                      3
 * Minimum RDM Command Size: [Referenced]
 * --------------------------------------
 * Total non-referenced:                3
 */
#define RDM_CMD_PDU_MIN_SIZE (3 + RDM_MIN_BYTES)

/*
 * RDM Command PDU Maximum Size:
 * Flags + Length:                      3
 * Maximum RDM Command Size: [Referenced]
 * --------------------------------------
 * Total non-referenced:                3
 */
#define RDM_CMD_PDU_MAX_SIZE (3 + RDM_MAX_BYTES)

#define REQUEST_PDU_MAX_SIZE (REQUEST_NOTIF_PDU_HEADER_SIZE + RDM_CMD_PDU_MAX_SIZE)

size_t rc_rpt_get_request_buffer_size(const RdmBuffer* cmd);
size_t rc_rpt_get_status_buffer_size(const RptStatusMsg* status);
size_t rc_rpt_get_notification_buffer_size(const RdmBuffer* cmd_arr, size_t cmd_arr_size);

size_t rc_rpt_pack_request(uint8_t*          buf,
                           size_t            buflen,
                           const EtcPalUuid* local_cid,
                           const RptHeader*  header,
                           const RdmBuffer*  cmd);
size_t rc_rpt_pack_status(uint8_t*            buf,
                          size_t              buflen,
                          const EtcPalUuid*   local_cid,
                          const RptHeader*    header,
                          const RptStatusMsg* status);
size_t rc_rpt_pack_notification(uint8_t*          buf,
                                size_t            buflen,
                                const EtcPalUuid* local_cid,
                                const RptHeader*  header,
                                const RdmBuffer*  cmd_arr,
                                size_t            cmd_arr_size);

etcpal_error_t rc_rpt_send_request(RCConnection*     conn,
                                   const EtcPalUuid* local_cid,
                                   const RptHeader*  header,
                                   const RdmBuffer*  cmd);
etcpal_error_t rc_rpt_send_status(RCConnection*       conn,
                                  const EtcPalUuid*   local_cid,
                                  const RptHeader*    header,
                                  const RptStatusMsg* status);
etcpal_error_t rc_rpt_send_notification(RCConnection*     conn,
                                        const EtcPalUuid* local_cid,
                                        const RptHeader*  header,
                                        const RdmBuffer*  cmd_arr,
                                        size_t            cmd_arr_size);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_RPT_PROT_H_ */
