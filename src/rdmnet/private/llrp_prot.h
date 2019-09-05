/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/
#ifndef _RDMNET_PRIVATE_LLRP_PROT_H_
#define _RDMNET_PRIVATE_LLRP_PROT_H_

#include "etcpal/int.h"
#include "etcpal/bool.h"
#include "etcpal/uuid.h"
#include "etcpal/root_layer_pdu.h"
#include "etcpal/socket.h"
#include "rdm/uid.h"
#include "rdm/message.h"
#include "rdmnet/core/llrp.h"

#define LLRP_HEADER_SIZE \
  (3 /* Flags + Length */ + 4 /* Vector */ + 16 /* Destination CID */ + 4 /* Transaction Number */)
#define PROBE_REQUEST_PDU_MIN_SIZE \
  (3 /* Flags + Length */ + 1 /* Vector */ + 6 /* Lower UID */ + 6 /* Upper UID */ + 2 /* Filter */)
#define PROBE_REQUEST_PDU_MAX_SIZE (PROBE_REQUEST_PDU_MIN_SIZE + (6 * LLRP_KNOWN_UID_SIZE) /* Known UIDS */)
#define LLRP_RDM_CMD_PDU_MAX_SIZE (3 /* Flags + Length */ + RDM_MAX_BYTES)
#define LLRP_TARGET_MAX_MESSAGE_SIZE \
  (ACN_UDP_PREAMBLE_SIZE + ACN_RLP_HEADER_SIZE_EXT_LEN + LLRP_HEADER_SIZE + LLRP_RDM_CMD_PDU_MAX_SIZE)
#define LLRP_MANAGER_MAX_MESSAGE_SIZE \
  (ACN_UDP_PREAMBLE_SIZE + ACN_RLP_HEADER_SIZE_EXT_LEN + LLRP_HEADER_SIZE + PROBE_REQUEST_PDU_MAX_SIZE)
#define LLRP_MAX_MESSAGE_SIZE LLRP_MANAGER_MAX_MESSAGE_SIZE

typedef struct LlrpHeader
{
  EtcPalUuid sender_cid;
  EtcPalUuid dest_cid;

  uint32_t transaction_number;
} LlrpHeader;

typedef struct LlrpMessageInterest
{
  bool interested_in_probe_request;
  bool interested_in_probe_reply;
  EtcPalUuid my_cid;
  RdmUid my_uid;
} LlrpMessageInterest;

typedef struct KnownUid KnownUid;
struct KnownUid
{
  RdmUid uid;
  KnownUid* next;
};

typedef struct RemoteProbeRequest
{
  /* True if this probe request contains my UID as registered in the LlrpMessageInterest struct, and
   * it is not suppressed by the Known UID list. */
  bool contains_my_uid;
  uint16_t filter;
} RemoteProbeRequest;

typedef struct LocalProbeRequest
{
  RdmUid lower_uid;
  RdmUid upper_uid;
  uint16_t filter;
  KnownUid* uid_list;
} LocalProbeRequest;

typedef struct LlrpMessage
{
  uint32_t vector;
  LlrpHeader header;
  union
  {
    RemoteProbeRequest probe_request;
    DiscoveredLlrpTarget probe_reply;
    RdmBuffer rdm;
  } data;
} LlrpMessage;

#define LLRP_MSG_GET_RDM(llrpmsgptr) (&(llrpmsgptr)->data.rdm)
#define LLRP_MSG_GET_PROBE_REPLY(llrpmsgptr) (&(llrpmsgptr)->data.probe_reply)
#define LLRP_MSG_GET_PROBE_REQUEST(llrpmsgptr) (&(llrpmsgptr)->data.probe_request)

#ifdef __cplusplus
extern "C" {
#endif

extern EtcPalUuid kLlrpBroadcastCid;

void llrp_prot_init();

bool get_llrp_destination_cid(const uint8_t* buf, size_t buflen, EtcPalUuid* dest_cid);
bool parse_llrp_message(const uint8_t* buf, size_t buflen, const LlrpMessageInterest* interest, LlrpMessage* msg);

etcpal_error_t send_llrp_probe_request(etcpal_socket_t sock, uint8_t* buf, bool ipv6, const LlrpHeader* header,
                                     const LocalProbeRequest* probe_request);
etcpal_error_t send_llrp_probe_reply(etcpal_socket_t sock, uint8_t* buf, bool ipv6, const LlrpHeader* header,
                                   const DiscoveredLlrpTarget* target_info);
etcpal_error_t send_llrp_rdm_command(etcpal_socket_t sock, uint8_t* buf, bool ipv6, const LlrpHeader* header,
                                   const RdmBuffer* cmd);
etcpal_error_t send_llrp_rdm_response(etcpal_socket_t sock, uint8_t* buf, bool ipv6, const LlrpHeader* header,
                                    const RdmBuffer* resp);

#ifdef __cplusplus
}
#endif

#endif /* _LLRP_PROT_PRIV_ */
