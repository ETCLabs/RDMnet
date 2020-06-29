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

#include "broker_discovery.h"

#include <algorithm>
#include <cstring>
#include "etcpal/common.h"
#include "rdmnet/core/util.h"
#include "rdmnet_mock/discovery.h"
#include "test_operators.h"
#include "gmock/gmock.h"

using testing::AllOf;
using testing::Field;
using testing::StrEq;

extern "C" {
etcpal_error_t rdmnet_disc_register_broker_and_set_handle(const RdmnetBrokerRegisterConfig* config,
                                                          rdmnet_registered_broker_t*       handle);
}

class MockBrokerDiscoveryNotify : public BrokerDiscoveryNotify
{
public:
  MOCK_METHOD(void, HandleBrokerRegistered, (const std::string& assigned_service_name), (override));
  MOCK_METHOD(void, HandleOtherBrokerFound, (const RdmnetBrokerDiscInfo& broker_info), (override));
  MOCK_METHOD(void, HandleOtherBrokerLost, (const std::string& scope, const std::string& service_name), (override));
  MOCK_METHOD(void, HandleBrokerRegisterError, (int platform_error), (override));
};

class TestBrokerDiscovery : public testing::Test
{
public:
  static TestBrokerDiscovery* instance;

  static const rdmnet_registered_broker_t kBrokerRegisterHandle;
  rdmnet::Broker::Settings                settings_;
  std::vector<unsigned int>               netints;

protected:
  testing::StrictMock<MockBrokerDiscoveryNotify> notify_;
  BrokerDiscoveryManager                         disc_mgr_;

  void SetUp() override
  {
    rdmnet_disc_reset_all_fakes();
    rdmnet_disc_register_broker_fake.custom_fake = rdmnet_disc_register_broker_and_set_handle;

    disc_mgr_.SetNotify(&notify_);

    settings_.cid = etcpal::Uuid::FromString("22672657-407a-4a83-b34c-0929ec6d0bfb");
    settings_.dns.manufacturer = "Test";
    settings_.dns.model = "Test Broker";
    settings_.dns.service_instance_name = "Test Broker Service Instance";
    settings_.scope = "Test Scope";
    settings_.dns.additional_txt_record_items.push_back(rdmnet::DnsTxtRecordItem("Key", "Value"));

    netints.push_back(1);

    instance = this;
  }

  void TearDown() override { instance = nullptr; }

  void RegisterBroker()
  {
    ASSERT_TRUE(disc_mgr_.RegisterBroker(settings_, netints));

    EXPECT_CALL(notify_, HandleBrokerRegistered(settings_.dns.service_instance_name));
    disc_mgr_.LibNotifyBrokerRegistered(kBrokerRegisterHandle, settings_.dns.service_instance_name.c_str());

    EXPECT_EQ(disc_mgr_.assigned_service_name(), settings_.dns.service_instance_name);
  }
};

const rdmnet_registered_broker_t TestBrokerDiscovery::kBrokerRegisterHandle =
    reinterpret_cast<rdmnet_registered_broker_t>(0xdead);
TestBrokerDiscovery* TestBrokerDiscovery::instance = nullptr;

extern "C" etcpal_error_t rdmnet_disc_register_broker_and_set_handle(const RdmnetBrokerRegisterConfig* config,
                                                                     rdmnet_registered_broker_t*       handle)
{
  ETCPAL_UNUSED_ARG(config);

  TestBrokerDiscovery* test = TestBrokerDiscovery::instance;

  // Make sure we were registered with the correct settings
  EXPECT_EQ(config->cid, test->settings_.cid);
  EXPECT_EQ(config->uid, test->settings_.uid);
  EXPECT_EQ(config->service_instance_name, test->settings_.dns.service_instance_name);
  EXPECT_EQ(config->port, test->settings_.listen_port);
  EXPECT_EQ(config->scope, test->settings_.scope);
  EXPECT_EQ(config->model, test->settings_.dns.model);
  EXPECT_EQ(config->manufacturer, test->settings_.dns.manufacturer);

  if (test->netints.empty())
  {
    EXPECT_EQ(config->netints, nullptr);
    EXPECT_EQ(config->num_netints, 0u);
  }
  else if (test->netints.size() == config->num_netints)
  {
    for (size_t i = 0; i < config->num_netints; ++i)
    {
      EXPECT_EQ(test->netints[i], config->netints[i]);
    }
  }
  else
  {
    ADD_FAILURE() << "test->netints.size() != config->num_netints";
  }

  if (test->settings_.dns.additional_txt_record_items.empty())
  {
    EXPECT_EQ(config->additional_txt_items, nullptr);
    EXPECT_EQ(config->num_additional_txt_items, 0u);
  }
  else if (test->settings_.dns.additional_txt_record_items.size() == config->num_additional_txt_items)
  {
    for (size_t i = 0; i < config->num_additional_txt_items; ++i)
    {
      const rdmnet::DnsTxtRecordItem& given_item = test->settings_.dns.additional_txt_record_items[i];
      const RdmnetDnsTxtRecordItem&   got_item = config->additional_txt_items[i];

      EXPECT_EQ(given_item.key, got_item.key);
      EXPECT_EQ(given_item.value.size(), got_item.value_len);
      if (given_item.value.size() == got_item.value_len)
        EXPECT_TRUE(std::equal(given_item.value.begin(), given_item.value.end(), got_item.value));
    }
  }
  else
  {
    ADD_FAILURE() << "additional_txt_record_items.size() != config->num_additional_txt_items";
  }

  *handle = TestBrokerDiscovery::kBrokerRegisterHandle;
  return kEtcPalErrOk;
}

TEST_F(TestBrokerDiscovery, RegisterWorksWithNoErrors)
{
  RegisterBroker();
}

TEST_F(TestBrokerDiscovery, EmptyFieldsTranslateToNull)
{
  // The expectations defined in rdmnet_disc_register_broker_and_set_handle() enforce this test -
  // all that's needed to set it up is to clear the appropriate vectors
  netints.clear();
  settings_.dns.additional_txt_record_items.clear();
  RegisterBroker();
}

TEST_F(TestBrokerDiscovery, SyncRegisterErrorIsHandled)
{
  rdmnet_disc_register_broker_fake.custom_fake = nullptr;
  rdmnet_disc_register_broker_fake.return_val = kEtcPalErrSys;
  auto result = disc_mgr_.RegisterBroker(settings_, netints);

  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), kEtcPalErrSys);
}

TEST_F(TestBrokerDiscovery, AsyncRegisterErrorIsForwarded)
{
  ASSERT_TRUE(disc_mgr_.RegisterBroker(settings_, netints));

  int platform_error = 42;
  EXPECT_CALL(notify_, HandleBrokerRegisterError(platform_error));
  disc_mgr_.LibNotifyBrokerRegisterError(kBrokerRegisterHandle, platform_error);
}

TEST_F(TestBrokerDiscovery, ServiceNameChangeIsHandled)
{
  constexpr const char* kActualServiceName = "A different service name";

  ASSERT_TRUE(disc_mgr_.RegisterBroker(settings_, netints));

  EXPECT_CALL(notify_, HandleBrokerRegistered(kActualServiceName));
  disc_mgr_.LibNotifyBrokerRegistered(kBrokerRegisterHandle, kActualServiceName);

  EXPECT_EQ(disc_mgr_.assigned_service_name(), kActualServiceName);
}

TEST_F(TestBrokerDiscovery, BrokerFoundIsForwarded)
{
  RegisterBroker();

  RdmnetBrokerDiscInfo found_info{};
  found_info.scope = settings_.scope.c_str();
  found_info.service_instance_name = "Other Broker Service Name";

  // TODO replace this with a proper matcher
  EXPECT_CALL(notify_, HandleOtherBrokerFound(AllOf(
                           Field(&RdmnetBrokerDiscInfo::scope, settings_.scope),
                           Field(&RdmnetBrokerDiscInfo::service_instance_name, StrEq("Other Broker Service Name")))));
  disc_mgr_.LibNotifyOtherBrokerFound(kBrokerRegisterHandle, &found_info);
}

TEST_F(TestBrokerDiscovery, BrokerLostIsForwarded)
{
  RegisterBroker();

  EXPECT_CALL(notify_, HandleOtherBrokerLost(settings_.scope, "Other Broker Service Name"));
  disc_mgr_.LibNotifyOtherBrokerLost(kBrokerRegisterHandle, settings_.scope.c_str(), "Other Broker Service Name");
}

// Using a strict mock - test will fail if any of these invalid calls are forwarded
TEST_F(TestBrokerDiscovery, InvalidNotificationsAreNotForwarded)
{
  RegisterBroker();
  const rdmnet_registered_broker_t kOtherBrokerHandle = reinterpret_cast<rdmnet_registered_broker_t>(0xbeef);
  RdmnetBrokerDiscInfo             other_broker_info{};

  disc_mgr_.LibNotifyBrokerRegistered(kOtherBrokerHandle, settings_.scope.c_str());
  disc_mgr_.LibNotifyBrokerRegistered(kBrokerRegisterHandle, nullptr);
  disc_mgr_.LibNotifyBrokerRegisterError(kOtherBrokerHandle, 42);
  disc_mgr_.LibNotifyOtherBrokerFound(kOtherBrokerHandle, &other_broker_info);
  disc_mgr_.LibNotifyOtherBrokerFound(kBrokerRegisterHandle, nullptr);
  disc_mgr_.LibNotifyOtherBrokerLost(kOtherBrokerHandle, settings_.scope.c_str(),
                                     settings_.dns.service_instance_name.c_str());
  disc_mgr_.LibNotifyOtherBrokerLost(kBrokerRegisterHandle, nullptr, settings_.dns.service_instance_name.c_str());
  disc_mgr_.LibNotifyOtherBrokerLost(kBrokerRegisterHandle, settings_.scope.c_str(), nullptr);
}
