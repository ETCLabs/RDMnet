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

#include "rdmnet/llrp_manager.h"

#include "etcpal/cpp/uuid.h"
#include "rdmnet_mock/core/common.h"
#include "rdmnet_mock/core/llrp_manager.h"
#include "gtest/gtest.h"
#include "fff.h"

FAKE_VOID_FUNC(handle_llrp_manager_target_discovered, llrp_manager_t, const LlrpDiscoveredTarget*, void*);
FAKE_VOID_FUNC(handle_llrp_manager_rdm_response_received, llrp_manager_t, const LlrpRdmResponse*, void*);
FAKE_VOID_FUNC(handle_llrp_manager_discovery_finished, llrp_manager_t, void*);

class TestLlrpManagerApi;

static TestLlrpManagerApi* current_test_fixture{nullptr};

class TestLlrpManagerApi : public testing::Test
{
public:
  LlrpManagerConfig config_ = LLRP_MANAGER_CONFIG_DEFAULT_INIT;

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
    current_test_fixture = this;

    ResetLocalFakes();
    rdmnet_mock_core_reset();
    ASSERT_EQ(rdmnet_init(nullptr, nullptr), kEtcPalErrOk);

    config_.manu_id = 0x6574;
    config_.netint.index = 1;
    config_.netint.ip_type = kEtcPalIpTypeV4;
    config_.cid = etcpal::Uuid::FromString("69c437e5-936e-4a6d-8d75-0a35512a0277").get();
    llrp_manager_config_set_callbacks(&config_, handle_llrp_manager_target_discovered,
                                      handle_llrp_manager_rdm_response_received, handle_llrp_manager_discovery_finished,
                                      nullptr);
  }

  void TearDown() override
  {
    rdmnet_deinit();
    current_test_fixture = nullptr;
  }
};

TEST_F(TestLlrpManagerApi, CreateRegistersManagerCorrectly)
{
  rc_llrp_manager_register_fake.custom_fake = [](RCLlrpManager* manager) {
    EXPECT_NE(manager, nullptr);
    EXPECT_NE(manager->lock, nullptr);
    EXPECT_EQ(manager->cid, current_test_fixture->config_.cid);
    EXPECT_EQ(manager->netint.index, current_test_fixture->config_.netint.index);
    EXPECT_EQ(manager->netint.ip_type, current_test_fixture->config_.netint.ip_type);
    EXPECT_EQ(RDM_GET_MANUFACTURER_ID(&manager->uid), current_test_fixture->config_.manu_id);
    EXPECT_NE(manager->callbacks.rdm_response_received, nullptr);
    EXPECT_NE(manager->callbacks.discovery_finished, nullptr);
    EXPECT_NE(manager->callbacks.target_discovered, nullptr);
    EXPECT_NE(manager->callbacks.destroyed, nullptr);

    return kEtcPalErrOk;
  };

  llrp_manager_t handle;
  EXPECT_EQ(llrp_manager_create(&config_, &handle), kEtcPalErrOk);
  EXPECT_EQ(rc_llrp_manager_register_fake.call_count, 1u);
}
