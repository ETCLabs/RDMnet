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
#include "rdmnet/core/message.h"

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
