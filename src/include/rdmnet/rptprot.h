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

/*! \file rdmnet/rptprot.h
 *  \brief Functions to pack, send and parse RPT PDUs and their encapsulated
 *         messages.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_RPTPROT_H_
#define _RDMNET_RPTPROT_H_

#include <stddef.h>
#include "lwpa_int.h"
#include "lwpa_cid.h"
#include "lwpa_error.h"
#include "lwpa_rootlayerpdu.h"
#include "rdmnet/rdmtypes.h"
#include "rdmnet/opts.h"

#define RPT_PDU_HEADER_SIZE                                                   \
  (3 /* Flags + Length */ + 4 /* Vector */ + 6         /* Source UID */       \
   + 2 /* Source Endpoint ID */ + 6 /* Dest UID */ + 2 /* Dest Endpoint ID */ \
   + 4 /* Sequence Number */ + 1 /* Reserved */)
#define RPT_PDU_FULL_HEADER_SIZE (RPT_PDU_HEADER_SIZE + RLP_HEADER_SIZE_EXT_LEN + ACN_TCP_PREAMBLE_SIZE)

#define RPT_STATUS_HEADER_SIZE (3 /* Flags + Length */ + 2 /* Vector */)
#define RPT_STATUS_STRING_MAXLEN 1024
#define RPT_STATUS_FULL_MSG_MAX_SIZE (RPT_PDU_FULL_HEADER_SIZE + RPT_STATUS_HEADER_SIZE + RPT_STATUS_STRING_MAXLEN)

#define NULL_ENDPOINT 0u

typedef struct RptHeader
{
  LwpaUid source_uid;
  uint16_t source_endpoint_id;
  LwpaUid dest_uid;
  uint16_t dest_endpoint_id;
  uint32_t seqnum;
} RptHeader;

#define RPT_STATUSCODE_UNKNOWN_RPT_UID 1u
#define RPT_STATUSCODE_RDM_TIMEOUT 2u
#define RPT_STATUSCODE_RDM_INVALID_RESPONSE 3u
#define RPT_STATUSCODE_UNKNOWN_RDM_UID 4u
#define RPT_STATUSCODE_UNKNOWN_ENDPOINT 5u
#define RPT_STATUSCODE_BROADCAST_COMPLETE 6u
#define RPT_STATUSCODE_UNKNOWN_VECTOR 7u
#define RPT_STATUSCODE_INVALID_MESSAGE 8u
#define RPT_STATUSCODE_INVALID_COMMAND_CLASS 9u

typedef struct RptStatusMsg
{
  uint16_t status_code;
#if RDMNET_DYNAMIC_MEM
  char *status_string;
#else
  char status_string[RPT_STATUS_STRING_MAXLEN + 1];
#endif
} RptStatusMsg;

typedef struct RdmCmdListEntry RdmCmdListEntry;
struct RdmCmdListEntry
{
  RdmBuffer msg;
  RdmCmdListEntry *next;
};

typedef struct RdmCmdList
{
  bool partial;
  RdmCmdListEntry *list;
} RdmCmdList;

typedef struct RptMessage
{
  uint32_t vector;
  RptHeader header;
  union
  {
    RptStatusMsg status;
    RdmCmdList rdm;
  } data;
} RptMessage;

#define get_rdm_cmd_list(rptmsgptr) (&(rptmsgptr)->data.rdm)
#define get_status_msg(rptmsgptr) (&(rptmsgptr)->data.status)

#ifdef __cplusplus
extern "C" {
#endif

size_t bufsize_rpt_request(const RdmBuffer *cmd);
size_t bufsize_rpt_status(const RptStatusMsg *status);
size_t bufsize_rpt_notification(const RdmCmdListEntry *cmd_list);

size_t pack_rpt_request(uint8_t *buf, size_t buflen, const LwpaCid *local_cid, const RptHeader *header,
                        const RdmBuffer *cmd);
size_t pack_rpt_status(uint8_t *buf, size_t buflen, const LwpaCid *local_cid, const RptHeader *header,
                       const RptStatusMsg *status);
size_t pack_rpt_notification(uint8_t *buf, size_t buflen, const LwpaCid *local_cid, const RptHeader *header,
                             const RdmCmdListEntry *cmd_list);

lwpa_error_t send_rpt_request(int handle, const LwpaCid *local_cid, const RptHeader *header, const RdmBuffer *cmd);
lwpa_error_t send_rpt_status(int handle, const LwpaCid *local_cid, const RptHeader *header, const RptStatusMsg *status);
lwpa_error_t send_rpt_notification(int handle, const LwpaCid *local_cid, const RptHeader *header,
                                   const RdmCmdListEntry *cmd_list);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_RPTPROT_H_ */
