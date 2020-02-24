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
 * \file rdmnet/llrp_manager.h
 * \brief Functions for implementing LLRP Manager functionality
 */

#ifndef RDMNET_LLRP_MANAGER_H_
#define RDMNET_LLRP_MANAGER_H_

#include "etcpal/uuid.h"
#include "etcpal/inet.h"
#include "rdm/message.h"
#include "rdmnet/common.h"
#include "rdmnet/llrp.h"
#include "rdmnet/message.h"

/*!
 * \defgroup llrp_manager LLRP Manager API
 * \ingroup rdmnet_api
 * \brief Implementation of LLRP manager funtionality; see \ref using_llrp_manager.
 *
 * LLRP managers perform the discovery and command functionality of RDMnet's Low Level Recovery
 * Protocol (LLRP). See \ref using_llrp_manager for details of how to use this API.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief An LLRP target has been discovered.
 * \param handle Handle to the LLRP manager which has discovered the target.
 * \param target Information about the target which has been discovered.
 * \param context Context pointer that was given at the creation of the LLRP manager instance.
 */
typedef void (*LlrpManagerTargetDiscoveredCallback)(llrp_manager_t handle, const DiscoveredLlrpTarget* target,
                                                    void* context);

/*!
 * \brief An RDM response has been received from an LLRP target.
 * \param handle Handle the LLRP manager which has received the RDM response.
 * \param resp The RDM response data.
 * \param context Context pointer that was given at the creation of the LLRP manager instance.
 */
typedef void (*LlrpManagerRdmResponseReceivedCallback)(llrp_manager_t handle, const LlrpRdmResponse* resp,
                                                       void* context);

/*!
 * \brief The previously-started LLRP discovery process has finished.
 * \param handle Handle to the LLRP manager which has finished discovery.
 * \param context Context pointer that was given at the creation of the LLRP manager instance.
 */
typedef void (*LlrpManagerDiscoveryFinishedCallback)(llrp_manager_t handle, void* context);

/*! A set of notification callbacks received about an LLRP manager. */
typedef struct LlrpManagerCallbacks
{
  LlrpManagerTargetDiscoveredCallback target_discovered;        /*!< An LLRP target has been discovered. */
  LlrpManagerRdmResponseReceivedCallback rdm_response_received; /*!< An LLRP RDM response has been received. */
  LlrpManagerDiscoveryFinishedCallback discovery_finished;      /*!< LLRP discovery is finished. */
} LlrpManagerCallbacks;

/*! A set of information that defines the startup parameters of an LLRP Manager. */
typedef struct LlrpManagerConfig
{
  /************************************************************************************************
   * Required Values
   ***********************************************************************************************/

  /*! The manager's CID. */
  EtcPalUuid cid;
  /*! The network interface that this manager operates on. */
  RdmnetMcastNetintId netint;
  /*! The manager's ESTA manufacturer ID. */
  uint16_t manu_id;
  /*! A set of callbacks for the manager to receive RDMnet notifications. */
  LlrpManagerCallbacks callbacks;

  /************************************************************************************************
   * Optional Values
   ***********************************************************************************************/

  /*! (optional) Pointer to opaque data passed back with each callback. */
  void* callback_context;
} LlrpManagerConfig;

etcpal_error_t llrp_manager_create(const LlrpManagerConfig* config, llrp_manager_t* handle);
void llrp_manager_destroy(llrp_manager_t handle);

etcpal_error_t llrp_manager_start_discovery(llrp_manager_t handle, uint16_t filter);
etcpal_error_t llrp_manager_stop_discovery(llrp_manager_t handle);

etcpal_error_t llrp_manager_send_rdm_command(llrp_manager_t handle, const LlrpDestinationAddr* destination,
                                             rdm_command_class_t command_class, uint16_t param_id, const uint8_t* data,
                                             uint8_t data_len, uint32_t* seq_num);
etcpal_error_t llrp_manager_send_get_command(llrp_manager_t handle, const LlrpDestinationAddr* destination,
                                             uint16_t param_id, const uint8_t* data, uint8_t data_len,
                                             uint32_t* seq_num);
etcpal_error_t llrp_manager_send_set_command(llrp_manager_t handle, const LlrpDestinationAddr* destination,
                                             uint16_t param_id, const uint8_t* data, uint8_t data_len,
                                             uint32_t* seq_num);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* RDMNET_LLRP_MANAGER_H_ */
