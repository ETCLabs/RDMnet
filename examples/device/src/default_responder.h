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

#ifndef _DEFAULTRESPONDER_H_
#define _DEFAULTRESPONDER_H_

#include "lwpa/int.h"
#include "lwpa/bool.h"
#include "lwpa/inet.h"
#include "rdm/message.h"
#include "rdmnet/defs.h"
#include "rdmnet/client.h"

#define MAX_RESPONSES_IN_ACK_OVERFLOW 2

typedef struct RdmParamData
{
  uint8_t datalen;
  uint8_t data[RDM_MAX_PDL];
} RdmParamData;

typedef RdmParamData param_data_list_t[MAX_RESPONSES_IN_ACK_OVERFLOW];

typedef enum
{
  kNoRdmnetDataChanged,
  kRdmnetScopeConfigChanged,
  kRdmnetSearchDomainChanged
} rdmnet_data_changed_t;

#ifdef __cplusplus
extern "C" {
#endif

void default_responder_init(const RdmnetScopeConfig *scope_config, const char *search_domain);
void default_responder_deinit();

/* Interface between the E1.33 Device logic and the default responder */
void default_responder_get_scope_config(RdmnetScopeConfig *scope_config);
void default_responder_get_search_domain(char *search_domain);
bool default_responder_supports_pid(uint16_t pid);
void default_responder_update_connection_status(bool connected, const LwpaSockaddr *broker_addr);
void default_responder_incr_unhealthy_count();
void default_responder_reset_unhealthy_count();

/* Generic PID get and set functions */
bool default_responder_set(uint16_t pid, const uint8_t *param_data, uint8_t param_data_len, uint16_t *nack_reason,
                           rdmnet_data_changed_t *data_changed);
bool default_responder_get(uint16_t pid, const uint8_t *param_data, uint8_t param_data_len,
                           param_data_list_t resp_data_list, size_t *num_responses, uint16_t *nack_reason);

#ifdef __cplusplus
}
#endif

#endif /* _DEFAULTRESPONDER_H_ */
