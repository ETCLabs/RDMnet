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

#include "rdmnet/core/llrp_target.h"

#include "gtest/gtest.h"
#include "fff.h"
#include "etcpal/cpp/mutex.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/core/mcast.h"
#include "rdmnet_mock/core/common.h"
#include "fake_mcast.h"

extern "C" {
FAKE_VOID_FUNC(targetcb_rdm_cmd_received, RCLlrpTarget*, const LlrpRdmCommand*, RCLlrpTargetSyncRdmResponse*);
FAKE_VOID_FUNC(targetcb_destroyed, RCLlrpTarget*);
}

class TestLlrpTarget : public testing::Test
{
protected:
  RCLlrpTarget  target_;
  etcpal::Mutex target_lock_;

  void SetUp() override
  {
    RESET_FAKE(targetcb_rdm_cmd_received);

    rdmnet_mock_core_reset_and_init();
    etcpal_reset_all_fakes();
    SetUpFakeMcastEnvironment();

    target_.cid = etcpal::Uuid::FromString("28e04e4a-9eda-44d1-b4f8-56af772ca4c9").get();
    target_.uid = rdm::Uid::FromString("6574:60313950").get();
    target_.lock = &target_lock_.get();
    target_.component_type = kLlrpCompRptDevice;
    target_.callbacks.rdm_command_received = targetcb_rdm_cmd_received;
    target_.callbacks.destroyed = targetcb_destroyed;

    ASSERT_EQ(kEtcPalErrOk, rc_llrp_module_init(nullptr));
    ASSERT_EQ(kEtcPalErrOk, rc_llrp_target_module_init(nullptr));
    ASSERT_EQ(kEtcPalErrOk, rc_llrp_target_register(&target_));
  }

  void TearDown() override
  {
    rc_llrp_target_unregister(&target_);
    rc_llrp_target_module_deinit();
    rc_llrp_module_deinit();
  }
};

TEST_F(TestLlrpTarget, DestroyedCalledOnUnregister)
{
  rc_llrp_target_unregister(&target_);
  rc_llrp_target_module_tick();

  EXPECT_EQ(targetcb_destroyed_fake.call_count, 1u);
  EXPECT_EQ(targetcb_destroyed_fake.arg0_val, &target_);
}
