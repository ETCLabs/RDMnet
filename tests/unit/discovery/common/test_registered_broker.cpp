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

#include "rdmnet/disc/registered_broker.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "gtest/gtest.h"
#include "test_disc_common_fakes.h"
#include "test_operators.h"

// Disable <cstring> warnings on Windows/MSVC
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

class TestRegisteredBroker : public testing::Test
{
protected:
  struct RegisteredBrokerDeleter
  {
    void operator()(RdmnetBrokerRegisterRef* ref)
    {
      registered_broker_remove(ref);
      registered_broker_delete(ref);
    }
  };
  using RegisteredBrokerUniquePtr = std::unique_ptr<RdmnetBrokerRegisterRef, RegisteredBrokerDeleter>;

  TestRegisteredBroker() { TestDiscoveryCommonResetAllFakes(); }

  RegisteredBrokerUniquePtr MakeDefaultRegisteredBroker()
  {
    return RegisteredBrokerUniquePtr(registered_broker_new(&default_config_));
  }

  RdmnetBrokerRegisterConfig                default_config_{};
  const std::vector<unsigned int>           kDefaultConfigNetints = {1, 2};
  const std::vector<RdmnetDnsTxtRecordItem> kDefaultAdditionalTxtItems = {
      {"Key 1", reinterpret_cast<const uint8_t*>("Value 1"), sizeof("Value 1") - 1},
      {"Key 2", reinterpret_cast<const uint8_t*>("Value 2"), sizeof("Value 2") - 1},
  };

  void SetUp() override
  {
    ASSERT_EQ(registered_broker_module_init(), kEtcPalErrOk);
    default_config_.cid = etcpal::Uuid::FromString("50b14416-8bc9-4e86-a65f-094934b8fd1b").get();
    default_config_.uid = rdm::Uid::FromString("6574:12345678").get();
    default_config_.service_instance_name = "Test Service Instance Name";
    default_config_.port = 8888;
    default_config_.scope = "Test Scope";
    default_config_.netints = kDefaultConfigNetints.data();
    default_config_.num_netints = kDefaultConfigNetints.size();
    default_config_.model = "Test Model";
    default_config_.manufacturer = "Test Manufacturer";
    default_config_.additional_txt_items = kDefaultAdditionalTxtItems.data();
    default_config_.num_additional_txt_items = kDefaultAdditionalTxtItems.size();
  }

  void TearDown() override { registered_broker_module_deinit(); }
};

#if RDMNET_DYNAMIC_MEM
TEST_F(TestRegisteredBroker, NewInitializesFieldsProperly)
{
  auto ref = MakeDefaultRegisteredBroker();

  ASSERT_NE(ref, nullptr);

  EXPECT_EQ(ref->cid, default_config_.cid);
  EXPECT_EQ(ref->uid, default_config_.uid);
  EXPECT_STREQ(ref->service_instance_name, default_config_.service_instance_name);
  EXPECT_EQ(ref->port, default_config_.port);
  EXPECT_STREQ(ref->scope, default_config_.scope);
  EXPECT_STREQ(ref->model, default_config_.model);
  EXPECT_STREQ(ref->manufacturer, default_config_.manufacturer);

  EXPECT_EQ(ref->scope_monitor_handle, nullptr);
  EXPECT_EQ(ref->state, kBrokerStateNotRegistered);
  EXPECT_STREQ(ref->full_service_name, "");
  EXPECT_EQ(ref->query_timeout_expired, false);

  ASSERT_EQ(ref->num_netints, default_config_.num_netints);
  for (size_t i = 0; i < ref->num_netints; ++i)
    EXPECT_EQ(ref->netints[i], default_config_.netints[i]);

  ASSERT_EQ(ref->num_additional_txt_items, default_config_.num_additional_txt_items);
  for (size_t i = 0; i < ref->num_additional_txt_items; ++i)
  {
    EXPECT_STREQ(ref->additional_txt_items[i].key, default_config_.additional_txt_items[i].key);
    EXPECT_EQ(ref->additional_txt_items[i].value_len, default_config_.additional_txt_items[i].value_len);
    EXPECT_EQ(std::memcmp(ref->additional_txt_items[i].value, default_config_.additional_txt_items[i].value,
                          ref->additional_txt_items[i].value_len),
              0);
  }
}

TEST_F(TestRegisteredBroker, NewInitializesNullAndZeroArrays)
{
  default_config_.netints = nullptr;
  default_config_.num_netints = 0;
  default_config_.additional_txt_items = nullptr;
  default_config_.num_additional_txt_items = 0;

  auto ref = MakeDefaultRegisteredBroker();

  EXPECT_EQ(ref->netints, nullptr);
  EXPECT_EQ(ref->num_netints, 0u);
  EXPECT_EQ(ref->additional_txt_items, nullptr);
  EXPECT_EQ(ref->num_additional_txt_items, 0u);
}

TEST_F(TestRegisteredBroker, InsertWorks)
{
  auto broker_1 = MakeDefaultRegisteredBroker();
  strcpy(broker_1->full_service_name, "Test Insert 1 Service Name");
  registered_broker_insert(broker_1.get());

  // We test the presence by using the for_each function.
  EXPECT_TRUE(broker_register_ref_is_valid(broker_1.get()));
  registered_broker_for_each(
      [](RdmnetBrokerRegisterRef* ref) { EXPECT_STREQ(ref->full_service_name, "Test Insert 1 Service Name"); });

  auto broker_2 = MakeDefaultRegisteredBroker();
  strcpy(broker_2->full_service_name, "Test Insert 2 Service Name");
  registered_broker_insert(broker_2.get());

  EXPECT_TRUE(broker_register_ref_is_valid(broker_1.get()));
  EXPECT_TRUE(broker_register_ref_is_valid(broker_2.get()));
}

// clang-format off
// These need to be at file scope because we are using stateless lambdas as C function pointers
constexpr size_t kNumBrokerNames = 4;
static std::pair<std::string, bool> broker_names[kNumBrokerNames] = {
  {"Test Broker 1", false},
  {"Test Broker 2", false},
  {"Test Broker 3", false},
  {"Test Broker 4", false}
};
// clang-format on

TEST_F(TestRegisteredBroker, ForEachWorks)
{
  std::array<RegisteredBrokerUniquePtr, kNumBrokerNames> brokers;
  // Insert each name into the list.
  for (size_t i = 0; i < kNumBrokerNames; ++i)
  {
    broker_names[i].second = false;
    brokers[i] = MakeDefaultRegisteredBroker();
    strcpy(brokers[i]->full_service_name, broker_names[i].first.c_str());
    registered_broker_insert(brokers[i].get());
  }

  // Flag each name as we hit it from the for_each function.
  registered_broker_for_each([](RdmnetBrokerRegisterRef* ref) {
    for (auto& name : broker_names)
    {
      if (0 == std::strcmp(ref->full_service_name, name.first.c_str()))
        name.second = true;
    }
  });

  // Make sure we've hit each of the names.
  for (const auto& name : broker_names)
    EXPECT_EQ(name.second, true);
}

// Needs to be at file scope because of C function pointers/stateless lambdas
static int remove_test_call_count = 0;

TEST_F(TestRegisteredBroker, RemoveWorks)
{
  remove_test_call_count = 0;

  auto broker_1 = MakeDefaultRegisteredBroker();
  registered_broker_insert(broker_1.get());
  auto broker_2 = MakeDefaultRegisteredBroker();
  registered_broker_insert(broker_2.get());

  registered_broker_remove(broker_2.get());

  // Only broker_1 should remain in the list
  EXPECT_TRUE(broker_register_ref_is_valid(broker_1.get()));
  EXPECT_FALSE(broker_register_ref_is_valid(broker_2.get()));
  registered_broker_for_each([](RdmnetBrokerRegisterRef*) { ++remove_test_call_count; });
  EXPECT_EQ(remove_test_call_count, 1);

  registered_broker_remove(broker_1.get());

  // No brokers should remain in the list
  EXPECT_FALSE(broker_register_ref_is_valid(broker_1.get()));
  registered_broker_for_each([](RdmnetBrokerRegisterRef*) { FAIL(); });

  // Need to clean up the resources manually since they've already been removed
  registered_broker_delete(broker_1.release());
  registered_broker_delete(broker_2.release());
}

TEST_F(TestRegisteredBroker, DeleteAllWorks)
{
  auto broker_1 = MakeDefaultRegisteredBroker();
  registered_broker_insert(broker_1.get());
  auto broker_2 = MakeDefaultRegisteredBroker();
  registered_broker_insert(broker_2.get());

  registered_broker_delete_all();

  // No brokers should remain in the list
  EXPECT_FALSE(broker_register_ref_is_valid(broker_1.get()));
  EXPECT_FALSE(broker_register_ref_is_valid(broker_2.get()));
  registered_broker_for_each([](RdmnetBrokerRegisterRef*) { FAIL(); });

  broker_1.release();
  broker_2.release();
}
#else
TEST_F(TestRegisteredBroker, CannotAllocateWhenBuiltStatic)
{
  EXPECT_EQ(registered_broker_new(&default_config_), nullptr);
}
#endif
