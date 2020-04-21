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

#include "rdmnet/llrp_target.h"

#include "gtest/gtest.h"
#include "fff.h"

FAKE_VOID_FUNC(handle_llrp_target_rdm_command_received, llrp_target_t, const LlrpRdmCommand*, RdmnetSyncRdmResponse*,
               void*);

class TestLlrpTargetApi : public testing::Test
{
protected:
  static constexpr uint16_t kTestManufId = 0x1234;

  void ResetLocalFakes() { RESET_FAKE(handle_llrp_target_rdm_command_received); }

  void SetUp() override
  {
    ResetLocalFakes();
    ASSERT_EQ(rdmnet_init(nullptr, nullptr), kEtcPalErrOk);
    config_.callbacks.rdm_command_received = handle_llrp_target_rdm_command_received;
  }

  void TearDown() override { rdmnet_deinit(); }

  LlrpTargetConfig config_ = LLRP_TARGET_CONFIG_DEFAULT_INIT(kTestManufId);
};

TEST_F(TestLlrpTargetApi, Placeholder)
{
  llrp_target_t handle;
  EXPECT_EQ(llrp_target_create(&config_, &handle), kEtcPalErrOk);
}
