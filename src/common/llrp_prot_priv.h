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
#ifndef _LLRP_PROT_PRIV_H_
#define _LLRP_PROT_PRIV_H_

#include "lwpa/int.h"
#include "lwpa/bool.h"
#include "lwpa/uuid.h"
#include "lwpa/root_layer_pdu.h"
#include "rdm/uid.h"
#include "rdm/message.h"
#include "rdmnet/llrp.h"

#define LLRP_HEADER_SIZE \
  (3 /* Flags + Length */ + 4 /* Vector */ + 16 /* Destination CID */ + 4 /* Transaction Number */)
#define PROBE_REQUEST_PDU_MIN_SIZE \
  (3 /* Flags + Length */ + 1 /* Vector */ + 6 /* Lower UID */ + 6 /* Upper UID */ + 1 /* Filter */)
#define PROBE_REQUEST_PDU_MAX_SIZE (PROBE_REQUEST_PDU_MIN_SIZE + (6 * LLRP_KNOWN_UID_SIZE) /* Known UIDS */)
#define LLRP_MAX_MESSAGE_SIZE \
  (ACN_UDP_PREAMBLE_SIZE + RLP_HEADER_SIZE_EXT_LEN + LLRP_HEADER_SIZE + PROBE_REQUEST_PDU_MAX_SIZE)

typedef struct LlrpHeader
{
  LwpaUuid sender_cid;
  LwpaUuid dest_cid;

  uint32_t transaction_number;
} LlrpHeader;

typedef struct LlrpMessageInterest
{
  bool interested_in_probe_request;
  bool interested_in_probe_reply;
  LwpaUuid my_cid;
  RdmUid my_uid;
} LlrpMessageInterest;

typedef struct KnownUid KnownUid;
struct KnownUid
{
  RdmUid uid;
  KnownUid *next;
};

typedef struct ProbeRequestRecv
{
  /* True if this probe request contains my UID as registered in the LlrpMessageInterest struct, and
   * it is not suppressed by the Known UID list. */
  bool contains_my_uid;
  uint8_t filter;
} ProbeRequestRecv;

typedef struct ProbeRequestSend
{
  RdmUid lower_uid;
  RdmUid upper_uid;
  uint8_t filter;
  KnownUid *uid_list;
} ProbeRequestSend;

typedef struct LlrpMessage
{
  uint32_t vector;
  LlrpHeader header;
  union
  {
    ProbeRequestRecv probe_request;
    LlrpTarget probe_reply;
    RdmBuffer rdm_cmd;
  } data;
} LlrpMessage;

#define llrp_msg_get_rdm_cmd(llrpmsgptr) (&(llrpmsgptr)->data.rdm_cmd)
#define llrp_msg_get_probe_reply(llrpmsgptr) (&(llrpmsgptr)->data.probe_reply)
#define llrp_msg_get_probe_request(llrpmsgptr) (&(llrpmsgptr)->data.probe_request)

#ifdef __cplusplus
extern "C" {
#endif

extern LwpaUuid kLLRPBroadcastCID;

void llrp_prot_init();

bool parse_llrp_message(const uint8_t *buf, size_t buflen, const LlrpMessageInterest *interest, LlrpMessage *msg);

lwpa_error_t send_llrp_probe_request(llrp_socket_t handle, const LwpaSockaddr *dest_addr, const LlrpHeader *header,
                                     const ProbeRequestSend *probe_request);

lwpa_error_t send_llrp_probe_reply(llrp_socket_t handle, const LwpaSockaddr *dest_addr, const LlrpHeader *header,
                                   const LlrpTarget *probe_reply);

lwpa_error_t send_llrp_rdm(llrp_socket_t handle, const LwpaSockaddr *dest_addr, const LlrpHeader *header,
                           const RdmBuffer *rdm_msg);

#ifdef __cplusplus
}
#endif

#endif /* _LLRP_PROT_PRIV_ */
