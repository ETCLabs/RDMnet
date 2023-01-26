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

/**
 * @file rdmnet/core/util.h
 * @brief Utilities used throughout the RDMnet library.
 */

#ifndef RDMNET_CORE_UTIL_H_
#define RDMNET_CORE_UTIL_H_

#include <stddef.h>
#include <stdbool.h>
#include "rdmnet/core/opts.h"
#include "etcpal/netint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * Memory management utilities
 *************************************************************************************************/

/*
 * The RC_DECLARE_BUF() macro declares one of two different types of contiguous arrays, depending
 * on the value of RDMNET_DYNAMIC_MEM.
 *
 * Given an invocation with type Foo, name foo, max_static_size 10:
 *
 * - If RDMNET_DYNAMIC_MEM=1, it will make the declaration:
 *
 *   Foo *foo;
 *   size_t foo_capacity;
 *   size_t num_foo;
 *
 *   max_static_size will be ignored. This pointer must then be initialized using malloc() and the
 *   capacity member is used to track how long the malloc'd array is.
 *
 * - If RDMNET_DYNAMIC_MEM=0, it will make the declaration:
 *
 *   Foo foo[10];
 *   size_t num_foo;
 */
#if RDMNET_DYNAMIC_MEM

#define RC_DECLARE_BUF(type, name, max_static_size) \
  type*  name;                                      \
  size_t name##_capacity;                           \
  size_t num_##name

#define RC_INIT_BUF(containing_struct_ptr, type, name, initial_capacity, max_static_size)         \
  (RDMNET_ASSERT_VERIFY(containing_struct_ptr) &&                                                 \
   rc_init_buf((void**)&(containing_struct_ptr)->name, &(containing_struct_ptr)->name##_capacity, \
               &(containing_struct_ptr)->num_##name, sizeof(type), initial_capacity))

#define RC_DEINIT_BUF(containing_struct_ptr, name)                                  \
  if (RDMNET_ASSERT_VERIFY(containing_struct_ptr) && (containing_struct_ptr)->name) \
  {                                                                                 \
    free((containing_struct_ptr)->name);                                            \
  }

#define RC_CHECK_BUF_CAPACITY(containing_struct_ptr, type, name, max_static_size, num_additional)           \
  (RDMNET_ASSERT_VERIFY(containing_struct_ptr) &&                                                           \
   rc_check_buf_capacity((void**)&(containing_struct_ptr)->name, &(containing_struct_ptr)->name##_capacity, \
                         (containing_struct_ptr)->num_##name, sizeof(type), (num_additional)))

bool rc_check_buf_capacity(void**  buf,
                           size_t* buf_capacity,
                           size_t  current_num,
                           size_t  elem_size,
                           size_t  num_additional);
bool rc_init_buf(void** buf, size_t* buf_capacity, size_t* current_num, size_t elem_size, size_t initial_capacity);

#else  // RDMNET_DYNAMIC_MEM

#define RC_DECLARE_BUF(type, name, max_static_size) \
  type   name[max_static_size];                     \
  size_t num_##name

#define RC_INIT_BUF(containing_struct_ptr, type, name, initial_capacity, max_static_size)  \
  (RDMNET_ASSERT_VERIFY(containing_struct_ptr) &&                                          \
   rc_init_buf((void*)(containing_struct_ptr)->name, &(containing_struct_ptr)->num_##name, \
               (sizeof(type) * max_static_size)))

#define RC_DEINIT_BUF(containing_struct_ptr, name)

#define RC_CHECK_BUF_CAPACITY(containing_struct_ptr, type, name, max_static_size, num_additional) \
  (RDMNET_ASSERT_VERIFY(containing_struct_ptr) &&                                                 \
   ((containing_struct_ptr)->num_##name + (num_additional) <= (max_static_size)))

bool rc_init_buf(void* buf, size_t* current_num, size_t buf_size_in_bytes);

#endif

/*
 * An RCRefList is a contiguous array of pointers to objects. RDMnet core library modules use this
 * to save references to different pieces of RDMnet functionality associated with clients and
 * process their state periodically.
 *
 * Depending on whether RDMNET_DYNAMIC_MEM=1, an RCRefList either grows dynamically or has a static
 * limit that is passed to the RC_DECLARE_REF_LIST() macro.
 *
 * The RCRefLists structure contains three ref lists labeled "pending", "active" and "to_remove".
 * This helps simplify the locking and socket management in the RDMnet core library. The active
 * list is only touched from the background tick thread; the pending and to_remove lists are used
 * to mark new resources and ones that should be cleaned up. The tick thread then adds new
 * resources to the active list and cleans up the ones marked for removal on its next iteration.
 *
 * This means that no locks need to be held when delivering notification callbacks from the tick
 * thread; the tick thread is the only context from which a reference can be removed, so no race
 * conditions regarding lifetime are possible. Also, sockets are only read and closed from the tick
 * thread, which works around thread-safety issues regarding sockets on certain embedded platforms.
 */

typedef struct RCRefList
{
#if RDMNET_DYNAMIC_MEM
  void** refs;
  size_t refs_capacity;
#else
  void** const refs;
  const size_t refs_capacity;
#endif
  size_t num_refs;
} RCRefList;

typedef struct RCRefLists
{
  RCRefList active;
  RCRefList pending;
  RCRefList to_remove;
} RCRefLists;

#if RDMNET_DYNAMIC_MEM
#define RC_DECLARE_REF_LIST(name, max_static) static RCRefList name
#define RC_DECLARE_REF_LISTS(name, max_static) static RCRefLists name
#else
#define RC_DECLARE_REF_LIST(name, max_static)  \
  static void*     name##_ref_buf[max_static]; \
  static RCRefList name = {name##_ref_buf, max_static, 0}
#define RC_DECLARE_REF_LISTS(name, max_static)        \
  static void*      name##_active_buf[max_static];    \
  static void*      name##_pending_buf[max_static];   \
  static void*      name##_to_remove_buf[max_static]; \
  static RCRefLists name = {                          \
      {name##_active_buf, max_static, 0}, {name##_pending_buf, max_static, 0}, {name##_to_remove_buf, max_static, 0}}
#endif

typedef void (*RCRefFunction)(void* ref, const void* context);
typedef bool (*RCRefPredicate)(void* ref, const void* context);

// Individual list functions
bool  rc_ref_list_init(RCRefList* list);
void  rc_ref_list_cleanup(RCRefList* list);
bool  rc_ref_list_add_ref(RCRefList* list, void* to_add);
void  rc_ref_list_remove_ref(RCRefList* list, const void* to_remove);
void  rc_ref_list_remove_all(RCRefList* list, RCRefFunction on_remove, const void* context);
int   rc_ref_list_find_ref_index(RCRefList* list, const void* to_find);
void* rc_ref_list_find_ref(const RCRefList* list, RCRefPredicate predicate, const void* context);
void  rc_ref_list_for_each(RCRefList* list, RCRefFunction fn, const void* context);

// Combined lists functions
bool rc_ref_lists_init(RCRefLists* lists);
void rc_ref_lists_cleanup(RCRefLists* lists);
void rc_ref_lists_add_pending(RCRefLists* lists);
void rc_ref_lists_remove_marked(RCRefLists* lists, RCRefFunction on_remove, const void* context);
void rc_ref_lists_remove_all(RCRefLists* lists, RCRefFunction on_remove, const void* context);

char* rdmnet_safe_strncpy(char* destination, const char* source, size_t num);

int netint_id_index_in_mcast_array(const EtcPalMcastNetintId* id, const EtcPalMcastNetintId* array, size_t array_size);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_UTIL_H_ */
