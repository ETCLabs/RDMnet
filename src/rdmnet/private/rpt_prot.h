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

/*! \file rdmnet/private/rpt_prot.h
 *  \brief Functions and definitions for RPT PDU messages that are only used internally.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_PRIVATE_RPT_PROT_H_
#define _RDMNET_PRIVATE_RPT_PROT_H_

#include "rdm/message.h"

#define REQUEST_NOTIF_PDU_HEADER_SIZE (3 /* Flags & Length */ + 4 /* Vector*/)
#define RDM_CMD_PDU_MIN_SIZE (3 /* Flags & Length */ + RDM_MIN_BYTES)
#define RDM_CMD_PDU_MAX_SIZE (3 /* Flags & Length */ + RDM_MAX_BYTES)
#define REQUEST_PDU_MAX_SIZE (REQUEST_PDU_HEADER_SIZE + RDM_CMD_PDU_MAX_SIZE)

#endif /* _RDMNET_CORE_RPT_PROT_PRIV_H_ */
