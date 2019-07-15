/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

/* rdmnet_mock/core/llrp_target.h
 * Mocking the functions of rdmnet/core/llrp_target.h
 */
#ifndef _RDMNET_MOCK_CORE_LLRP_TARGET_H_
#define _RDMNET_MOCK_CORE_LLRP_TARGET_H_

#include "rdmnet/core/llrp_target.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_llrp_target_create, const LlrpTargetConfig*, llrp_target_t*);
DECLARE_FAKE_VOID_FUNC(rdmnet_llrp_target_destroy, llrp_target_t);
DECLARE_FAKE_VOID_FUNC(rdmnet_llrp_target_update_connection_state, llrp_target_t, bool);
DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_llrp_send_rdm_response, llrp_target_t, const LlrpLocalRdmResponse*);

#define RDMNET_CORE_LLRP_TARGET_DO_FOR_ALL_FAKES(operation) \
  operation(rdmnet_llrp_target_create);                     \
  operation(rdmnet_llrp_target_destroy);                    \
  operation(rdmnet_llrp_target_update_connection_state);    \
  operation(rdmnet_llrp_send_rdm_response)

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_MOCK_CORE_LLRP_TARGET_H_ */
