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

/*!
 * \file rdmnet/private/util.h
 * \brief Utilities used internally by the RDMnet library
 */

#ifndef _RDMNET_PRIVATE_UTIL_H_
#define _RDMNET_PRIVATE_UTIL_H_

#include "etcpal/bool.h"

typedef bool (*HandleValueInUseFunction)(int handle_val);

/* Manage generic integer handle values.
 *
 * This struct and the accompanying functions are a utility to manage handing out integer handles
 * to resources. It first assigns monotonically-increasing positive values starting at 0 to handles;
 * after wraparound, it uses the value_in_use function to find holes where new handle values can be
 * assigned.
 */
typedef struct IntHandleManager
{
  int next_handle;
  /* Optimize the handle generation algorithm by tracking whether the handle value has wrapped around. */
  bool handle_has_wrapped_around;
  /* Function pointer to determine if a handle value is currently in use. Used only after the handle
   * value has wrapped around once. */
  HandleValueInUseFunction value_in_use;
} IntHandleManager;

void init_int_handle_manager(IntHandleManager* manager, HandleValueInUseFunction value_in_use_func);
int get_next_int_handle(IntHandleManager* manager);

#endif /* _RDMNET_PRIVATE_UTIL_H_ */
