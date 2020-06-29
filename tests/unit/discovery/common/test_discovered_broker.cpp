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

#include "rdmnet/disc/discovered_broker.h"

#include <array>
#include <memory>
#include <string>
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/inet.h"
#include "rdm/cpp/uid.h"
#include "gtest/gtest.h"
#include "test_disc_common_fakes.h"
#include "test_operators.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

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
        discovered_broker_new(monitor_ref_, service_instance_name_.c_str(), full_service_name_.c_str()));
  }

  const std::string      service_instance_name_ = "Test service name";
  const std::string      full_service_name_ = "Test full service name";
  rdmnet_scope_monitor_t monitor_ref_ = reinterpret_cast<rdmnet_scope_monitor_t>(0xcc);
};

TEST_F(TestDiscoveredBroker, NewInitializesFieldsProperly)
{
  auto db = MakeDefaultDiscoveredBroker();

  ASSERT_NE(db, nullptr);

  EXPECT_EQ(db->full_service_name, full_service_name_);
  EXPECT_EQ(db->monitor_ref, monitor_ref_);
  EXPECT_EQ(db->cid, etcpal::Uuid{});  // UUID should be null
  EXPECT_EQ(db->uid, rdm::Uid{});
  EXPECT_EQ(db->e133_version, 0);
  EXPECT_EQ(db->service_instance_name, service_instance_name_);
  EXPECT_EQ(db->port, 0u);
  EXPECT_EQ(db->num_listen_addrs, 0u);
  EXPECT_EQ(db->num_additional_txt_items, 0u);
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
  // Create a list head -> dummy_1 -> dummy_2 -> end
  DiscoveredBroker dummy_1{};
  DiscoveredBroker dummy_2{};
  dummy_1.next = &dummy_2;
  DiscoveredBroker* list = &dummy_1;

  // Insert to_insert, should be at the end of the list
  DiscoveredBroker to_insert{};
  discovered_broker_insert(&list, &to_insert);
  EXPECT_EQ(list, &dummy_1);
  EXPECT_EQ(dummy_1.next, &dummy_2);
  EXPECT_EQ(dummy_2.next, &to_insert);
}

TEST_F(TestDiscoveredBroker, RemoveWorksAtHeadOfList)
{
  DiscoveredBroker to_remove{};

  DiscoveredBroker* list = &to_remove;
  discovered_broker_remove(&list, &to_remove);
  EXPECT_EQ(list, nullptr);
}

TEST_F(TestDiscoveredBroker, RemoveWorksAtEndOfList)
{
  // Create a list head -> dummy_1 -> dummy_2 -> end
  DiscoveredBroker dummy_1{};
  DiscoveredBroker dummy_2{};
  dummy_1.next = &dummy_2;
  DiscoveredBroker* list = &dummy_1;

  // Remove dummy_2 from the list
  discovered_broker_remove(&list, &dummy_2);
  EXPECT_EQ(list, &dummy_1);
  EXPECT_EQ(dummy_1.next, nullptr);
}

TEST_F(TestDiscoveredBroker, AddListenAddrWorks)
{
  auto db = MakeDefaultDiscoveredBroker();

  const EtcPalIpAddr test_addr = etcpal::IpAddr::FromString("10.101.1.1").get();
  ASSERT_TRUE(discovered_broker_add_listen_addr(db.get(), &test_addr));

  EXPECT_EQ(db->num_listen_addrs, 1u);

  ASSERT_NE(db->listen_addr_array, nullptr);
  EXPECT_EQ(db->listen_addr_array[0], test_addr);
}

TEST_F(TestDiscoveredBroker, AddTxtRecordItemWorks)
{
  auto db = MakeDefaultDiscoveredBroker();

  const RdmnetDnsTxtRecordItem test_txt_item = {"Test Key", reinterpret_cast<const uint8_t*>("Test Value"),
                                                sizeof("Test Value") - 1};
  ASSERT_TRUE(
      discovered_broker_add_txt_record_item(db.get(), test_txt_item.key, test_txt_item.value, test_txt_item.value_len));

  EXPECT_EQ(db->num_additional_txt_items, 1u);

  ASSERT_NE(db->additional_txt_items_array, nullptr);
  EXPECT_EQ(db->additional_txt_items_array[0], test_txt_item);
}

TEST_F(TestDiscoveredBroker, AddMultipleTxtRecordItemsWorks)
{
  auto db = MakeDefaultDiscoveredBroker();

  const std::string kKeyBase = "Test Key ";
  const std::string kValueBase = "Test Value ";

  for (size_t i = 0; i < 100; ++i)
  {
    const std::string key = kKeyBase + std::to_string(i);
    const std::string value = kValueBase + std::to_string(i);
    ASSERT_TRUE(discovered_broker_add_txt_record_item(
        db.get(), key.c_str(), reinterpret_cast<const uint8_t*>(value.c_str()), static_cast<uint8_t>(value.length())));
  }

  EXPECT_EQ(db->num_additional_txt_items, 100);

  for (size_t i = 0; i < 100; ++i)
  {
    const std::string key = kKeyBase + std::to_string(i);
    const std::string value = kValueBase + std::to_string(i);

    EXPECT_STREQ(db->additional_txt_items_array[i].key, key.c_str());
    ASSERT_EQ(db->additional_txt_items_array[i].value_len, static_cast<uint8_t>(value.length()));
    EXPECT_EQ(std::memcmp(db->additional_txt_items_array[i].value, value.c_str(), value.length()), 0);

    EXPECT_STREQ(db->additional_txt_items_data[i].key, key.c_str());
    ASSERT_EQ(db->additional_txt_items_data[i].value_len, static_cast<uint8_t>(value.length()));
    EXPECT_EQ(std::memcmp(db->additional_txt_items_data[i].value, value.c_str(), value.length()), 0);
  }
}

TEST_F(TestDiscoveredBroker, FindByNameWorks)
{
  // An array of DiscoveredBroker pointers that will automatically call discovered_broker_delete()
  // on each one on destruction.
  constexpr int                                      kNumBrokers = 10;
  std::array<DiscoveredBrokerUniquePtr, kNumBrokers> brokers;

  DiscoveredBroker* list = nullptr;

  // Fill the array and linked list of DiscoveredBrokers
  for (int i = 0; i < 10; ++i)
  {
    const auto this_full_service_name = full_service_name_ + " " + std::to_string(i);
    brokers[i].reset(
        discovered_broker_new(monitor_ref_, service_instance_name_.c_str(), this_full_service_name.c_str()));
    discovered_broker_insert(&list, brokers[i].get());
  }

  // Find the kNumBrokers / 2 broker instance by name.
  auto found_db = discovered_broker_find_by_name(
      list, std::string(full_service_name_ + " " + std::to_string(kNumBrokers / 2)).c_str());
  ASSERT_NE(found_db, nullptr);
  ASSERT_EQ(found_db->full_service_name, full_service_name_ + " " + std::to_string(kNumBrokers / 2));
}

TEST_F(TestDiscoveredBroker, ConvertToDiscInfoWorks)
{
  auto listen_addr = etcpal::IpAddr::FromString("192.168.30.40").get();
  // clang-format off
  RdmnetDnsTxtRecordItem txt_item = {
    "Test Key",
    reinterpret_cast<const uint8_t*>("Test Value"),
    sizeof("Test Value") - 1
  };
  const RdmnetBrokerDiscInfo valid_disc_info = {
    etcpal::Uuid::FromString("b8d1853d-d7df-46c9-a9a6-e3f02584c03f").get(),
    rdm::Uid::FromString("6574:12345678").get(),
    1,
    "Test Service Instance Name",
    8888,
    &listen_addr,
    1,
    "Test Scope",
    "Test Model",
    "Test Manufacturer",
    &txt_item,
    1
  };
  auto db = MakeDefaultDiscoveredBroker();

  db->cid = valid_disc_info.cid;
  db->uid = valid_disc_info.uid;
  db->e133_version = valid_disc_info.e133_version;
  strcpy(db->service_instance_name, valid_disc_info.service_instance_name);
  db->port = valid_disc_info.port;
  discovered_broker_add_listen_addr(db.get(), &listen_addr);
  strcpy(db->scope, valid_disc_info.scope);
  strcpy(db->model, valid_disc_info.model);
  strcpy(db->manufacturer, valid_disc_info.manufacturer);
  discovered_broker_add_txt_record_item(db.get(), txt_item.key, txt_item.value, txt_item.value_len);

  RdmnetBrokerDiscInfo test_disc_info;
  discovered_broker_fill_disc_info(db.get(), &test_disc_info);
  EXPECT_EQ(test_disc_info, valid_disc_info);
}
