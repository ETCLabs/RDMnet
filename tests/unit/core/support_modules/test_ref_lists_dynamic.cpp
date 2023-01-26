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

#include <algorithm>
#include "gtest/gtest.h"
#include "fff.h"

extern "C" {
FAKE_VOID_FUNC(ref_function, void*, const void*);
FAKE_VALUE_FUNC(bool, ref_predicate, void*, const void*);
}

RC_DECLARE_REF_LISTS(test_refs, 20);

class TestRefLists : public testing::Test
{
protected:
  void SetUp() override
  {
    RESET_FAKE(ref_function);
    RESET_FAKE(ref_predicate);
    ASSERT_TRUE(rc_ref_lists_init(&test_refs));
  }
  void TearDown() override { rc_ref_lists_cleanup(&test_refs); }

  void AddRefsOneThroughThree(RCRefList* list)
  {
    ASSERT_TRUE(rc_ref_list_add_ref(list, reinterpret_cast<void*>(1)));
    ASSERT_TRUE(rc_ref_list_add_ref(list, reinterpret_cast<void*>(2)));
    ASSERT_TRUE(rc_ref_list_add_ref(list, reinterpret_cast<void*>(3)));
  }
};

TEST_F(TestRefLists, AddRefWorks)
{
  for (size_t i = 0; i < 50; ++i)
  {
    void* ref = reinterpret_cast<void*>(i + 1);
    ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.pending, ref)) << "Failed on iteration " << i;
  }

  EXPECT_EQ(test_refs.pending.num_refs, 50u);
  for (size_t i = 0; i < 50; ++i)
  {
    EXPECT_EQ(test_refs.pending.refs[i], reinterpret_cast<void*>(i + 1));
  }
}

TEST_F(TestRefLists, RemoveRefWorks)
{
  AddRefsOneThroughThree(&test_refs.pending);

  // Remove ref 2
  rc_ref_list_remove_ref(&test_refs.pending, reinterpret_cast<void*>(2));

  EXPECT_EQ(test_refs.pending.num_refs, 2u);
  EXPECT_EQ(test_refs.pending.refs[0], reinterpret_cast<void*>(1));
  EXPECT_EQ(test_refs.pending.refs[1], reinterpret_cast<void*>(3));
}

TEST_F(TestRefLists, FindRefIndexWhenElementExists)
{
  AddRefsOneThroughThree(&test_refs.active);

  EXPECT_EQ(rc_ref_list_find_ref_index(&test_refs.active, reinterpret_cast<void*>(3)), 2);
}

TEST_F(TestRefLists, FindRefIndexWhenElementDoesNotExist)
{
  AddRefsOneThroughThree(&test_refs.to_remove);

  EXPECT_EQ(rc_ref_list_find_ref_index(&test_refs.to_remove, reinterpret_cast<void*>(4)), -1);
}

TEST_F(TestRefLists, FindRefWhenElementExists)
{
#define FIND_REF_WORKS_REF_TO_FIND reinterpret_cast<void*>(2)
#define FIND_REF_WORKS_CONTEXT_PTR reinterpret_cast<const void*>(20)

  AddRefsOneThroughThree(&test_refs.pending);

  ref_predicate_fake.custom_fake = [](void* ref, const void* context) {
    EXPECT_EQ(context, FIND_REF_WORKS_CONTEXT_PTR);
    if (ref == FIND_REF_WORKS_REF_TO_FIND)
      return true;
    return false;
  };

  EXPECT_EQ(rc_ref_list_find_ref(&test_refs.pending, ref_predicate, FIND_REF_WORKS_CONTEXT_PTR),
            FIND_REF_WORKS_REF_TO_FIND);
}

TEST_F(TestRefLists, FindRefWhenElementDoesNotExist)
{
  AddRefsOneThroughThree(&test_refs.pending);

  ref_predicate_fake.custom_fake = [](void* /*ref*/, const void* context) {
    EXPECT_EQ(context, FIND_REF_WORKS_CONTEXT_PTR);
    return false;
  };

  EXPECT_EQ(rc_ref_list_find_ref(&test_refs.pending, ref_predicate, FIND_REF_WORKS_CONTEXT_PTR), nullptr);
}

TEST_F(TestRefLists, AddPendingRefsWorks)
{
  AddRefsOneThroughThree(&test_refs.pending);

  EXPECT_EQ(test_refs.pending.num_refs, 3u);

  rc_ref_lists_add_pending(&test_refs);

  EXPECT_EQ(test_refs.pending.num_refs, 0u);
  EXPECT_EQ(test_refs.active.num_refs, 3u);
  EXPECT_EQ(test_refs.active.refs[0], reinterpret_cast<void*>(1));
  EXPECT_EQ(test_refs.active.refs[1], reinterpret_cast<void*>(2));
  EXPECT_EQ(test_refs.active.refs[2], reinterpret_cast<void*>(3));
}

#define DESTROY_MARKED_REFS_CONTEXT_PTR reinterpret_cast<const void*>(30)

TEST_F(TestRefLists, RemoveMarkedRefFromOneElementPending)
{
  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.pending, reinterpret_cast<void*>(1)));
  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.to_remove, reinterpret_cast<void*>(1)));
  ASSERT_EQ(test_refs.to_remove.num_refs, 1u);

  rc_ref_lists_remove_marked(&test_refs, ref_function, DESTROY_MARKED_REFS_CONTEXT_PTR);
  EXPECT_EQ(ref_function_fake.call_count, 1u);
  EXPECT_EQ(ref_function_fake.arg0_val, reinterpret_cast<void*>(1));
  EXPECT_EQ(ref_function_fake.arg1_val, DESTROY_MARKED_REFS_CONTEXT_PTR);
  EXPECT_EQ(test_refs.pending.num_refs, 0u);
  EXPECT_EQ(test_refs.to_remove.num_refs, 0u);
}

TEST_F(TestRefLists, RemoveMarkedRefFromPending)
{
  AddRefsOneThroughThree(&test_refs.pending);

  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.to_remove, reinterpret_cast<void*>(2)));
  ASSERT_EQ(test_refs.to_remove.num_refs, 1u);

  rc_ref_lists_remove_marked(&test_refs, ref_function, DESTROY_MARKED_REFS_CONTEXT_PTR);
  EXPECT_EQ(ref_function_fake.call_count, 1u);
  EXPECT_EQ(ref_function_fake.arg0_val, reinterpret_cast<void*>(2));
  EXPECT_EQ(ref_function_fake.arg1_val, DESTROY_MARKED_REFS_CONTEXT_PTR);
  EXPECT_EQ(test_refs.pending.num_refs, 2u);
  EXPECT_EQ(test_refs.to_remove.num_refs, 0u);
}

TEST_F(TestRefLists, RemoveMarkedRefFromOneElementActive)
{
  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.active, reinterpret_cast<void*>(1)));
  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.to_remove, reinterpret_cast<void*>(1)));
  ASSERT_EQ(test_refs.to_remove.num_refs, 1u);

  rc_ref_lists_remove_marked(&test_refs, ref_function, DESTROY_MARKED_REFS_CONTEXT_PTR);
  EXPECT_EQ(ref_function_fake.call_count, 1u);
  EXPECT_EQ(ref_function_fake.arg0_val, reinterpret_cast<void*>(1));
  EXPECT_EQ(ref_function_fake.arg1_val, DESTROY_MARKED_REFS_CONTEXT_PTR);
  EXPECT_EQ(test_refs.active.num_refs, 0u);
  EXPECT_EQ(test_refs.to_remove.num_refs, 0u);
}

TEST_F(TestRefLists, RemoveMarkedRefFromActive)
{
  AddRefsOneThroughThree(&test_refs.active);

  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.to_remove, reinterpret_cast<void*>(2)));
  ASSERT_EQ(test_refs.to_remove.num_refs, 1u);

  rc_ref_lists_remove_marked(&test_refs, ref_function, DESTROY_MARKED_REFS_CONTEXT_PTR);
  EXPECT_EQ(ref_function_fake.call_count, 1u);
  EXPECT_EQ(ref_function_fake.arg0_val, reinterpret_cast<void*>(2));
  EXPECT_EQ(ref_function_fake.arg1_val, DESTROY_MARKED_REFS_CONTEXT_PTR);
  EXPECT_EQ(test_refs.active.num_refs, 2u);
  EXPECT_EQ(test_refs.to_remove.num_refs, 0u);
}

// rc_ref_lists_remove_marked() should not call the on_remove() callback when a ref was not present in
// the active or pending lists.
TEST_F(TestRefLists, RemoveMarkedRefWhenNotPendingOrActive)
{
  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.to_remove, reinterpret_cast<void*>(1)));

  rc_ref_lists_remove_marked(&test_refs, ref_function, nullptr);
  EXPECT_EQ(ref_function_fake.call_count, 0u);
  EXPECT_EQ(test_refs.to_remove.num_refs, 0u);
}

#define DESTROY_ALL_REFS_CONTEXT_PTR reinterpret_cast<const void*>(40)

TEST_F(TestRefLists, RemoveAllRefs)
{
  AddRefsOneThroughThree(&test_refs.active);
  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.pending, reinterpret_cast<void*>(4)));
  ASSERT_TRUE(rc_ref_list_add_ref(&test_refs.to_remove, reinterpret_cast<void*>(2)));

  rc_ref_lists_remove_all(&test_refs, ref_function, DESTROY_ALL_REFS_CONTEXT_PTR);

  // Expect the destroy function to have been called exactly 4 times, once with each ref, and each
  // time with the context pointer given.
  EXPECT_EQ(ref_function_fake.call_count, 4u);
  EXPECT_NE(
      std::find(&ref_function_fake.arg0_history[0], &ref_function_fake.arg0_history[3], reinterpret_cast<void*>(1)),
      &ref_function_fake.arg0_history[4]);
  EXPECT_NE(
      std::find(&ref_function_fake.arg0_history[0], &ref_function_fake.arg0_history[3], reinterpret_cast<void*>(2)),
      &ref_function_fake.arg0_history[4]);
  EXPECT_NE(
      std::find(&ref_function_fake.arg0_history[0], &ref_function_fake.arg0_history[4], reinterpret_cast<void*>(3)),
      &ref_function_fake.arg0_history[4]);
  EXPECT_NE(
      std::find(&ref_function_fake.arg0_history[0], &ref_function_fake.arg0_history[4], reinterpret_cast<void*>(4)),
      &ref_function_fake.arg0_history[4]);
  EXPECT_TRUE(std::all_of(&ref_function_fake.arg1_history[0], &ref_function_fake.arg1_history[3],
                          [](const void* context) { return (context == DESTROY_ALL_REFS_CONTEXT_PTR); }));

  // Expect no more refs to be present in the lists.
  EXPECT_EQ(test_refs.active.num_refs, 0u);
  EXPECT_EQ(test_refs.pending.num_refs, 0u);
  EXPECT_EQ(test_refs.to_remove.num_refs, 0u);
}

#define FOR_EACH_REF_CONTEXT_PTR reinterpret_cast<const void*>(50)

TEST_F(TestRefLists, ForEachRef)
{
  AddRefsOneThroughThree(&test_refs.pending);
  rc_ref_list_for_each(&test_refs.pending, ref_function, FOR_EACH_REF_CONTEXT_PTR);

  EXPECT_EQ(ref_function_fake.call_count, 3u);
  EXPECT_EQ(ref_function_fake.arg0_history[0], reinterpret_cast<void*>(1));
  EXPECT_EQ(ref_function_fake.arg0_history[1], reinterpret_cast<void*>(2));
  EXPECT_EQ(ref_function_fake.arg0_history[2], reinterpret_cast<void*>(3));
  EXPECT_TRUE(std::all_of(&ref_function_fake.arg1_history[0], &ref_function_fake.arg1_history[3],
                          [](const void* context) { return (context == FOR_EACH_REF_CONTEXT_PTR); }));
}
