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

#include <array>
#include "gtest/gtest.h"
#include "etcpal_mock/common.h"

class TestLwMdnsCommon : public testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    ASSERT_EQ(lwmdns_common_module_init(), kEtcPalErrOk);
  }

  void TearDown() override { lwmdns_common_module_deinit(); }
};

TEST_F(TestLwMdnsCommon, ParsesNormalDomainName)
{
  // clang-format off
  const uint8_t msg[] = {
    0x08, 0x5f, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, // _default
    0x04, 0x5f, 0x73, 0x75, 0x62,                         // _sub
    0x07, 0x5f, 0x72, 0x64, 0x6d, 0x6e, 0x65, 0x74,       // _rdmnet
    0x04, 0x5f, 0x74, 0x63, 0x70,                         // _tcp
    0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c,                   // local
    0x00
  };
  // clang-format on

  DnsDomainName name;
  EXPECT_EQ(lwmdns_parse_domain_name(msg, msg, sizeof(msg), &name), msg + sizeof(msg));
  EXPECT_EQ(name.name, msg);
  EXPECT_EQ(name.name_ptr, nullptr);
}

TEST_F(TestLwMdnsCommon, ParsesDomainNameWithPointer)
{
  // clang-format off
  const uint8_t msg[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Filler
    0x08, 0x5f, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, // _default
    0x04, 0x5f, 0x73, 0x75, 0x62,                         // _sub
    0x07, 0x5f, 0x72, 0x64, 0x6d, 0x6e, 0x65, 0x74,       // _rdmnet
    0x04, 0x5f, 0x74, 0x63, 0x70,                         // _tcp
    0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c,                   // local

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Filler

    // RDMnet Broker Instance
    0x16, 0x52, 0x44, 0x4D, 0x6E, 0x65, 0x74, 0x20, 0x42, 0x72, 0x6F, 0x6B, 0x65, 0x72, 0x20, 0x49, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x63, 0x65,
    // Pointer
    0xc0, 0x16,
  };
  // clang-format on

  DnsDomainName name;
  EXPECT_EQ(lwmdns_parse_domain_name(msg, &msg[49], sizeof(msg) - 49, &name), msg + sizeof(msg));
  EXPECT_EQ(name.name, &msg[49]);
  EXPECT_EQ(name.name_ptr, &msg[22]);
}

TEST_F(TestLwMdnsCommon, HandlesMalformedDomainNameTooShort)
{
  // clang-format off
  const uint8_t msg[] = {
    0x08, 0x5f, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, // _default
    0x04, 0x5f, 0x73, 0x75, 0x62,                         // _sub
    0x07, 0x5f, 0x72, 0x64, 0x6d, 0x6e, 0x65, 0x74,       // _rdmnet
    0x04, 0x5f, 0x74, 0x63, 0x70,                         // _tcp
    0x05, 0x6c,                                           // local (truncated)
  };
  // clang-format on

  DnsDomainName name;
  EXPECT_EQ(lwmdns_parse_domain_name(msg, msg, sizeof(msg), &name), nullptr);
}

TEST_F(TestLwMdnsCommon, HandlesMalformedDomainNameMissingNull)
{
  // clang-format off
  const uint8_t msg[] = {
    0x08, 0x5f, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, // _default
    0x04, 0x5f, 0x73, 0x75, 0x62,                         // _sub
    0x07, 0x5f, 0x72, 0x64, 0x6d, 0x6e, 0x65, 0x74,       // _rdmnet
    0x04, 0x5f, 0x74, 0x63, 0x70,                         // _tcp
    0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c,                   // local
  };
  // clang-format on

  DnsDomainName name;
  EXPECT_EQ(lwmdns_parse_domain_name(msg, msg, sizeof(msg), &name), nullptr);
}
