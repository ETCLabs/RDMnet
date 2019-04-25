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

/*! \file rdmnet/controller.h
 *  \brief Definitions for the RDMnet Controller API
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CONTROLLER_H_
#define _RDMNET_CONTROLLER_H_

#include "lwpa/bool.h"
#include "lwpa/uuid.h"
#include "lwpa/inet.h"
#include "rdm/uid.h"
#include "rdmnet/client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RdmnetController *rdmnet_controller_t;

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
                    const RdmnetClientConnectedInfo *info, void *context);
  void (*connect_failed)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetClientConnectFailedInfo *info, void *context);
  void (*disconnected)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                       const RdmnetClientDisconnectedInfo *info, void *context);
  void (*client_list_update)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                             client_list_action_t list_action, const ClientList *list, void *context);
  void (*rdm_response_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                const RemoteRdmResponse *resp, void *context);
  void (*rdm_command_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                               const RemoteRdmCommand *cmd, void *context);
  void (*status_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle, const RemoteRptStatus *status,
                          void *context);
  void (*llrp_rdm_command_received)(rdmnet_controller_t handle, const LlrpRemoteRdmCommand *cmd, void *context);
} RdmnetControllerCallbacks;

typedef struct RdmnetControllerConfig
{
  /*! The controller's CID. */
  LwpaUuid cid;
  /*! A set of callbacks for the controller to receive RDMnet notifications. */
  RdmnetControllerCallbacks callbacks;
  /*! Pointer to opaque data passed back with each callback. Can be NULL. */
  void *callback_context;
  /*! Optional configuration data for the controller's RPT Client functionality. */
  RptClientOptionalConfig optional;
  /*! Optional configuration data for the controller's LLRP Target functionality. */
  LlrpTargetOptionalConfig llrp_optional;
} RdmnetControllerConfig;

#define RDMNET_CONTROLLER_CONFIG_INIT(controllercfgptr, manu_id) RPT_CLIENT_CONFIG_INIT(controllercfgptr, manu_id)

lwpa_error_t rdmnet_controller_init(const LwpaLogParams *lparams);
void rdmnet_controller_deinit();

lwpa_error_t rdmnet_controller_create(const RdmnetControllerConfig *config, rdmnet_controller_t *handle);
lwpa_error_t rdmnet_controller_destroy(rdmnet_controller_t handle);

lwpa_error_t rdmnet_controller_add_scope(rdmnet_controller_t handle, const RdmnetScopeConfig *scope_config,
                                         rdmnet_client_scope_t *scope_handle);
lwpa_error_t rdmnet_controller_remove_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                            rdmnet_disconnect_reason_t reason);
lwpa_error_t rdmnet_controller_change_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                            const RdmnetScopeConfig *new_config, rdmnet_disconnect_reason_t reason);

lwpa_error_t rdmnet_controller_send_rdm_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                const LocalRdmCommand *cmd, uint32_t *seq_num);
lwpa_error_t rdmnet_controller_send_rdm_response(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                 const LocalRdmResponse *resp);
lwpa_error_t rdmnet_controller_request_client_list(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle);

#ifdef __cplusplus
};
#endif

#endif /* _RDMNET_CONTROLLER_H_ */
