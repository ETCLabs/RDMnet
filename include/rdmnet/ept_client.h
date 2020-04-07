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
 * \file rdmnet/ept_client.h
 * \brief Definitions for the RDMnet EPT Client API
 */

#ifndef RDMNET_EPT_CLIENT_H_
#define RDMNET_EPT_CLIENT_H_

#include "etcpal/uuid.h"
#include "rdmnet/client.h"
#include "rdmnet/common.h"
#include "rdmnet/message.h"

/*!
 * \defgroup rdmnet_ept_client EPT Client API
 * \ingroup rdmnet_api
 * \brief Implementation of EPT client functionality; see \ref using_ept_client.
 *
 * EPT clients use the Extensible Packet Tranpsort protocol to exchange opaque,
 * manufacturer-specific non-RDM data across the network infrastructure defined by RDMnet. EPT
 * clients participate in RDMnet scopes and exchange messages through an RDMnet broker, similarly
 * to RDMnet controllers and devices.
 *
 * See \ref using_ept_client for a detailed description of how to use this API.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! A handle to an RDMnet EPT Client. */
typedef struct RdmnetEptClient* rdmnet_ept_client_t;
/*! An invalid RDMnet EPT Client handle value. */
#define RDMNET_EPT_CLIENT_INVALID NULL

/*!
 * \brief An EPT client has successfully connected to a broker.
 * \param[in] handle Handle to the EPT client which has connected.
 * \param[in] scope_handle Handle to the scope on which the EPT client has connected.
 * \param[in] info More information about the successful connection.
 * \param[in] context Context pointer that was given at the creation of the client instance.
 */
typedef void (*RdmnetEptClientConnectedCallback)(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                                 const RdmnetClientConnectedInfo* info, void* context);

/*!
 * \brief A connection attempt failed between an EPT client and a broker.
 * \param[in] handle Handle to the EPT client which has failed to connect.
 * \param[in] scope_handle Handle to the scope on which the connection failed.
 * \param[in] info More information about the failed connection.
 * \param[in] context Context pointer that was given at the creation of the client instance.
 */
typedef void (*RdmnetEptClientConnectFailedCallback)(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                                     const RdmnetClientConnectFailedInfo* info, void* context);

/*!
 * \brief An EPT client which was previously connected to a broker has disconnected.
 * \param[in] handle Handle to the EPT client which has disconnected.
 * \param[in] scope_handle Handle to the scope on which the disconnect occurred.
 * \param[in] info More information about the disconnect event.
 * \param[in] context Context pointer that was given at the creation of the client instance.
 */
typedef void (*RdmnetEptClientDisconnectedCallback)(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                                    const RdmnetClientDisconnectedInfo* info, void* context);

/*!
 * \brief A client list update has been received from a broker.
 * \param[in] handle Handle to the EPT client which has received the client list update.
 * \param[in] scope_handle Handle to the scope on which the client list update was received.
 * \param[in] list_action The way the updates in client_list should be applied to the EPT client's
 *                        cached list.
 * \param[in] client_list The list of updates.
 * \param[in] context Context pointer that was given at the creation of the client instance.
 */
typedef void (*RdmnetEptClientClientListUpdateReceivedCallback)(rdmnet_ept_client_t handle,
                                                                rdmnet_client_scope_t scope_handle,
                                                                client_list_action_t list_action,
                                                                const EptClientList* client_list, void* context);

/*!
 * \brief EPT data has been received addressed to an EPT client.
 * \param[in] handle Handle to the EPT client which has received the data.
 * \param[in] scope_handle Handle to the scope on which the EPT data was received.
 * \param[in] data The EPT data.
 * \param[out] response Fill in with response data if responding synchronously.
 * \param[in] context Context pointer that was given at the creation of the client instance.
 */
typedef void (*RdmnetEptClientDataReceivedCallback)(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                                    const RdmnetEptData* data, RdmnetSyncEptResponse* response,
                                                    void* context);

/*!
 * \brief An EPT status message has been received in response to a previously-sent EPT data message.
 * \param[in] handle Handle to the EPT client which has received the EPT status message.
 * \param[in] scope_handle Handle to the scope on which the EPT status message was received.
 * \param[in] status The EPT status data.
 * \param[in] context Context pointer that was given at the creation of the client instance.
 */
typedef void (*RdmnetEptClientStatusReceivedCallback)(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                                      const RdmnetEptStatus* status, void* context);

/*! A set of notification callbacks received about an EPT client. */
typedef struct RdmnetEptClientCallbacks
{
  RdmnetEptClientConnectedCallback connected;                                  /*!< Required. */
  RdmnetEptClientConnectFailedCallback connect_failed;                         /*!< Required. */
  RdmnetEptClientDisconnectedCallback disconnected;                            /*!< Required. */
  RdmnetEptClientClientListUpdateReceivedCallback client_list_update_received; /*!< Required. */
  RdmnetEptClientDataReceivedCallback data_received;                           /*!< Required. */
  RdmnetEptClientStatusReceivedCallback status_received;                       /*!< Required. */
} RdmnetEptClientCallbacks;

/*! A set of information that defines the startup parameters of an EPT client. */
typedef struct RdmnetEptClientConfig
{
  /************************************************************************************************
   * Required Values
   ***********************************************************************************************/

  /*! The EPT client's CID. */
  EtcPalUuid cid;
  /*! A set of callbacks for the client to receive RDMnet notifications. */
  RdmnetEptClientCallbacks callbacks;

  /************************************************************************************************
   * Optional Values
   ***********************************************************************************************/

  /*!
   * (optional) A data buffer to be used to respond synchronously to EPT data notifications. See
   * \ref using_ept_client for more information.
   */
  uint8_t* response_buf;

  /*! (optional) Pointer to opaque data passed back with each callback. */
  void* callback_context;

  /*!
   * (optional) The EPT client's configured search domain for discovery. NULL to use the default
   * search domain(s).
   */
  const char* search_domain;
} RdmnetEptClientConfig;

/*!
 * \brief A set of default initializer values for an RdmnetEptClientConfig struct.
 *
 * Usage:
 * \code
 * RdmnetEptClientConfig config = { RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT_VALUES };
 * // Now fill in the required portions as necessary with your data...
 * \endcode
 *
 * To omit the enclosing brackets, use #RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT.
 */
#define RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT_VALUES {{0}}, {NULL, NULL, NULL, NULL, NULL, NULL}, NULL, NULL, NULL

/*!
 * \brief A default-value initializer for an RdmnetEptClientConfig struct.
 *
 * Usage:
 * \code
 * RdmnetEptClientConfig config = RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT;
 * // Now fill in the required portions as necessary with your data...
 * \endcode
 */
#define RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT    \
  {                                              \
    RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT_VALUES \
  }

void rdmnet_ept_client_config_init(RdmnetEptClientConfig* config);
void rdmnet_ept_client_set_callbacks(RdmnetEptClientConfig* config, RdmnetEptClientConnectedCallback connected,
                                     RdmnetEptClientConnectFailedCallback connect_failed,
                                     RdmnetEptClientDisconnectedCallback disconnected,
                                     RdmnetEptClientClientListUpdateReceivedCallback client_list_update_received,
                                     RdmnetEptClientDataReceivedCallback data_received,
                                     RdmnetEptClientStatusReceivedCallback status_received, void* callback_context);

etcpal_error_t rdmnet_ept_client_create(const RdmnetEptClientConfig* config, rdmnet_ept_client_t* handle);
etcpal_error_t rdmnet_ept_client_destroy(rdmnet_ept_client_t handle, rdmnet_disconnect_reason_t disconnect_reason);

etcpal_error_t rdmnet_ept_client_add_scope(rdmnet_ept_client_t handle, const RdmnetScopeConfig* scope_config,
                                           rdmnet_client_scope_t* scope_handle);
etcpal_error_t rdmnet_ept_client_add_default_scope(rdmnet_ept_client_t handle, rdmnet_client_scope_t* scope_handle);
etcpal_error_t rdmnet_ept_client_remove_scope(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                              rdmnet_disconnect_reason_t disconnect_reason);
etcpal_error_t rdmnet_ept_client_change_scope(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                              const RdmnetScopeConfig* new_scope_config,
                                              rdmnet_disconnect_reason_t disconnect_reason);
etcpal_error_t rdmnet_ept_client_get_scope(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                           char* scope_str_buf, EtcPalSockAddr* static_broker_addr);

etcpal_error_t rdmnet_ept_client_request_client_list(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle);

etcpal_error_t rdmnet_ept_client_send_data(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                           const EtcPalUuid* dest_cid, uint16_t manufacturer_id, uint16_t protocol_id,
                                           const uint8_t* data, size_t data_len);
etcpal_error_t rdmnet_ept_client_send_status(rdmnet_ept_client_t handle, rdmnet_client_scope_t scope_handle,
                                             const EtcPalUuid* dest_cid, ept_status_code_t status_code,
                                             const char* status_string);

#ifdef __cplusplus
};
#endif

/*!
 * @}
 */

#endif /* RDMNET_EPT_CLIENT_H_ */
