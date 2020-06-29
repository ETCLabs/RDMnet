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
 * rdmnet_mock/llrp_manager.h
 * Mocking the functions of rdmnet/llrp_manager.h
 */

#ifndef RDMNET_MOCK_LLRP_MANAGER_H_
#define RDMNET_MOCK_LLRP_MANAGER_H_

#include "rdmnet/llrp_manager.h"
#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_manager_create, const LlrpManagerConfig*, llrp_manager_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_manager_destroy, llrp_manager_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_manager_start_discovery, llrp_manager_t, uint16_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, llrp_manager_stop_discovery, llrp_manager_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        llrp_manager_send_rdm_command,
                        llrp_manager_t,
                        const LlrpDestinationAddr*,
                        rdmnet_command_class_t,
                        uint16_t,
                        const uint8_t*,
                        uint8_t,
                        uint32_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        llrp_manager_send_get_command,
                        llrp_manager_t,
                        const LlrpDestinationAddr*,
                        uint16_t,
                        const uint8_t*,
                        uint8_t,
                        uint32_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        llrp_manager_send_set_command,
                        llrp_manager_t,
                        const LlrpDestinationAddr*,
                        uint16_t,
                        const uint8_t*,
                        uint8_t,
                        uint32_t*);

#endif /* RDMNET_MOCK_LLRP_MANAGER_H_ */
