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
 * rdmnet_mock/core/rpt_prot.h
 * Mocking the functions of rdmnet/core/rpt_prot.h
 */

#ifndef RDMNET_MOCK_CORE_RPT_PROT_H_
#define RDMNET_MOCK_CORE_RPT_PROT_H_

#include "rdmnet/core/rpt_prot.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(size_t, rc_rpt_get_request_buffer_size, const RdmBuffer*);
DECLARE_FAKE_VALUE_FUNC(size_t, rc_rpt_get_status_buffer_size, const RptStatusMsg*);
DECLARE_FAKE_VALUE_FUNC(size_t, rc_rpt_get_notification_buffer_size, const RdmBuffer*, size_t);
DECLARE_FAKE_VALUE_FUNC(size_t,
                        rc_rpt_pack_request,
                        uint8_t*,
                        size_t,
                        const EtcPalUuid*,
                        const RptHeader*,
                        const RdmBuffer*);
DECLARE_FAKE_VALUE_FUNC(size_t,
                        rc_rpt_pack_status,
                        uint8_t*,
                        size_t,
                        const EtcPalUuid*,
                        const RptHeader*,
                        const RptStatusMsg*);
DECLARE_FAKE_VALUE_FUNC(size_t,
                        rc_rpt_pack_notification,
                        uint8_t*,
                        size_t,
                        const EtcPalUuid*,
                        const RptHeader*,
                        const RdmBuffer*,
                        size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_rpt_send_request,
                        RCConnection*,
                        const EtcPalUuid*,
                        const RptHeader*,
                        const RdmBuffer*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_rpt_send_status,
                        RCConnection*,
                        const EtcPalUuid*,
                        const RptHeader*,
                        const RptStatusMsg*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_rpt_send_notification,
                        RCConnection*,
                        const EtcPalUuid*,
                        const RptHeader*,
                        const RdmBuffer*,
                        size_t);

void rc_rpt_prot_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MOCK_CORE_RPT_PROT_H_ */
