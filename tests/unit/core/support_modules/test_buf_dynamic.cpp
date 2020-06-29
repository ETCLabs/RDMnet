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
#include <cstring>
#include "gtest/gtest.h"

typedef struct DummyStruct
{
  int a;
  int b;
} DummyStruct;

class TestRcBuf : public testing::Test
{
protected:
  static constexpr size_t kBufMaxStaticSize = 5;
  static constexpr size_t kInitialCapacity = 10;
  RC_DECLARE_BUF(DummyStruct, dummy_structs, kBufMaxStaticSize);

  void SetUp() override
  {
    ASSERT_TRUE(RC_INIT_BUF(this, DummyStruct, dummy_structs, kInitialCapacity, kBufMaxStaticSize));
  }

  void TearDown() override { RC_DEINIT_BUF(this, dummy_structs); }
};

TEST_F(TestRcBuf, InitWorks)
{
  EXPECT_NE(dummy_structs, nullptr);
  EXPECT_EQ(dummy_structs_capacity, kInitialCapacity);
  EXPECT_EQ(num_dummy_structs, 0u);

  // Allocated memory should be zeroed
  EXPECT_TRUE(std::all_of(dummy_structs, dummy_structs + dummy_structs_capacity,
                          [](const DummyStruct& ds) { return (ds.a == 0 && ds.b == 0); }));

  // Test that the buffer was actually allocated at the correct size - ASAN or similar should
  // catch an out-of-bounds error
  std::memset(dummy_structs, 0x33, sizeof(DummyStruct) * kInitialCapacity);
}

TEST_F(TestRcBuf, CheckCapacityZeroItems)
{
  // Calling CHECK_BUF_CAPACITY() with a value less than the current capacity, when there are zero
  // items, should return true and have no effect.
  auto old_ptr = dummy_structs;
  EXPECT_TRUE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, 1));
  EXPECT_EQ(old_ptr, dummy_structs);
  EXPECT_EQ(dummy_structs_capacity, kInitialCapacity);

  EXPECT_TRUE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, kInitialCapacity));
  EXPECT_EQ(old_ptr, dummy_structs);
  EXPECT_EQ(dummy_structs_capacity, kInitialCapacity);
}

TEST_F(TestRcBuf, CheckCapacityOneLessThan)
{
  num_dummy_structs = kInitialCapacity - 1;

  // Calling CHECK_BUF_CAPACITY() with one additional item when there is still room for it should
  // have no effect
  auto old_ptr = dummy_structs;
  EXPECT_TRUE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, 1));
  EXPECT_EQ(old_ptr, dummy_structs);
  EXPECT_EQ(dummy_structs_capacity, kInitialCapacity);
}

TEST_F(TestRcBuf, CheckCapacityOneAdditional)
{
  // Set sentinel values in the existing range
  std::for_each(dummy_structs, dummy_structs + dummy_structs_capacity, [](DummyStruct& ds) {
    ds.a = 42;
    ds.b = 43;
  });

  num_dummy_structs = kInitialCapacity;

  // Calling CHECK_BUF_CAPACITY() when there is no room for another item should result in a
  // reallocation
  EXPECT_TRUE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, 1));
  EXPECT_GT(dummy_structs_capacity, kInitialCapacity);

  // The initial part of the range should be unmodified
  EXPECT_TRUE(std::all_of(dummy_structs, dummy_structs + kInitialCapacity,
                          [](const DummyStruct& ds) { return (ds.a == 42 && ds.b == 43); }));

  // The new part of the range should be zeroed
  EXPECT_TRUE(std::all_of(&dummy_structs[kInitialCapacity], &dummy_structs[dummy_structs_capacity],
                          [](const DummyStruct& ds) { return (ds.a == 0 && ds.b == 0); }));

  // Test that the buffer was actually reallocated at the correct size - ASAN or similar should
  // catch an out-of-bounds error
  std::memset(dummy_structs, 0x33, sizeof(DummyStruct) * dummy_structs_capacity);
}

TEST_F(TestRcBuf, CheckCapacityMultipleAdditional)
{
  // Set sentinel values in the existing range
  std::for_each(dummy_structs, dummy_structs + dummy_structs_capacity, [](DummyStruct& ds) {
    ds.a = 42;
    ds.b = 43;
  });

  EXPECT_TRUE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, kInitialCapacity * 10));
  EXPECT_GE(dummy_structs_capacity, kInitialCapacity * 10);

  // The initial part of the range should be unmodified
  EXPECT_TRUE(std::all_of(dummy_structs, dummy_structs + kInitialCapacity,
                          [](const DummyStruct& ds) { return (ds.a == 42 && ds.b == 43); }));

  // The new part of the range should be zeroed
  EXPECT_TRUE(std::all_of(&dummy_structs[kInitialCapacity], &dummy_structs[dummy_structs_capacity],
                          [](const DummyStruct& ds) { return (ds.a == 0 && ds.b == 0); }));

  // Test that the buffer was actually reallocated at the correct size - ASAN or similar should
  // catch an out-of-bounds error
  std::memset(dummy_structs, 0x33, sizeof(DummyStruct) * dummy_structs_capacity);
}
