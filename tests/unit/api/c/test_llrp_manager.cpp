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

#include "rdmnet/llrp_manager.h"

#include "gtest/gtest.h"
#include "fff.h"

FAKE_VOID_FUNC(handle_llrp_manager_target_discovered, llrp_manager_t, const LlrpDiscoveredTarget*, void*);
FAKE_VOID_FUNC(handle_llrp_manager_rdm_response_received, llrp_manager_t, const LlrpRdmResponse*, void*);
FAKE_VOID_FUNC(handle_llrp_manager_discovery_finished, llrp_manager_t, void*);

class TestLlrpManagerApi : public testing::Test
{
protected:
  static constexpr uint16_t kTestManufId = 0x1234;

  void ResetLocalFakes()
  {
    RESET_FAKE(handle_llrp_manager_target_discovered);
    RESET_FAKE(handle_llrp_manager_rdm_response_received);
    RESET_FAKE(handle_llrp_manager_discovery_finished);
  }

  void SetUp() override
  {
    ResetLocalFakes();
    ASSERT_EQ(rdmnet_init(nullptr, nullptr), kEtcPalErrOk);

    llrp_manager_config_set_callbacks(&config_, handle_llrp_manager_target_discovered,
                                      handle_llrp_manager_rdm_response_received, handle_llrp_manager_discovery_finished,
                                      nullptr);
  }

  void TearDown() override { rdmnet_deinit(); }

  LlrpManagerConfig config_ = LLRP_MANAGER_CONFIG_DEFAULT_INIT;
};

TEST_F(TestLlrpManagerApi, Placeholder)
{
  llrp_manager_t handle;
  EXPECT_EQ(llrp_manager_create(&config_, &handle), kEtcPalErrOk);
}
