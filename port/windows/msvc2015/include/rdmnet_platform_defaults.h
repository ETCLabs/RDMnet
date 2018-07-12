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

#ifndef _RDMNET_PLATFORM_DEFAULTS_H_
#define _RDMNET_PLATFORM_DEFAULTS_H_

#ifndef RDMNET_TICK_THREAD_PRIORITY
#define RDMNET_TICK_THREAD_PRIORITY THREAD_PRIORITY_NORMAL
#endif

#ifndef RDMNET_TICK_THREAD_STACK
#define RDMNET_TICK_THREAD_STACK 0
#endif

/* Windows default values for platform-neutral options */

#ifndef RDMNET_DYNAMIC_MEM
#define RDMNET_DYNAMIC_MEM 1
#endif

/* This has to be defined to 0 on Windows or else things will break */
#undef LLRP_BIND_TO_MCAST_ADDRESS
#define LLRP_BIND_TO_MCAST_ADDRESS 0

#endif /* _RDMNET_PLATFORM_DEFAULTS_H_ */
