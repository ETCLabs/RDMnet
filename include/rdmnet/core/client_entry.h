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
 * \author Sam Kearney
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

typedef enum
{
  kClientProtocolRPT = E133_CLIENT_PROTOCOL_RPT,
  kClientProtocolEPT = E133_CLIENT_PROTOCOL_EPT,
  kClientProtocolUnknown = 0xffffffff
} client_protocol_t;

typedef enum
{
  kRPTClientTypeDevice = E133_RPT_CLIENT_TYPE_DEVICE,
  kRPTClientTypeController = E133_RPT_CLIENT_TYPE_CONTROLLER,
  kRPTClientTypeUnknown = 0xff
} rpt_client_type_t;

#define EPT_PROTOCOL_STRING_PADDED_LENGTH 32

typedef struct EptSubProtocol EptSubProtocol;
struct EptSubProtocol
{
  uint32_t protocol_vector;
  char protocol_string[EPT_PROTOCOL_STRING_PADDED_LENGTH];
  EptSubProtocol* next;
};

typedef struct EptClientEntryData EptClientEntryData;
typedef struct EptClientEntryData
{
  EtcPalUuid cid;
  bool more_coming;
  EptSubProtocol* protocol_list;
  EptClientEntryData* next;
} EptClientEntryData;

typedef struct RptClientEntryData RptClientEntryData;
struct RptClientEntryData
{
  EtcPalUuid cid;
  RdmUid uid;
  rpt_client_type_t type;
  EtcPalUuid binding_cid;
  RptClientEntryData* next;
};

typedef struct ClientEntry
{
  client_protocol_t client_protocol;
  union
  {
    RptClientEntryData rpt;
    EptClientEntryData ept;
  } data;
} ClientEntry;

#define IS_RPT_CLIENT_ENTRY(clientryptr) ((clientryptr)->client_protocol == E133_CLIENT_PROTOCOL_RPT)
#define GET_RPT_CLIENT_ENTRY_DATA(clientryptr) (&(clientryptr)->data.rpt)
#define IS_EPT_CLIENT_ENTRY(clientryptr) ((clientryptr)->client_protocol == E133_CLIENT_PROTOCOL_EPT)
#define GET_EPT_CLIENT_ENTRY_DATA(clientryptr) (&(clientryptr)->data.ept)

bool create_rpt_client_entry(const EtcPalUuid* cid, const RdmUid* uid, rpt_client_type_t client_type,
                             const EtcPalUuid* binding_cid, RptClientEntryData* entry);
bool create_ept_client_entry(const EtcPalUuid* cid, const EptSubProtocol* protocol_arr, size_t protocol_arr_size,
                             EptClientEntryData* entry);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_CLIENT_ENTRY_H_ */
