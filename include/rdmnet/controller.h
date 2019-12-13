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
 * \file rdmnet/controller.h
 * \brief Definitions for the RDMnet Controller API
 */

#ifndef RDMNET_CONTROLLER_H_
#define RDMNET_CONTROLLER_H_

#include "etcpal/bool.h"
#include "etcpal/uuid.h"
#include "etcpal/inet.h"
#include "rdm/uid.h"
#include "rdmnet/client.h"

/*!
 * \defgroup rdmnet_controller Controller API
 * \brief Implementation of RDMnet controller functionality; see \ref using_controller.
 *
 * RDMnet controllers are clients which originate RDM commands and receive responses. Controllers
 * can participate in multiple scopes; the default scope string "default" must be configured as a
 * default setting. This API wraps the RDMnet Client API and provides functions tailored
 * specifically to the usage concerns of an RDMnet controller.
 */

/*!
 * \defgroup rdmnet_controller_c Controller C Language API
 * \ingroup rdmnet_controller
 * \brief C Language version of the Controller API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! A handle to an RDMnet controller. */
typedef struct RdmnetController* rdmnet_controller_t;
/*! An invalid RDMnet controller handle value. */
#define RDMNET_CONTROLLER_INVALID NULL

typedef enum
{
  kRdmnetClientListAppend = VECTOR_BROKER_CLIENT_ADD,
  kRdmnetClientListRemove = VECTOR_BROKER_CLIENT_REMOVE,
  kRdmnetClientListUpdate = VECTOR_BROKER_CLIENT_ENTRY_CHANGE,
  kRdmnetClientListReplace = VECTOR_BROKER_CONNECTED_CLIENT_LIST
} client_list_action_t;

typedef struct RdmnetControllerCallbacks
{
  void (*connected)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                    const RdmnetClientConnectedInfo* info, void* context);
  void (*connect_failed)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetClientConnectFailedInfo* info, void* context);
  void (*disconnected)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                       const RdmnetClientDisconnectedInfo* info, void* context);
  void (*client_list_update)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                             client_list_action_t list_action, const ClientList* list, void* context);
  void (*rdm_response_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                const RemoteRdmResponse* resp, void* context);
  void (*rdm_command_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                               const RemoteRdmCommand* cmd, void* context);
  void (*status_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle, const RemoteRptStatus* status,
                          void* context);
  void (*llrp_rdm_command_received)(rdmnet_controller_t handle, const LlrpRemoteRdmCommand* cmd, void* context);
} RdmnetControllerCallbacks;

/*! A set of information that defines the startup parameters of an RDMnet Controller. */
typedef struct RdmnetControllerConfig
{
  /*! The controller's CID. */
  EtcPalUuid cid;
  /*! A set of callbacks for the controller to receive RDMnet notifications. */
  RdmnetControllerCallbacks callbacks;
  /*! Pointer to opaque data passed back with each callback. Can be NULL. */
  void* callback_context;
  /*! Optional configuration data for the controller's RPT Client functionality. */
  RptClientOptionalConfig optional;
  /*! Optional configuration data for the controller's LLRP Target functionality. */
  LlrpTargetOptionalConfig llrp_optional;
} RdmnetControllerConfig;

/*!
 * \brief Initialize an RDMnet Controller Config with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not initialized by this macro. Those members
 * do not have default values and must be initialized manually before passing the config struct to
 * an API function.
 *
 * Usage example:
 * \code
 * RdmnetControllerConfig config;
 * RDMNET_CONTROLLER_CONFIG_INIT(&config, 0x6574);
 * \endcode
 *
 * \param controllercfgptr Pointer to RdmnetControllerConfig.
 * \param manu_id ESTA manufacturer ID. All RDMnet Controllers must have one.
 */
#define RDMNET_CONTROLLER_CONFIG_INIT(controllercfgptr, manu_id) RPT_CLIENT_CONFIG_INIT(controllercfgptr, manu_id)

etcpal_error_t rdmnet_controller_init(const EtcPalLogParams* lparams, const RdmnetNetintConfig* netint_config);
void rdmnet_controller_deinit();

etcpal_error_t rdmnet_controller_create(const RdmnetControllerConfig* config, rdmnet_controller_t* handle);
etcpal_error_t rdmnet_controller_destroy(rdmnet_controller_t handle, rdmnet_disconnect_reason_t reason);

etcpal_error_t rdmnet_controller_add_scope(rdmnet_controller_t handle, const RdmnetScopeConfig* scope_config,
                                           rdmnet_client_scope_t* scope_handle);
etcpal_error_t rdmnet_controller_add_default_scope(rdmnet_controller_t handle, rdmnet_client_scope_t* scope_handle);
etcpal_error_t rdmnet_controller_remove_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                              rdmnet_disconnect_reason_t reason);
etcpal_error_t rdmnet_controller_get_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                           RdmnetScopeConfig* scope_config);

etcpal_error_t rdmnet_controller_send_rdm_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const LocalRdmCommand* cmd, uint32_t* seq_num);
etcpal_error_t rdmnet_controller_send_rdm_response(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                   const LocalRdmResponse* resp);
etcpal_error_t rdmnet_controller_send_llrp_response(rdmnet_controller_t handle, const LlrpLocalRdmResponse* resp);
etcpal_error_t rdmnet_controller_request_client_list(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle);

#ifdef __cplusplus
};
#endif

/*!
 * @}
 */

#endif /* RDMNET_CONTROLLER_H_ */
