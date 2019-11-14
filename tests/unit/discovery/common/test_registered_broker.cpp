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
#include <memory>
#include <string>
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/inet.h"
#include "gtest/gtest.h"
#include "test_disc_common_fakes.h"

class TestRegisteredBroker : public testing::Test
{
protected:
  struct RegisteredBrokerDeleter
  {
    void operator()(RdmnetBrokerRegisterRef* ref) { registered_broker_delete(ref); }
  };
  using RegisteredBrokerUniquePtr = std::unique_ptr<RdmnetBrokerRegisterRef, RegisteredBrokerDeleter>;

  TestRegisteredBroker() { TestDiscoveryCommonResetAllFakes(); }

  RegisteredBrokerUniquePtr MakeDefaultRegisteredBroker()
  {
    return RegisteredBrokerUniquePtr(
        registered_broker_new(monitor_ref_, service_name_.c_str(), full_service_name_.c_str()));
  }

  const std::string service_name_ = "Test service name";
  const std::string full_service_name_ = "Test full service name";
  rdmnet_scope_monitor_t monitor_ref_ = reinterpret_cast<rdmnet_scope_monitor_t>(0xcc);
};

TEST_F(TestRegisteredBroker, NewInitializesFieldsProperly)
{
  auto ref = MakeDefaultRegisteredBroker();

  ASSERT_NE(ref, nullptr);

  EXPECT_EQ(ref->full_service_name, full_service_name_);
  EXPECT_EQ(ref->monitor_ref, monitor_ref_);
  EXPECT_EQ(ref->info.cid, etcpal::Uuid{});  // UUID should be null
  EXPECT_EQ(ref->info.service_name, service_name_);
  EXPECT_EQ(ref->info.port, 0u);
  EXPECT_EQ(ref->info.listen_addrs, nullptr);
  EXPECT_EQ(ref->info.num_listen_addrs, 0u);
  EXPECT_STREQ(ref->info.scope, E133_DEFAULT_SCOPE);
  EXPECT_STREQ(ref->info.model, "");
  EXPECT_STREQ(ref->info.manufacturer, "");
  EXPECT_EQ(ref->next, nullptr);
}

TEST_F(TestRegisteredBroker, InsertWorksAtHeadOfList)
{
  RdmnetBrokerRegisterRef to_insert{};

  RdmnetBrokerRegisterRef* list = nullptr;
  registered_broker_insert(&list, &to_insert);
  EXPECT_EQ(list, &to_insert);
}

TEST_F(TestRegisteredBroker, InsertWorksAtEndOfList)
{
  // Create a list head -> dummy_1 -> dummy_2 -> end
  RegisteredBroker dummy_1{};
  RegisteredBroker dummy_2{};
  dummy_1.next = &dummy_2;
  RegisteredBroker* list = &dummy_1;

  // Insert to_insert, should be at the end of the list
  RegisteredBroker to_insert{};
  registered_broker_insert(&list, &to_insert);
  EXPECT_EQ(list, &dummy_1);
  EXPECT_EQ(dummy_1.next, &dummy_2);
  EXPECT_EQ(dummy_2.next, &to_insert);
}

TEST_F(TestRegisteredBroker, RemoveWorksAtHeadOfList)
{
  RegisteredBroker to_remove{};

  RegisteredBroker* list = &to_remove;
  registered_broker_remove(&list, &to_remove);
  EXPECT_EQ(list, nullptr);
}

TEST_F(TestRegisteredBroker, RemoveWorksAtEndOfList)
{
  // Create a list head -> dummy_1 -> dummy_2 -> end
  RegisteredBroker dummy_1{};
  RegisteredBroker dummy_2{};
  dummy_1.next = &dummy_2;
  RegisteredBroker* list = &dummy_1;

  // Remove dummy_2 from the list
  registered_broker_remove(&list, &dummy_2);
  EXPECT_EQ(list, &dummy_1);
  EXPECT_EQ(dummy_1.next, nullptr);
}

TEST_F(TestRegisteredBroker, AddListenAddrWorks)
{
  auto ref = MakeDefaultRegisteredBroker();

  const EtcPalIpAddr test_addr = etcpal::IpAddr::FromString("10.101.1.1").get();
  ASSERT_TRUE(registered_broker_add_listen_addr(ref.get(), &test_addr));

  EXPECT_EQ(ref->info.num_listen_addrs, 1u);

  ASSERT_NE(ref->info.listen_addrs, nullptr);
  EXPECT_EQ(ref->info.listen_addrs[0], test_addr);
}

TEST_F(TestRegisteredBroker, LookupByNameWorks)
{
  // An array of RegisteredBroker pointers that will automatically call registered_broker_delete()
  // on each one on destruction.
  constexpr int kNumBrokers = 10;
  std::array<RegisteredBrokerUniquePtr, kNumBrokers> brokers;

  RegisteredBroker* list = nullptr;

  // Fill the array and linked list of RegisteredBrokers
  for (int i = 0; i < 10; ++i)
  {
    const auto this_full_service_name = full_service_name_ + " " + std::to_string(i);
    brokers[i].reset(registered_broker_new(monitor_ref_, service_name_.c_str(), this_full_service_name.c_str()));
    registered_broker_insert(&list, brokers[i].get());
  }

  // Find the kNumBrokers / 2 broker instance by name.
  auto found_ref = registered_broker_lookup_by_name(
      list, std::string(full_service_name_ + " " + std::to_string(kNumBrokers / 2)).c_str());
  ASSERT_NE(found_ref, nullptr);
  ASSERT_EQ(found_ref->full_service_name, full_service_name_ + " " + std::to_string(kNumBrokers / 2));
}
