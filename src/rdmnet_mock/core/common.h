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

/*
 * rdmnet_mock/core/common.h
 * Mocking the functions of rdmnet/core/common.h
 */

#ifndef RDMNET_MOCK_CORE_COMMON_H_
#define RDMNET_MOCK_CORE_COMMON_H_

#include "rdmnet/core/common.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_init, const EtcPalLogParams*, const RdmnetNetintConfig*);
DECLARE_FAKE_VOID_FUNC(rc_deinit);
DECLARE_FAKE_VALUE_FUNC(bool, rc_initialized);
DECLARE_FAKE_VOID_FUNC(rc_tick);
DECLARE_FAKE_VALUE_FUNC(bool, rdmnet_readlock);
DECLARE_FAKE_VOID_FUNC(rdmnet_readunlock);
DECLARE_FAKE_VALUE_FUNC(bool, rdmnet_writelock);
DECLARE_FAKE_VOID_FUNC(rdmnet_writeunlock);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_add_polled_socket,
                        etcpal_socket_t,
                        etcpal_poll_events_t,
                        RCPolledSocketInfo*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_modify_polled_socket,
                        etcpal_socket_t,
                        etcpal_poll_events_t,
                        RCPolledSocketInfo*);
DECLARE_FAKE_VOID_FUNC(rc_remove_polled_socket, etcpal_socket_t);

DECLARE_FAKE_VALUE_FUNC(int, rc_send, etcpal_socket_t, const void*, size_t, int);

void rdmnet_mock_core_reset_and_init(void);
void rdmnet_mock_core_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MOCK_CORE_COMMON_H_ */
