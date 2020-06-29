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

#ifndef DEVICE_H_
#define DEVICE_H_

#include <stdint.h>
#include "etcpal/log.h"
#include "rdm/message.h"
#include "rdmnet/defs.h"
#include "rdmnet/device.h"
#include "default_responder.h"

#ifdef __cplusplus
extern "C" {
#endif

void device_print_version(void);

etcpal_error_t device_init(const EtcPalLogParams* lparams, const char* scope, const EtcPalSockAddr* static_broker_addr);
void           device_deinit(void);
void           device_run(void);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_H_ */
