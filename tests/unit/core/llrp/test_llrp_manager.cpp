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

#include "rdmnet/core/llrp_manager.h"

#include "gtest/gtest.h"
#include "fff.h"
#include "etcpal/cpp/lock.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "rdm/cpp/uid.h"
#include "rdmnet_mock/core/common.h"
#include "rdmnet/core/mcast.h"
#include "fake_mcast.h"

extern "C" {
FAKE_VOID_FUNC(managercb_target_discovered, RCLlrpManager*, const LlrpDiscoveredTarget*);
FAKE_VOID_FUNC(managercb_rdm_response_received, RCLlrpManager*, const LlrpRdmResponse*);
FAKE_VOID_FUNC(managercb_discovery_finished, RCLlrpManager*);
FAKE_VOID_FUNC(managercb_destroyed, RCLlrpManager*);
}

class TestLlrpManager : public testing::Test
{
protected:
  RCLlrpManager manager_;
  etcpal::Mutex manager_lock_;

  void SetUp() override
  {
    RESET_FAKE(managercb_target_discovered);
    RESET_FAKE(managercb_rdm_response_received);
    RESET_FAKE(managercb_discovery_finished);

    rdmnet_mock_core_reset_and_init();
    etcpal_reset_all_fakes();
    SetUpFakeMcastEnvironment();

    manager_.cid = etcpal::Uuid::FromString("48eaee88-2d5e-43d4-b0e9-7a9d5977ae9d").get();
    manager_.uid = rdm::Uid::FromString("e574:a686dee7").get();
    manager_.netint.index = 1;
    manager_.netint.ip_type = kEtcPalIpTypeV4;
    manager_.callbacks.target_discovered = managercb_target_discovered;
    manager_.callbacks.rdm_response_received = managercb_rdm_response_received;
    manager_.callbacks.discovery_finished = managercb_discovery_finished;
    manager_.callbacks.destroyed = managercb_destroyed;
    manager_.lock = &manager_lock_.get();

    ASSERT_EQ(kEtcPalErrOk, rc_llrp_module_init());
    ASSERT_EQ(kEtcPalErrOk, rc_llrp_manager_module_init());
    ASSERT_EQ(kEtcPalErrOk, rc_llrp_manager_register(&manager_));
  }

  void TearDown() override
  {
    rc_llrp_manager_unregister(&manager_);
    rc_llrp_manager_module_deinit();
    rc_llrp_module_deinit();
  }
};

TEST_F(TestLlrpManager, DestroyedCalledOnUnregister)
{
  rc_llrp_manager_unregister(&manager_);
  rc_llrp_manager_module_tick();

  EXPECT_EQ(managercb_destroyed_fake.call_count, 1u);
  EXPECT_EQ(managercb_destroyed_fake.arg0_val, &manager_);
}
