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
  void (*status_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle, const RemoteRptStatus* status,
                          void* context);
} RdmnetControllerCallbacks;

typedef struct RdmnetControllerRdmCmdCallbacks
{
  void (*rdm_command_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                               const RemoteRdmCommand* cmd, void* context);
  void (*llrp_rdm_command_received)(rdmnet_controller_t handle, const LlrpRemoteRdmCommand* cmd, void* context);
} RdmnetControllerRdmCmdCallbacks;

typedef struct RdmnetControllerRdmData
{
  const char* manufacturer_label;
  const char* device_model_description;
  const char* software_version_label;
  const char* device_label;
} RdmnetControllerRdmData;

typedef struct RdmnetControllerOptionalConfig
{
  /*! The client's UID. If the client has a static UID, fill in the values normally. If a dynamic
   *  UID is desired, assign using RPT_CLIENT_DYNAMIC_UID(manu_id), passing your ESTA manufacturer
   *  ID. All RDMnet components are required to have a valid ESTA manufacturer ID. */
  RdmUid uid;
  /*! The client's configured search domain for discovery. */
  const char* search_domain;
} RdmnetControllerOptionalConfig;

/*! A set of information that defines the startup parameters of an RDMnet Controller. */
typedef struct RdmnetControllerConfig
{
  /*! The controller's CID. */
  EtcPalUuid cid;
  /*! A set of callbacks for the controller to receive RDMnet notifications. */
  RdmnetControllerCallbacks callbacks;
  /*! Callbacks for the controller to receive RDM commands over RDMnet. Either this or rdm_data
   *  must be provided. */
  RdmnetControllerRdmCmdCallbacks rdm_callbacks;
  /*! Pointer to opaque data passed back with each callback. Can be NULL. */
  void* callback_context;
  /*! Data for the library to use for handling RDM commands internally. Either this or
   *  rdm_callbacks must be provided. */
  RdmnetControllerRdmData rdm_data;
  /*! Optional configuration data for the controller's RPT Client functionality. */
  RdmnetControllerOptionalConfig optional;
  /*! Optional configuration data for the controller's LLRP Target functionality. */
  LlrpTargetOptionalConfig llrp_optional;
} RdmnetControllerConfig;

#define RDMNET_CONTROLLER_SET_CALLBACKS(configptr, connected_cb, connect_failed_cb, disconnected_cb,         \
                                        client_list_update_cb, rdm_response_received_cb, status_received_cb, \
                                        callback_context)                                                    \
  do                                                                                                         \
  {                                                                                                          \
    (configptr)->callbacks.connected = (connected_cb);                                                       \
    (configptr)->callbacks.connect_failed = (connect_failed_cb);                                             \
    (configptr)->callbacks.disconnected = (disconnected_cb);                                                 \
    (configptr)->callbacks.client_list_update = (client_list_update_cb);                                     \
    (configptr)->callbacks.rdm_response_received = (rdm_response_received_cb);                               \
    (configptr)->callbacks.status_received = (status_received_cb);                                           \
    (configptr)->callback_context = (callback_context);                                                      \
  } while (0)

#define RDMNET_CONTROLLER_SET_RDM_DATA(configptr, manu_label, dev_model_desc, sw_vers_label, dev_label) \
  do                                                                                                    \
  {                                                                                                     \
    (configptr)->rdm_data.manufacturer_label = (manu_label);                                            \
    (configptr)->rdm_data.device_model_description = (dev_model_desc);                                  \
    (configptr)->rdm_data.software_version_label = (sw_vers_label);                                     \
    (configptr)->rdm_data.device_label = (dev_label);                                                   \
  } while (0)

#define RDMNET_CONTROLLER_SET_RDM_CMD_CALLBACKS(configptr, rdm_cmd_received_cb, llrp_rdm_cmd_received_cb) \
  do                                                                                                      \
  {                                                                                                       \
    (configptr)->rdm_callbacks.rdm_command_received = (rdm_cmd_received_cb);                              \
    (configptr)->rdm_callbacks.llrp_rdm_command_received = (llrp_rdm_cmd_received_cb);                    \
  } while (0)

etcpal_error_t rdmnet_controller_init(const EtcPalLogParams* lparams, const RdmnetNetintConfig* netint_config);
void rdmnet_controller_deinit();

void rdmnet_controller_config_init(RdmnetControllerConfig* config, uint16_t manufacturer_id);

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
