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

/*! \file rdmnet/core.h
 *  \brief Functions to init, deinit and drive the rdmnet/core modules.
 *  \author Sam Kearney
 */

#ifndef _RDMNET_CORE_H_
#define _RDMNET_CORE_H_

#include "lwpa/error.h"
#include "lwpa/log.h"

/*! \addtogroup rdmnet_conn
 *  @{
 */

/*! A handle to an RDMnet connection. */
typedef int rdmnet_conn_t;

/*! An invalid RDMnet connection handle value. */
#define RDMNET_CONN_INVALID -1

/*! @} */

/*! \defgroup rdmnet_core_lib RDMnet Core Library
 *  \brief Implementation of the core functions of RDMnet.
 *
 *  The core library sits underneath the higher-level \ref rdmnet_client "client" and
 *  \ref rdmnet_broker "broker" APIs, and contains the functionality that every component of RDMnet
 *  needs. This includes discovery, connections, and LLRP, as well as message packing and unpacking.
 *
 *  Most applications will not need to interact with the API functions in the core library directly,
 *  although it does define types that are exposed through the higher-level APIs.
 *
 *  @{
 */

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_core_init(const LwpaLogParams *log_params);
void rdmnet_core_deinit();

void rdmnet_core_tick();

#ifdef __cplusplus
}
#endif

/*! @} */

#endif /* _RDMNET_CORE_H_ */
