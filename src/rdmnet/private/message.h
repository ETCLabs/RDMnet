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

#ifndef RDMNET_PRIVATE_MESSAGE_H_
#define RDMNET_PRIVATE_MESSAGE_H_

#include "etcpal/error.h"
#include "rdmnet/private/opts.h"
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif
#include "rdmnet/core/message.h"

#if RDMNET_DYNAMIC_MEM
#define alloc_client_entry() malloc(sizeof(ClientEntryData))
#define alloc_ept_subprot() malloc(sizeof(EptSubProtocol))
#define alloc_dynamic_uid_request_entry() malloc(sizeof(DynamicUidRequestListEntry))
#define alloc_dynamic_uid_mapping() malloc(sizeof(DynamicUidMapping))
#define alloc_fetch_uid_assignment_entry() malloc(sizeof(FetchUidAssignmentListEntry))
#define alloc_rdm_command() malloc(sizeof(RdmBufListEntry))
#define alloc_rpt_status_str(size) malloc(size)
#define free_client_entry(ptr) free(ptr)
#define free_ept_subprot(ptr) free(ptr)
#define free_dynamic_uid_request_entry(ptr) free(ptr)
#define free_dynamic_uid_mapping(ptr) free(ptr)
#define free_fetch_uid_assignment_entry(ptr) free(ptr)
#define free_rdm_command(ptr) free(ptr)
#define free_rpt_status_str(ptr) free(ptr)
#else
#define alloc_client_entry() etcpal_mempool_alloc(client_entries)
#define alloc_ept_subprot() etcpal_mempool_alloc(ept_subprots)
#define alloc_dynamic_uid_request_entry() etcpal_mempool_alloc(dynamic_uid_request_entries)
#define alloc_dynamic_uid_mapping() etcpal_mempool_alloc(dynamic_uid_mappings)
#define alloc_fetch_uid_assignment_entry() etcpal_mempool_alloc(fetch_uid_assignment_entries)
#define alloc_rdm_command() etcpal_mempool_alloc(rdm_commands)
#define alloc_rpt_status_str(size) etcpal_mempool_alloc(rpt_status_strings, ptr)
#define free_client_entry(ptr) etcpal_mempool_free(client_entries, ptr)
#define free_ept_subprot(ptr) etcpal_mempool_free(ept_subprots, ptr)
#define free_dynamic_uid_request_entry(ptr) etcpal_mempool_free(dynamic_uid_request_entries, ptr)
#define free_dynamic_uid_mapping(ptr) etcpal_mempool_free(dynamic_uid_mappings, ptr)
#define free_fetch_uid_assignment_entry(ptr) etcpal_mempool_free(fetch_uid_assignment_entries, ptr)
#define free_rdm_command(ptr) etcpal_mempool_free(rdm_commands, ptr)
#define free_rpt_status_str(size) etcpal_mempool_free(rpt_status_strings, ptr)
#endif

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DECLARE(client_entries);
ETCPAL_MEMPOOL_DECLARE(ept_subprots);
ETCPAL_MEMPOOL_DECLARE(dynamic_uid_request_entries)
ETCPAL_MEMPOOL_DECLARE(dynamic_uid_mappings)
ETCPAL_MEMPOOL_DECLARE(fetch_uid_assignment_entries)
ETCPAL_MEMPOOL_DECLARE(rdm_commands);
ETCPAL_MEMPOOL_DECLARE(rpt_status_strings);
#endif

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t rdmnet_message_init();

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MESSAGE_PRIV_H_ */
