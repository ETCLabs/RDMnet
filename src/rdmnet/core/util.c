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

#include "rdmnet/core/util.h"

#include <string.h>
#include "etcpal/common.h"
#include "rdmnet/core/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif

/*************************** Private constants *******************************/

#define INITIAL_REF_CAPACITY 8

/*************************** Function definitions ****************************/

#if RDMNET_DYNAMIC_MEM
bool rc_check_buf_capacity(void**  buf,
                           size_t* buf_capacity,
                           size_t  current_num,
                           size_t  elem_size,
                           size_t  num_additional)
{
  size_t num_requested = current_num + num_additional;
  if (num_requested > *buf_capacity)
  {
    // Multiply the capacity by two until it's large enough to hold the number requested.
    size_t new_capacity = (*buf_capacity) * 2;
    while (new_capacity < num_requested)
      new_capacity *= 2;

    char* new_buffer = (char*)realloc(*buf, new_capacity * elem_size);
    if (new_buffer)
    {
      memset(&new_buffer[(*buf_capacity) * elem_size], 0, (new_capacity - (*buf_capacity)) * elem_size);
      *buf = (void*)new_buffer;
      *buf_capacity = new_capacity;
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
    return true;
  }
}

bool rc_init_buf(void** buf, size_t* buf_capacity, size_t* current_num, size_t elem_size, size_t initial_capacity)
{
  *buf = calloc(initial_capacity, elem_size);
  if (*buf)
  {
    memset(*buf, 0, initial_capacity * elem_size);
    *buf_capacity = initial_capacity;
    *current_num = 0;
    return true;
  }
  else
  {
    return false;
  }
}
#else
bool rc_init_buf(void* buf, size_t* current_num, size_t buf_size_in_bytes)
{
  memset(buf, 0, buf_size_in_bytes);
  *current_num = 0;
  return true;
}
#endif

/******************************************************************************
 * RCRefList functions
 *****************************************************************************/

bool rc_ref_list_init(RCRefList* list)
{
#if RDMNET_DYNAMIC_MEM
  list->refs = (void**)calloc(INITIAL_REF_CAPACITY, sizeof(void*));
  if (list->refs)
  {
    list->refs_capacity = INITIAL_REF_CAPACITY;
    list->num_refs = 0;
    return true;
  }
  else
  {
    return false;
  }
#else
  list->num_refs = 0;
  return true;
#endif
}

void rc_ref_list_cleanup(RCRefList* list)
{
#if RDMNET_DYNAMIC_MEM
  if (list->refs)
    free(list->refs);
  list->refs_capacity = 0;
#endif
  list->num_refs = 0;
}

bool rc_ref_lists_init(RCRefLists* lists)
{
  if (!rc_ref_list_init(&lists->active) || !rc_ref_list_init(&lists->pending) || !rc_ref_list_init(&lists->to_remove))
  {
    rc_ref_list_cleanup(&lists->active);
    rc_ref_list_cleanup(&lists->pending);
    rc_ref_list_cleanup(&lists->to_remove);
    return false;
  }
  return true;
}

void rc_ref_lists_cleanup(RCRefLists* lists)
{
  rc_ref_list_cleanup(&lists->active);
  rc_ref_list_cleanup(&lists->pending);
  rc_ref_list_cleanup(&lists->to_remove);
}

bool rc_ref_list_add_ref(RCRefList* list, void* to_add)
{
  RDMNET_ASSERT(list);
  RDMNET_ASSERT(list->refs);

#if RDMNET_DYNAMIC_MEM
  if (list->num_refs < list->refs_capacity)
  {
    list->refs[list->num_refs++] = to_add;
    return true;
  }
  else
  {
    size_t new_capacity = list->refs_capacity * 2;
    void** new_list = (void**)realloc(list->refs, (new_capacity * sizeof(void*)));
    if (new_list)
    {
      list->refs = new_list;
      list->refs_capacity = new_capacity;
      list->refs[list->num_refs++] = to_add;
      return true;
    }
    else
    {
      return false;
    }
  }
#else
  if (list->num_refs < list->refs_capacity)
  {
    list->refs[list->num_refs++] = to_add;
    return true;
  }
  else
  {
    return false;
  }
#endif
}

void rc_ref_list_remove_ref(RCRefList* list, const void* to_remove)
{
  RDMNET_ASSERT(list);
  if (!list->num_refs)
    return;

  int index = rc_ref_list_find_ref_index(list, to_remove);
  if (index >= 0)
  {
    if ((size_t)index < list->num_refs - 1)
    {
      memmove(&list->refs[index], &list->refs[index + 1], (list->num_refs - (index + 1)) * sizeof(void*));
    }
    --list->num_refs;
  }
}

void rc_ref_list_remove_all(RCRefList* list, RCRefFunction on_remove, const void* context)
{
  RDMNET_ASSERT(list);

  for (void** ref_ptr = list->refs; ref_ptr < list->refs + list->num_refs; ++ref_ptr)
  {
    if (on_remove)
      on_remove(*ref_ptr, context);
  }
  list->num_refs = 0;
}

int rc_ref_list_find_ref_index(RCRefList* list, const void* to_find)
{
  RDMNET_ASSERT(list);

  for (size_t i = 0; i < list->num_refs; ++i)
  {
    if (list->refs[i] == to_find)
      return (int)i;
  }
  return -1;
}

void* rc_ref_list_find_ref(const RCRefList* list, RCRefPredicate predicate, const void* context)
{
  RDMNET_ASSERT(list);
  RDMNET_ASSERT(predicate);

  for (void** ref_ptr = list->refs; ref_ptr < list->refs + list->num_refs; ++ref_ptr)
  {
    if (predicate(*ref_ptr, context))
      return *ref_ptr;
  }
  return NULL;
}

void rc_ref_list_for_each(RCRefList* list, RCRefFunction fn, const void* context)
{
  RDMNET_ASSERT(list);
  RDMNET_ASSERT(fn);

  for (void** ref_ptr = list->refs; ref_ptr < list->refs + list->num_refs; ++ref_ptr)
  {
    fn(*ref_ptr, context);
  }
}

void rc_ref_lists_add_pending(RCRefLists* lists)
{
  RDMNET_ASSERT(lists);

  RCRefList* active = &lists->active;
  RCRefList* pending = &lists->pending;

  for (void** ref_ptr = pending->refs; ref_ptr < pending->refs + pending->num_refs; ++ref_ptr)
  {
    rc_ref_list_add_ref(active, *ref_ptr);
  }
  pending->num_refs = 0;
}

void rc_ref_lists_remove_marked(RCRefLists* lists, RCRefFunction on_remove, const void* context)
{
  RDMNET_ASSERT(lists);

  RCRefList* active = &lists->active;
  RCRefList* pending = &lists->pending;
  RCRefList* to_remove = &lists->to_remove;

  for (void** ref_ptr = to_remove->refs; ref_ptr < to_remove->refs + to_remove->num_refs; ++ref_ptr)
  {
    // Only call the on_remove callback if the ref was present in either active or pending.
    if ((rc_ref_list_find_ref_index(active, *ref_ptr) != -1) || (rc_ref_list_find_ref_index(pending, *ref_ptr) != -1))
    {
      if (on_remove)
        on_remove(*ref_ptr, context);
      rc_ref_list_remove_ref(active, *ref_ptr);
      // In case it never made it to active
      rc_ref_list_remove_ref(pending, *ref_ptr);
    }
  }
  to_remove->num_refs = 0;
}

void rc_ref_lists_remove_all(RCRefLists* lists, RCRefFunction on_remove, const void* context)
{
  RDMNET_ASSERT(lists);

  RCRefList* pending = &lists->pending;
  RCRefList* active = &lists->active;

  rc_ref_lists_remove_marked(lists, on_remove, context);
  rc_ref_list_remove_all(pending, on_remove, context);
  rc_ref_list_remove_all(active, on_remove, context);
}

/*
 * A wrapper for the C library function strncpy() which truncates safely.
 *
 * Always puts a null character at destination[num - 1].
 * [out] destination Pointer to the destination array where the content is to be copied.
 * [in] source C string to be copied.
 * [in] num Maximum number of characters to be copied from source.
 * Returns destination.
 */
char* rdmnet_safe_strncpy(char* destination, const char* source, size_t num)
{
  if (!destination || num == 0)
    return NULL;

  ETCPAL_MSVC_NO_DEP_WRN strncpy(destination, source, num);
  destination[num - 1] = '\0';
  return destination;
}

int netint_id_index_in_mcast_array(const EtcPalMcastNetintId* id, const EtcPalMcastNetintId* array, size_t array_size)
{
  for (size_t i = 0; i < array_size; ++i)
  {
    if (array[i].index == id->index && array[i].ip_type == id->ip_type)
      return (int)i;
  }
  return -1;
}
