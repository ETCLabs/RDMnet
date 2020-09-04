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
 * rdmnet_mock/core/llrp.h
 * Mocking the functions of rdmnet/core/llrp.h
 */

#ifndef RDMNET_MOCK_CORE_LLRP_H_
#define RDMNET_MOCK_CORE_LLRP_H_

#include "rdmnet/core/llrp.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_llrp_module_init);
DECLARE_FAKE_VOID_FUNC(rc_llrp_module_deinit);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_llrp_recv_netint_add, const EtcPalMcastNetintId*, llrp_socket_t);
DECLARE_FAKE_VOID_FUNC(rc_llrp_recv_netint_remove, const EtcPalMcastNetintId*, llrp_socket_t);

void rc_llrp_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MOCK_CORE_LLRP_H_ */
