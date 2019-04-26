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

/* rdmnet_mock/core.h
 * Mocking the functions of rdmnet/core.h
 */
#ifndef _RDMNET_MOCK_CORE_H_
#define _RDMNET_MOCK_CORE_H_

#include "rdmnet/core.h"
#include "fff.h"

#include "rdmnet_mock/core/broker_prot.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/discovery.h"
#include "rdmnet_mock/core/rpt_prot.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(lwpa_error_t, rdmnet_core_init, const LwpaLogParams *);
DECLARE_FAKE_VOID_FUNC(rdmnet_core_deinit);
DECLARE_FAKE_VOID_FUNC(rdmnet_core_tick);
DECLARE_FAKE_VALUE_FUNC(bool, rdmnet_core_initialized);

#define RDMNET_CORE_DO_FOR_ALL_FAKES(operation)        \
  operation(rdmnet_core_init);                         \
  operation(rdmnet_core_deinit);                       \
  operation(rdmnet_core_tick);                         \
  operation(rdmnet_core_initialized);                  \
  RDMNET_CORE_BROKER_PROT_DO_FOR_ALL_FAKES(operation); \
  RDMNET_CORE_CONNECTION_DO_FOR_ALL_FAKES(operation);  \
  RDMNET_CORE_DISCOVERY_DO_FOR_ALL_FAKES(operation);   \
  RDMNET_CORE_LLRP_TARGET_DO_FOR_ALL_FAKES(operation); \
  RDMNET_CORE_RPT_PROT_DO_FOR_ALL_FAKES(operation)

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_MOCK_CORE_H_ */
