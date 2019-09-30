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

/* rdmnet_mock/core/connection.h
 * Mocking the functions of rdmnet/core/connection.h
 */
#ifndef _RDMNET_MOCK_CORE_CONNECTION_H_
#define _RDMNET_MOCK_CORE_CONNECTION_H_

#include "rdmnet/core/connection.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_connection_create, const RdmnetConnectionConfig*, rdmnet_conn_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_connect, rdmnet_conn_t, const EtcPalSockaddr*, const ClientConnectMsg*);
// DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_set_blocking, rdmnet_conn_t, bool);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_connection_destroy, rdmnet_conn_t, const rdmnet_disconnect_reason_t*);

DECLARE_FAKE_VALUE_FUNC(int, rdmnet_send, rdmnet_conn_t, const uint8_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_start_message, rdmnet_conn_t);
DECLARE_FAKE_VALUE_FUNC(int, rdmnet_send_partial_message, rdmnet_conn_t, const uint8_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_end_message, rdmnet_conn_t);

DECLARE_FAKE_VOID_FUNC(rdmnet_conn_tick);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_attach_existing_socket, rdmnet_conn_t, etcpal_socket_t,
                        const EtcPalSockaddr*);
DECLARE_FAKE_VOID_FUNC(rdmnet_socket_data_received, rdmnet_conn_t, const uint8_t*, size_t);
DECLARE_FAKE_VOID_FUNC(rdmnet_socket_error, rdmnet_conn_t, etcpal_error_t);

void rdmnet_connection_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_MOCK_CORE_CONNECTION_H_ */
