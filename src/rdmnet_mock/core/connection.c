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

#include "rdmnet_mock/core/connection.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_conn_module_init);
DEFINE_FAKE_VOID_FUNC(rc_conn_module_deinit);
DEFINE_FAKE_VOID_FUNC(rc_conn_module_tick);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_conn_register, RCConnection*);
DEFINE_FAKE_VOID_FUNC(rc_conn_unregister, RCConnection*, const rdmnet_disconnect_reason_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_conn_connect,
                       RCConnection*,
                       const EtcPalSockAddr*,
                       const BrokerClientConnectMsg*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_conn_reconnect,
                       RCConnection*,
                       const EtcPalSockAddr*,
                       const BrokerClientConnectMsg*,
                       rdmnet_disconnect_reason_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_conn_disconnect, RCConnection*, rdmnet_disconnect_reason_t);
DEFINE_FAKE_VALUE_FUNC(int, rc_conn_send, RCConnection*, const uint8_t*, size_t);

void rc_connection_reset_all_fakes(void)
{
  RESET_FAKE(rc_conn_module_init);
  RESET_FAKE(rc_conn_module_deinit);
  RESET_FAKE(rc_conn_module_tick);
  RESET_FAKE(rc_conn_register);
  RESET_FAKE(rc_conn_unregister);
  RESET_FAKE(rc_conn_connect);
  RESET_FAKE(rc_conn_reconnect);
  RESET_FAKE(rc_conn_disconnect);
  RESET_FAKE(rc_conn_send);
}
