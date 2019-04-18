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

/*! \file rdmnet/core/llrp.h
 *  \brief Functions for initializing/deinitializing LLRP and handling LLRP discovery and
 *         networking.
 *  \author Christian Reese and Sam Kearney
 */
#ifndef _RDMNET_CORE_LLRP_H_
#define _RDMNET_CORE_LLRP_H_

#include "lwpa/common.h"
#include "lwpa/uuid.h"
#include "lwpa/int.h"
#include "lwpa/error.h"
#include "lwpa/inet.h"
#include "rdm/uid.h"
#include "rdm/message.h"
#include "rdmnet/core.h"

/*! \defgroup llrp LLRP
 *  \ingroup rdmnet_core_lib
 *  \brief Send and receive Low Level Recovery Protocol (LLRP) messages.
 *
 *  @{
 */

/*! Identifies the type of RPT Component with which this LLRP Target is associated. */
typedef enum
{
  /*! This LLRP Target is associated with an RPT Device. */
  kLlrpCompRPTDevice = 0,
  /*! This LLRP Target is associated with an RPT Controller. */
  kLlrpCompRPTController = 1,
  /*! This LLRP Target is associated with a Broker. */
  kLlrpCompBroker = 2,
  /*! This LLRP Target is standalone or associated with an unknown Component type. */
  kLlrpCompUnknown = 255
} llrp_component_t;

/*! An RDM command to be sent from a local LLRP Manager. */
typedef struct LlrpLocalRdmCommand
{
  /*! The CID of the LLRP Target to which this command is addressed. */
  LwpaUuid dest_cid;
  /*! The RDM command. */
  RdmCommand rdm;
} LlrpLocalRdmCommand;

/*! An RDM command to be sent from a local LLRP Target. */
typedef struct LlrpLocalRdmResponse
{
  /*! The CID of the LLRP Manager to which this response is addressed. */
  LwpaUuid dest_cid;
  /*! The sequence number received in the corresponding LlrpRemoteRdmCommand. */
  uint32_t seq_num;
  /*! The interface index in the corresponding LlrpRemoteRdmCommand. */
  size_t interface_index;
  /*! The RDM response. */
  RdmResponse rdm;
} LlrpLocalRdmResponse;

/*! An RDM command received from a remote LLRP Manager. */
typedef struct LlrpRemoteRdmCommand
{
  /*! The CID of the LLRP Mangaer from which this command was received. */
  LwpaUuid src_cid;
  /*! The sequence number received with this command, to be echoed in the corresponding
   *  LlrpLocalRdmResponse. */
  uint32_t seq_num;
  /*! The interface index on which this command was received, to be echoed in the corresponding
   *  LlrpLocalRdmResponse. */
  size_t interface_index;
  /*! The RDM command. */
  RdmCommand rdm;
} LlrpRemoteRdmCommand;

/*! An RDM response received from a remote LLRP Target. */
typedef struct LlrpRemoteRdmResponse
{
  /*! The CID of the LLRP Target from which this command was received. */
  LwpaUuid src_cid;
  /*! The sequence number of this response (to be associated with a previously-sent command). */
  uint32_t seq_num;
  /*! The RDM response. */
  RdmResponse rdm;
} LlrpRemoteRdmResponse;

/*! A set of information associated with an LLRP Target. */
typedef struct DiscoveredLlrpTarget
{
  /*! The LLRP Target's CID. */
  LwpaUuid target_cid;
  /*! The LLRP Target's UID. */
  RdmUid target_uid;
  /*! The LLRP Target's hardware address (usually the MAC address) */
  uint8_t hardware_address[6];
  /*! The type of RPT Component this LLRP Target is associated with. */
  llrp_component_t component_type;
} DiscoveredLlrpTarget;

typedef struct LlrpManagerCallbacks
{
  void (*target_discovered)(llrp_manager_t handle, const DiscoveredLlrpTarget *target, void *context);
  void (*discovery_finished)(llrp_manager_t handle, void *context);
  void (*rdm_resp_received)(llrp_manager_t handle, const LlrpRemoteRdmResponse *resp, void *context);
} LlrpManagerCallbacks;

typedef struct LlrpTargetCallbacks
{
  void (*rdm_cmd_received)(llrp_target_t handle, const LlrpRemoteRdmCommand *cmd, void *context);
} LlrpTargetCallbacks;

typedef struct LlrpManagerConfig
{
  LwpaIpAddr netint;
  LwpaUuid cid;
  LlrpManagerCallbacks callbacks;
  void *callback_context;
} LlrpManagerConfig;

typedef struct LlrpTargetConfig
{
  LwpaIpAddr *netint_arr;
  size_t num_netints;
  LwpaUuid cid;
  RdmUid uid;
  llrp_component_t component_type;
  LlrpTargetCallbacks callbacks;
  void *callback_context;
} LlrpTargetConfig;

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_llrp_manager_create(const LlrpManagerConfig *config, llrp_manager_t *handle);
lwpa_error_t rdmnet_llrp_target_create(const LlrpTargetConfig *config, llrp_target_t *handle);
void rdmnet_llrp_destroy_manager(llrp_manager_t handle);
void rdmnet_llrp_destroy_target(llrp_target_t handle);

lwpa_error_t rdmnet_llrp_start_discovery(llrp_manager_t handle, uint16_t filter);
lwpa_error_t rdmnet_llrp_stop_discovery(llrp_manager_t handle);

void rdmnet_llrp_update_target_connection_state(llrp_target_t handle, bool connected_to_broker, const RdmUid *new_uid);

lwpa_error_t rdmnet_llrp_send_rdm_command(llrp_manager_t handle, const LlrpLocalRdmCommand *cmd,
                                          uint32_t *transaction_num);
lwpa_error_t rdmnet_llrp_send_rdm_response(llrp_target_t handle, const LlrpLocalRdmResponse *resp);

#ifdef __cplusplus
}
#endif

/*! @} */

#endif /* _RDMNET_CORE_LLRP_H_ */
