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
  RC_DECLARE_BUF(DummyStruct, dummy_structs, kBufMaxStaticSize);

  void SetUp() override { ASSERT_TRUE(RC_INIT_BUF(this, DummyStruct, dummy_structs, 10, kBufMaxStaticSize)); }

  void TearDown() override { RC_DEINIT_BUF(this, dummy_structs); }
};

TEST_F(TestRcBuf, InitWorks)
{
  EXPECT_EQ(num_dummy_structs, 0u);

  // Buffer should be zeroed
  EXPECT_TRUE(std::all_of(dummy_structs, dummy_structs + kBufMaxStaticSize,
                          [](const DummyStruct& ds) { return (ds.a == 0 && ds.b == 0); }));

  // Test that the buffer was actually declared at the correct size - ASAN or similar should
  // catch an out-of-bounds error
  std::memset(dummy_structs, 0x33, sizeof(DummyStruct) * kBufMaxStaticSize);
}

TEST_F(TestRcBuf, CheckCapacityZeroItems)
{
  EXPECT_TRUE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, 1));
  EXPECT_TRUE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, kBufMaxStaticSize));
  EXPECT_FALSE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, kBufMaxStaticSize + 1));
}

TEST_F(TestRcBuf, CheckCapacityOneLessThan)
{
  num_dummy_structs = kBufMaxStaticSize - 1;

  EXPECT_TRUE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, 1));
  EXPECT_FALSE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, 2));
}

TEST_F(TestRcBuf, CheckCapacityOneAdditional)
{
  // Set sentinel values in the existing range
  std::for_each(dummy_structs, dummy_structs + kBufMaxStaticSize, [](DummyStruct& ds) {
    ds.a = 42;
    ds.b = 43;
  });

  num_dummy_structs = kBufMaxStaticSize;

  // Calling CHECK_BUF_CAPACITY() when there is no room for another item should return false
  EXPECT_FALSE(RC_CHECK_BUF_CAPACITY(this, DummyStruct, dummy_structs, kBufMaxStaticSize, 1));

  // The range should be unmodified
  EXPECT_TRUE(std::all_of(dummy_structs, dummy_structs + kBufMaxStaticSize,
                          [](const DummyStruct& ds) { return (ds.a == 42 && ds.b == 43); }));
}
