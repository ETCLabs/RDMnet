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

#ifndef RDMNET_PRIVATE_CONTROLLER_H_
#define RDMNET_PRIVATE_CONTROLLER_H_

#include "rdmnet/controller.h"
#include "rdmnet/core/client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  kRdmHandleMethodUseCallbacks,
  kRdmHandleMethodUseData
} rdm_handle_method_t;

typedef struct RdmnetController
{
  rdmnet_client_t client_handle;
  RdmnetControllerCallbacks callbacks;

  rdm_handle_method_t rdm_handle_method;
  union
  {
    RdmnetControllerRdmCmdCallbacks callbacks;
    RdmnetControllerRdmData data;
  } rdm_handler;

  void* callback_context;
} RdmnetController;

etcpal_error_t rdmnet_controller_init(void);
void rdmnet_controller_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_CONTROLLER_H_ */
