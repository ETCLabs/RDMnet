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

#include "rdmnet/cpp/device.h"

#include "gmock/gmock.h"
#include "rdmnet_mock/common.h"

class MockDeviceNotifyHandler : public rdmnet::DeviceNotifyHandler
{
  MOCK_METHOD(void, HandleConnectedToBroker, (rdmnet::DeviceHandle handle, const RdmnetClientConnectedInfo& info),
              (override));
  MOCK_METHOD(void, HandleBrokerConnectFailed, (rdmnet::DeviceHandle handle, const RdmnetClientConnectFailedInfo& info),
              (override));
  MOCK_METHOD(void, HandleDisconnectedFromBroker,
              (rdmnet::DeviceHandle handle, const RdmnetClientDisconnectedInfo& info), (override));
  MOCK_METHOD(rdmnet::ResponseAction, HandleRdmCommand,
              (rdmnet::DeviceHandle handle, const rdmnet::RdmCommand& command), (override));
  MOCK_METHOD(rdmnet::ResponseAction, HandleLlrpRdmCommand,
              (rdmnet::DeviceHandle handle, const rdmnet::llrp::RdmCommand& cmd), (override));
};

class TestCppDeviceApi : public testing::Test
{
protected:
  void SetUp() override
  {
    rdmnet_mock_common_reset();
    ASSERT_EQ(rdmnet::Init(), kEtcPalErrOk);
  }

  void TearDown() override { rdmnet::Deinit(); }
};

TEST_F(TestCppDeviceApi, AddVirtualEndpoint)
{
  rdmnet::Device device;
  MockDeviceNotifyHandler notify;

  device.Startup(notify, rdmnet::DeviceSettings(etcpal::Uuid::OsPreferred(), 0x6574), "default");
  EXPECT_EQ(device.AddVirtualEndpoint(1), kEtcPalErrOk);
}
