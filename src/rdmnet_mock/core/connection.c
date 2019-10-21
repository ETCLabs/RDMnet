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

#include "rdmnet_mock/core/connection.h"

static rdmnet_conn_t next_conn_handle;

static etcpal_error_t fake_connection_create(const RdmnetConnectionConfig* config, rdmnet_conn_t* handle);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_connection_create, const RdmnetConnectionConfig*, rdmnet_conn_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_connect, rdmnet_conn_t, const EtcPalSockaddr*, const ClientConnectMsg*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_set_blocking, rdmnet_conn_t, bool);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_connection_destroy, rdmnet_conn_t, const rdmnet_disconnect_reason_t*);

DEFINE_FAKE_VALUE_FUNC(int, rdmnet_send, rdmnet_conn_t, const uint8_t*, size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_start_message, rdmnet_conn_t);
DEFINE_FAKE_VALUE_FUNC(int, rdmnet_send_partial_message, rdmnet_conn_t, const uint8_t*, size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_end_message, rdmnet_conn_t);

DEFINE_FAKE_VOID_FUNC(rdmnet_conn_tick);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_attach_existing_socket, rdmnet_conn_t, etcpal_socket_t,
                       const EtcPalSockaddr*);
DEFINE_FAKE_VOID_FUNC(rdmnet_socket_data_received, rdmnet_conn_t, const uint8_t*, size_t);
DEFINE_FAKE_VOID_FUNC(rdmnet_socket_error, rdmnet_conn_t, etcpal_error_t);

void rdmnet_connection_reset_all_fakes()
{
  RESET_FAKE(rdmnet_connection_create);
  RESET_FAKE(rdmnet_connect);
  RESET_FAKE(rdmnet_set_blocking);
  RESET_FAKE(rdmnet_connection_destroy);
  RESET_FAKE(rdmnet_send);
  RESET_FAKE(rdmnet_start_message);
  RESET_FAKE(rdmnet_send_partial_message);
  RESET_FAKE(rdmnet_end_message);
  RESET_FAKE(rdmnet_conn_tick);
  RESET_FAKE(rdmnet_attach_existing_socket);
  RESET_FAKE(rdmnet_socket_data_received);
  RESET_FAKE(rdmnet_socket_error);

  next_conn_handle = 0;
  rdmnet_connection_create_fake.custom_fake = fake_connection_create;
}

etcpal_error_t fake_connection_create(const RdmnetConnectionConfig* config, rdmnet_conn_t* handle)
{
  (void)config;
  *handle = next_conn_handle++;
  return kEtcPalErrOk;
}
