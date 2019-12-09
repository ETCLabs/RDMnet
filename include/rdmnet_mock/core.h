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

/* rdmnet_mock/core.h
 * Mocking the functions of rdmnet/core.h
 */
#ifndef RDMNET_MOCK_CORE_H_
#define RDMNET_MOCK_CORE_H_

#include "rdmnet/core.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_core_init, const EtcPalLogParams*, const RdmnetNetintConfig*);
DECLARE_FAKE_VOID_FUNC(rdmnet_core_deinit);
DECLARE_FAKE_VOID_FUNC(rdmnet_core_tick);

void rdmnet_mock_core_reset(void);
void rdmnet_mock_core_reset_and_init(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MOCK_CORE_H_ */
