/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

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

DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_new_connection, const RdmnetConnectionConfig *, rdmnet_conn_t *);
DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_connect, rdmnet_conn_t, const LwpaSockaddr *, const ClientConnectMsg *);
// DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_set_blocking, rdmnet_conn_t, bool);
DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_attach_existing_socket, rdmnet_conn_t, lwpa_socket_t,
                        const LwpaSockaddr *);
DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_destroy_connection, rdmnet_conn_t, const rdmnet_disconnect_reason_t *);
DECLARE_FAKE_VALUE_FUNC(int, rdmnet_send, rdmnet_conn_t, const uint8_t *, size_t);
DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_start_message, rdmnet_conn_t);
DECLARE_FAKE_VALUE_FUNC(int, rdmnet_send_partial_message, rdmnet_conn_t, const uint8_t *, size_t);
DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_end_message, rdmnet_conn_t);
DECLARE_FAKE_VOID_FUNC(rdmnet_conn_tick);
DECLARE_FAKE_VOID_FUNC(rdmnet_conn_socket_activity, rdmnet_conn_t, const LwpaPollfd *);

#define RDMNET_CORE_CONNECTION_DO_FOR_ALL_FAKES(operation) \
  operation(rdmnet_new_connection);                        \
  operation(rdmnet_connect);                               \
  /* operation(rdmnet_set_blocking); */                    \
  operation(rdmnet_attach_existing_socket);                \
  operation(rdmnet_destroy_connection);                    \
  operation(rdmnet_send);                                  \
  operation(rdmnet_start_message);                         \
  operation(rdmnet_send_partial_message);                  \
  operation(rdmnet_end_message);                           \
  operation(rdmnet_conn_tick);                             \
  operation(rdmnet_conn_socket_activity)

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_MOCK_CORE_CONNECTION_H_ */