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

#include "rdmnet/cpp/device.h"

#include "gmock/gmock.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet_mock/common.h"
#include "rdmnet_mock/device.h"

class MockDeviceNotifyHandler : public rdmnet::Device::NotifyHandler
{
  MOCK_METHOD(void,
              HandleConnectedToBroker,
              (rdmnet::Device::Handle handle, const rdmnet::ClientConnectedInfo& info),
              (override));
  MOCK_METHOD(void,
              HandleBrokerConnectFailed,
              (rdmnet::Device::Handle handle, const rdmnet::ClientConnectFailedInfo& info),
              (override));
  MOCK_METHOD(void,
              HandleDisconnectedFromBroker,
              (rdmnet::Device::Handle handle, const rdmnet::ClientDisconnectedInfo& info),
              (override));
  MOCK_METHOD(rdmnet::RdmResponseAction,
              HandleRdmCommand,
              (rdmnet::Device::Handle handle, const rdmnet::RdmCommand& command),
              (override));
  MOCK_METHOD(rdmnet::RdmResponseAction,
              HandleLlrpRdmCommand,
              (rdmnet::Device::Handle handle, const rdmnet::llrp::RdmCommand& cmd),
              (override));
};

class TestCppDeviceApi : public testing::Test
{
protected:
  rdmnet::Device          device_;
  MockDeviceNotifyHandler notify_;

  void SetUp() override
  {
    rdmnet_mock_common_reset();
    rdmnet_device_reset_all_fakes();
    ASSERT_EQ(rdmnet::Init(), kEtcPalErrOk);
  }

  void TearDown() override { rdmnet::Deinit(); }
};

TEST_F(TestCppDeviceApi, InitialEndpointsAreTranslated)
{
  static const auto virtual_endpoint_responder = etcpal::Uuid::FromString("7f94c037-dbb2-44b6-ad68-9fe3159f1699");
  static const auto virtual_endpoint_static_responder = rdm::Uid::FromString("6574:12345678");
  static const auto physical_endpoint_responder = rdmnet::PhysicalEndpointResponder(
      rdm::Uid::FromString("6574:87654321"), 0x8, rdm::Uid::FromString("6574:0000001"));

  rdmnet::Device::Settings settings(etcpal::Uuid::OsPreferred(), 0x6574);
  settings.virtual_endpoints.push_back(1);
  settings.virtual_endpoints.push_back(
      rdmnet::VirtualEndpointConfig(2, {virtual_endpoint_static_responder}, {virtual_endpoint_responder}));
  settings.physical_endpoints.push_back(3);
  settings.physical_endpoints.push_back(rdmnet::PhysicalEndpointConfig(4, {physical_endpoint_responder}));

  rdmnet_device_create_fake.custom_fake = [](const RdmnetDeviceConfig* config, rdmnet_device_t* handle) {
    EXPECT_EQ(config->num_virtual_endpoints, 2u);
    EXPECT_EQ(config->num_physical_endpoints, 2u);

    EXPECT_EQ(config->virtual_endpoints[0].endpoint_id, 1u);
    EXPECT_EQ(config->virtual_endpoints[0].dynamic_responders, nullptr);
    EXPECT_EQ(config->virtual_endpoints[0].num_dynamic_responders, 0u);
    EXPECT_EQ(config->virtual_endpoints[0].static_responders, nullptr);
    EXPECT_EQ(config->virtual_endpoints[0].num_static_responders, 0u);

    EXPECT_EQ(config->virtual_endpoints[1].endpoint_id, 2u);
    EXPECT_EQ(config->virtual_endpoints[1].num_dynamic_responders, 1u);
    EXPECT_EQ(config->virtual_endpoints[1].dynamic_responders[0], virtual_endpoint_responder);
    EXPECT_EQ(config->virtual_endpoints[1].num_static_responders, 1u);
    EXPECT_EQ(config->virtual_endpoints[1].static_responders[0], virtual_endpoint_static_responder);

    EXPECT_EQ(config->physical_endpoints[0].endpoint_id, 3u);
    EXPECT_EQ(config->physical_endpoints[0].responders, nullptr);
    EXPECT_EQ(config->physical_endpoints[0].num_responders, 0u);

    EXPECT_EQ(config->physical_endpoints[1].endpoint_id, 4u);
    EXPECT_EQ(config->physical_endpoints[1].num_responders, 1u);
    EXPECT_EQ(config->physical_endpoints[1].responders[0].uid, physical_endpoint_responder.get().uid);
    EXPECT_EQ(config->physical_endpoints[1].responders[0].control_field, physical_endpoint_responder.get().control_field);
    EXPECT_EQ(config->physical_endpoints[1].responders[0].binding_uid, physical_endpoint_responder.get().binding_uid);

    *handle = 0;
    return kEtcPalErrOk;
  };

  ASSERT_TRUE(device_.Startup(notify_, settings, "default"));
  EXPECT_EQ(rdmnet_device_create_fake.call_count, 1u);
}

TEST_F(TestCppDeviceApi, AddVirtualEndpoint)
{
  ASSERT_TRUE(device_.Startup(notify_, rdmnet::Device::Settings(etcpal::Uuid::OsPreferred(), 0x6574), "default"));
  EXPECT_EQ(device_.AddVirtualEndpoint(1), kEtcPalErrOk);
}
