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

#include "rdmnet/device.h"

#include <array>
#include "etcpal/cpp/uuid.h"
#include "rdmnet_mock/core/common.h"
#include "rdmnet_mock/core/client.h"
#include "gtest/gtest.h"
#include "fff.h"

FAKE_VOID_FUNC(handle_device_connected, rdmnet_device_t, const RdmnetClientConnectedInfo*, void*);
FAKE_VOID_FUNC(handle_device_connect_failed, rdmnet_device_t, const RdmnetClientConnectFailedInfo*, void*);
FAKE_VOID_FUNC(handle_device_disconnected, rdmnet_device_t, const RdmnetClientDisconnectedInfo*, void*);
FAKE_VOID_FUNC(handle_device_rdm_command_received,
               rdmnet_device_t,
               const RdmnetRdmCommand*,
               RdmnetSyncRdmResponse*,
               void*);
FAKE_VOID_FUNC(handle_device_llrp_rdm_command_received,
               rdmnet_device_t,
               const LlrpRdmCommand*,
               RdmnetSyncRdmResponse*,
               void*);
FAKE_VOID_FUNC(handle_device_dynamic_uid_status, rdmnet_device_t, const RdmnetDynamicUidAssignmentList*, void*);

class TestDeviceApi;

static TestDeviceApi* current_test_fixture{nullptr};

class TestDeviceApi : public testing::Test
{
public:
  RdmnetDeviceConfig config = RDMNET_DEVICE_CONFIG_DEFAULT_INIT(kTestManufId);

protected:
  static constexpr uint16_t kTestManufId = 0x1234;
  rdmnet_device_t           default_device_handle_{RDMNET_DEVICE_INVALID};

  void ResetLocalFakes()
  {
    RESET_FAKE(handle_device_connected);
    RESET_FAKE(handle_device_connect_failed);
    RESET_FAKE(handle_device_disconnected);
    RESET_FAKE(handle_device_rdm_command_received);
    RESET_FAKE(handle_device_llrp_rdm_command_received);
    RESET_FAKE(handle_device_dynamic_uid_status);
  }

  void SetUp() override
  {
    current_test_fixture = this;

    ResetLocalFakes();
    rdmnet_mock_core_reset();
    ASSERT_EQ(rdmnet_init(nullptr, nullptr), kEtcPalErrOk);

    config.cid = etcpal::Uuid::FromString("cef3f6dc-c42d-4f39-884e-ee106029dbb8").get();
    rdmnet_device_set_callbacks(&config, handle_device_connected, handle_device_connect_failed,
                                handle_device_disconnected, handle_device_rdm_command_received,
                                handle_device_llrp_rdm_command_received, handle_device_dynamic_uid_status, nullptr);
  }

  void TearDown() override
  {
    rdmnet_deinit();
    current_test_fixture = nullptr;
  }

  void CreateDeviceWithDefaultConfig()
  {
    ASSERT_EQ(rdmnet_device_create(&config, &default_device_handle_), kEtcPalErrOk);
  }
};

TEST_F(TestDeviceApi, CreateWorksWithValidConfig)
{
  CreateDeviceWithDefaultConfig();
}

TEST_F(TestDeviceApi, CreateRegistersClientCorrectly)
{
  rc_rpt_client_register_fake.custom_fake = [](RCClient* client, bool create_llrp_target,
                                               const RdmnetMcastNetintId* llrp_netints, size_t num_llrp_netints) {
    EXPECT_NE(client->lock, nullptr);
    EXPECT_EQ(client->type, kClientProtocolRPT);
    EXPECT_EQ(client->cid, current_test_fixture->config.cid);
    EXPECT_EQ(RC_RPT_CLIENT_DATA(client)->type, kRPTClientTypeDevice);
    EXPECT_EQ(RC_RPT_CLIENT_DATA(client)->uid, current_test_fixture->config.uid);
    if (current_test_fixture->config.search_domain)
      EXPECT_STREQ(client->search_domain, current_test_fixture->config.search_domain);
    else
      EXPECT_STREQ(client->search_domain, "");
    EXPECT_EQ(client->sync_resp_buf, current_test_fixture->config.response_buf);

    EXPECT_TRUE(create_llrp_target);
    EXPECT_EQ(llrp_netints, nullptr);
    EXPECT_EQ(num_llrp_netints, 0u);

    return kEtcPalErrOk;
  };

  CreateDeviceWithDefaultConfig();
  EXPECT_EQ(rc_rpt_client_register_fake.call_count, 1u);
}

// clang-format off
const std::array<RdmnetPhysicalEndpointResponder, 2> kTestPhysEndpt2Responders = {
  {
    {
      { 0x6574, 0x1234 },
      0,
      {}
    },
    {
      { 0x6574, 0x4321 },
      0,
      {}
    }
  }
};
std::array<RdmnetPhysicalEndpointConfig, 2> kTestPhysEndptConfigs = {
  {
    {
      1,
      nullptr,
      0
    },
    {
      2,
      kTestPhysEndpt2Responders.data(),
      kTestPhysEndpt2Responders.size()
    }
  }
};
// clang-format on

TEST_F(TestDeviceApi, AddValidPhysicalEndpointWorks)
{
  CreateDeviceWithDefaultConfig();

  EXPECT_EQ(rdmnet_device_add_physical_endpoint(default_device_handle_, &kTestPhysEndptConfigs[1]), kEtcPalErrOk);
}

TEST_F(TestDeviceApi, AddValidPhysicalEndpointsWorks)
{
  CreateDeviceWithDefaultConfig();

  EXPECT_EQ(rdmnet_device_add_physical_endpoints(default_device_handle_, kTestPhysEndptConfigs.data(),
                                                 kTestPhysEndptConfigs.size()),
            kEtcPalErrOk);
}

// clang-format off
const std::array<EtcPalUuid, 2> kTestVirtualEndpt1Responders = {
  {
    {
      { 0xb0, 0x21, 0x28, 0x29, 0x01, 0x3f, 0x43, 0xf0, 0x8c, 0x49, 0x35, 0x4b, 0x95, 0x4f, 0xda, 0xfc }
    },
    {
      { 0x52, 0x8d, 0xfa, 0x20, 0x3b, 0x46, 0x4e, 0x8b, 0xbc, 0xf5, 0x6b, 0xee, 0x9a, 0xe1, 0xa1, 0x35 }
    }
  }
};
const std::array<RdmUid, 2> kTestVirtualEndpt2Responders = {
  {
    { 0x6574, 0x1234 },
    { 0x6574, 0x4321 }
  }
};
const std::array<RdmnetVirtualEndpointConfig, 2> kTestVirtualEndpointConfigs = {
  {
    {
      1,
      kTestVirtualEndpt1Responders.data(),
      kTestVirtualEndpt1Responders.size(),
      nullptr,
      0
    },
    {
      2,
      nullptr,
      0,
      kTestVirtualEndpt2Responders.data(),
      kTestVirtualEndpt2Responders.size()
    }
  }
};
// clang-format on

TEST_F(TestDeviceApi, AddValidVirtualEndpointWorks)
{
  CreateDeviceWithDefaultConfig();

  EXPECT_EQ(rdmnet_device_add_virtual_endpoint(default_device_handle_, &kTestVirtualEndpointConfigs[0]), kEtcPalErrOk);
}

TEST_F(TestDeviceApi, AddValidVirtualEndpointsWorks)
{
  CreateDeviceWithDefaultConfig();

  EXPECT_EQ(rdmnet_device_add_virtual_endpoints(default_device_handle_, kTestVirtualEndpointConfigs.data(),
                                                kTestVirtualEndpointConfigs.size()),
            kEtcPalErrOk);
}
