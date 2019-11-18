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

#include "registered_broker.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include "rdmnet/core/util.h"
#include "gtest/gtest.h"
#include "test_disc_common_fakes.h"
#include "test_operators.h"

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

  RdmnetBrokerRegisterConfig default_config_{};
};

TEST_F(TestRegisteredBroker, NewInitializesFieldsProperly)
{
  auto ref = MakeDefaultRegisteredBroker();

  ASSERT_NE(ref, nullptr);

  EXPECT_EQ(ref->config, default_config_);
  EXPECT_EQ(ref->scope_monitor_handle, nullptr);
  EXPECT_EQ(ref->state, kBrokerStateNotRegistered);
  EXPECT_STREQ(ref->full_service_name, "");
  EXPECT_EQ(ref->query_timeout_expired, false);
  EXPECT_EQ(ref->next, nullptr);
}

TEST_F(TestRegisteredBroker, InsertWorks)
{
  auto broker_1 = MakeDefaultRegisteredBroker();
  rdmnet_safe_strncpy(broker_1->full_service_name, "Test Insert 1 Service Name", RDMNET_DISC_SERVICE_NAME_MAX_LENGTH);
  registered_broker_insert(broker_1.get());

  // We test the presence by using the for_each function.
  EXPECT_TRUE(broker_register_ref_is_valid(broker_1.get()));
  registered_broker_for_each(
      [](RdmnetBrokerRegisterRef* ref) { EXPECT_STREQ(ref->full_service_name, "Test Insert 1 Service Name"); });

  auto broker_2 = MakeDefaultRegisteredBroker();
  rdmnet_safe_strncpy(broker_2->full_service_name, "Test Insert 2 Service Name", RDMNET_DISC_SERVICE_NAME_MAX_LENGTH);
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
    rdmnet_safe_strncpy(brokers[i]->full_service_name, broker_names[i].first.c_str(),
                        RDMNET_DISC_SERVICE_NAME_MAX_LENGTH);
    registered_broker_insert(brokers[i].get());
  }

  // Flag each name as we hit it from the for_each function.
  registered_broker_for_each([](RdmnetBrokerRegisterRef* ref) {
    for (auto& name : broker_names)
    {
      if (0 == std::strncmp(ref->full_service_name, name.first.c_str(), RDMNET_DISC_SERVICE_NAME_MAX_LENGTH))
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
