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

#include "rdmnet_mock/core/rpt_prot.h"

DEFINE_FAKE_VALUE_FUNC(size_t, rpt_get_request_buffer_size, const RdmBuffer*);
DEFINE_FAKE_VALUE_FUNC(size_t, rpt_get_status_buffer_size, const RptStatusMsg*);
DEFINE_FAKE_VALUE_FUNC(size_t, rpt_get_notification_buffer_size, const RdmBuffer*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, rpt_pack_request, uint8_t*, size_t, const EtcPalUuid*, const RptHeader*,
                       const RdmBuffer*);
DEFINE_FAKE_VALUE_FUNC(size_t, rpt_pack_status, uint8_t*, size_t, const EtcPalUuid*, const RptHeader*,
                       const RptStatusMsg*);
DEFINE_FAKE_VALUE_FUNC(size_t, rpt_pack_notification, uint8_t*, size_t, const EtcPalUuid*, const RptHeader*,
                       const RdmBuffer*, size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rpt_send_request, rdmnet_conn_t, const EtcPalUuid*, const RptHeader*,
                       const RdmBuffer*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rpt_send_status, rdmnet_conn_t, const EtcPalUuid*, const RptHeader*,
                       const RptStatusMsg*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rpt_send_notification, rdmnet_conn_t, const EtcPalUuid*, const RptHeader*,
                       const RdmBuffer*, size_t);
