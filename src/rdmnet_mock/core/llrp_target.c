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

#include "rdmnet_mock/core/llrp_target.h"

static llrp_target_t next_target_handle;

static etcpal_error_t fake_target_create(const LlrpTargetConfig* config, llrp_target_t* handle);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_target_create, const LlrpTargetConfig*, llrp_target_t*);
DEFINE_FAKE_VOID_FUNC(llrp_target_destroy, llrp_target_t);
DEFINE_FAKE_VOID_FUNC(llrp_target_update_connection_state, llrp_target_t, bool);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_target_send_ack, llrp_target_t, const LlrpRemoteRdmCommand*, const uint8_t*,
                       uint8_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_target_send_nack, llrp_target_t, const LlrpRemoteRdmCommand*,
                       rdm_nack_reason_t);

void llrp_target_reset_all_fakes(void)
{
  RESET_FAKE(llrp_target_create);
  RESET_FAKE(llrp_target_destroy);
  RESET_FAKE(llrp_target_update_connection_state);
  RESET_FAKE(llrp_target_send_ack);
  RESET_FAKE(llrp_target_send_nack);

  next_target_handle = 0;
  llrp_target_create_fake.custom_fake = fake_target_create;
}

etcpal_error_t fake_target_create(const LlrpTargetConfig* config, llrp_target_t* handle)
{
  (void)config;
  *handle = next_target_handle++;
  return kEtcPalErrOk;
}
