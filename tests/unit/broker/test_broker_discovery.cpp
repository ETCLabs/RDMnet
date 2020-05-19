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

#include "broker_discovery.h"

#include <cstring>
#include "etcpal/common.h"
#include "rdmnet/core/util.h"
#include "rdmnet_mock/discovery.h"
#include "test_operators.h"
#include "gmock/gmock.h"

extern "C" {
etcpal_error_t rdmnet_disc_register_broker_and_set_handle(const RdmnetBrokerRegisterConfig* config,
                                                          rdmnet_registered_broker_t* handle);
}

class MockBrokerDiscoveryNotify : public BrokerDiscoveryNotify
{
public:
  MOCK_METHOD(void, HandleBrokerRegistered,
              (const std::string& scope, const std::string& requested_service_name,
               const std::string& assigned_service_name),
              (override));
  MOCK_METHOD(void, HandleOtherBrokerFound, (const RdmnetBrokerDiscInfo& broker_info), (override));
  MOCK_METHOD(void, HandleOtherBrokerLost, (const std::string& scope, const std::string& service_name), (override));
  MOCK_METHOD(void, HandleBrokerRegisterError,
              (const std::string& scope, const std::string& requested_service_name, int platform_error), (override));
  MOCK_METHOD(void, HandleScopeMonitorError, (const std::string& scope, int platform_error), (override));
};

class TestBrokerDiscovery : public testing::Test
{
public:
  static TestBrokerDiscovery* instance;

  static const rdmnet_registered_broker_t kBrokerRegisterHandle;
  rdmnet::Broker::Settings settings_;

protected:
  testing::StrictMock<MockBrokerDiscoveryNotify> notify_;
  BrokerDiscoveryManager disc_mgr_;

  void SetUp() override
  {
    rdmnet_discovery_reset_all_fakes();
    rdmnet_disc_register_broker_fake.custom_fake = rdmnet_disc_register_broker_and_set_handle;

    disc_mgr_.SetNotify(&notify_);

    settings_.cid = etcpal::Uuid::FromString("22672657-407a-4a83-b34c-0929ec6d0bfb");
    settings_.dns.manufacturer = "Test";
    settings_.dns.model = "Test Broker";
    settings_.dns.service_instance_name = "Test Broker Service Instance";
    settings_.scope = "Test Scope";

    EtcPalIpAddr addr;
    ETCPAL_IP_SET_V4_ADDRESS(&addr, 0x0a650203);
    settings_.listen_addrs.insert(addr);

    instance = this;
  }

  void TearDown() override { instance = nullptr; }

  void RegisterBroker()
  {
    ASSERT_TRUE(disc_mgr_.RegisterBroker(settings_));

    EXPECT_CALL(notify_, HandleBrokerRegistered(settings_.scope, settings_.dns.service_instance_name,
                                                settings_.dns.service_instance_name));
    disc_mgr_.LibNotifyBrokerRegistered(kBrokerRegisterHandle, settings_.dns.service_instance_name.c_str());

    EXPECT_EQ(disc_mgr_.scope(), settings_.scope);
    EXPECT_EQ(disc_mgr_.requested_service_name(), settings_.dns.service_instance_name);
    EXPECT_EQ(disc_mgr_.assigned_service_name(), settings_.dns.service_instance_name);
  }
};

const rdmnet_registered_broker_t TestBrokerDiscovery::kBrokerRegisterHandle =
    reinterpret_cast<rdmnet_registered_broker_t>(0xdead);
TestBrokerDiscovery* TestBrokerDiscovery::instance = nullptr;

extern "C" etcpal_error_t rdmnet_disc_register_broker_and_set_handle(const RdmnetBrokerRegisterConfig* config,
                                                                     rdmnet_registered_broker_t* handle)
{
  ETCPAL_UNUSED_ARG(config);

  TestBrokerDiscovery* test = TestBrokerDiscovery::instance;

  // Make sure we were registered with the correct settings
  EXPECT_EQ(config->my_info.cid, test->settings_.cid);
  EXPECT_EQ(config->my_info.service_name, test->settings_.dns.service_instance_name);
  EXPECT_EQ(config->my_info.port, test->settings_.listen_port);
  EXPECT_EQ(config->my_info.scope, test->settings_.scope);
  EXPECT_EQ(config->my_info.model, test->settings_.dns.model);
  EXPECT_EQ(config->my_info.manufacturer, test->settings_.dns.manufacturer);

  // Can't assert in this callback
  EXPECT_EQ(test->settings_.listen_addrs.size(), config->my_info.num_listen_addrs);
  if (test->settings_.listen_addrs.size() == config->my_info.num_listen_addrs)
  {
    size_t i = 0;
    for (const auto& addr : test->settings_.listen_addrs)
    {
      EXPECT_EQ(addr, config->my_info.listen_addrs[i++]);
    }
  }

  *handle = TestBrokerDiscovery::kBrokerRegisterHandle;
  return kEtcPalErrOk;
}

TEST_F(TestBrokerDiscovery, RegisterWorksWithNoErrors)
{
  RegisterBroker();
}

TEST_F(TestBrokerDiscovery, SyncRegisterErrorIsHandled)
{
  rdmnet_disc_register_broker_fake.custom_fake = nullptr;
  rdmnet_disc_register_broker_fake.return_val = kEtcPalErrSys;
  auto result = disc_mgr_.RegisterBroker(settings_);

  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), kEtcPalErrSys);
}

TEST_F(TestBrokerDiscovery, AsyncRegisterErrorIsForwarded)
{
  ASSERT_TRUE(disc_mgr_.RegisterBroker(settings_));

  int platform_error = 42;
  EXPECT_CALL(notify_, HandleBrokerRegisterError(settings_.scope, settings_.dns.service_instance_name, platform_error));
  disc_mgr_.LibNotifyBrokerRegisterError(kBrokerRegisterHandle, platform_error);
}

TEST_F(TestBrokerDiscovery, ServiceNameChangeIsHandled)
{
  constexpr const char* kActualServiceName = "A different service name";

  ASSERT_TRUE(disc_mgr_.RegisterBroker(settings_));

  EXPECT_CALL(notify_,
              HandleBrokerRegistered(settings_.scope, settings_.dns.service_instance_name, kActualServiceName));
  disc_mgr_.LibNotifyBrokerRegistered(kBrokerRegisterHandle, kActualServiceName);

  EXPECT_EQ(disc_mgr_.scope(), settings_.scope);
  EXPECT_EQ(disc_mgr_.requested_service_name(), settings_.dns.service_instance_name);
  EXPECT_EQ(disc_mgr_.assigned_service_name(), kActualServiceName);
}

TEST_F(TestBrokerDiscovery, BrokerFoundIsForwarded)
{
  RegisterBroker();

  RdmnetBrokerDiscInfo found_info{};
  rdmnet_safe_strncpy(found_info.scope, settings_.scope.c_str(), E133_SCOPE_STRING_PADDED_LENGTH);
  rdmnet_safe_strncpy(found_info.service_name, "Other Broker Service Name", E133_SERVICE_NAME_STRING_PADDED_LENGTH);

  // TODO replace this with a proper matcher
  EXPECT_CALL(notify_, HandleOtherBrokerFound(found_info));
  disc_mgr_.LibNotifyBrokerFound(kBrokerRegisterHandle, &found_info);
}

TEST_F(TestBrokerDiscovery, BrokerLostIsForwarded)
{
  RegisterBroker();

  EXPECT_CALL(notify_, HandleOtherBrokerLost(settings_.scope, "Other Broker Service Name"));
  disc_mgr_.LibNotifyBrokerLost(kBrokerRegisterHandle, settings_.scope.c_str(), "Other Broker Service Name");
}

TEST_F(TestBrokerDiscovery, ScopeMonitorErrorIsForwarded)
{
  RegisterBroker();

  int platform_error = 42;
  EXPECT_CALL(notify_, HandleScopeMonitorError(settings_.scope, platform_error));
  disc_mgr_.LibNotifyScopeMonitorError(kBrokerRegisterHandle, settings_.scope.c_str(), platform_error);
}

// Using a strict mock - test will fail if any of these invalid calls are forwarded
TEST_F(TestBrokerDiscovery, InvalidNotificationsAreNotForwarded)
{
  RegisterBroker();
  const rdmnet_registered_broker_t kOtherBrokerHandle = reinterpret_cast<rdmnet_registered_broker_t>(0xbeef);
  RdmnetBrokerDiscInfo other_broker_info{};

  disc_mgr_.LibNotifyBrokerRegistered(kOtherBrokerHandle, settings_.scope.c_str());
  disc_mgr_.LibNotifyBrokerRegistered(kBrokerRegisterHandle, nullptr);
  disc_mgr_.LibNotifyBrokerRegisterError(kOtherBrokerHandle, 42);
  disc_mgr_.LibNotifyBrokerFound(kOtherBrokerHandle, &other_broker_info);
  disc_mgr_.LibNotifyBrokerFound(kBrokerRegisterHandle, nullptr);
  disc_mgr_.LibNotifyBrokerLost(kOtherBrokerHandle, settings_.scope.c_str(),
                                settings_.dns.service_instance_name.c_str());
  disc_mgr_.LibNotifyBrokerLost(kBrokerRegisterHandle, nullptr, settings_.dns.service_instance_name.c_str());
  disc_mgr_.LibNotifyBrokerLost(kBrokerRegisterHandle, settings_.scope.c_str(), nullptr);
  disc_mgr_.LibNotifyScopeMonitorError(kOtherBrokerHandle, settings_.scope.c_str(), 42);
  disc_mgr_.LibNotifyScopeMonitorError(kBrokerRegisterHandle, nullptr, 42);
}
