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

DEFINE_FAKE_VALUE_FUNC(size_t, broker_get_rpt_client_list_buffer_size, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, broker_get_ept_client_list_buffer_size, const EptClientEntry*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, broker_get_uid_assignment_list_buffer_size, size_t);

DEFINE_FAKE_VALUE_FUNC(size_t, broker_pack_connect_reply, uint8_t*, size_t, const EtcPalUuid*, const BrokerConnectReplyMsg*);
DEFINE_FAKE_VALUE_FUNC(size_t, broker_pack_rpt_client_list, uint8_t*, size_t, const EtcPalUuid*, uint16_t,
                       const RptClientEntry*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, broker_pack_ept_client_list, uint8_t*, size_t, const EtcPalUuid*, uint16_t,
                       const EptClientEntry*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, broker_pack_uid_assignment_list, uint8_t*, size_t, const EtcPalUuid*,
                       const BrokerDynamicUidMapping*, size_t);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, broker_send_connect_reply, rdmnet_conn_t, const EtcPalUuid*, const BrokerConnectReplyMsg*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, broker_send_fetch_client_list, rdmnet_conn_t, const EtcPalUuid*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, broker_send_request_dynamic_uids, rdmnet_conn_t, const EtcPalUuid*,
                       const BrokerDynamicUidRequest*, size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, broker_send_fetch_uid_assignment_list, rdmnet_conn_t, const EtcPalUuid*, const RdmUid*,
                       size_t);
