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

/*! \file rdmnet/core/llrp_target.h
 *  \brief Functions for implementing LLRP Target functionality.
 *  \author Christian Reese and Sam Kearney
 */
#ifndef _RDMNET_CORE_LLRP_TARGET_H_
#define _RDMNET_CORE_LLRP_TARGET_H_

#include "lwpa/common.h"
#include "lwpa/uuid.h"
#include "lwpa/int.h"
#include "lwpa/error.h"
#include "lwpa/inet.h"
#include "rdm/uid.h"
#include "rdm/message.h"
#include "rdmnet/core/llrp.h"

/*! \defgroup llrp_target LLRP Target
 *  \ingroup llrp
 *  \brief Implement the functionality required by an LLRP Target in E1.33.
 *
 *  @{
 */

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

typedef struct LlrpTargetCallbacks
{
  void (*rdm_cmd_received)(llrp_target_t handle, const LlrpRemoteRdmCommand *cmd, void *context);
} LlrpTargetCallbacks;

typedef struct LlrpTargetOptionalConfig
{
  LwpaIpAddr *netint_arr;
  size_t num_netints;
  RdmUid uid;
} LlrpTargetOptionalConfig;

#define LLRP_TARGET_INIT_OPTIONAL_CONFIG_VALUES(optionalcfgptr, manu_id) \
  do                                                                     \
  {                                                                      \
    (optionalcfgptr)->netint_arr = NULL;                                 \
    (optionalcfgptr)->num_netints = 0;                                   \
    rdmnet_init_dynamic_uid_request(&(optionalcfgptr)->uid, (manu_id));  \
  } while (0)

typedef struct LlrpTargetConfig
{
  LlrpTargetOptionalConfig optional;

  LwpaUuid cid;
  llrp_component_t component_type;
  LlrpTargetCallbacks callbacks;
  void *callback_context;
} LlrpTargetConfig;

#define LLRP_TARGET_CONFIG_INIT(targetcfgptr, manu_id) \
  LLRP_TARGET_INIT_OPTIONAL_CONFIG_VALUES(&(targetcfgptr)->optional, manu_id)

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_llrp_target_create(const LlrpTargetConfig *config, llrp_target_t *handle);
void rdmnet_llrp_target_destroy(llrp_target_t handle);

void rdmnet_llrp_target_update_connection_state(llrp_target_t handle, bool connected_to_broker);
lwpa_error_t rdmnet_llrp_send_rdm_response(llrp_target_t handle, const LlrpLocalRdmResponse *resp);

#ifdef __cplusplus
}
#endif

/*! @} */

#endif /* _RDMNET_CORE_LLRP_H_ */
