/******************************************************************************
 * Copyright 2020 ETC Inc.
 *
 * Licensed under the, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of RDMnet. For, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

#ifndef RDMNET_MOCK_DEVICE_H_
#define RDMNET_MOCK_DEVICE_H_

#include "rdmnet/device.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_device_create, const RdmnetDeviceConfig*, rdmnet_device_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_device_destroy, rdmnet_device_t, rdmnet_disconnect_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_send_rdm_ack,
                        rdmnet_device_t,
                        const RdmnetSavedRdmCommand*,
                        const uint8_t*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_send_rdm_nack,
                        rdmnet_device_t,
                        const RdmnetSavedRdmCommand*,
                        rdm_nack_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_send_rdm_update,
                        rdmnet_device_t,
                        uint16_t,
                        uint16_t,
                        const uint8_t*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_send_rdm_update_from_responder,
                        rdmnet_device_t,
                        const RdmnetSourceAddr*,
                        uint16_t,
                        const uint8_t*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_send_status,
                        rdmnet_device_t,
                        const RdmnetSavedRdmCommand*,
                        rpt_status_code_t,
                        const char*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_send_llrp_ack,
                        rdmnet_device_t,
                        const LlrpSavedRdmCommand*,
                        const uint8_t*,
                        uint8_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_send_llrp_nack,
                        rdmnet_device_t,
                        const LlrpSavedRdmCommand*,
                        rdm_nack_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_add_physical_endpoint,
                        rdmnet_device_t,
                        const RdmnetPhysicalEndpointConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_add_physical_endpoints,
                        rdmnet_device_t,
                        const RdmnetPhysicalEndpointConfig*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_add_virtual_endpoint,
                        rdmnet_device_t,
                        const RdmnetVirtualEndpointConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_add_virtual_endpoints,
                        rdmnet_device_t,
                        const RdmnetVirtualEndpointConfig*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_device_remove_endpoint, rdmnet_device_t, uint16_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_device_remove_endpoints, rdmnet_device_t, const uint16_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_add_static_responders,
                        rdmnet_device_t,
                        uint16_t,
                        const RdmUid*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_add_dynamic_responders,
                        rdmnet_device_t,
                        uint16_t,
                        const EtcPalUuid*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_add_physical_responders,
                        rdmnet_device_t,
                        uint16_t,
                        const RdmnetPhysicalEndpointResponder*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_remove_static_responders,
                        rdmnet_device_t,
                        uint16_t,
                        const RdmUid*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_remove_dynamic_responders,
                        rdmnet_device_t,
                        uint16_t,
                        const EtcPalUuid*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_remove_physical_responders,
                        rdmnet_device_t,
                        uint16_t,
                        const RdmUid*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_change_scope,
                        rdmnet_device_t,
                        const RdmnetScopeConfig*,
                        rdmnet_disconnect_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rdmnet_device_change_search_domain,
                        rdmnet_device_t,
                        const char*,
                        rdmnet_disconnect_reason_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_device_get_scope, rdmnet_device_t, char*, EtcPalSockAddr*);

void rdmnet_device_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MOCK_DEVICE_H_ */
