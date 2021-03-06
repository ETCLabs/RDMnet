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

#include "rdmnet_mock/controller.h"

DEFINE_FAKE_VOID_FUNC(rdmnet_controller_config_init, RdmnetControllerConfig*, uint16_t);
DEFINE_FAKE_VOID_FUNC(rdmnet_controller_set_callbacks,
                      RdmnetControllerConfig*,
                      RdmnetControllerConnectedCallback,
                      RdmnetControllerConnectFailedCallback,
                      RdmnetControllerDisconnectedCallback,
                      RdmnetControllerClientListUpdateReceivedCallback,
                      RdmnetControllerRdmResponseReceivedCallback,
                      RdmnetControllerStatusReceivedCallback,
                      RdmnetControllerResponderIdsReceivedCallback,
                      void*);
DEFINE_FAKE_VOID_FUNC(rdmnet_controller_set_rdm_data,
                      RdmnetControllerConfig*,
                      const char*,
                      const char*,
                      const char*,
                      const char*,
                      bool);
DEFINE_FAKE_VOID_FUNC(rdmnet_controller_set_rdm_cmd_callbacks,
                      RdmnetControllerConfig*,
                      RdmnetControllerRdmCommandReceivedCallback,
                      RdmnetControllerLlrpRdmCommandReceivedCallback,
                      uint8_t*,
                      void*);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_controller_create, const RdmnetControllerConfig*, rdmnet_controller_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_controller_destroy, rdmnet_controller_t, rdmnet_disconnect_reason_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_add_scope,
                       rdmnet_controller_t,
                       const RdmnetScopeConfig*,
                       rdmnet_client_scope_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_add_default_scope,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_remove_scope,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       rdmnet_disconnect_reason_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_get_scope,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       char*,
                       EtcPalSockAddr*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_request_client_list,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_request_responder_ids,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       const RdmUid*,
                       size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_send_rdm_command,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       const RdmnetDestinationAddr*,
                       rdmnet_command_class_t,
                       uint16_t,
                       const uint8_t*,
                       uint8_t,
                       uint32_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_send_get_command,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       const RdmnetDestinationAddr*,
                       uint16_t,
                       const uint8_t*,
                       uint8_t,
                       uint32_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_send_set_command,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       const RdmnetDestinationAddr*,
                       uint16_t,
                       const uint8_t*,
                       uint8_t,
                       uint32_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_send_rdm_ack,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       const RdmnetSavedRdmCommand*,
                       const uint8_t*,
                       size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_send_rdm_nack,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       const RdmnetSavedRdmCommand*,
                       rdm_nack_reason_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_send_rdm_update,
                       rdmnet_controller_t,
                       rdmnet_client_scope_t,
                       uint16_t,
                       uint16_t,
                       const uint8_t*,
                       size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_send_llrp_ack,
                       rdmnet_controller_t,
                       const LlrpSavedRdmCommand*,
                       const uint8_t*,
                       uint8_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rdmnet_controller_send_llrp_nack,
                       rdmnet_controller_t,
                       const LlrpSavedRdmCommand*,
                       rdm_nack_reason_t);

void rdmnet_controller_reset_all_fakes(void)
{
  RESET_FAKE(rdmnet_controller_config_init);
  RESET_FAKE(rdmnet_controller_set_callbacks);
  RESET_FAKE(rdmnet_controller_set_rdm_data);
  RESET_FAKE(rdmnet_controller_set_rdm_cmd_callbacks);
  RESET_FAKE(rdmnet_controller_create);
  RESET_FAKE(rdmnet_controller_destroy);
  RESET_FAKE(rdmnet_controller_add_scope);
  RESET_FAKE(rdmnet_controller_add_default_scope);
  RESET_FAKE(rdmnet_controller_remove_scope);
  RESET_FAKE(rdmnet_controller_get_scope);
  RESET_FAKE(rdmnet_controller_request_client_list);
  RESET_FAKE(rdmnet_controller_request_responder_ids);
  RESET_FAKE(rdmnet_controller_send_rdm_command);
  RESET_FAKE(rdmnet_controller_send_get_command);
  RESET_FAKE(rdmnet_controller_send_set_command);
  RESET_FAKE(rdmnet_controller_send_rdm_ack);
  RESET_FAKE(rdmnet_controller_send_rdm_nack);
  RESET_FAKE(rdmnet_controller_send_rdm_update);
  RESET_FAKE(rdmnet_controller_send_llrp_ack);
  RESET_FAKE(rdmnet_controller_send_llrp_nack);
}
