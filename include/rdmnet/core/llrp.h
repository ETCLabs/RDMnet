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

/*! An RDM command or response sent over LLRP. */
typedef struct LlrpRdmMessage
{
  /*! The CID of the LLRP Component sending this message. */
  LwpaUuid source_cid;
  /*! The LLRP transaction number of this message. */
  uint32_t transaction_num;
  /*! The RDM message. */
  RdmBuffer msg;
} LlrpRdmMessage;

/*! A set of information associated with an LLRP Target. */
typedef struct LlrpTarget
{
  /*! The LLRP Target's CID. */
  LwpaUuid target_cid;
  /*! The LLRP Target's UID. */
  RdmUid target_uid;
  /*! The LLRP Target's hardware address (usually the MAC address) */
  uint8_t hardware_address[6];
  /*! The type of RPT Component this LLRP Target is associated with. */
  llrp_component_t component_type;
} LlrpTarget;

typedef struct LlrpManagerCallbacks
{
  void (*target_discovered)(llrp_socket_t handle, const LlrpTarget *target, void *context);
  void (*discovery_finished)(llrp_socket_t handle, void *context);
  void (*rdm_received)(llrp_socket_t handle, const LlrpRdmMessage *msg, void *context);
} LlrpManagerCallbacks;

typedef struct LlrpTargetCallbacks
{
  void (*rdm_received)(llrp_socket_t handle, const LlrpRdmMessage *msg, void *context);
} LlrpTargetCallbacks;

typedef struct LlrpManagerConfig
{
  LwpaIpAddr netint;
  LwpaUuid cid;
} LlrpManagerConfig;

typedef struct LlrpTargetConfig
{
  LwpaIpAddr netint;
  LwpaUuid cid;
  RdmUid uid;
  uint8_t hardware_addr[6];
  llrp_component_t component_type;
} LlrpTargetConfig;

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_llrp_create_manager_socket(const LlrpManagerConfig *config, llrp_socket_t *handle);
lwpa_error_t rdmnet_llrp_create_target_socket(const LlrpTargetConfig *config, llrp_socket_t *handle);
bool rdmnet_llrp_close_socket(llrp_socket_t handle);
bool rdmnet_llrp_start_discovery(llrp_socket_t handle, uint8_t filter);
bool rdmnet_llrp_stop_discovery(llrp_socket_t handle);

void rdmnet_llrp_update_target_connection_state(llrp_socket_t handle, bool connected_to_broker, const RdmUid *new_uid);

lwpa_error_t rdmnet_llrp_send_rdm_command(llrp_socket_t handle, const LwpaUuid *destination, const RdmBuffer *command,
                                          uint32_t *transaction_num);

lwpa_error_t rdmnet_llrp_send_rdm_response(llrp_socket_t handle, const LwpaUuid *destination, const RdmBuffer *response,
                                           uint32_t transaction_num);

#ifdef __cplusplus
}
#endif

/*! @} */

#endif /* _RDMNET_CORE_LLRP_H_ */
