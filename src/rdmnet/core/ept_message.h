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

/*
 * rdmnet/core/ept_message.h
 * Functions to pack, send, and parse EPT PDUs and their encapsulated messages.
 */

#ifndef RDMNET_CORE_EPT_MESSAGE_H_
#define RDMNET_CORE_EPT_MESSAGE_H_

#include <stdint.h>
#include "rdmnet/defs.h"
#include "rdmnet/message.h"

#ifdef __cplusplus
extern "C" {
#endif

/** An EPT message. */
typedef struct EptMessage
{
  /** The vector indicates which type of message is present in the data section. Valid values are
   *  indicated by VECTOR_EPT_* in rdmnet/defs.h. */
  uint32_t vector;
  union
  {
    RdmnetEptData   ept_data;
    RdmnetEptStatus ept_status;
  } data;
} EptMessage;

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_EPT_MESSAGE_H_ */
