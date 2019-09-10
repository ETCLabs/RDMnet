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

#include "gtest/gtest.h"
#include "rdmnet/core/llrp_target.h"

class TestLlrpTarget : public testing::Test
{
};

// Test the macros that init LlrpTargetConfig structures.
TEST_F(TestLlrpTarget, init_macros)
{
  LlrpTargetConfig test_config;

  LLRP_TARGET_CONFIG_INIT(&test_config, 0x1234);
  EXPECT_EQ(test_config.optional.netint_arr, nullptr);
  EXPECT_EQ(test_config.optional.num_netints, 0u);
  EXPECT_TRUE(RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&test_config.optional.uid));
  EXPECT_EQ(RDM_GET_MANUFACTURER_ID(&test_config.optional.uid), 0x1234u);
}
