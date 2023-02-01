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

/*
 * rdmnet_mock/core/client.h
 * Mocking the functions of rdmnet/core/client.h
 */

#ifndef RDMNET_MOCK_CORE_CLIENT_H_
#define RDMNET_MOCK_CORE_CLIENT_H_

#include "rdmnet/core/client.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_client_module_init);
DECLARE_FAKE_VOID_FUNC(rc_client_module_deinit);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_rpt_client_register, RCClient*, bool);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_ept_client_register, RCClient*);
DECLARE_FAKE_VALUE_FUNC(bool, rc_client_unregister, RCClient*, rdmnet_disconnect_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_add_scope,
                        RCClient*,
                        const RdmnetScopeConfig*,
                        rdmnet_client_scope_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_remove_scope,
                        RCClient*,
                        rdmnet_client_scope_t,
                        rdmnet_disconnect_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_change_scope,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const RdmnetScopeConfig*,
                        rdmnet_disconnect_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_client_get_scope, RCClient*, rdmnet_client_scope_t, char*, EtcPalSockAddr*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_change_search_domain,
                        RCClient*,
                        const char*,
                        rdmnet_disconnect_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_client_request_client_list, RCClient*, rdmnet_client_scope_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_request_dynamic_uids,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const EtcPalUuid*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_request_responder_ids,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const RdmUid*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_rdm_command,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const RdmnetDestinationAddr*,
                        rdmnet_command_class_t,
                        uint16_t,
                        const uint8_t*,
                        uint8_t,
                        uint32_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_rdm_ack,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const RdmnetSavedRdmCommand*,
                        const uint8_t*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_rdm_nack,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const RdmnetSavedRdmCommand*,
                        rdm_nack_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_rdm_update,
                        RCClient*,
                        rdmnet_client_scope_t,
                        uint16_t,
                        uint16_t,
                        const uint8_t*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_rdm_update_from_responder,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const RdmnetSourceAddr*,
                        uint16_t,
                        const uint8_t*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_rpt_status,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const RdmnetSavedRdmCommand*,
                        rpt_status_code_t,
                        const char*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_llrp_ack,
                        RCClient*,
                        const LlrpSavedRdmCommand*,
                        const uint8_t*,
                        uint8_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_llrp_nack,
                        RCClient*,
                        const LlrpSavedRdmCommand*,
                        rdm_nack_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_ept_data,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const EtcPalUuid*,
                        uint16_t,
                        uint16_t,
                        const uint8_t*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_client_send_ept_status,
                        RCClient*,
                        rdmnet_client_scope_t,
                        const EtcPalUuid*,
                        ept_status_code_t,
                        const char*);

DECLARE_FAKE_VALUE_FUNC(uint8_t*, rc_client_get_internal_response_buf, size_t);

void rc_client_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MOCK_CORE_CLIENT_H_ */
