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

#include "rdmnet_mock/core/llrp_manager.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_llrp_manager_module_init, const RdmnetNetintConfig*);
DEFINE_FAKE_VOID_FUNC(rc_llrp_manager_module_deinit);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_llrp_manager_register, RCLlrpManager*);
DEFINE_FAKE_VOID_FUNC(rc_llrp_manager_unregister, RCLlrpManager*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_llrp_manager_start_discovery, RCLlrpManager*, uint16_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_llrp_manager_stop_discovery, RCLlrpManager*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_llrp_manager_send_rdm_command,
                       RCLlrpManager*,
                       const LlrpDestinationAddr*,
                       rdmnet_command_class_t,
                       uint16_t,
                       const uint8_t*,
                       uint8_t,
                       uint32_t*);
DEFINE_FAKE_VOID_FUNC(rc_llrp_manager_module_tick);
DEFINE_FAKE_VOID_FUNC(rc_llrp_manager_data_received, const uint8_t*, size_t, const EtcPalMcastNetintId*);

void rc_llrp_manager_reset_all_fakes(void)
{
  RESET_FAKE(rc_llrp_manager_module_init);
  RESET_FAKE(rc_llrp_manager_module_deinit);
  RESET_FAKE(rc_llrp_manager_register);
  RESET_FAKE(rc_llrp_manager_unregister);
  RESET_FAKE(rc_llrp_manager_start_discovery);
  RESET_FAKE(rc_llrp_manager_stop_discovery);
  RESET_FAKE(rc_llrp_manager_send_rdm_command);
  RESET_FAKE(rc_llrp_manager_module_tick);
  RESET_FAKE(rc_llrp_manager_data_received);
}
