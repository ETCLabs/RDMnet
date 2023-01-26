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

#include "lwmdns_common.h"

#include <cstdint>
#include <cstring>
#include <vector>
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/disc/discovered_broker.h"
#include "gtest/gtest.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

class TestLwMdnsTxtRecordParsing : public testing::Test
{
  void SetUp() override { ASSERT_EQ(discovered_broker_module_init(), kEtcPalErrOk); }
};

std::vector<uint8_t> StringToBytes(const char* str)
{
  size_t str_len = std::strlen(str);
  return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(str), reinterpret_cast<const uint8_t*>(str + str_len));
}

TEST_F(TestLwMdnsTxtRecordParsing, ParsesNormalTxtRecord)
{
  constexpr const char* kNormalTxtRecord =
      "\011TxtVers=1\021E133Scope=default\012E133Vers=1\044CID=da30bf9383174140a7714840483f71d7\020UID="
      "6574d574a27a\016Model=Test App\011Manuf=ETC\021XtraItem=BlahBlah\13XtraKeyOnly\14XtraNoValue=";

  DiscoveredBroker* db = discovered_broker_new((rdmnet_scope_monitor_t)1, "service", "service");
  ASSERT_NE(db, nullptr);

  auto txt_bytes = StringToBytes(kNormalTxtRecord);
  EXPECT_EQ(lwmdns_txt_record_to_broker_info(txt_bytes.data(), static_cast<uint16_t>(txt_bytes.size()), db),
            kTxtRecordParseOkDataChanged);

  EXPECT_STREQ(db->scope, "default");
  EXPECT_EQ(db->cid, etcpal::Uuid::FromString("da30bf93-8317-4140-a771-4840483f71d7"));
  EXPECT_EQ(db->uid, rdm::Uid::FromString("6574:d574a27a"));
  EXPECT_STREQ(db->model, "Test App");
  EXPECT_STREQ(db->manufacturer, "ETC");

  ASSERT_EQ(db->num_additional_txt_items, 3u);
  EXPECT_STREQ(db->additional_txt_items_array[0].key, "XtraItem");
  EXPECT_EQ(db->additional_txt_items_array[0].value_len, 8);
  EXPECT_EQ(std::memcmp(db->additional_txt_items_array[0].value, "BlahBlah", sizeof("BlahBlah") - 1), 0);
  EXPECT_STREQ(db->additional_txt_items_array[1].key, "XtraKeyOnly");
  EXPECT_EQ(db->additional_txt_items_array[1].value_len, 0);
  EXPECT_STREQ(db->additional_txt_items_array[2].key, "XtraNoValue");
  EXPECT_EQ(db->additional_txt_items_array[2].value_len, 0);

  discovered_broker_delete(db);
}

TEST_F(TestLwMdnsTxtRecordParsing, DoesNotParseWhenTxtVersMissing)
{
  constexpr const char* kTxtRecordTxtVersMissing =
      "\021E133Scope=default\012E133Vers=1\044CID=da30bf9383174140a7714840483f71d7\020UID="
      "6574d574a27a\016Model=Test App\011Manuf=ETC\021XtraItem=BlahBlah";

  auto             txt_bytes = StringToBytes(kTxtRecordTxtVersMissing);
  DiscoveredBroker db{};
  EXPECT_EQ(lwmdns_txt_record_to_broker_info(txt_bytes.data(), static_cast<uint16_t>(txt_bytes.size()), &db),
            kTxtRecordParseError);
}

TEST_F(TestLwMdnsTxtRecordParsing, DoesNotParseWhenTxtVersTooHigh)
{
  constexpr const char* kTxtRecordTxtVersTooHigh =
      "\011TxtVers=2\021E133Scope=default\012E133Vers=1\044CID=da30bf9383174140a7714840483f71d7\020UID="
      "6574d574a27a\016Model=Test App\011Manuf=ETC\021XtraItem=BlahBlah";

  auto             txt_bytes = StringToBytes(kTxtRecordTxtVersTooHigh);
  DiscoveredBroker db{};
  EXPECT_EQ(lwmdns_txt_record_to_broker_info(txt_bytes.data(), static_cast<uint16_t>(txt_bytes.size()), &db),
            kTxtRecordParseError);
}

TEST_F(TestLwMdnsTxtRecordParsing, RecognizesNoDataChanged)
{
  constexpr const char* kNormalTxtRecord =
      "\011TxtVers=1\021E133Scope=default\012E133Vers=1\044CID=da30bf9383174140a7714840483f71d7\020UID="
      "6574d574a27a\016Model=Test App\011Manuf=ETC\021XtraItem=BlahBlah";

  DiscoveredBroker* db = discovered_broker_new((rdmnet_scope_monitor_t)1, "service", "service");
  ASSERT_NE(db, nullptr);

  db->cid = etcpal::Uuid::FromString("da30bf9383174140a7714840483f71d7").get();
  db->uid = rdm::Uid::FromString("6574d574a27a").get();
  db->e133_version = 1;
  std::strcpy(db->scope, "default");
  std::strcpy(db->model, "Test App");
  std::strcpy(db->manufacturer, "ETC");

  ASSERT_TRUE(discovered_broker_add_txt_record_item(db, "XtraItem", reinterpret_cast<const uint8_t*>("BlahBlah"),
                                                    sizeof("BlahBlah") - 1));

  auto txt_bytes = StringToBytes(kNormalTxtRecord);
  ASSERT_EQ(lwmdns_txt_record_to_broker_info(txt_bytes.data(), static_cast<uint16_t>(txt_bytes.size()), db),
            kTxtRecordParseOkNoDataChanged);

  // Make sure nothing has actually changed
  EXPECT_STREQ(db->scope, "default");
  EXPECT_EQ(db->cid, etcpal::Uuid::FromString("da30bf9383174140a7714840483f71d7"));
  EXPECT_EQ(db->uid, rdm::Uid::FromString("6574d574a27a"));
  EXPECT_STREQ(db->model, "Test App");
  EXPECT_STREQ(db->manufacturer, "ETC");

  ASSERT_EQ(db->num_additional_txt_items, 1u);
  EXPECT_STREQ(db->additional_txt_items_array[0].key, "XtraItem");
  EXPECT_EQ(db->additional_txt_items_array[0].value_len, 8);
  EXPECT_EQ(std::memcmp(db->additional_txt_items_array[0].value, "BlahBlah", sizeof("BlahBlah") - 1), 0);

  discovered_broker_delete(db);
}

TEST_F(TestLwMdnsTxtRecordParsing, RecognizesStandardDataChanged)
{
  constexpr const char* kNormalTxtRecord =
      "\011TxtVers=1\025E133Scope=not default\012E133Vers=1\044CID=da30bf9383174140a7714840483f71d7\020UID="
      "6574d574a27a\016Model=Test App\011Manuf=ETC\021XtraItem=BlahBlah";

  DiscoveredBroker* db = discovered_broker_new((rdmnet_scope_monitor_t)1, "service", "service");
  ASSERT_NE(db, nullptr);

  db->cid = etcpal::Uuid::FromString("da30bf9383174140a7714840483f71d7").get();
  db->uid = rdm::Uid::FromString("6574d574a27a").get();
  std::strcpy(db->scope, "default");
  std::strcpy(db->model, "Test App");
  std::strcpy(db->manufacturer, "ETC");

  ASSERT_TRUE(discovered_broker_add_txt_record_item(db, "XtraItem", reinterpret_cast<const uint8_t*>("BlahBlah"),
                                                    sizeof("BlahBlah") - 1));

  auto txt_bytes = StringToBytes(kNormalTxtRecord);
  ASSERT_EQ(lwmdns_txt_record_to_broker_info(txt_bytes.data(), static_cast<uint16_t>(txt_bytes.size()), db),
            kTxtRecordParseOkDataChanged);

  // Make sure the proper data has actually changed
  EXPECT_STREQ(db->scope, "not default");
  EXPECT_EQ(db->cid, etcpal::Uuid::FromString("da30bf9383174140a7714840483f71d7"));
  EXPECT_EQ(db->uid, rdm::Uid::FromString("6574d574a27a"));
  EXPECT_STREQ(db->model, "Test App");
  EXPECT_STREQ(db->manufacturer, "ETC");

  ASSERT_EQ(db->num_additional_txt_items, 1u);
  EXPECT_STREQ(db->additional_txt_items_array[0].key, "XtraItem");
  EXPECT_EQ(db->additional_txt_items_array[0].value_len, 8);
  EXPECT_EQ(std::memcmp(db->additional_txt_items_array[0].value, "BlahBlah", sizeof("BlahBlah") - 1), 0);

  discovered_broker_delete(db);
}

TEST_F(TestLwMdnsTxtRecordParsing, RecognizesAdditionalDataChanged)
{
  constexpr const char* kNormalTxtRecord =
      "\011TxtVers=1\021E133Scope=default\012E133Vers=1\044CID=da30bf9383174140a7714840483f71d7\020UID="
      "6574d574a27a\016Model=Test App\011Manuf=ETC\021XtraItem=BlahBlah\011XtraItem2";

  DiscoveredBroker* db = discovered_broker_new((rdmnet_scope_monitor_t)1, "service", "service");
  ASSERT_NE(db, nullptr);

  db->cid = etcpal::Uuid::FromString("da30bf9383174140a7714840483f71d7").get();
  db->uid = rdm::Uid::FromString("6574d574a27a").get();
  std::strcpy(db->scope, "default");
  std::strcpy(db->model, "Test App");
  std::strcpy(db->manufacturer, "ETC");

  ASSERT_TRUE(discovered_broker_add_txt_record_item(db, "XtraItem", reinterpret_cast<const uint8_t*>("BlahBlah"),
                                                    sizeof("BlahBlah") - 1));

  auto txt_bytes = StringToBytes(kNormalTxtRecord);
  ASSERT_EQ(lwmdns_txt_record_to_broker_info(txt_bytes.data(), static_cast<uint16_t>(txt_bytes.size()), db),
            kTxtRecordParseOkDataChanged);

  // Make sure the proper data has actually changed
  EXPECT_STREQ(db->scope, "default");
  EXPECT_EQ(db->cid, etcpal::Uuid::FromString("da30bf9383174140a7714840483f71d7"));
  EXPECT_EQ(db->uid, rdm::Uid::FromString("6574d574a27a"));
  EXPECT_STREQ(db->model, "Test App");
  EXPECT_STREQ(db->manufacturer, "ETC");

  ASSERT_EQ(db->num_additional_txt_items, 2u);
  EXPECT_STREQ(db->additional_txt_items_array[0].key, "XtraItem");
  EXPECT_EQ(db->additional_txt_items_array[0].value_len, 8);
  EXPECT_EQ(memcmp(db->additional_txt_items_array[0].value, "BlahBlah", sizeof("BlahBlah") - 1), 0);
  EXPECT_STREQ(db->additional_txt_items_array[1].key, "XtraItem2");
  EXPECT_EQ(db->additional_txt_items_array[1].value_len, 0);

  discovered_broker_delete(db);
}

TEST_F(TestLwMdnsTxtRecordParsing, MalformedStandardKey)
{
  constexpr const char* kTxtRecordMalformedStandardKey =
      "\011TxtVers=1\012E133Scope=\012E133Vers=1\044CID=da30bf9383174140a7714840483f71d7\020UID="
      "6574d574a27a\016Model=Test App\011Manuf=ETC\021XtraItem=BlahBlah\13XtraKeyOnly\14XtraNoValue=";

  auto              txt_bytes = StringToBytes(kTxtRecordMalformedStandardKey);
  DiscoveredBroker* db = discovered_broker_new((rdmnet_scope_monitor_t)1, "service", "service");
  ASSERT_NE(db, nullptr);
  EXPECT_EQ(lwmdns_txt_record_to_broker_info(txt_bytes.data(), static_cast<uint16_t>(txt_bytes.size()), db),
            kTxtRecordParseError);
  discovered_broker_delete(db);
}
