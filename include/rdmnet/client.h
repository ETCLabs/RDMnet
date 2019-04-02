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
#include "rdm/message.h"
#include "rdmnet/defs.h"
#include "rdmnet/core.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/util.h"

typedef int rdmnet_client_t;
typedef rdmnet_conn_t rdmnet_client_scope_t;

#define RDMNET_CLIENT_INVALID -1
#define RDMNET_CLIENT_SCOPE_INVALID -1

typedef struct RdmnetClientConnectedInfo
{
  LwpaSockaddr broker_addr;
} RdmnetClientConnectedInfo;

typedef struct RdmnetClientConnectFailedInfo
{
  rdmnet_connect_fail_cause_t cause;
  lwpa_error_t socket_err;
  rdmnet_connect_status_t rdmnet_reason;
  bool will_retry;
} RdmnetClientConnectFailedInfo;

typedef struct RdmnetClientDisconnectedInfo
{
  rdmnet_disconnect_cause_t cause;
  lwpa_error_t socket_err;
  rdmnet_disconnect_reason_t rdmnet_reason;
  bool will_retry;
} RdmnetClientDisconnectedInfo;

typedef struct RptClientCallbacks
{
  /*! \brief An RDMnet client has connected successfully to a broker on a scope.
   *
   *  Messages may now be sent using the relevant API functions, and messages may be received via
   *  the msg_received callback.
   *
   *  \param[in] handle Handle to client which has connected.
   *  \param[in] scope_handle Handle to scope on which the client has connected.
   *  \param[in] info More information about the successful connection.
   *  \param[in] context Context pointer that was given at the creation of the client.
   */
  void (*connected)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo *info,
                    void *context);

  /*! \brief An RDMnet client experienced a failure while attempting to connect to a broker on a
   *         scope.
   *
   *  Connection failures can be fatal or non-fatal; the will_retry member of the info struct
   *  indicates whether the connection will be retried automatically. If will_retry is false, it
   *  usually indicates a misconfiguration that needs to be resolved by an application user.
   *
   *  \param[in] handle Handle to client on which connection has failed.
   *  \param[in] scope_handle Handle to scope on which connection has failed.
   *  \param[in] info More information about the failed connection.
   *  \param[in] context Context pointer that was given at the creation of the client.
   */
  void (*connect_failed)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetClientConnectFailedInfo *info, void *context);
  void (*not_connected)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                        const RdmnetClientDisconnectedInfo *info, void *context);
  void (*broker_msg_received)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const BrokerMessage *msg,
                              void *context);
  void (*msg_received)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RptClientMessage *msg,
                       void *context);
} RptClientCallbacks;

typedef struct EptClientCallbacks
{
  void (*connected)(rdmnet_client_t handle, rdmnet_client_scope_t scope, void *context);
  void (*disconnected)(rdmnet_client_t handle, rdmnet_client_scope_t scope, void *context);
  void (*broker_msg_received)(rdmnet_client_t handle, rdmnet_client_scope_t scope, const BrokerMessage *msg,
                              void *context);
  void (*msg_received)(rdmnet_client_t handle, rdmnet_client_scope_t scope, const EptClientMessage *msg, void *context);
} EptClientCallbacks;

typedef struct RdmnetScopeConfig
{
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  bool has_static_broker_addr;
  LwpaSockaddr static_broker_addr;
} RdmnetScopeConfig;

/*! \brief Initialization value for a dynamic UID.
 *
 *  Usage example:
 *  ```
 *  RdmnetRptClientConfig config;
 *  config.uid = RPT_CLIENT_DYNAMIC_UID(0x6574);
 *  ```
 *
 *  \param manu_id ESTA manufacturer ID. All RDMnet components must have one.
 */
#define RPT_CLIENT_DYNAMIC_UID(manu_id) \
  {                                     \
    manu_id, 0                          \
  }

/*! A set of information that defines the startup parameters of an RPT RDMnet Client. */
typedef struct RdmnetRptClientConfig
{
  /*! The client type, either controller or device. */
  rpt_client_type_t type;
  /*! The client's UID. If the client has a static UID, fill in the values normally. If a dynamic
   *  UID is desired, assign using RPT_CLIENT_DYNAMIC_UID(manu_id), passing your ESTA manufacturer
   *  ID. All RDMnet components are required to have a valid ESTA manufacturer ID. */
  RdmUid uid;
  /*! The client's CID. */
  LwpaUuid cid;
  /*! A set of callbacks for the client to received RDMnet notifications. */
  RptClientCallbacks callbacks;
  /*! Pointer to opaque data passed back with each callback. */
  void *callback_context;
} RdmnetRptClientConfig;

typedef struct RdmnetEptClientConfig
{
  EptSubProtocol *protocol_list;
  size_t num_protocols;
  EptClientCallbacks callbacks;
  void *callback_context;
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

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_rpt_client_create(const RdmnetRptClientConfig *config, rdmnet_client_t *handle);
void rdmnet_rpt_client_destroy(rdmnet_client_t handle);

lwpa_error_t rdmnet_client_add_scope(rdmnet_client_t handle, const RdmnetScopeConfig *scope_config,
                                     rdmnet_client_scope_t *scope_handle);
lwpa_error_t rdmnet_client_remove_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle);
lwpa_error_t rdmnet_client_change_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                        const RdmnetScopeConfig *new_config);

lwpa_error_t rdmnet_rpt_client_send_rdm_command(rdmnet_client_t handle, rdmnet_client_scope_t scope,
                                                const LocalRdmCommand *cmd);
lwpa_error_t rdmnet_rpt_client_send_rdm_response(rdmnet_client_t handle, rdmnet_client_scope_t scope,
                                                 const LocalRdmResponse *resp);
lwpa_error_t rdmnet_rpt_client_send_status(rdmnet_client_t handle, const char *scope, const RptStatusMsg *status);

lwpa_error_t rdmnet_ept_client_create(const RdmnetEptClientConfig *config, rdmnet_client_t *handle);
void rdmnet_ept_client_destroy(rdmnet_client_t handle);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_CLIENT_H_ */
