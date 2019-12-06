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

#ifndef RDMNET_PRIVATE_DISCOVERY_H_
#define RDMNET_PRIVATE_DISCOVERY_H_

#include "rdmnet/core.h"
#include "rdmnet/core/discovery.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t rdmnet_disc_init(const RdmnetNetintConfig* netint_config);
void rdmnet_disc_deinit(void);
void rdmnet_disc_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_DISCOVERY_H_ */
