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
#include "gmock/gmock.h"
#include "fff.h"
#include "broker_discovery.h"
#include "rdmnet_mock/core/discovery.h"

extern "C" {
etcpal_error_t rdmnetdisc_register_broker_and_set_handle(const RdmnetBrokerRegisterConfig* config,
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
};

class TestBrokerDiscovery : public testing::Test
{
public:
  static constexpr rdmnet_registered_broker_t kBrokerRegisterHandle{
      reinterpret_cast<rdmnet_registered_broker_t>(0xdead)};

protected:
  MockBrokerDiscoveryNotify notify_;
  BrokerDiscoveryManager disc_mgr_;
  rdmnet::BrokerSettings settings_;

  void SetUp() override
  {
    RDMNET_CORE_DISCOVERY_DO_FOR_ALL_FAKES(RESET_FAKE);
    rdmnetdisc_register_broker_fake.custom_fake = rdmnetdisc_register_broker_and_set_handle;

    disc_mgr_.SetNotify(&notify_);

    settings_.cid = etcpal::Uuid::FromString("22672657-407a-4a83-b34c-0929ec6d0bfb");
    settings_.dns.manufacturer = "Test";
    settings_.dns.model = "Test Broker";
    settings_.dns.service_instance_name = "Test Broker Service Instance";
    settings_.scope = "Test Scope";
  }

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

extern "C" etcpal_error_t rdmnetdisc_register_broker_and_set_handle(const RdmnetBrokerRegisterConfig* config,
                                                                    rdmnet_registered_broker_t* handle)
{
  (void)config;
  *handle = TestBrokerDiscovery::kBrokerRegisterHandle;
  return kEtcPalErrOk;
}

TEST_F(TestBrokerDiscovery, RegisterWorksWithNoErrors)
{
  RegisterBroker();
}

TEST_F(TestBrokerDiscovery, SyncRegisterErrorIsHandled)
{
  rdmnetdisc_register_broker_fake.custom_fake = nullptr;
  rdmnetdisc_register_broker_fake.return_val = kEtcPalErrSys;
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
  constexpr char* kActualServiceName = "A different service name";

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
}
