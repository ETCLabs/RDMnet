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

/**
 * @file rdmnet/core/client_entry.h
 * @brief Functions to create Client Entry structures for RPT and EPT clients.
 */

#ifndef RDMNET_CORE_CLIENT_ENTRY_H_
#define RDMNET_CORE_CLIENT_ENTRY_H_

#include <stdbool.h>
#include "etcpal/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"
#include "rdmnet/message.h"

#ifdef __cplusplus
extern "C" {
#endif

/** An RDMnet client protocol. */
typedef enum
{
  /** An RPT client. RPT clients implement the RDM functionality of RDMnet, and are further divided
   *  into controllers and devices (see #rpt_client_type_t). */
  kClientProtocolRPT = E133_CLIENT_PROTOCOL_RPT,
  /** An EPT client. EPT clients use RDMnet's extensibility to transport arbitrary
   *  manufacturer-specific data across an RDMnet broker. */
  kClientProtocolEPT = E133_CLIENT_PROTOCOL_EPT,
  /** A placeholder for when a client protocol has not been determined. */
  kClientProtocolUnknown = 0xffffffff
} client_protocol_t;

typedef union
{
  RdmnetRptClientEntry rpt;
  RdmnetEptClientEntry ept;
} ClientEntryUnion;

/** A generic client entry which could represent either an RPT or EPT client. */
typedef struct ClientEntry
{
  /** The client's protocol, which identifies which member of data is valid. */
  client_protocol_t client_protocol;
  /** The client entry data as a union. */
  ClientEntryUnion data;
} ClientEntry;

/**
 * @brief Determine whether a ClientEntry contains an RdmnetRptClientEntry.
 * @param clientryptr Pointer to ClientEntry.
 * @return (bool) whether the ClientEntry contains an RdmnetRptClientEntry.
 */
#define IS_RPT_CLIENT_ENTRY(clientryptr) ((clientryptr)->client_protocol == E133_CLIENT_PROTOCOL_RPT)

/**
 * @brief Get the encapsulated RdmnetRptClientEntry from a ClientEntry.
 * @param clientryptr Pointer to ClientEntry.
 * @return Pointer to encapsulated RdmnetRptClientEntry structure (RdmnetRptClientEntry*).
 */
#define GET_RPT_CLIENT_ENTRY(clientryptr) (&(clientryptr)->data.rpt)

/**
 * @brief Determine whether a ClientEntry contains an RdmnetEptClientEntry.
 * @param clientryptr Pointer to ClientEntry.
 * @return (bool) whether the ClientEntry contains an RdmnetEptClientEntry.
 */
#define IS_EPT_CLIENT_ENTRY(clientryptr) ((clientryptr)->client_protocol == E133_CLIENT_PROTOCOL_EPT)

/**
 * @brief Get the encapsulated RdmnetEptClientEntry from a ClientEntry.
 * @param clientryptr Pointer to ClientEntry.
 * @return Pointer to encapsulated RdmnetEptClientEntry structure (RdmnetEptClientEntry*).
 */
#define GET_EPT_CLIENT_ENTRY(clientryptr) (&(clientryptr)->data.ept)

bool rc_create_rpt_client_entry(const EtcPalUuid*     cid,
                                const RdmUid*         uid,
                                rpt_client_type_t     client_type,
                                const EtcPalUuid*     binding_cid,
                                RdmnetRptClientEntry* entry);
bool rc_create_ept_client_entry(const EtcPalUuid*           cid,
                                const RdmnetEptSubProtocol* protocol_arr,
                                size_t                      protocol_arr_size,
                                RdmnetEptClientEntry*       entry);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_CLIENT_ENTRY_H_ */
