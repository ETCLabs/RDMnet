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

class TestLwMdnsDomainParsing : public testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    ASSERT_EQ(lwmdns_common_module_init(), kEtcPalErrOk);
  }

  void TearDown() override { lwmdns_common_module_deinit(); }
};

TEST_F(TestLwMdnsDomainParsing, ParsesNormalDomainName)
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

  EXPECT_EQ(lwmdns_parse_domain_name(msg, msg, sizeof(msg)), msg + sizeof(msg));
}

TEST_F(TestLwMdnsDomainParsing, ParsesDomainNameWithPointer)
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

  EXPECT_EQ(lwmdns_parse_domain_name(msg, &msg[49], sizeof(msg) - 49), msg + sizeof(msg));
}

TEST_F(TestLwMdnsDomainParsing, HandlesMalformedDomainNameTooShort)
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

  EXPECT_EQ(lwmdns_parse_domain_name(msg, msg, sizeof(msg)), nullptr);
}

TEST_F(TestLwMdnsDomainParsing, HandlesMalformedDomainNameMissingNull)
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

  EXPECT_EQ(lwmdns_parse_domain_name(msg, msg, sizeof(msg)), nullptr);
}

TEST_F(TestLwMdnsDomainParsing, CopiesNormalDomainName)
{
  const uint8_t msg[] = {
      13, 116, 101, 115, 116, 45,  104, 111, 115, 116, 110, 97, 109, 101,  // test-hostname
      5,  108, 111, 99,  97,  108, 0                                       // local
  };
  uint8_t msg_buf[sizeof(msg)];

  EXPECT_EQ(lwmdns_copy_domain_name(msg, msg, msg_buf), sizeof(msg));
  EXPECT_EQ(std::memcmp(msg, msg_buf, sizeof(msg)), 0);
}

TEST_F(TestLwMdnsDomainParsing, CopiesDomainNameWithPointer)
{
  const uint8_t msg[] = {
      0,    0,    0,   0,   0,   0,   0,   0,  // Filler
      5,    108,  111, 99,  97,  108, 0,       // local

      0,    0,    0,   0,   0,   0,   0,   0,                                 // Filler
      13,   116,  101, 115, 116, 45,  104, 111, 115, 116, 110, 97, 109, 101,  // test-hostname
      0xc0, 0x08,                                                             // Pointer
  };
  const uint8_t validation_msg[] = {
      13, 116, 101, 115, 116, 45,  104, 111, 115, 116, 110, 97, 109, 101,  // test-hostname
      5,  108, 111, 99,  97,  108, 0,                                      // local
  };
  uint8_t msg_buf[DNS_FQDN_MAX_LENGTH];

  EXPECT_EQ(lwmdns_copy_domain_name(msg, &msg[23], msg_buf), 21u);
  EXPECT_EQ(std::memcmp(validation_msg, msg_buf, sizeof(validation_msg)), 0);
}

TEST_F(TestLwMdnsDomainParsing, DoesNotCopyDomainNameTooLong)
{
  const uint8_t msg[] = {
      28,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,
      108, 111, 110, 103, 108, 111, 110, 103, 110, 97,  109, 101,  // longlonglonglonglonglongname
      28,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,
      108, 111, 110, 103, 108, 111, 110, 103, 110, 97,  109, 101,  // longlonglonglonglonglongname
      28,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,
      108, 111, 110, 103, 108, 111, 110, 103, 110, 97,  109, 101,  // longlonglonglonglonglongname
      28,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,
      108, 111, 110, 103, 108, 111, 110, 103, 110, 97,  109, 101,  // longlonglonglonglonglongname
      28,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,
      108, 111, 110, 103, 108, 111, 110, 103, 110, 97,  109, 101,  // longlonglonglonglonglongname
      28,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,
      108, 111, 110, 103, 108, 111, 110, 103, 110, 97,  109, 101,  // longlonglonglonglonglongname
      28,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,
      108, 111, 110, 103, 108, 111, 110, 103, 110, 97,  109, 101,  // longlonglonglonglonglongname
      28,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,
      108, 111, 110, 103, 108, 111, 110, 103, 110, 97,  109, 101,  // longlonglonglonglonglongname
      16,  108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103, 108, 111, 110, 103,  // longlonglonglonglo
      5,   108, 111, 99,  97,  108, 0                                                       // local
  };
  uint8_t msg_buf[DNS_FQDN_MAX_LENGTH];

  EXPECT_EQ(lwmdns_copy_domain_name(msg, msg, msg_buf), 0u);
}

TEST_F(TestLwMdnsDomainParsing, DomainNameLengthWorks)
{
  const uint8_t msg[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  116, 99,  112,                                // _tcp
      5,  108, 111, 99,  97,  108, 0                         // local
  };

  EXPECT_EQ(lwmdns_domain_name_length(msg, msg), 42u);
}

TEST_F(TestLwMdnsDomainParsing, DomainNameLengthWorksWithPointer)
{
  const uint8_t msg[] = {
      0,    0,    0,   0,   0,   0,   0,   0,    // Filler
      7,    95,   114, 100, 109, 110, 101, 116,  // _rdmnet
      4,    95,   116, 99,  112,                 // _tcp
      5,    108,  111, 99,  97,  108, 0,         // local

      0,    0,    0,   0,   0,   0,   0,   0,  // Filler
      21,   84,   101, 115, 116, 32,  83,  101, 114, 118, 105,
      99,   101,  32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      0xc0, 0x08,                                               // Pointer
  };

  EXPECT_EQ(lwmdns_domain_name_length(msg, &msg[36]), 42u);
}

TEST_F(TestLwMdnsDomainParsing, DomainNamesEqualWorks)
{
  const uint8_t msg1[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  116, 99,  112,                                // _tcp
      5,  108, 111, 99,  97,  108, 0                         // local
  };
  uint8_t msg2[sizeof(msg1)];
  memcpy(msg2, msg1, sizeof(msg2));

  EXPECT_TRUE(lwmdns_domain_names_equal(msg1, msg1, msg2, msg2));

  const uint8_t msg3[] = {
      21,  84,  101, 115, 116, 32,  83,  101, 114, 118, 105, 99,
      101, 32,  73,  110, 115, 116, 97,  110, 99,  101,            // Test Service Instance
      11,  95,  110, 111, 116, 45,  114, 100, 109, 110, 101, 116,  // _not-rdmnet
      4,   95,  116, 99,  112,                                     // _tcp
      5,   108, 111, 99,  97,  108, 0                              // local
  };

  EXPECT_FALSE(lwmdns_domain_names_equal(msg1, msg1, msg3, msg3));
}

TEST_F(TestLwMdnsDomainParsing, DomainNamesEqualWorksWithPointer)
{
  const uint8_t msg1[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  116, 99,  112,                                // _tcp
      5,  108, 111, 99,  97,  108, 0                         // local
  };
  const uint8_t msg1_pointer[] = {
      0,    0,   0,   0,   0,   0,   0,   0,  // Filler
      5,    108, 111, 99,  97,  108, 0,       // local
      0,    0,   0,   0,   0,   0,   0,   0,  // Filler
      21,   84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99,   101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,    95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,    95,  116, 99,  112,                                // _tcp
      0xc0, 0x08                                               // Pointer
  };

  EXPECT_TRUE(lwmdns_domain_names_equal(msg1, msg1, msg1_pointer, &msg1_pointer[23]));

  const uint8_t msg2[] = {
      0,    0,   0,   0,   0,   0,   0,   0,  // Filler
      5,    108, 111, 99,  97,  108, 0,       // local
      0,    0,   0,   0,   0,   0,   0,   0,  // Filler
      21,   84,  101, 115, 116, 32,  83,  101, 114, 118, 105, 99,
      101,  32,  73,  110, 115, 116, 97,  110, 99,  101,            // Test Service Instance
      11,   95,  110, 111, 116, 45,  114, 100, 109, 110, 101, 116,  // _not-rdmnet
      4,    95,  116, 99,  112,                                     // _tcp
      0xc0, 0x08                                                    // Pointer
  };

  EXPECT_FALSE(lwmdns_domain_names_equal(msg1, msg1, msg2, &msg2[23]));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceInstanceWorks)
{
  const uint8_t msg[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  116, 99,  112,                                // _tcp
      5,  108, 111, 99,  97,  108, 0                         // local
  };

  EXPECT_TRUE(lwmdns_domain_name_matches_service_instance(msg, msg, "Test Service Instance"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(msg, msg, "Test Service Instanc"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(msg, msg, "Test Service Instance Extra"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(msg, msg, "Not Test Service Instance"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceInstanceFailsWithNonLocalDomain)
{
  const uint8_t msg[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  116, 99,  112,                                // _tcp
      9,  100, 110, 115, 109, 105, 114, 114, 111, 114,       // dnsmirror
      7,  101, 120, 97,  109, 112, 108, 101,                 // example
      3,  99,  111, 109, 0                                   // com
  };

  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(msg, msg, "Test Service Instance"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceInstanceFailsWithNonRdmnetServices)
{
  const uint8_t msg[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  117, 100, 112,                                // _udp
      5,  108, 111, 99,  97,  108, 0                         // local
  };
  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(msg, msg, "Test Service Instance"));

  const uint8_t msg2[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      5,  95,  104, 116, 116, 112,                           // _http
      4,  95,  116, 99,  112,                                // _tcp
      5,  108, 111, 99,  97,  108, 0                         // local
  };
  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(msg2, msg2, "Test Service Instance"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceInstanceWorksWithStartPtr)
{
  const uint8_t msg[] = {
      21,   84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99,   101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,    95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,    95,  116, 99,  112,                                // _tcp
      5,    108, 111, 99,  97,  108, 0,                        // local
      0,    0,   0,   0,   0,   0,   0,   0,                   // Filler
      0xc0, 0x00                                               // Pointer
  };

  EXPECT_TRUE(lwmdns_domain_name_matches_service_instance(msg, &msg[50], "Test Service Instance"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceInstanceWorksWithIntermediatePtr)
{
  const uint8_t msg[] = {
      0,    0,    0,   0,   0,   0,   0,   0,    // Filler
      7,    95,   114, 100, 109, 110, 101, 116,  // _rdmnet
      4,    95,   116, 99,  112,                 // _tcp
      5,    108,  111, 99,  97,  108, 0,         // local

      0,    0,    0,   0,   0,   0,   0,   0,  // Filler
      21,   84,   101, 115, 116, 32,  83,  101, 114, 118, 105,
      99,   101,  32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      0xc0, 0x08,                                               // Pointer
  };

  EXPECT_TRUE(lwmdns_domain_name_matches_service_instance(msg, &msg[36], "Test Service Instance"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceInstanceHandlesInvalid)
{
  const uint8_t msg[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  116, 99,  112,                                // _tcp
      5,  108, 111, 99,  97,  108, 0                         // local
  };

  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(NULL, msg, "Test Service Instance"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(msg, NULL, "Test Service Instance"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_instance(msg, msg, NULL));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceSubtypeWorks)
{
  const uint8_t msg[] = {
      8, 95,  100, 101, 102, 97,  117, 108, 116,  // _default
      4, 95,  115, 117, 98,                       // _sub
      7, 95,  114, 100, 109, 110, 101, 116,       // _rdmnet
      4, 95,  116, 99,  112,                      // _tcp
      5, 108, 111, 99,  97,  108, 0               // local
  };

  EXPECT_TRUE(lwmdns_domain_name_matches_service_subtype(msg, msg, "default"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg, msg, "defaul"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg, msg, "default extra"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg, msg, "not default"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceSubtypeFailsWithNonLocalDomain)
{
  const uint8_t msg[] = {
      8, 95,  100, 101, 102, 97,  117, 108, 116,       // _default
      4, 95,  115, 117, 98,                            // _sub
      7, 95,  114, 100, 109, 110, 101, 116,            // _rdmnet
      4, 95,  116, 99,  112,                           // _tcp
      9, 100, 110, 115, 109, 105, 114, 114, 111, 114,  // dnsmirror
      7, 101, 120, 97,  109, 112, 108, 101,            // example
      3, 99,  111, 109, 0                              // com
  };
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg, msg, "default"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceSubtypeFailsWithNonRdmnetServices)
{
  const uint8_t msg[] = {
      8, 95,  100, 101, 102, 97,  117, 108, 116,  // _default
      4, 95,  115, 117, 98,                       // _sub
      7, 95,  114, 100, 109, 110, 101, 116,       // _rdmnet
      4, 95,  117, 100, 112,                      // _udp
      5, 108, 111, 99,  97,  108, 0               // local
  };
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg, msg, "default"));

  const uint8_t msg2[] = {
      8, 95,  100, 101, 102, 97,  117, 108, 116,  // _default
      4, 95,  115, 117, 98,                       // _sub
      5, 95,  104, 116, 116, 112,                 // _http
      4, 95,  116, 99,  112,                      // _tcp
      5, 108, 111, 99,  97,  108, 0               // local
  };
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg2, msg2, "default"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceSubtypeFailsWithoutSub)
{
  const uint8_t msg[] = {
      8, 95,  100, 101, 102, 97,  117, 108, 116,  // _default
      7, 95,  114, 100, 109, 110, 101, 116,       // _rdmnet
      4, 95,  116, 99,  112,                      // _tcp
      5, 108, 111, 99,  97,  108, 0               // local
  };
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg, msg, "default"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceSubtypeWorksWithStartPtr)
{
  const uint8_t msg[] = {
      8,    95,  100, 101, 102, 97,  117, 108, 116,  // _default
      4,    95,  115, 117, 98,                       // _sub
      7,    95,  114, 100, 109, 110, 101, 116,       // _rdmnet
      4,    95,  116, 99,  112,                      // _tcp
      5,    108, 111, 99,  97,  108, 0,              // local
      0,    0,   0,   0,   0,   0,   0,   0,         // Filler
      0xc0, 0x00                                     // Pointer
  };

  EXPECT_TRUE(lwmdns_domain_name_matches_service_subtype(msg, &msg[42], "default"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceSubtypeWorksWithIntermediatePtr)
{
  const uint8_t msg[] = {
      0,    0,    0,   0,   0,   0,   0,   0,    // Filler
      7,    95,   114, 100, 109, 110, 101, 116,  // _rdmnet
      4,    95,   116, 99,  112,                 // _tcp
      5,    108,  111, 99,  97,  108, 0,         // local

      0,    0,    0,   0,   0,   0,   0,   0,         // Filler
      8,    95,   100, 101, 102, 97,  117, 108, 116,  // _default
      4,    95,   115, 117, 98,                       // _sub
      0xc0, 0x08,                                     // Pointer
  };

  EXPECT_TRUE(lwmdns_domain_name_matches_service_subtype(msg, &msg[36], "default"));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameMatchesServiceSubtypeHandlesInvalid)
{
  const uint8_t msg[] = {
      8, 95,  100, 101, 102, 97,  117, 108, 116,  // _default
      4, 95,  115, 117, 98,                       // _sub
      7, 95,  114, 100, 109, 110, 101, 116,       // _rdmnet
      4, 95,  116, 99,  112,                      // _tcp
      5, 108, 111, 99,  97,  108, 0               // local
  };

  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(NULL, msg, "default"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg, NULL, "default"));
  EXPECT_FALSE(lwmdns_domain_name_matches_service_subtype(msg, msg, NULL));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameLabelToStringWorks)
{
  const uint8_t msg[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  116, 99,  112,                                // _tcp
      5,  108, 111, 99,  97,  108, 0                         // local
  };
  char str_buf[64];

  EXPECT_TRUE(lwmdns_domain_label_to_string(msg, msg, str_buf));
  EXPECT_STREQ(str_buf, "Test Service Instance");

  const uint8_t msg2[] = {0};
  EXPECT_FALSE(lwmdns_domain_label_to_string(msg2, msg2, str_buf));

  // Invalid length
  const uint8_t msg3[] = {
      64, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
  };
  EXPECT_FALSE(lwmdns_domain_label_to_string(msg3, msg3, str_buf));
}

TEST_F(TestLwMdnsDomainParsing, DomainNameLabelToStringWorksWithPointer)
{
  const uint8_t msg[] = {
      0,    0,   0,   0,   0,   0,   0,   0,  // Filler
      21,   84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99,   101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,    95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,    95,  116, 99,  112,                                // _tcp
      5,    108, 111, 99,  97,  108, 0,                        // local
      0,    0,   0,   0,   0,   0,   0,   0,                   // Filler
      0xc0, 0x08                                               // Pointer
  };
  char str_buf[64];

  EXPECT_TRUE(lwmdns_domain_label_to_string(msg, &msg[58], str_buf));
  EXPECT_STREQ(str_buf, "Test Service Instance");
}
