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

#include "rdmnet/cpp/broker.h"
#include "gtest/gtest.h"

TEST(BrokerDnsTxtRecordItem, CharStarConstructor)
{
  const auto                 item = rdmnet::DnsTxtRecordItem("Key", "Value");
  const std::vector<uint8_t> kValueData = {0x56, 0x61, 0x6c, 0x75, 0x65};
  EXPECT_EQ(item.key, "Key");
  EXPECT_EQ(item.value, kValueData);
}

TEST(BrokerDnsTxtRecordItem, CharStarValueBinaryConstructor)
{
  const std::vector<uint8_t> kValueData = {1, 2, 3, 4, 5, 6};
  const auto                 item = rdmnet::DnsTxtRecordItem("Key", kValueData.data(), kValueData.size());
  EXPECT_EQ(item.key, "Key");
  EXPECT_EQ(item.value, kValueData);
}

TEST(BrokerDnsTxtRecordItem, StringConstructor)
{
  const auto                 item = rdmnet::DnsTxtRecordItem(std::string("Key"), std::string("Value"));
  const std::vector<uint8_t> kValueData = {0x56, 0x61, 0x6c, 0x75, 0x65};
  EXPECT_EQ(item.key, "Key");
  EXPECT_EQ(item.value, kValueData);
}

TEST(BrokerDnsTxtRecordItem, StringBinaryConstructor)
{
  const std::vector<uint8_t> kValueData = {1, 2, 3, 4, 5, 6};
  const auto                 item = rdmnet::DnsTxtRecordItem(std::string("Key"), kValueData.data(), kValueData.size());
  EXPECT_EQ(item.key, "Key");
  EXPECT_EQ(item.value, kValueData);
}

TEST(BrokerDnsTxtRecordItem, NormalValueConstructor)
{
  const std::vector<uint8_t> kValueData = {1, 2, 3, 4, 5, 6};
  const auto                 item = rdmnet::DnsTxtRecordItem(std::string("Key"), kValueData);
  EXPECT_EQ(item.key, "Key");
  EXPECT_EQ(item.value, kValueData);
}
