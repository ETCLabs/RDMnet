/******************************************************************************
 * Copyright 2020 ETC Inc.
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

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_llrp_target_module_init);
DEFINE_FAKE_VOID_FUNC(rc_llrp_target_module_deinit);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_llrp_target_register, RCLlrpTarget*);
DEFINE_FAKE_VOID_FUNC(rc_llrp_target_unregister, RCLlrpTarget*);
DEFINE_FAKE_VOID_FUNC(rc_llrp_target_update_connection_state, RCLlrpTarget*, bool);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_llrp_target_send_ack,
                       RCLlrpTarget*,
                       const LlrpSavedRdmCommand*,
                       const uint8_t*,
                       uint8_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_llrp_target_send_nack,
                       RCLlrpTarget*,
                       const LlrpSavedRdmCommand*,
                       rdm_nack_reason_t);
DEFINE_FAKE_VOID_FUNC(rc_llrp_target_module_tick);
DEFINE_FAKE_VOID_FUNC(rc_llrp_target_data_received, const uint8_t*, size_t, const EtcPalMcastNetintId*);

void rc_llrp_target_reset_all_fakes(void)
{
  RESET_FAKE(rc_llrp_target_module_init);
  RESET_FAKE(rc_llrp_target_module_deinit);
  RESET_FAKE(rc_llrp_target_register);
  RESET_FAKE(rc_llrp_target_unregister);
  RESET_FAKE(rc_llrp_target_update_connection_state);
  RESET_FAKE(rc_llrp_target_send_ack);
  RESET_FAKE(rc_llrp_target_send_nack);
  RESET_FAKE(rc_llrp_target_module_tick);
  RESET_FAKE(rc_llrp_target_data_received);
}
