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
#include "rdmnet/core/connection.h"
#include "rdmnet/core/llrp_target.h"
#include "rdmnet/core/util.h"

/*!
 * \defgroup rdmnet_client RDMnet Client Library
 * \brief Implementation of RDMnet client functionality
 *
 * RDMnet clients encompass controllers (which originate RDM commands on the network and receive
 * responses) and devices (which receive RDM commands and respond to them), as well as EPT clients,
 * which use the Extensible Packet Transport feature of RDMnet to transport opaque data through a
 * broker.
 *
 * Clients and the scopes they participate in are tracked by handles. Management of connections to
 * brokers, as well as LLRP functionality, is handled under the hood.
 */

typedef int rdmnet_client_t;
typedef rdmnet_conn_t rdmnet_client_scope_t;

#define RDMNET_CLIENT_INVALID -1
#define RDMNET_CLIENT_SCOPE_INVALID RDMNET_CONN_INVALID

typedef struct RdmnetClientConnectedInfo
{
  LwpaSockaddr broker_addr;
} RdmnetClientConnectedInfo;

typedef struct RdmnetClientConnectFailedInfo
{
  rdmnet_connect_fail_event_t event;
  lwpa_error_t socket_err;
  rdmnet_connect_status_t rdmnet_reason;
  bool will_retry;
} RdmnetClientConnectFailedInfo;

typedef struct RdmnetClientDisconnectedInfo
{
  rdmnet_disconnect_event_t event;
  lwpa_error_t socket_err;
  rdmnet_disconnect_reason_t rdmnet_reason;
  bool will_retry;
} RdmnetClientDisconnectedInfo;

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
typedef void (*RdmnetClientConnectedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                        const RdmnetClientConnectedInfo *info, void *context);

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
typedef void (*RdmnetClientConnectFailedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                            const RdmnetClientConnectFailedInfo *info, void *context);

/*! \brief An RDMnet client disconnected from a broker on a scope.
 *
 *  Disconnection can be fatal or non-fatal; the will_retry member of the info struct indicates
 *  whether the connection will be retried automatically. If will_retry is false, it usually
 *  indicates a misconfiguration that needs to be resolved by an application user.
 *
 *  \param[in] handle Handle to client which has disconnected.
 *  \param[in] scope_handle Handle to scope on which the disconnect occurred.
 *  \param[in] info More information about the disconnect.
 *  \param[in] context Context pointer that was given at the creation of the client.
 */
typedef void (*RdmnetClientDisconnectedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                           const RdmnetClientDisconnectedInfo *info, void *context);

typedef void (*RdmnetClientBrokerMsgReceivedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                const BrokerMessage *msg, void *context);

typedef void (*RdmnetClientLlrpMsgReceivedCb)(rdmnet_client_t handle, const LlrpRemoteRdmCommand *cmd, void *context);

typedef void (*RptClientMsgReceivedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                       const RptClientMessage *msg, void *context);

typedef void (*EptClientMsgReceivedCb)(rdmnet_client_t handle, rdmnet_client_scope_t scope, const EptClientMessage *msg,
                                       void *context);

typedef struct RptClientCallbacks
{
  RdmnetClientConnectedCb connected;
  RdmnetClientConnectFailedCb connect_failed;
  RdmnetClientDisconnectedCb disconnected;
  RdmnetClientBrokerMsgReceivedCb broker_msg_received;
  RdmnetClientLlrpMsgReceivedCb llrp_msg_received;
  RptClientMsgReceivedCb msg_received;
} RptClientCallbacks;

typedef struct EptClientCallbacks
{
  RdmnetClientConnectedCb connected;
  RdmnetClientConnectFailedCb connect_failed;
  RdmnetClientDisconnectedCb disconnected;
  RdmnetClientBrokerMsgReceivedCb broker_msg_received;
  EptClientMsgReceivedCb msg_received;
} EptClientCallbacks;

typedef struct RdmnetScopeConfig
{
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  bool has_static_broker_addr;
  LwpaSockaddr static_broker_addr;
} RdmnetScopeConfig;

/*! The optional values contained in an RPT Client configuration. These values have defaults that
 *  can be initialized using the appropriate initialization macros.
 */
typedef struct RptClientOptionalConfig
{
  /*! The client's UID. If the client has a static UID, fill in the values normally. If a dynamic
   *  UID is desired, assign using RPT_CLIENT_DYNAMIC_UID(manu_id), passing your ESTA manufacturer
   *  ID. All RDMnet components are required to have a valid ESTA manufacturer ID. */
  RdmUid uid;
  /*! The client's configured search domain for discovery. */
  const char *search_domain;
} RptClientOptionalConfig;

#define RPT_CLIENT_INIT_OPTIONAL_CONFIG_VALUES(optionalcfgptr, manu_id) \
  do                                                                    \
  {                                                                     \
    rdmnet_init_dynamic_uid_request(&(optionalcfgptr)->uid, (manu_id)); \
    (optionalcfgptr)->search_domain = E133_DEFAULT_DOMAIN;              \
  } while (0)

/*! A set of information that defines the startup parameters of an RPT RDMnet Client. */
typedef struct RdmnetRptClientConfig
{
  /*! The client type, either controller or device. */
  rpt_client_type_t type;
  /*! The client's CID. */
  LwpaUuid cid;
  /*! A set of callbacks for the client to receive RDMnet notifications. */
  RptClientCallbacks callbacks;
  /*! Pointer to opaque data passed back with each callback. */
  void *callback_context;

  LlrpTargetOptionalConfig llrp_optional;
  RptClientOptionalConfig optional;
} RdmnetRptClientConfig;

/*! \brief Initialize an RPT Client Config with default values for the optional config options.
 *
 *  The config struct members not marked 'optional' are not initialized by this macro. Those
 *  members do not have default values and must be initialized manually before passing the config
 *  struct to an API function.
 *
 *  Usage example:
 *  \code
 *  RdmnetRptClientConfig config;
 *  RPT_CLIENT_CONFIG_INIT(&config, 0x6574);
 *  \endcode
 *
 *  \param configptr Pointer to RdmnetRptClientConfig, RdmnetDeviceConfig or RdmnetControllerConfig.
 *  \param manu_id ESTA manufacturer ID. All RDMnet RPT components must have one.
 */
#define RPT_CLIENT_CONFIG_INIT(clientcfgptr, manu_id)                                 \
  do                                                                                  \
  {                                                                                   \
    RPT_CLIENT_INIT_OPTIONAL_CONFIG_VALUES(&(clientcfgptr)->optional, manu_id);       \
    LLRP_TARGET_INIT_OPTIONAL_CONFIG_VALUES(&(clientcfgptr)->llrp_optional, manu_id); \
  } while (0)

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
#define RDMNET_CLIENT_SET_SCOPE(configptr, scope_str)                                      \
  do                                                                                       \
  {                                                                                        \
    rdmnet_safe_strncpy((configptr)->scope, (scope_str), E133_SCOPE_STRING_PADDED_LENGTH); \
    (configptr)->has_static_broker_addr = false;                                           \
  } while (0)

/*! \brief Initialize an RdmnetScopeConfig struct with the default RDMnet scope.
 *
 *  Scopes are resolved using RDMnet discovery (DNS-SD) by default; to override this behavior with a
 *  static broker address and port, use rdmnet_set_static_default_scope().
 *
 *  \param configptr Pointer to RdmnetScopeConfig.
 */
#define RDMNET_CLIENT_SET_DEFAULT_SCOPE(configptr)                                                           \
  do                                                                                                         \
  {                                                                                                          \
    RDMNET_MSVC_NO_DEP_WRN strncpy((configptr)->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH); \
    (configptr)->has_static_broker_addr = false;                                                             \
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
#define RDMNET_CLIENT_SET_STATIC_SCOPE(configptr, scope_str, broker_addr)                  \
  do                                                                                       \
  {                                                                                        \
    rdmnet_safe_strncpy((configptr)->scope, (scope_str), E133_SCOPE_STRING_PADDED_LENGTH); \
    (configptr)->has_static_broker_addr = true;                                            \
    (configptr)->static_broker_addr = (broker_addr);                                       \
  } while (0)

/*! \brief Initialize an RdmnetScopeConfig struct with the default RDMnet scope and a static broker
 *         address..
 *
 *  DNS-SD discovery will be bypassed and broker connection will be attempted using the address and
 *  port given.
 *
 *  \param configptr Pointer to RdmnetScopeConfig.
 *  \param broker_addr Address and port for a static broker (LwpaSockaddr)
 */
#define RDMNET_CLIENT_SET_STATIC_DEFAULT_SCOPE(configptr, broker_addr)                                       \
  do                                                                                                         \
  {                                                                                                          \
    RDMNET_MSVC_NO_DEP_WRN strncpy((configptr)->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH); \
    (configptr)->has_static_broker_addr = true;                                                              \
    (configptr)->static_broker_addr = (broker_addr);                                                         \
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_client_init(const LwpaLogParams *lparams);
void rdmnet_client_deinit();

lwpa_error_t rdmnet_rpt_client_create(const RdmnetRptClientConfig *config, rdmnet_client_t *handle);
lwpa_error_t rdmnet_ept_client_create(const RdmnetEptClientConfig *config, rdmnet_client_t *handle);
lwpa_error_t rdmnet_client_destroy(rdmnet_client_t handle);

lwpa_error_t rdmnet_client_add_scope(rdmnet_client_t handle, const RdmnetScopeConfig *scope_config,
                                     rdmnet_client_scope_t *scope_handle);
lwpa_error_t rdmnet_client_remove_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                        rdmnet_disconnect_reason_t reason);
lwpa_error_t rdmnet_client_change_scope(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                        const RdmnetScopeConfig *new_config, rdmnet_disconnect_reason_t reason);

lwpa_error_t rdmnet_client_change_search_domain(rdmnet_client_t handle, const char *new_search_domain,
                                                rdmnet_disconnect_reason_t reason);

lwpa_error_t rdmnet_client_request_client_list(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle);
// lwpa_error_t rdmnet_client_request_dynamic_uids(rdmnet_conn_t handle, rdmnet_client_scope_t scope_handle,
//                                            const DynamicUidRequestListEntry *request_list);
// lwpa_error_t rdmnet_client_request_uid_assignment_list(rdmnet_conn_t handle, rdmnet_client_scope_t scope_handle,
//                                                   const FetchUidAssignmentListEntry *uid_list);

lwpa_error_t rdmnet_rpt_client_send_rdm_command(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                const LocalRdmCommand *cmd, uint32_t *seq_num);
lwpa_error_t rdmnet_rpt_client_send_rdm_response(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                                 const LocalRdmResponse *resp);
lwpa_error_t rdmnet_rpt_client_send_status(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                           const LocalRptStatus *status);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_CLIENT_H_ */
