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

#ifndef DEFAULTRESPONDER_H_
#define DEFAULTRESPONDER_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/inet.h"
#include "rdm/message.h"
#include "rdmnet/defs.h"
#include "rdmnet/client.h"

#define NUM_SUPPORTED_PIDS 12
#define RDM_RESPONSE_BUF_LENGTH RDM_MAX_PDL

#if (NUM_SUPPORTED_PIDS * 2 > RDM_RESPONSE_BUF_LENGTH)
#error "Response buffer must be made bigger!!"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void default_responder_init(const RdmnetScopeConfig* scope_config, const char* search_domain);
void default_responder_deinit();

/* Interface between the E1.33 Device logic and the default responder */
void default_responder_get_scope_config(RdmnetScopeConfig* scope_config);
void default_responder_get_search_domain(char* search_domain);
bool default_responder_supports_pid(uint16_t pid);
void default_responder_update_connection_status(bool connected, const EtcPalSockAddr* broker_addr);
void default_responder_incr_unhealthy_count();
void default_responder_reset_unhealthy_count();

/* Generic PID get and set functions */
void default_responder_set(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len,
                           RdmnetSyncRdmResponse* response);
void default_responder_get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len,
                           RdmnetSyncRdmResponse* response, uint8_t* response_buf);

#ifdef __cplusplus
}
#endif

#endif /* DEFAULTRESPONDER_H_ */
