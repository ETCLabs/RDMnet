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

#include <stdbool.h>
#include "etcpal/uuid.h"
#include "etcpal/inet.h"
#include "rdm/uid.h"
#include "rdmnet/client.h"

/*!
 * \defgroup rdmnet_controller Controller API
 * \ingroup rdmnet_api
 * \brief Implementation of RDMnet controller functionality; see \ref using_controller.
 *
 * RDMnet controllers are clients which originate RDM commands and receive responses. Controllers
 * can participate in multiple scopes; the default scope string "default" must be configured as a
 * default setting. This API wraps the RDMnet Client API and provides functions tailored
 * specifically to the usage concerns of an RDMnet controller.
 *
 * See \ref using_controller for a detailed description of how to use this API.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! A handle to an RDMnet controller. */
typedef struct RdmnetController* rdmnet_controller_t;
/*! An invalid RDMnet controller handle value. */
#define RDMNET_CONTROLLER_INVALID NULL

/*! How to apply the client entries to the existing client list in a client_list_update_received
 *  callback. */
typedef enum
{
  /*! The client entries should be appended to the existing client list. */
  kRdmnetClientListAppend = VECTOR_BROKER_CLIENT_ADD,
  /*! The client entries should be removed from the existing client list. */
  kRdmnetClientListRemove = VECTOR_BROKER_CLIENT_REMOVE,
  /*! The client entries should be updated in the existing client list. */
  kRdmnetClientListUpdate = VECTOR_BROKER_CLIENT_ENTRY_CHANGE,
  /*! The existing client list should be replaced wholesale with this one. */
  kRdmnetClientListReplace = VECTOR_BROKER_CONNECTED_CLIENT_LIST
} client_list_action_t;

/*! A set of notification callbacks received about a controller. */
typedef struct RdmnetControllerCallbacks
{
  /*!
   * \brief A controller has successfully connected to a broker.
   * \param[in] handle Handle to the controller which has connected.
   * \param[in] scope_handle Handle to the scope on which the controller has connected.
   * \param[in] info More information about the successful connection.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*connected)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                    const RdmnetClientConnectedInfo* info, void* context);

  /*!
   * \brief A connection attempt failed between a controller and a broker.
   * \param[in] handle Handle to the controller which has failed to connect.
   * \param[in] scope_handle Handle to the scope on which the connection failed.
   * \param[in] info More information about the failed connection.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*connect_failed)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetClientConnectFailedInfo* info, void* context);

  /*!
   * \brief A controller which was previously connected to a broker has disconnected.
   * \param[in] handle Handle to the controller which has disconnected.
   * \param[in] scope_handle Handle to the scope on which the disconnect occurred.
   * \param[in] info More information about the disconnect event.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*disconnected)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                       const RdmnetClientDisconnectedInfo* info, void* context);

  /*!
   * \brief A client list update has been received from a broker.
   * \param[in] handle Handle to the controller which has received the client list update.
   * \param[in] scope_handle Handle to the scope on which the client list update was received.
   * \param[in] list_action The way the updates in client_list should be applied to the
   *                        controller's cached list.
   * \param[in] client_list The list of updates.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*client_list_update_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                      client_list_action_t list_action, const RptClientList* client_list,
                                      void* context);

  /*!
   * \brief An RDM response has been received.
   * \param[in] handle Handle to the controller which has received the RDM response.
   * \param[in] scope_handle Handle to the scope on which the RDM response was received.
   * \param[in] resp The RDM response data.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*rdm_response_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                const RdmnetRemoteRdmResponse* resp, void* context);

  /*!
   * \brief An RPT status message has been received in response to a previously-sent RDM command.
   * \param[in] handle Handle to the controller which has received the RPT status message.
   * \param[in] scope_handle Handle to the scope on which the RPT status message was received.
   * \param[in] status The RPT status data.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*status_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle, const RemoteRptStatus* status,
                          void* context);

  /*!
   * \brief A set of previously-requested dynamic UID mappings has been received.
   * \param[in] handle Handle to the controller which has received the dynamic UID mappings.
   * \param[in] scope_handle Handle to the scope on which the dynamic UID mappings were received.
   * \param[in] list The list of dynamic UID mappings.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*dynamic_uid_mappings_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                        const DynamicUidAssignmentList* list, void* context);
} RdmnetControllerCallbacks;

/*! A set of callbacks which can be optionally provided to handle RDM commands addressed to a
 *  controller. */
typedef struct RdmnetControllerRdmCmdCallbacks
{
  /*!
   * \brief An RDM command has been received addressed to a controller.
   * \param[in] handle Handle to the controller which has received the RDM command.
   * \param[in] scope_handle Handle to the scope on which the RDM command was received.
   * \param[in] cmd The RDM command data.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*rdm_command_received)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                               const RdmnetRemoteRdmCommand* cmd, void* context);

  /*!
   * \brief An RDM command has been received over LLRP, addressed to a controller.
   * \param[in] handle Handle to the controller which has received the RDM command.
   * \param[in] cmd The RDM command data.
   * \param[in] context Context pointer that was given at the creation of the controller instance.
   */
  void (*llrp_rdm_command_received)(rdmnet_controller_t handle, const LlrpRemoteRdmCommand* cmd, void* context);
} RdmnetControllerRdmCmdCallbacks;

/*! A set of data for the controller library to use for handling RDM commands internally. */
typedef struct RdmnetControllerRdmData
{
  /*! A string representing the manufacturer of the controller. */
  const char* manufacturer_label;
  /*! A string representing the name of the product model which implements the controller. */
  const char* device_model_description;
  /*! A string representing the software version of the controller. */
  const char* software_version_label;
  /*! A user-settable string representing a name for this particular controller instance. */
  const char* device_label;
  /*! Whether the library should allow the device_label to be changed remotely. */
  bool device_label_settable;
} RdmnetControllerRdmData;

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
  RptClientOptionalConfig optional;
} RdmnetControllerConfig;

/*!
 * \brief Set the main callbacks in an RDMnet controller configuration structure.
 *
 * All callbacks are required.
 *
 * \param configptr Pointer to the RdmnetControllerConfig in which to set the callbacks.
 * \param connected_cb Function pointer to use for the \ref RdmnetControllerCallbacks::connected
 *                     "connected" callback.
 * \param connect_failed_cb Function pointer to use for the \ref RdmnetControllerCallbacks::connect_failed
 *                          "connect_failed" callback.
 * \param disconnected_cb Function pointer to use for the \ref RdmnetControllerCallbacks::disconnected
 *                       "disconnected" callback.
 * \param client_list_update_received_cb Function pointer to use for the
 *    \ref RdmnetControllerCallbacks::client_list_update_received "client_list_update_received" callback.
 * \param rdm_response_received_cb Function pointer to use for the \ref RdmnetControllerCallbacks::rdm_response_received
 *                                 "rdm_response_received" callback.
 * \param status_received_cb Function pointer to use for the \ref RdmnetControllerCallbacks::status_received
 *                           "status_received" callback.
 * \param cb_context Pointer to opaque data passed back with each callback. Can be NULL.
 */
#define RDMNET_CONTROLLER_SET_CALLBACKS(configptr, connected_cb, connect_failed_cb, disconnected_cb,                  \
                                        client_list_update_received_cb, rdm_response_received_cb, status_received_cb, \
                                        cb_context)                                                                   \
  do                                                                                                                  \
  {                                                                                                                   \
    (configptr)->callbacks.connected = (connected_cb);                                                                \
    (configptr)->callbacks.connect_failed = (connect_failed_cb);                                                      \
    (configptr)->callbacks.disconnected = (disconnected_cb);                                                          \
    (configptr)->callbacks.client_list_update_received = (client_list_update_received_cb);                            \
    (configptr)->callbacks.rdm_response_received = (rdm_response_received_cb);                                        \
    (configptr)->callbacks.status_received = (status_received_cb);                                                    \
    (configptr)->callback_context = (cb_context);                                                                     \
  } while (0)

/*!
 * \brief Provide a set of basic information that the library will use for responding to RDM commands.
 *
 * RDMnet controllers are required to respond to a basic set of RDM commands. This library provides
 * two possible approaches to this. With this approach, the library stores some basic data about
 * a controller instance and handles and responds to all RDM commands internally. Use this macro to
 * set that data in the configuration structure. See \ref using_controller for more information.
 * The strings provided here are copied into the controller instance.
 *
 * \param configptr Pointer to the RdmnetControllerConfig in which to set the callbacks.
 * \param manu_label A string (const char*) representing the manufacturer of the controller.
 * \param dev_model_desc A string (const char*) representing the name of the product model
 *                       implementing the controller.
 * \param sw_vers_label A string (const char*) representing the software version of the controller.
 * \param dev_label A string (const char*) representing a user-settable name for this controller
 *                  instance.
 * \param dev_label_settable (bool) Whether the library should allow the device label to be changed
 *                           remotely.
 */
#define RDMNET_CONTROLLER_SET_RDM_DATA(configptr, manu_label, dev_model_desc, sw_vers_label, dev_label, \
                                       dev_label_settable)                                              \
  do                                                                                                    \
  {                                                                                                     \
    (configptr)->rdm_data.manufacturer_label = (manu_label);                                            \
    (configptr)->rdm_data.device_model_description = (dev_model_desc);                                  \
    (configptr)->rdm_data.software_version_label = (sw_vers_label);                                     \
    (configptr)->rdm_data.device_label = (dev_label);                                                   \
    (configptr)->rdm_data.device_label_settable = (dev_label_settable);                                 \
  } while (0)

/*!
 * \brief Set callbacks to handle RDM commands in an RDMnet controller configuration structure.
 *
 * RDMnet controllers are required to respond to a basic set of RDM commands. This library provides
 * two possible approaches to this. With this approach, the library forwards RDM commands received
 * via callbacks to the application to handle. GET requests for COMPONENT_SCOPE, SEARCH_DOMAIN, and
 * TCP_COMMS_STATUS will still be consumed internally. See \ref using_controller for more
 * information.
 *
 * \param configptr Pointer to the RdmnetControllerConfig in which to set the callbacks.
 * \param rdm_cmd_received_cb Function pointer to use for the
 *    \ref RdmnetControllerRdmCmdCallbacks::rdm_command_received "rdm_command_received" callback.
 * \param llrp_rdm_cmd_received_cb Function pointer to use for the
 *    \ref RdmnetControllerRdmCmdCallbacks::llrp_rdm_command_received "llrp_rdm_command_received" callback.
 */
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
                                                  const RdmnetLocalRdmCommand* cmd, uint32_t* seq_num);
etcpal_error_t rdmnet_controller_send_rdm_response(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                   const RdmnetLocalRdmResponse* resp);
etcpal_error_t rdmnet_controller_send_llrp_response(rdmnet_controller_t handle, const LlrpLocalRdmResponse* resp);

etcpal_error_t rdmnet_controller_request_client_list(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle);
etcpal_error_t rdmnet_controller_request_dynamic_uid_mappings(rdmnet_controller_t handle,
                                                              rdmnet_client_scope_t scope_handle, const RdmUid* uids,
                                                              size_t num_uids);

#ifdef __cplusplus
};
#endif

/*!
 * @}
 */

#endif /* RDMNET_CONTROLLER_H_ */
