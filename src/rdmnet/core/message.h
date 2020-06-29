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

#ifndef RDMNET_CORE_MESSAGE_H_
#define RDMNET_CORE_MESSAGE_H_

#include <assert.h>
#include "etcpal/error.h"
#include "rdm/uid.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/core/broker_message.h"
#include "rdmnet/core/client_entry.h"
#include "rdmnet/core/rpt_message.h"
#include "rdmnet/core/ept_message.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** An RDMnet received from one of RDMnet's TCP protocols. */
typedef struct RdmnetMessage
{
  /** The root layer vector. Compare to the vectors in @ref etcpal_acn_rlp. */
  uint32_t vector;
  /** The CID of the Component that sent this message. */
  EtcPalUuid sender_cid;
  /** The encapsulated message; use the helper macros to access it. */
  union
  {
    BrokerMessage broker;
    RptMessage    rpt;
    EptMessage    ept;
  } data;
} RdmnetMessage;

/**
 * @brief Determine whether an RdmnetMessage contains a Broker message.
 * @param msgptr Pointer to RdmnetMessage.
 * @return (bool) whether the message contains a Broker message.
 */
#define RDMNET_IS_BROKER_MSG(msgptr) ((msgptr)->vector == ACN_VECTOR_ROOT_BROKER)

/**
 * @brief Get the encapsulated Broker message from an RdmnetMessage.
 * @param msgptr Pointer to RdmnetMessage.
 * @return Pointer to encapsulated Broker message (BrokerMessage*).
 */
#define RDMNET_GET_BROKER_MSG(msgptr) (&(msgptr)->data.broker)

/**
 * @brief Determine whether an RdmnetMessage contains a RPT message.
 * @param msgptr Pointer to RdmnetMessage.
 * @return (bool) whether the message contains a RPT message.
 */
#define RDMNET_IS_RPT_MSG(msgptr) ((msgptr)->vector == ACN_VECTOR_ROOT_RPT)

/**
 * @brief Get the encapsulated RPT message from an RdmnetMessage.
 * @param msgptr Pointer to RdmnetMessage.
 * @return Pointer to encapsulated RPT message (RptMessage*).
 */
#define RDMNET_GET_RPT_MSG(msgptr) (&(msgptr)->data.rpt)

/**
 * @brief Determine whether an RdmnetMessage contains a EPT message.
 * @param msgptr Pointer to RdmnetMessage.
 * @return (bool) whether the message contains a EPT message.
 */
#define RDMNET_IS_EPT_MSG(msgptr) ((msgptr)->vector == ACN_VECTOR_ROOT_EPT)

/**
 * @brief Get the encapsulated EPT message from an RdmnetMessage.
 * @param msgptr Pointer to RdmnetMessage.
 * @return Pointer to encapsulated EPT message (EptMessage*).
 */
#define RDMNET_GET_EPT_MSG(msgptr) (&(msgptr)->data.ept)

#define RPT_CLIENT_ENTRIES_MAX_SIZE RDMNET_MAX_CLIENT_ENTRIES
#define EPT_CLIENT_ENTRIES_MAX_SIZE RDMNET_MAX_CLIENT_ENTRIES
#define DYNAMIC_UID_REQUESTS_MAX_SIZE RDMNET_MAX_DYNAMIC_UID_ENTRIES
#define DYNAMIC_UID_MAPPINGS_MAX_SIZE RDMNET_MAX_DYNAMIC_UID_ENTRIES
#define FETCH_UID_ASSIGNMENTS_MAX_SIZE RDMNET_MAX_DYNAMIC_UID_ENTRIES
#define RDM_BUFFERS_MAX_SIZE RDMNET_MAX_RECEIVED_ACK_OVERFLOW_RESPONSES

typedef union
{
  RdmnetRptClientEntry    rpt_client_entries[RPT_CLIENT_ENTRIES_MAX_SIZE];
  RdmnetEptClientEntry    ept_client_entries[EPT_CLIENT_ENTRIES_MAX_SIZE];
  BrokerDynamicUidRequest dynamic_uid_requests[DYNAMIC_UID_REQUESTS_MAX_SIZE];
  RdmnetDynamicUidMapping dynamic_uid_mappings[DYNAMIC_UID_MAPPINGS_MAX_SIZE];
  RdmUid                  fetch_uid_assignments[FETCH_UID_ASSIGNMENTS_MAX_SIZE];
  RdmBuffer               rdm_buffers[RDM_BUFFERS_MAX_SIZE];
} StaticMessageBuffer;

extern StaticMessageBuffer rdmnet_static_msg_buf;
extern char                rpt_status_string_buffer[RPT_STATUS_STRING_MAXLEN + 1];

#if RDMNET_DYNAMIC_MEM

#define ALLOC_RPT_CLIENT_ENTRY() malloc(sizeof(RdmnetRptClientEntry))
#define ALLOC_EPT_CLIENT_ENTRY() malloc(sizeof(RdmnetEptClientEntry))
#define ALLOC_DYNAMIC_UID_REQUEST_ENTRY() malloc(sizeof(BrokerDynamicUidRequest))
#define ALLOC_DYNAMIC_UID_MAPPING() malloc(sizeof(RdmnetDynamicUidMapping))
#define ALLOC_FETCH_UID_ASSIGNMENT() malloc(sizeof(RdmUid))
#define ALLOC_RDM_BUFFER() malloc(sizeof(RdmBuffer))

#define REALLOC_RPT_CLIENT_ENTRY(ptr, new_size) realloc((ptr), ((new_size) * sizeof(RdmnetRptClientEntry)))
#define REALLOC_EPT_CLIENT_ENTRY(ptr, new_size) realloc((ptr), ((new_size) * sizeof(RdmnetEptClientEntry)))
#define REALLOC_DYNAMIC_UID_REQUEST_ENTRY(ptr, new_size) realloc((ptr), ((new_size) * sizeof(BrokerDynamicUidRequest)))
#define REALLOC_DYNAMIC_UID_MAPPING(ptr, new_size) realloc((ptr), ((new_size) * sizeof(RdmnetDynamicUidMapping)))
#define REALLOC_FETCH_UID_ASSIGNMENT(ptr, new_size) realloc((ptr), ((new_size) * sizeof(RdmUid)))
#define REALLOC_RDM_BUFFER(ptr, new_size) realloc((ptr), ((new_size) * sizeof(RdmBuffer)))

#define ALLOC_EPT_SUBPROT_LIST() malloc(sizeof(RdmnetEptSubProtocol))
#define REALLOC_EPT_SUBPROT_LIST(ptr, new_size) realloc((ptr), ((new_size) * sizeof(RdmnetEptSubProtocol)))
#define FREE_EPT_SUBPROT_LIST(ptr) free(ptr)

#define ALLOC_RPT_STATUS_STR(size) malloc(size)

#define FREE_MESSAGE_BUFFER(ptr) free(ptr)

#else

// Static buffer space for RDMnet messages is held in a union. Only one field can be used at a
// time.
#define ALLOC_FROM_ARRAY(array, array_size) rdmnet_static_msg_buf.array
#define REALLOC_FROM_ARRAY(ptr, new_size, array, array_size) \
  (assert((ptr) == (rdmnet_static_msg_buf.array)), ((new_size) <= (array_size) ? rdmnet_static_msg_buf.array : NULL))

#define ALLOC_RPT_CLIENT_ENTRY() ALLOC_FROM_ARRAY(rpt_client_entries, RPT_CLIENT_ENTRIES_MAX_SIZE)
#define ALLOC_EPT_CLIENT_ENTRY() ALLOC_FROM_ARRAY(ept_client_entries, EPT_CLIENT_ENTRIES_MAX_SIZE)
#define ALLOC_DYNAMIC_UID_REQUEST_ENTRY() ALLOC_FROM_ARRAY(dynamic_uid_requests, DYNAMIC_UID_REQUESTS_MAX_SIZE)
#define ALLOC_DYNAMIC_UID_MAPPING() ALLOC_FROM_ARRAY(dynamic_uid_mappings, DYNAMIC_UID_MAPPINGS_MAX_SIZE)
#define ALLOC_FETCH_UID_ASSIGNMENT() ALLOC_FROM_ARRAY(fetch_uid_assignments, FETCH_UID_ASSIGNMENTS_MAX_SIZE)
#define ALLOC_RDM_BUFFER() ALLOC_FROM_ARRAY(rdm_buffers, RDM_BUFFERS_MAX_SIZE)

#define REALLOC_RPT_CLIENT_ENTRY(ptr, new_size) \
  REALLOC_FROM_ARRAY(ptr, new_size, rpt_client_entries, RPT_CLIENT_ENTRIES_MAX_SIZE)
#define REALLOC_EPT_CLIENT_ENTRY(ptr, new_size) \
  REALLOC_FROM_ARRAY(ptr, new_size, ept_client_entries, EPT_CLIENT_ENTRIES_MAX_SIZE)
#define REALLOC_DYNAMIC_UID_REQUEST_ENTRY(ptr, new_size) \
  REALLOC_FROM_ARRAY(ptr, new_size, dynamic_uid_requests, DYNAMIC_UID_REQUESTS_MAX_SIZE)
#define REALLOC_DYNAMIC_UID_MAPPING(ptr, new_size) \
  REALLOC_FROM_ARRAY(ptr, new_size, dynamic_uid_mappings, DYNAMIC_UID_MAPPINGS_MAX_SIZE)
#define REALLOC_FETCH_UID_ASSIGNMENT(ptr, new_size) \
  REALLOC_FROM_ARRAY(ptr, new_size, fetch_uid_assignments, FETCH_UID_ASSIGNMENTS_MAX_SIZE)
#define REALLOC_RDM_BUFFER(ptr, new_size) REALLOC_FROM_ARRAY(ptr, new_size, rdm_buffers, RDM_BUFFERS_MAX_SIZE)

#define ALLOC_RPT_STATUS_STR(size) rpt_status_string_buffer

// TODO
#define ALLOC_EPT_SUBPROT_LIST() NULL
#define REALLOC_EPT_SUBPROT_LIST() NULL
#define FREE_EPT_SUBPROT_LIST(ptr)

#define FREE_MESSAGE_BUFFER(ptr)

#endif

void rc_free_message_resources(RdmnetMessage* msg);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MESSAGE_PRIV_H_ */
