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
 * \file rdmnet/core/client_entry.h
 * \brief Functions to create Client Entry structures for RPT and EPT clients.
 */

#ifndef RDMNET_CORE_CLIENT_ENTRY_H_
#define RDMNET_CORE_CLIENT_ENTRY_H_

#include "etcpal/bool.h"
#include "etcpal/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! An RDMnet client protocol. */
typedef enum
{
  /*! An RPT client. RPT clients implement the RDM functionality of RDMnet, and are further divided
   *  into controllers and devices (see #rpt_client_type_t). */
  kClientProtocolRPT = E133_CLIENT_PROTOCOL_RPT,
  /*! An EPT client. EPT clients use RDMnet's extensibility to transport arbitrary
   *  manufacturer-specific data across an RDMnet broker. */
  kClientProtocolEPT = E133_CLIENT_PROTOCOL_EPT,
  /*! A placeholder for when a client protocol has not been determined. */
  kClientProtocolUnknown = 0xffffffff
} client_protocol_t;

/*! An RPT client type. */
typedef enum
{
  /*! An RPT device receives RDM commands and sends responses. */
  kRPTClientTypeDevice = E133_RPT_CLIENT_TYPE_DEVICE,
  /*! An RPT controller originates RDM commands and receives responses. */
  kRPTClientTypeController = E133_RPT_CLIENT_TYPE_CONTROLLER,
  /*! A placeholder for when a type has not been determined. */
  kRPTClientTypeUnknown = 0xff
} rpt_client_type_t;

/*! The maximum length of an EPT sub-protocol string, including the null terminator. */
#define EPT_PROTOCOL_STRING_PADDED_LENGTH 32

/*! A description of an EPT sub-protocol. EPT clients can implement multiple protocols, each of
 *  which is identified by a two-part identifier including an ESTA manufacturer ID and a protocol
 *  ID. */
typedef struct EptSubProtocol
{
  /*! The ESTA manufacturer ID under which this protocol is namespaced. */
  uint16_t manufacturer_id;
  /*! The identifier for this protocol. */
  uint16_t protocol_id;
  /*! A descriptive string for the protocol. */
  char protocol_string[EPT_PROTOCOL_STRING_PADDED_LENGTH];
} EptSubProtocol;

/*! A descriptive structure for an EPT client. */
typedef struct EptClientEntry
{
  EtcPalUuid cid;            /*!< The client's Component Identifier (CID). */
  EptSubProtocol* protocols; /*!< A list of EPT protocols that this client implements. */
  size_t num_protocols;      /*!< The size of the protocols array. */
} EptClientEntry;

/*! A descriptive structure for an RPT client. */
typedef struct RptClientEntry
{
  EtcPalUuid cid;         /*!< The client's Component Identifier (CID). */
  RdmUid uid;             /*!< The client's RDM UID. */
  rpt_client_type_t type; /*!< Whether the client is a controller or device. */
  EtcPalUuid binding_cid; /*!< An optional identifier for another component that the client is associated with. */
} RptClientEntry;

typedef union
{
  RptClientEntry rpt;
  EptClientEntry ept;
} ClientEntryUnion;

/*! A generic client entry which could represent either an RPT or EPT client. */
typedef struct ClientEntry
{
  /*! The client's protocol, which identifies which member of data is valid. */
  client_protocol_t client_protocol;
  /*! The client entry data as a union. */
  ClientEntryUnion data;
} ClientEntry;

/*!
 * \brief Determine whether a ClientEntry contains an RptClientEntry.
 * \param clientryptr Pointer to ClientEntry.
 * \return (true or false) whether the ClientEntry contains an RptClientEntry.
 */
#define IS_RPT_CLIENT_ENTRY(clientryptr) ((clientryptr)->client_protocol == E133_CLIENT_PROTOCOL_RPT)

/*!
 * \brief Get the encapsulated RptClientEntry from a ClientEntry.
 * \param clientryptr Pointer to ClientEntry.
 * \return Pointer to encapsulated RptClientEntry structure (RptClientEntry*).
 */
#define GET_RPT_CLIENT_ENTRY(clientryptr) (&(clientryptr)->data.rpt)

/*!
 * \brief Determine whether a ClientEntry contains an EptClientEntry.
 * \param clientryptr Pointer to ClientEntry.
 * \return (true or false) whether the ClientEntry contains an EptClientEntry.
 */
#define IS_EPT_CLIENT_ENTRY(clientryptr) ((clientryptr)->client_protocol == E133_CLIENT_PROTOCOL_EPT)

/*!
 * \brief Get the encapsulated EptClientEntry from a ClientEntry.
 * \param clientryptr Pointer to ClientEntry.
 * \return Pointer to encapsulated EptClientEntry structure (EptClientEntry*).
 */
#define GET_EPT_CLIENT_ENTRY(clientryptr) (&(clientryptr)->data.ept)

bool create_rpt_client_entry(const EtcPalUuid* cid, const RdmUid* uid, rpt_client_type_t client_type,
                             const EtcPalUuid* binding_cid, RptClientEntry* entry);
bool create_ept_client_entry(const EtcPalUuid* cid, const EptSubProtocol* protocol_arr, size_t protocol_arr_size,
                             EptClientEntry* entry);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_CLIENT_ENTRY_H_ */
