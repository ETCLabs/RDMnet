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

#include "rdmnet/cpp/broker.h"

#include "gmock/gmock.h"
#include "rdmnet_mock/common.h"

class MockBrokerNotifyHander : public rdmnet::Broker::NotifyHandler
{
  MOCK_METHOD(void, HandleScopeChanged, (const std::string& new_scope), (override));
};

class TestBrokerApi : public testing::Test
{
  void SetUp() override
  {
    rdmnet_mock_common_reset();
    ASSERT_EQ(rdmnet::Init(), kEtcPalErrOk);
  }

  void TearDown() override { rdmnet::Deinit(); }
};

TEST_F(TestBrokerApi, Startup)
{
  MockBrokerNotifyHander notify;
  rdmnet::Broker         broker;

  EXPECT_TRUE(broker.Startup(rdmnet::Broker::Settings(etcpal::Uuid::OsPreferred(), 0x6574)));
}
