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

/*! \file rdmnet/client.h
 *  \brief Defining information about an RDMnet Client, including all information that is sent on
 *         initial connection to a Broker.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CLIENT_H_
#define _RDMNET_CLIENT_H_

#include "lwpa/uuid.h"
#include "lwpa/inet.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"
#include "rdmnet/core.h"
#include "rdmnet/core/util.h"

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
  EptSubProtocol *next;
};

typedef struct RdmnetClientInternal *rdmnet_client_t;

typedef void (*RdmnetClientConnectedCb)(rdmnet_client_t handle, const char *scope, void *context);
typedef void (*RdmnetClientDisconnectedCb)(rdmnet_client_t handle, const char *scope, void *context);

typedef struct RdmnetClientCallbacks
{
  RdmnetClientConnectedCb connected;
  RdmnetClientDisconnectedCb disconnected;
} RdmnetClientCallbacks;

typedef struct RdmnetScopeConfig
{
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  bool has_static_broker_addr;
  LwpaSockaddr static_broker_addr;
} RdmnetScopeConfig;

typedef struct RdmnetRptClientConfig
{
  rpt_client_type_t type;
  bool has_static_uid;
  RdmUid static_uid;
  LwpaUuid cid;
  const RdmnetScopeConfig *scope_list;
  size_t num_scopes;
  RdmnetClientCallbacks callbacks;
  void *callback_context;
} RdmnetRptClientConfig;

typedef struct RdmnetEptClientConfig
{
  EptSubProtocol *protocol_list;
  size_t num_protocols;
} RdmnetEptClientConfig;

/*! \brief Initialize an RdmnetScopeConfig struct with a scope string.
 *
 *  Scopes are resolved using RDMnet discovery (DNS-SD) by default; to override this behavior with a
 *  static broker address and port, use rdmnet_set_static_scope().
 *
 *  \param configptr Pointer to RdmnetScopeConfig.
 *  \param scope_str UTF-8 scope string to copy to the RdmnetScopeConfig (const char *).
 */
#define rdmnet_client_set_scope(configptr, scope_str)                                    \
  do                                                                                     \
  {                                                                                      \
    rdmnet_safe_strncpy((configptr)->scope, scope_str, E133_SCOPE_STRING_PADDED_LENGTH); \
    (configptr)->has_static_broker_addr = false;                                         \
  } while (0)

/*! \brief Initialize an RdmnetScopeConfig struct with a scope string and static broker address.
 *
 *  DNS-SD discovery will be bypassed and broker connection will be attempted using the address and
 *  port given.
 *
 *  \param configptr Pointer to RdmnetScopeConfig.
 *  \param scope_str UTF-8 scope string to copy to the RdmnetScopeConfig (const char *).
 *  \param broker_addr Address and port for a static broker (LwpaSockaddr).
 */
#define rdmnet_client_set_static_scope(configptr, scope_str, broker_addr)                \
  do                                                                                     \
  {                                                                                      \
    rdmnet_safe_strncpy((configptr)->scope, scope_str, E133_SCOPE_STRING_PADDED_LENGTH); \
    (configptr)->has_static_broker_addr = true;                                          \
    (configptr)->static_broker_addr = broker_addr;                                       \
  } while (0)

typedef struct ClientEntryDataRpt
{
  RdmUid client_uid;
  rpt_client_type_t client_type;
  LwpaUuid binding_cid;
} ClientEntryDataRpt;

typedef struct ClientEntryDataEpt
{
  bool partial;
  EptSubProtocol *protocol_list;
} ClientEntryDataEpt;

typedef struct ClientEntryData ClientEntryData;
struct ClientEntryData
{
  client_protocol_t client_protocol;
  LwpaUuid client_cid;
  union
  {
    ClientEntryDataRpt rpt_data;
    ClientEntryDataEpt ept_data;
  } data;
  ClientEntryData *next;
};

#define is_rpt_client_entry(clientryptr) ((clientryptr)->client_protocol == E133_CLIENT_PROTOCOL_RPT)
#define get_rpt_client_entry_data(clientryptr) (&(clientryptr)->data.rpt_data)
#define is_ept_client_entry(clientryptr) ((clientryptr)->client_protocol == E133_CLIENT_PROTOCOL_EPT)
#define get_ept_client_entry_data(clientryptr) (&(clientryptr)->data.ept_data)

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_rpt_client_create(const RdmnetRptClientConfig *config, rdmnet_client_t *handle);
void rdmnet_rpt_client_destroy(rdmnet_client_t handle);

lwpa_error_t rdmnet_ept_client_create(const RdmnetEptClientConfig *config, rdmnet_client_t *handle);
void rdmnet_ept_client_destroy(rdmnet_client_t handle);

bool create_rpt_client_entry(const LwpaUuid *cid, const RdmUid *uid, rpt_client_type_t client_type,
                             const LwpaUuid *binding_cid, ClientEntryData *entry);
bool create_ept_client_entry(const LwpaUuid *cid, const EptSubProtocol *protocol_arr, size_t protocol_arr_size,
                             ClientEntryData *entry);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_CLIENT_H_ */
