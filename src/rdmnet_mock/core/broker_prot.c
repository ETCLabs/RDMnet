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

#include "rdmnet_mock/core/broker_prot.h"

DEFINE_FAKE_VALUE_FUNC(size_t, bufsize_rpt_client_list, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, bufsize_ept_client_list, const EptClientEntry*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, bufsize_dynamic_uid_assignment_list, size_t);

DEFINE_FAKE_VALUE_FUNC(size_t, pack_connect_reply, uint8_t*, size_t, const EtcPalUuid*, const ConnectReplyMsg*);
DEFINE_FAKE_VALUE_FUNC(size_t, pack_rpt_client_list, uint8_t*, size_t, const EtcPalUuid*, uint16_t,
                       const RptClientEntry*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, pack_ept_client_list, uint8_t*, size_t, const EtcPalUuid*, uint16_t,
                       const EptClientEntry*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, pack_dynamic_uid_assignment_list, uint8_t*, size_t, const EtcPalUuid*,
                       const DynamicUidMapping*, size_t);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, send_connect_reply, rdmnet_conn_t, const EtcPalUuid*, const ConnectReplyMsg*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, send_fetch_client_list, rdmnet_conn_t, const EtcPalUuid*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, send_request_dynamic_uids, rdmnet_conn_t, const EtcPalUuid*,
                       const DynamicUidRequest*, size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, send_fetch_uid_assignment_list, rdmnet_conn_t, const EtcPalUuid*, const RdmUid*,
                       size_t);
