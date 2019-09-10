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

/* rdmnet_mock/private/core.h
 * Mocking the functions of rdmnet/private/core.h
 */
#ifndef _RDMNET_MOCK_PRIVATE_CORE_H_
#define _RDMNET_MOCK_PRIVATE_CORE_H_

#include "rdmnet/private/core.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(bool, rdmnet_core_initialized);
DECLARE_FAKE_VALUE_FUNC(bool, rdmnet_readlock);
DECLARE_FAKE_VOID_FUNC(rdmnet_readunlock);
DECLARE_FAKE_VALUE_FUNC(bool, rdmnet_writelock);
DECLARE_FAKE_VOID_FUNC(rdmnet_writeunlock);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_core_add_polled_socket, etcpal_socket_t, etcpal_poll_events_t,
                        PolledSocketInfo*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_core_modify_polled_socket, etcpal_socket_t, etcpal_poll_events_t,
                        PolledSocketInfo*);
DECLARE_FAKE_VOID_FUNC(rdmnet_core_remove_polled_socket, etcpal_socket_t);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_MOCK_PRIVATE_CORE_H_ */
