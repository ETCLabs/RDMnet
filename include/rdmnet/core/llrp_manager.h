/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

/*! \file rdmnet/core/llrp_manager.h
 *  \brief Functions for implementing LLRP Manager functionality
 *  \author Christian Reese and Sam Kearney
 */
#ifndef _RDMNET_CORE_LLRP_MANAGER_H_
#define _RDMNET_CORE_LLRP_MANAGER_H_

#include "lwpa/uuid.h"
#include "lwpa/inet.h"
#include "rdm/message.h"
#include "rdmnet/core/llrp.h"

/*! \defgroup llrp_manager LLRP Manager
 *  \ingroup llrp
 *  \brief Implement the functionality required by an LLRP Manager in E1.33.
 *
 *  @{
 */

/*! An RDM command to be sent from a local LLRP Manager. */
typedef struct LlrpLocalRdmCommand
{
  /*! The CID of the LLRP Target to which this command is addressed. */
  LwpaUuid dest_cid;
  /*! The RDM command. */
  RdmCommand rdm;
} LlrpLocalRdmCommand;

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

typedef struct LlrpManagerCallbacks
{
  void (*target_discovered)(llrp_manager_t handle, const DiscoveredLlrpTarget* target, void* context);
  void (*discovery_finished)(llrp_manager_t handle, void* context);
  void (*rdm_resp_received)(llrp_manager_t handle, const LlrpRemoteRdmResponse* resp, void* context);
} LlrpManagerCallbacks;

typedef struct LlrpManagerConfig
{
  LwpaIpAddr netint;
  LwpaUuid cid;
  uint16_t manu_id;
  LlrpManagerCallbacks callbacks;
  void* callback_context;
} LlrpManagerConfig;

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_llrp_manager_create(const LlrpManagerConfig* config, llrp_manager_t* handle);
void rdmnet_llrp_manager_destroy(llrp_manager_t handle);

lwpa_error_t rdmnet_llrp_start_discovery(llrp_manager_t handle, uint16_t filter);
lwpa_error_t rdmnet_llrp_stop_discovery(llrp_manager_t handle);

lwpa_error_t rdmnet_llrp_send_rdm_command(llrp_manager_t handle, const LlrpLocalRdmCommand* cmd,
                                          uint32_t* transaction_num);

#ifdef __cplusplus
}
#endif

/*! @} */

#endif /* _RDMNET_CORE_LLRP_MANAGER_H_ */
