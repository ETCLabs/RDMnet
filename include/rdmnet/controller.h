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
#include "rdmnet/message.h"
#include "rdmnet/llrp.h"

/*!
 * \defgroup rdmnet_controller Controller API
 * \ingroup rdmnet_api
 * \brief Implementation of RDMnet controller functionality; see \ref using_controller.
 *
 * RDMnet controllers are clients which originate RDM commands and receive responses. Controllers
 * can participate in multiple scopes; the default scope string "default" must be configured as a
 * default setting.
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

/*!
 * \brief A controller has successfully connected to a broker.
 * \param[in] handle Handle to the controller which has connected.
 * \param[in] scope_handle Handle to the scope on which the controller has connected.
 * \param[in] info More information about the successful connection.
 * \param[in] context Context pointer that was given at the creation of the controller instance.
 */
typedef void (*RdmnetControllerConnectedCallback)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetClientConnectedInfo* info, void* context);

/*!
 * \brief A connection attempt failed between a controller and a broker.
 * \param[in] handle Handle to the controller which has failed to connect.
 * \param[in] scope_handle Handle to the scope on which the connection failed.
 * \param[in] info More information about the failed connection.
 * \param[in] context Context pointer that was given at the creation of the controller instance.
 */
typedef void (*RdmnetControllerConnectFailedCallback)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                      const RdmnetClientConnectFailedInfo* info, void* context);

/*!
 * \brief A controller which was previously connected to a broker has disconnected.
 * \param[in] handle Handle to the controller which has disconnected.
 * \param[in] scope_handle Handle to the scope on which the disconnect occurred.
 * \param[in] info More information about the disconnect event.
 * \param[in] context Context pointer that was given at the creation of the controller instance.
 */
typedef void (*RdmnetControllerDisconnectedCallback)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
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
typedef void (*RdmnetControllerClientListUpdateReceivedCallback)(rdmnet_controller_t handle,
                                                                 rdmnet_client_scope_t scope_handle,
                                                                 client_list_action_t list_action,
                                                                 const RptClientList* client_list, void* context);

/*!
 * \brief An RDM response has been received.
 * \param[in] handle Handle to the controller which has received the RDM response.
 * \param[in] scope_handle Handle to the scope on which the RDM response was received.
 * \param[in] resp The RDM response data.
 * \param[in] context Context pointer that was given at the creation of the controller instance.
 */
typedef void (*RdmnetControllerRdmResponseReceivedCallback)(rdmnet_controller_t handle,
                                                            rdmnet_client_scope_t scope_handle,
                                                            const RdmnetRdmResponse* resp, void* context);

/*!
 * \brief An RPT status message has been received in response to a previously-sent RDM command.
 * \param[in] handle Handle to the controller which has received the RPT status message.
 * \param[in] scope_handle Handle to the scope on which the RPT status message was received.
 * \param[in] status The RPT status data.
 * \param[in] context Context pointer that was given at the creation of the controller instance.
 */
typedef void (*RdmnetControllerStatusReceivedCallback)(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                       const RdmnetRptStatus* status, void* context);

/*!
 * \brief A set of previously-requested mappings of dynamic UIDs to responder IDs has been received.
 *
 * This callback does not need to be implemented if the controller implementation never intends
 * to request dynamic UID mappings.
 *
 * \param[in] handle Handle to the controller which has received the dynamic UID mappings.
 * \param[in] scope_handle Handle to the scope on which the dynamic UID mappings were received.
 * \param[in] list The list of dynamic UID mappings.
 * \param[in] context Context pointer that was given at the creation of the controller instance.
 */
typedef void (*RdmnetControllerResponderIdsReceivedCallback)(rdmnet_controller_t handle,
                                                             rdmnet_client_scope_t scope_handle,
                                                             const RdmnetDynamicUidAssignmentList* list, void* context);

/*! A set of notification callbacks received about a controller. */
typedef struct RdmnetControllerCallbacks
{
  RdmnetControllerConnectedCallback connected;                                  /*!< Required. */
  RdmnetControllerConnectFailedCallback connect_failed;                         /*!< Required. */
  RdmnetControllerDisconnectedCallback disconnected;                            /*!< Required. */
  RdmnetControllerClientListUpdateReceivedCallback client_list_update_received; /*!< Required. */
  RdmnetControllerRdmResponseReceivedCallback rdm_response_received;            /*!< Required. */
  RdmnetControllerStatusReceivedCallback status_received;                       /*!< Required. */
  RdmnetControllerResponderIdsReceivedCallback responder_ids_received;          /*!< Optional. */
} RdmnetControllerCallbacks;

/*!
 * \brief An RDM command has been received addressed to a controller.
 * \param[in] handle Handle to the controller which has received the RDM command.
 * \param[in] scope_handle Handle to the scope on which the RDM command was received.
 * \param[in] cmd The RDM command data.
 * \param[out] response Fill in with response data if responding synchronously (see \ref handling_rdm_commands).
 * \param[in] context Context pointer that was given at the creation of the controller instance.
 */
typedef void (*RdmnetControllerRdmCommandReceivedCallback)(rdmnet_controller_t handle,
                                                           rdmnet_client_scope_t scope_handle,
                                                           const RdmnetRdmCommand* cmd, RdmnetSyncRdmResponse* response,
                                                           void* context);

/*!
 * \brief An RDM command has been received over LLRP, addressed to a controller.
 * \param[in] handle Handle to the controller which has received the RDM command.
 * \param[in] cmd The RDM command data.
 * \param[out] response Fill in with response data if responding synchronously (see \ref handling_rdm_commands).
 * \param[in] context Context pointer that was given at the creation of the controller instance.
 */
typedef void (*RdmnetControllerLlrpRdmCommandReceivedCallback)(rdmnet_controller_t handle, const LlrpRdmCommand* cmd,
                                                               RdmnetSyncRdmResponse* response, void* context);
/*!
 * A buffer and set of callbacks which can be optionally provided to handle RDM commands addressed
 * to a controller. See \ref handling_rdm_commands for more information.
 */
typedef struct RdmnetControllerRdmCmdHandler
{
  /*! Callback called when an RDM command is received from a controller. */
  RdmnetControllerRdmCommandReceivedCallback rdm_command_received;
  /*! Callback called when an RDM command is received over LLRP. */
  RdmnetControllerLlrpRdmCommandReceivedCallback llrp_rdm_command_received;
  /*!
   * (optional) A data buffer used to respond synchronously to RDM commands. See
   * \ref handling_rdm_commands for more information.
   */
  uint8_t* response_buf;
} RdmnetControllerRdmCmdHandler;

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
  /************************************************************************************************
   * Required Values
   ***********************************************************************************************/

  /*! The controller's CID. */
  EtcPalUuid cid;
  /*! A set of callbacks for the controller to receive RDMnet notifications. */
  RdmnetControllerCallbacks callbacks;
  /*!
   * Callbacks and a buffer for the controller to receive RDM commands over RDMnet. Either this or
   * rdm_data must be provided.
   */
  RdmnetControllerRdmCmdHandler rdm_handler;
  /*!
   * Data for the library to use for handling RDM commands internally. Either this or rdm_callbacks
   * must be provided.
   */
  RdmnetControllerRdmData rdm_data;

  /************************************************************************************************
   * Optional Values
   ***********************************************************************************************/

  /*! (optional) Pointer to opaque data passed back with each callback. */
  void* callback_context;

  /*!
   * (optional) The controller's UID. This will be initialized with a Dynamic UID request using the
   * initialization functions/macros for this structure. If you want to use a static UID instead,
   * just fill this in with the static UID after initializing.
   */
  RdmUid uid;

  /*!
   * (optional) The controller's configured search domain for discovery. NULL to use the default
   * search domain(s).
   */
  const char* search_domain;

  /*!
   * (optional) Whether to create an LLRP target associated with this controller. Default is false.
   */
  bool create_llrp_target;

  /*!
   * (optional) A set of network interfaces to use for the LLRP target associated with this
   * controller. If NULL, the set passed to rdmnet_init() will be used, or all network interfaces
   * on the system if that was not provided.
   */
  const RdmnetMcastNetintId* llrp_netints;
  /*! (optional) The size of the llrp_netints array. */
  size_t num_llrp_netints;
} RdmnetControllerConfig;

/*!
 * \brief A set of default initializer values for an RdmnetControllerConfig struct.
 *
 * Usage:
 * \code
 * RdmnetControllerConfig config = { RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT_VALUES(MY_ESTA_MANUFACTURER_ID) };
 * // Now fill in the required portions as necessary with your data...
 * \endcode
 *
 * To omit the enclosing brackets, use #RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT().
 *
 * \param manu_id Your ESTA manufacturer ID.
 */
#define RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT_VALUES(manu_id)                                                   \
  {{0}}, {NULL, NULL, NULL, NULL, NULL, NULL, NULL}, {NULL, NULL, NULL}, {NULL, NULL, NULL, NULL, false}, NULL, \
      {(0x8000 | manu_id), 0}, NULL, false, NULL, 0

/*!
 * \brief A default-value initializer for an RdmnetControllerConfig struct.
 *
 * Usage:
 * \code
 * RdmnetControllerConfig config = RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT(MY_ESTA_MANUFACTURER_ID);
 * // Now fill in the required portions as necessary with your data...
 * \endcode
 *
 * \param manu_id Your ESTA manufacturer ID.
 */
#define RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT(manu_id)    \
  {                                                       \
    RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT_VALUES(manu_id) \
  }

void rdmnet_controller_config_init(RdmnetControllerConfig* config, uint16_t manufacturer_id);
void rdmnet_controller_set_callbacks(RdmnetControllerConfig* config, RdmnetControllerConnectedCallback connected,
                                     RdmnetControllerConnectFailedCallback connect_failed,
                                     RdmnetControllerDisconnectedCallback disconnected,
                                     RdmnetControllerClientListUpdateReceivedCallback client_list_update_received,
                                     RdmnetControllerRdmResponseReceivedCallback rdm_response_received,
                                     RdmnetControllerStatusReceivedCallback status_received,
                                     RdmnetControllerResponderIdsReceivedCallback responder_ids_received,
                                     void* callback_context);
void rdmnet_controller_set_rdm_data(RdmnetControllerConfig* config, const char* manufacturer_label,
                                    const char* device_model_description, const char* software_version_label,
                                    const char* device_label, bool device_label_settable);
void rdmnet_controller_set_rdm_cmd_callbacks(RdmnetControllerConfig* config,
                                             RdmnetControllerRdmCommandReceivedCallback rdm_command_received,
                                             RdmnetControllerLlrpRdmCommandReceivedCallback llrp_rdm_command_received);

etcpal_error_t rdmnet_controller_create(const RdmnetControllerConfig* config, rdmnet_controller_t* handle);
etcpal_error_t rdmnet_controller_destroy(rdmnet_controller_t handle, rdmnet_disconnect_reason_t reason);

etcpal_error_t rdmnet_controller_add_scope(rdmnet_controller_t handle, const RdmnetScopeConfig* scope_config,
                                           rdmnet_client_scope_t* scope_handle);
etcpal_error_t rdmnet_controller_add_default_scope(rdmnet_controller_t handle, rdmnet_client_scope_t* scope_handle);
etcpal_error_t rdmnet_controller_remove_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                              rdmnet_disconnect_reason_t reason);
etcpal_error_t rdmnet_controller_change_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetScopeConfig* new_scope_config,
                                              rdmnet_disconnect_reason_t disconnect_reason);
etcpal_error_t rdmnet_controller_get_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                           char* scope_str_buf, EtcPalSockAddr* static_broker_addr);

etcpal_error_t rdmnet_controller_request_client_list(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle);
etcpal_error_t rdmnet_controller_request_responder_ids(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                       const RdmUid* uids, size_t num_uids);

etcpal_error_t rdmnet_controller_send_rdm_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetDestinationAddr* destination,
                                                  rdmnet_command_class_t command_class, uint16_t param_id,
                                                  const uint8_t* data, uint8_t data_len, uint32_t* seq_num);
etcpal_error_t rdmnet_controller_send_get_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetDestinationAddr* destination, uint16_t param_id,
                                                  const uint8_t* data, uint8_t data_len, uint32_t* seq_num);
etcpal_error_t rdmnet_controller_send_set_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetDestinationAddr* destination, uint16_t param_id,
                                                  const uint8_t* data, uint8_t data_len, uint32_t* seq_num);

etcpal_error_t rdmnet_controller_send_rdm_ack(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetSavedRdmCommand* received_cmd, const uint8_t* response_data,
                                              size_t response_data_len);
etcpal_error_t rdmnet_controller_send_rdm_nack(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                               const RdmnetSavedRdmCommand* received_cmd,
                                               rdm_nack_reason_t nack_reason);
etcpal_error_t rdmnet_controller_send_rdm_update(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                 uint16_t param_id, const uint8_t* data, size_t data_len);

etcpal_error_t rdmnet_controller_send_llrp_ack(rdmnet_controller_t handle, const LlrpSavedRdmCommand* received_cmd,
                                               const uint8_t* response_data, uint8_t response_data_len);
etcpal_error_t rdmnet_controller_send_llrp_nack(rdmnet_controller_t handle, const LlrpSavedRdmCommand* received_cmd,
                                                rdm_nack_reason_t nack_reason);

#ifdef __cplusplus
};
#endif

/*!
 * @}
 */

#endif /* RDMNET_CONTROLLER_H_ */
