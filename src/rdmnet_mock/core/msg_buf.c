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

#include "rdmnet_mock/core/msg_buf.h"

DEFINE_FAKE_VOID_FUNC(rc_msg_buf_init, RCMsgBuf*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_msg_buf_recv, RCMsgBuf*, etcpal_socket_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_msg_buf_parse_data, RCMsgBuf*);

void rc_msg_buf_reset_all_fakes(void)
{
  RESET_FAKE(rc_msg_buf_init);
  RESET_FAKE(rc_msg_buf_recv);
  RESET_FAKE(rc_msg_buf_parse_data);
}
