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

#include "discovered_broker.h"

#include <array>
#include <memory>
#include <string>
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/inet.h"
#include "gtest/gtest.h"
#include "test_disc_common_fakes.h"

class TestDiscoveredBroker : public testing::Test
{
protected:
  struct DiscoveredBrokerDeleter
  {
    void operator()(DiscoveredBroker* db) { discovered_broker_delete(db); }
  };
  using DiscoveredBrokerUniquePtr = std::unique_ptr<DiscoveredBroker, DiscoveredBrokerDeleter>;

  TestDiscoveredBroker() { TestDiscoveryCommonResetAllFakes(); }

  DiscoveredBrokerUniquePtr MakeDefaultDiscoveredBroker()
  {
    return DiscoveredBrokerUniquePtr(
        discovered_broker_new(monitor_ref_, service_name_.c_str(), full_service_name_.c_str()));
  }

  const std::string service_name_ = "Test service name";
  const std::string full_service_name_ = "Test full service name";
  rdmnet_scope_monitor_t monitor_ref_ = reinterpret_cast<rdmnet_scope_monitor_t>(0xcc);
};

TEST_F(TestDiscoveredBroker, NewInitializesFieldsProperly)
{
  auto db = MakeDefaultDiscoveredBroker();

  ASSERT_NE(db, nullptr);

  EXPECT_EQ(db->full_service_name, full_service_name_);
  EXPECT_EQ(db->monitor_ref, monitor_ref_);
  EXPECT_EQ(db->info.cid, etcpal::Uuid{});  // UUID should be null
  EXPECT_EQ(db->info.service_name, service_name_);
  EXPECT_EQ(db->info.port, 0u);
  EXPECT_EQ(db->info.listen_addrs, nullptr);
  EXPECT_EQ(db->info.num_listen_addrs, 0u);
  EXPECT_STREQ(db->info.scope, E133_DEFAULT_SCOPE);
  EXPECT_STREQ(db->info.model, "");
  EXPECT_STREQ(db->info.manufacturer, "");
  EXPECT_EQ(db->next, nullptr);
}

TEST_F(TestDiscoveredBroker, InsertWorksAtHeadOfList)
{
  DiscoveredBroker to_insert{};

  DiscoveredBroker* list = nullptr;
  discovered_broker_insert(&list, &to_insert);
  EXPECT_EQ(list, &to_insert);
}

TEST_F(TestDiscoveredBroker, InsertWorksAtEndOfList)
{
  DiscoveredBroker dummy_1{};
  DiscoveredBroker dummy_2{};
  dummy_1.next = &dummy_2;
  DiscoveredBroker* list = &dummy_1;

  DiscoveredBroker to_insert{};
  discovered_broker_insert(&list, &to_insert);
  EXPECT_EQ(list, &dummy_1);
  EXPECT_EQ(dummy_1.next, &dummy_2);
  EXPECT_EQ(dummy_2.next, &to_insert);
}

TEST_F(TestDiscoveredBroker, AddListenAddrWorks)
{
  auto db = MakeDefaultDiscoveredBroker();

  EtcPalIpAddr test_addr = etcpal::IpAddr::FromString("10.101.1.1").get();
  ASSERT_TRUE(discovered_broker_add_listen_addr(db.get(), &test_addr));

  EXPECT_EQ(db->info.num_listen_addrs, 1u);

  ASSERT_NE(db->info.listen_addrs, nullptr);
  EXPECT_EQ(db->info.listen_addrs[0], test_addr);
}

TEST_F(TestDiscoveredBroker, LookupByNameWorks)
{
  // An array of DiscoveredBroker pointers that will automatically call discovered_broker_delete()
  // on each one on destruction.
  constexpr int kNumBrokers = 10;
  std::array<DiscoveredBrokerUniquePtr, kNumBrokers> brokers;

  DiscoveredBroker* list = nullptr;

  // Fill the array and linked list of DiscoveredBrokers
  for (int i = 0; i < 10; ++i)
  {
    const auto this_full_service_name = full_service_name_ + " " + std::to_string(i);
    brokers[i].reset(discovered_broker_new(monitor_ref_, service_name_.c_str(), this_full_service_name.c_str()));
    discovered_broker_insert(&list, brokers[i].get());
  }

  // Find the kNumBrokers / 2 broker instance by name.
  auto found_db = discovered_broker_lookup_by_name(
      list, std::string(full_service_name_ + " " + std::to_string(kNumBrokers / 2)).c_str());
  ASSERT_NE(found_db, nullptr);
  ASSERT_EQ(found_db->full_service_name, full_service_name_ + " " + std::to_string(kNumBrokers / 2));
}
