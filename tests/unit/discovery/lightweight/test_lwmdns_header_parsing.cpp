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

#include "gtest/gtest.h"
#include "etcpal_mock/common.h"

class TestLwMdnsHeaderParsing : public testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    ASSERT_EQ(lwmdns_common_module_init(), kEtcPalErrOk);
  }

  void TearDown() override { lwmdns_common_module_deinit(); }
};

TEST_F(TestLwMdnsHeaderParsing, ParsesNormalAnswerRR)
{
  // _default._sub._rdmnet._tcp.local PTR Test Service Instance._rdmnet._tcp.local
  uint8_t msg[] = {
      // DNS header
      0, 0,        // Transaction ID: 0
      0x84, 0x00,  // Flags: Standard query response, not truncated, no error.
      0, 0,        // Question count: 0
      0, 1,        // Answer count: 1
      0, 0,        // Authority count: 0
      0, 0,        // Additional count: 0

      // RR name
      8, 95, 100, 101, 102, 97, 117, 108, 116,  // _default
      4, 95, 115, 117, 98,                      // _sub
      7, 95, 114, 100, 109, 110, 101, 116,      // _rdmnet
      4, 95, 116, 99, 112,                      // _tcp
      5, 108, 111, 99, 97, 108, 0,              // local
      // RR header
      0x00, 0x0c,    // Type: PTR
      0x80, 0x01,    // Class IN, cache flush = true
      0, 0, 0, 120,  // TTL: 120 (2 minutes)
      0, 24,         // Data length
      21, 84, 101, 115, 116, 32, 83, 101, 114, 118, 105, 99, 101, 32, 73, 110, 115, 116, 97, 110, 99,
      101,        // Test Service Instance
      0xc0, 0x1b  // Pointer: _rdmnet._tcp.local
  };

  DnsResourceRecord rr;
  EXPECT_EQ(lwmdns_parse_resource_record(msg, &msg[12], sizeof(msg) - 12, &rr), msg + sizeof(msg));
  EXPECT_EQ(rr.name, &msg[12]);
  EXPECT_EQ(rr.record_type, kDnsRecordTypePTR);
  EXPECT_EQ(rr.cache_flush, true);
  EXPECT_EQ(rr.ttl, 120u);
  EXPECT_EQ(rr.data_len, 24u);
  EXPECT_EQ(rr.data_ptr, &msg[56]);
}
