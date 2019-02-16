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

#ifndef _RDMNET_PRIVATE_MESSAGE_H_
#define _RDMNET_PRIVATE_MESSAGE_H_

#include "lwpa/error.h"
#include "rdmnet/private/opts.h"
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "lwpa/mempool.h"
#endif
#include "rdmnet/core/message.h"

#if RDMNET_DYNAMIC_MEM
#define alloc_client_entry() malloc(sizeof(ClientEntryData))
#define alloc_ept_subprot() malloc(sizeof(EptSubProtocol))
#define alloc_dynamic_uid_request_entry() malloc(sizeof(DynamicUidRequestListEntry))
#define alloc_dynamic_uid_mapping() malloc(sizeof(DynamicUidMapping))
#define alloc_fetch_uid_assignment_entry() malloc(sizeof(FetchUidAssignmentListEntry))
#define alloc_rdm_command() malloc(sizeof(RdmCmdListEntry))
#define free_client_entry(ptr) free(ptr)
#define free_ept_subprot(ptr) free(ptr)
#define free_dynamic_uid_request_entry(ptr) free(ptr)
#define free_dynamic_uid_mapping(ptr) free(ptr)
#define free_fetch_uid_assignment_entry(ptr) free(ptr)
#define free_rdm_command(ptr) free(ptr)
#else
#define alloc_client_entry() lwpa_mempool_alloc(client_entries)
#define alloc_ept_subprot() lwpa_mempool_alloc(ept_subprots)
#define alloc_dynamic_uid_request_entry() lwpa_mempool_alloc(dynamic_uid_request_entries)
#define alloc_dynamic_uid_mapping() lwpa_mempool_alloc(dynamic_uid_mappings)
#define alloc_fetch_uid_assignment_entry() lwpa_mempool_alloc(fetch_uid_assignment_entries)
#define alloc_rdm_command() lwpa_mempool_alloc(rdm_commands)
#define free_client_entry(ptr) lwpa_mempool_free(client_entries, ptr)
#define free_ept_subprot(ptr) lwpa_mempool_free(ept_subprots, ptr)
#define free_dynamic_uid_request_entry(ptr) lwpa_mempool_free(dynamic_uid_request_entries, ptr)
#define free_dynamic_uid_mapping(ptr) lwpa_mempool_free(dynamic_uid_mappings, ptr)
#define free_fetch_uid_assignment_entry(ptr) lwpa_mempool_free(fetch_uid_assignment_entries, ptr)
#define free_rdm_command(ptr) lwpa_mempool_free(rdm_commands, ptr)
#endif

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DECLARE(client_entries);
LWPA_MEMPOOL_DECLARE(ept_subprots);
LWPA_MEMPOOL_DECLARE(dynamic_uid_request_entries)
LWPA_MEMPOOL_DECLARE(dynamic_uid_mappings)
LWPA_MEMPOOL_DECLARE(fetch_uid_assignment_entries)
LWPA_MEMPOOL_DECLARE(rdm_commands);
#endif

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_message_init();

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_MESSAGE_PRIV_H_ */
