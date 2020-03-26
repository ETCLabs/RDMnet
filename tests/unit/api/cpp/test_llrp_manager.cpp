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

#include "rdmnet/cpp/llrp_manager.h"

#include "gmock/gmock.h"
#include "rdmnet_mock/common.h"

namespace llrp = rdmnet::llrp;

class MockLlrpManagerNotifyHandler : public llrp::ManagerNotifyHandler
{
  MOCK_METHOD(void, HandleLlrpTargetDiscovered, (llrp::ManagerHandle handle, const llrp::DiscoveredTarget& target),
              (override));
  MOCK_METHOD(void, HandleLlrpDiscoveryFinished, (llrp::ManagerHandle handle), (override));
  MOCK_METHOD(void, HandleLlrpRdmResponse, (llrp::ManagerHandle handle, const llrp::RdmResponse& resp), (override));
};

class TestCppLlrpManagerApi : public testing::Test
{
protected:
  void SetUp() override
  {
    rdmnet_mock_common_reset();
    ASSERT_EQ(rdmnet::Init(), kEtcPalErrOk);
  }

  void TearDown() override { rdmnet::Deinit(); }
};

TEST_F(TestCppLlrpManagerApi, Startup)
{
  MockLlrpManagerNotifyHandler notify;
  llrp::Manager manager;

  EXPECT_EQ(manager.Startup(notify, 0x6574, 1), kEtcPalErrOk);
}
