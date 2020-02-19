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
 * \file rdmnet/core/llrp_target.h
 * \brief Functions for implementing LLRP Target functionality.
 * \author Christian Reese and Sam Kearney
 */

#ifndef RDMNET_CORE_LLRP_TARGET_H_
#define RDMNET_CORE_LLRP_TARGET_H_

#include <stdint.h>
#include "etcpal/common.h"
#include "etcpal/uuid.h"
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "rdm/uid.h"
#include "rdm/message.h"
#include "rdmnet/core.h"
#include "rdmnet/core/llrp.h"

/*!
 * \defgroup llrp_target LLRP Target
 * \ingroup llrp
 * \brief Implement the functionality required by an LLRP Target in E1.33.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! An RDM command to be sent from a local LLRP Target. */
typedef struct LlrpLocalRdmResponse
{
  /*! The CID of the LLRP Manager to which this response is addressed. */
  EtcPalUuid dest_cid;
  /*! The sequence number received in the corresponding LlrpRemoteRdmCommand. */
  uint32_t seq_num;
  /*! The network interface ID in the corresponding LlrpRemoteRdmCommand. */
  RdmnetMcastNetintId netint_id;
  /*! The RDM response. */
  RdmResponse rdm;
} LlrpLocalRdmResponse;

/*! An RDM command received from a remote LLRP Manager. */
typedef struct LlrpRemoteRdmCommand
{
  /*! The CID of the LLRP Mangaer from which this command was received. */
  EtcPalUuid src_cid;
  /*! The sequence number received with this command, to be echoed in the corresponding
   *  LlrpLocalRdmResponse. */
  uint32_t seq_num;
  /*! An ID for the network interface on which this command was received, to be echoed in the
   *  corresponding LlrpLocalRdmResponse. This helps the LLRP library send the response on the same
   *  interface on which it was received. */
  RdmnetMcastNetintId netint_id;
  /*! The RDM command. */
  RdmCommand rdm;
} LlrpRemoteRdmCommand;

/*!
 * \brief Initialize a LlrpLocalRdmResponse to a received LlrpRemoteRdmCommand.
 *
 * Provide the received command and the RdmResponse to be sent in response.
 *
 * \param resp Response to initialize (LlrpLocalRdmResponse *).
 * \param received_cmd Received command (LlrpRemoteRdmCommand *).
 * \param rdm_resp RDM response to send (RdmResponse *).
 */
#define LLRP_CREATE_RESPONSE_FROM_COMMAND(resp, received_cmd, rdm_resp) \
  do                                                                    \
  {                                                                     \
    (resp)->dest_cid = (received_cmd)->src_cid;                         \
    (resp)->seq_num = (received_cmd)->seq_num;                          \
    (resp)->netint_id = (received_cmd)->netint_id;                      \
    (resp)->rdm = *(rdm_resp);                                          \
  } while (0)

typedef struct LlrpTargetCallbacks
{
  void (*rdm_cmd_received)(llrp_target_t handle, const LlrpRemoteRdmCommand* cmd, void* context);
} LlrpTargetCallbacks;

typedef struct LlrpTargetOptionalConfig
{
  RdmnetMcastNetintId* netint_arr;
  size_t num_netints;
  RdmUid uid;
} LlrpTargetOptionalConfig;

#define LLRP_TARGET_INIT_OPTIONAL_CONFIG_VALUES(optionalcfgptr, manu_id) \
  do                                                                     \
  {                                                                      \
    (optionalcfgptr)->netint_arr = NULL;                                 \
    (optionalcfgptr)->num_netints = 0;                                   \
    RDMNET_INIT_DYNAMIC_UID_REQUEST(&(optionalcfgptr)->uid, (manu_id));  \
  } while (0)

typedef struct LlrpTargetConfig
{
  LlrpTargetOptionalConfig optional;

  EtcPalUuid cid;
  llrp_component_t component_type;
  LlrpTargetCallbacks callbacks;
  void* callback_context;
} LlrpTargetConfig;

#define LLRP_TARGET_CONFIG_INIT(targetcfgptr, manu_id) \
  LLRP_TARGET_INIT_OPTIONAL_CONFIG_VALUES(&(targetcfgptr)->optional, manu_id)

etcpal_error_t rdmnet_llrp_target_create(const LlrpTargetConfig* config, llrp_target_t* handle);
void rdmnet_llrp_target_destroy(llrp_target_t handle);

void rdmnet_llrp_target_update_connection_state(llrp_target_t handle, bool connected_to_broker);
etcpal_error_t rdmnet_llrp_send_rdm_response(llrp_target_t handle, const LlrpLocalRdmResponse* resp);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* RDMNET_CORE_LLRP_H_ */
