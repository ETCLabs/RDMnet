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

#include "rdmnet_mock/core/connection.h"

DEFINE_FAKE_VALUE_FUNC(int, rdmnet_new_connection, const LwpaUuid *);
DEFINE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_connect, int, const LwpaSockaddr *, const ClientConnectMsg *, RdmnetData *);
DEFINE_FAKE_VALUE_FUNC(int, rdmnet_connect_poll, RdmnetPoll *, size_t, int);
DEFINE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_set_blocking, int, bool);
DEFINE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_attach_existing_socket, int, lwpa_socket_t, const LwpaSockaddr *);
DEFINE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_disconnect, int, bool, rdmnet_disconnect_reason_t);
DEFINE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_destroy_connection, int);
DEFINE_FAKE_VALUE_FUNC(int, rdmnet_send, int, const uint8_t *, size_t);
DEFINE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_start_message, int);
DEFINE_FAKE_VALUE_FUNC(int, rdmnet_send_partial_message, int, const uint8_t *, size_t);
DEFINE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_end_message, int);
DEFINE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_recv, int, RdmnetData *);
DEFINE_FAKE_VALUE_FUNC(int, rdmnet_poll, RdmnetPoll *, size_t, int);
DEFINE_FAKE_VOID_FUNC(rdmnet_tick);
