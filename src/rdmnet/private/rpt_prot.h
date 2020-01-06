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
 * \file rdmnet/private/rpt_prot.h
 * \brief Functions and definitions for RPT PDU messages that are only used internally.
 */

#ifndef RDMNET_PRIVATE_RPT_PROT_H_
#define RDMNET_PRIVATE_RPT_PROT_H_

#include "rdm/message.h"

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

#endif /* RDMNET_PRIVATE_RPT_PROT_H_ */
