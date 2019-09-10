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

#include "rdmnet/core/util.h"
#include "rdmnet/private/util.h"

#include <string.h>

/*! \brief A wrapper for the C library function strncpy() which truncates safely.
 *
 *  Always puts a null character at destination[num - 1].
 *  \param[out] destination Pointer to the destination array where the content is to be copied.
 *  \param[in] source C string to be copied.
 *  \param[in] num Maximum number of characters to be copied from source.
 *  \return Destination.
 */
char* rdmnet_safe_strncpy(char* destination, const char* source, size_t num)
{
  if (!destination || num == 0)
    return NULL;

  RDMNET_MSVC_NO_DEP_WRN strncpy(destination, source, num);
  destination[num - 1] = '\0';
  return destination;
}

/* IntHandleManager functions */

void init_int_handle_manager(IntHandleManager* manager, HandleValueInUseFunction value_in_use_func)
{
  manager->next_handle = 0;
  manager->handle_has_wrapped_around = false;
  manager->value_in_use = value_in_use_func;
}

int get_next_int_handle(IntHandleManager* manager)
{
  int new_handle = manager->next_handle;
  if (++manager->next_handle < 0)
  {
    manager->next_handle = 0;
    manager->handle_has_wrapped_around = true;
  }
  // Optimization - keep track of whether the handle counter has wrapped around.
  // If not, we don't need to check if the new handle is in use.
  if (manager->handle_has_wrapped_around)
  {
    // We have wrapped around at least once, we need to check for handles in use
    int original = new_handle;
    while (manager->value_in_use(new_handle))
    {
      if (manager->next_handle == original)
      {
        // Incredibly unlikely case of all handles used
        new_handle = -1;
        break;
      }
      new_handle = manager->next_handle;
      if (++manager->next_handle < 0)
        manager->next_handle = 0;
    }
  }
  return new_handle;
}
