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

#include "rdmnet_mock/core/rpt_prot.h"

DEFINE_FAKE_VALUE_FUNC(size_t, rc_rpt_get_request_buffer_size, const RdmBuffer*);
DEFINE_FAKE_VALUE_FUNC(size_t, rc_rpt_get_status_buffer_size, const RptStatusMsg*);
DEFINE_FAKE_VALUE_FUNC(size_t, rc_rpt_get_notification_buffer_size, const RdmBuffer*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t,
                       rc_rpt_pack_request,
                       uint8_t*,
                       size_t,
                       const EtcPalUuid*,
                       const RptHeader*,
                       const RdmBuffer*);
DEFINE_FAKE_VALUE_FUNC(size_t,
                       rc_rpt_pack_status,
                       uint8_t*,
                       size_t,
                       const EtcPalUuid*,
                       const RptHeader*,
                       const RptStatusMsg*);
DEFINE_FAKE_VALUE_FUNC(size_t,
                       rc_rpt_pack_notification,
                       uint8_t*,
                       size_t,
                       const EtcPalUuid*,
                       const RptHeader*,
                       const RdmBuffer*,
                       size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_rpt_send_request,
                       RCConnection*,
                       const EtcPalUuid*,
                       const RptHeader*,
                       const RdmBuffer*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_rpt_send_status,
                       RCConnection*,
                       const EtcPalUuid*,
                       const RptHeader*,
                       const RptStatusMsg*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_rpt_send_notification,
                       RCConnection*,
                       const EtcPalUuid*,
                       const RptHeader*,
                       const RdmBuffer*,
                       size_t);

void rc_rpt_prot_reset_all_fakes(void)
{
  RESET_FAKE(rc_rpt_get_request_buffer_size);
  RESET_FAKE(rc_rpt_get_status_buffer_size);
  RESET_FAKE(rc_rpt_get_notification_buffer_size);
  RESET_FAKE(rc_rpt_pack_request);
  RESET_FAKE(rc_rpt_pack_status);
  RESET_FAKE(rc_rpt_pack_notification);
  RESET_FAKE(rc_rpt_send_request);
  RESET_FAKE(rc_rpt_send_status);
  RESET_FAKE(rc_rpt_send_notification);
}
