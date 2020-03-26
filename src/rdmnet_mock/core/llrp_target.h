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

/* rdmnet_mock/core/llrp_target.h
 * Mocking the functions of rdmnet/core/llrp_target.h
 */
#ifndef RDMNET_MOCK_CORE_LLRP_TARGET_H_
#define RDMNET_MOCK_CORE_LLRP_TARGET_H_

#include "rdmnet/core/llrp_target.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_target_create, const LlrpTargetConfig*, llrp_target_t*);
DECLARE_FAKE_VOID_FUNC(llrp_target_destroy, llrp_target_t);
DECLARE_FAKE_VOID_FUNC(llrp_target_update_connection_state, llrp_target_t, bool);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_target_send_ack, llrp_target_t, const LlrpRemoteRdmCommand*,
                        const uint8_t*, uint8_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_target_send_nack, llrp_target_t, const LlrpRemoteRdmCommand*,
                        rdm_nack_reason_t);

void llrp_target_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MOCK_CORE_LLRP_TARGET_H_ */
