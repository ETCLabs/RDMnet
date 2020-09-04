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

#include "lwmdns_send.h"

#include "gtest/gtest.h"
#include "fff.h"
#include "etcpal/inet.h"
#include "etcpal/pack.h"
#include "etcpal_mock/timer.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "rdmnet_mock/core/mcast.h"
#include "rdmnet/disc/discovered_broker.h"
#include "rdmnet/disc/monitored_scope.h"
#include "lwmdns_common.h"
#include "fake_mcast.h"

class TestLwMdnsSendInit : public testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    rc_mcast_reset_all_fakes();
    SetUpFakeMcastEnvironment();
  }
};

TEST_F(TestLwMdnsSendInit, InitWorksWithNoConfig)
{
  ASSERT_EQ(lwmdns_send_module_init(nullptr), kEtcPalErrOk);
  EXPECT_EQ(rc_mcast_get_send_socket_fake.call_count, kFakeNetints.size());

  for (size_t i = 0; i < kFakeNetints.size(); ++i)
  {
    EXPECT_EQ(rc_mcast_get_send_socket_fake.arg1_history[i], E133_MDNS_PORT);
    EXPECT_NE(rc_mcast_get_send_socket_fake.arg2_history[i], nullptr);
  }

  lwmdns_send_module_deinit();
}

TEST_F(TestLwMdnsSendInit, InitWorksWithConfig)
{
  RdmnetNetintConfig netint_config;
  netint_config.netints = kFakeNetints.data();
  netint_config.num_netints = 1;

  rc_mcast_get_send_socket_fake.custom_fake = [](const EtcPalMcastNetintId* netint_id, uint16_t source_port,
                                                 etcpal_socket_t* socket) {
    EXPECT_EQ(netint_id->index, kFakeNetints[0].index);
    EXPECT_EQ(netint_id->ip_type, kFakeNetints[0].ip_type);
    EXPECT_EQ(source_port, E133_MDNS_PORT);
    *socket = (etcpal_socket_t)0;
    return kEtcPalErrOk;
  };
  ASSERT_EQ(lwmdns_send_module_init(&netint_config), kEtcPalErrOk);
  EXPECT_EQ(rc_mcast_get_send_socket_fake.call_count, 1u);
  lwmdns_send_module_deinit();
}

class TestLwMdnsSend : public testing::Test
{
protected:
  RdmnetScopeMonitorRef*      monitor_ref_;
  static std::vector<uint8_t> sent_data_;

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    rc_mcast_reset_all_fakes();
    SetUpFakeMcastEnvironment();
    ASSERT_EQ(discovered_broker_module_init(), kEtcPalErrOk);
    ASSERT_EQ(monitored_scope_module_init(), kEtcPalErrOk);
    ASSERT_EQ(lwmdns_common_module_init(), kEtcPalErrOk);
    ASSERT_EQ(lwmdns_send_module_init(nullptr), kEtcPalErrOk);

    RdmnetScopeMonitorConfig config = RDMNET_SCOPE_MONITOR_CONFIG_DEFAULT_INIT;
    monitor_ref_ = scope_monitor_new(&config);
    scope_monitor_insert(monitor_ref_);

    sent_data_.clear();
    etcpal_sendto_fake.custom_fake = [](etcpal_socket_t, const void* data, size_t size, int, const EtcPalSockAddr*) {
      if (sent_data_.empty())
        sent_data_.assign(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size);
      return static_cast<int>(size);
    };
  }

  void TearDown() override
  {
    scope_monitor_remove(monitor_ref_);
    scope_monitor_delete(monitor_ref_);
    lwmdns_send_module_deinit();
    lwmdns_common_module_deinit();
    monitored_scope_module_deinit();
  }
};

std::vector<uint8_t> TestLwMdnsSend::sent_data_;

TEST_F(TestLwMdnsSend, SendPtrQueryWorks)
{
  lwmdns_send_ptr_query(monitor_ref_);
  EXPECT_EQ(etcpal_sendto_fake.call_count, kFakeNetints.size());

  // Check the data
  // DNS header: 12 bytes, query name _default._sub._rdmnet._tcp.local, 34 bytes, Query fields 4 bytes
  ASSERT_EQ(sent_data_.size(), 50u);
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[2]), 0u);   // DNS header flags should be all 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[4]), 1u);   // Question count: 1
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[6]), 0u);   // Answer count: 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[8]), 0u);   // Authority count: 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[10]), 0u);  // Additional count: 0

  const uint8_t kQueryName[] = {
      8, 95,  100, 101, 102, 97,  117, 108, 116,  // _default
      4, 95,  115, 117, 98,                       // _sub
      7, 95,  114, 100, 109, 110, 101, 116,       // _rdmnet
      4, 95,  116, 99,  112,                      // _tcp
      5, 108, 111, 99,  97,  108, 0               // local
  };
  EXPECT_EQ(std::memcmp(&sent_data_[12], kQueryName, sizeof kQueryName), 0);
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[46]), 12u);      // Query Type PTR
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[48]), 0x8001u);  // QU question (first query), class IN
}

TEST_F(TestLwMdnsSend, SendsQMQuestionOnRetransmission)
{
  monitor_ref_->platform_data.sent_first_query = true;
  lwmdns_send_ptr_query(monitor_ref_);

  ASSERT_EQ(sent_data_.size(), 50u);
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[48]), 0x0001u);
}

TEST_F(TestLwMdnsSend, SendPtrQueryWorksWithKnownAnswers)
{
  etcpal_getms_fake.return_val = 20000;

  auto db = discovered_broker_new(monitor_ref_, "Test Service Instance", "");
  db->platform_data.ttl_timer.interval = 120 * 1000;
  db->platform_data.ttl_timer.reset_time = 0;
  discovered_broker_insert(&monitor_ref_->broker_list, db);

  auto db2 = discovered_broker_new(monitor_ref_, "Test Service Instance 2", "");
  db2->platform_data.ttl_timer.interval = 1000 * 1000;
  db2->platform_data.ttl_timer.reset_time = 0;
  discovered_broker_insert(&monitor_ref_->broker_list, db2);

  lwmdns_send_ptr_query(monitor_ref_);
  EXPECT_EQ(etcpal_sendto_fake.call_count, kFakeNetints.size());

  // Base size 50, plus size of known answers 36 and 38
  ASSERT_EQ(sent_data_.size(), 124u);
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[2]), 0u);   // DNS header flags should be all 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[4]), 1u);   // Question count: 1
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[6]), 2u);   // Answer count: 1
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[8]), 0u);   // Authority count: 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[10]), 0u);  // Additional count: 0

  const uint8_t kKnownAnswer1[] = {
      0xc0, 0x0c,            // Pointer to _default._sub._rdmnet._tcp.local
      0,    12,              // Type PTR
      0,    1,               // Type IN, cache flush false
      0,    0,    0,   100,  // TTL 100 seconds
      0,    24,              // Data length

      21,   84,   101, 115, 116, 32,  83,  101, 114, 118, 105,
      99,   101,  32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      0xc0, 0x1a,                                               // Pointer to _rdmnet._tcp.local
  };
  const uint8_t kKnownAnswer2[] = {
      0xc0, 0x0c,              // Pointer to _default._sub._rdmnet._tcp.local
      0,    12,                // Type PTR
      0,    1,                 // Type IN, cache flush false
      0,    0,    0x03, 0xd4,  // TTL 980 seconds
      0,    26,                // Data length

      23,   84,   101,  115,  116, 32,  83, 101, 114, 118, 105, 99,
      101,  32,   73,   110,  115, 116, 97, 110, 99,  101, 32,  50,  // Test Service Instance 2
      0xc0, 0x1a,                                                    // Pointer to _rdmnet._tcp.local
  };
  EXPECT_EQ(std::memcmp(&sent_data_[50], kKnownAnswer1, sizeof kKnownAnswer1), 0);
  EXPECT_EQ(std::memcmp(&sent_data_[86], kKnownAnswer2, sizeof kKnownAnswer2), 0);
}

TEST_F(TestLwMdnsSend, SendAnyQueryOnServiceWorks)
{
  auto db = discovered_broker_new(monitor_ref_, "Test Service Instance", "");
  lwmdns_send_any_query_on_service(db);

  EXPECT_EQ(etcpal_sendto_fake.call_count, kFakeNetints.size());

  // Check the data
  // DNS header: 12 bytes, query name Test Service Instance._rdmnet._tcp.local, 42 bytes, Query fields 4 bytes
  ASSERT_EQ(sent_data_.size(), 58u);
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[2]), 0u);   // DNS header flags should be all 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[4]), 1u);   // Question count: 1
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[6]), 0u);   // Answer count: 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[8]), 0u);   // Authority count: 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[10]), 0u);  // Additional count: 0

  const uint8_t kQueryName[] = {
      21, 84,  101, 115, 116, 32,  83,  101, 114, 118, 105,
      99, 101, 32,  73,  110, 115, 116, 97,  110, 99,  101,  // Test Service Instance
      7,  95,  114, 100, 109, 110, 101, 116,                 // _rdmnet
      4,  95,  116, 99,  112,                                // _tcp
      5,  108, 111, 99,  97,  108, 0                         // local
  };
  EXPECT_EQ(std::memcmp(&sent_data_[12], kQueryName, sizeof kQueryName), 0);
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[54]), 255u);     // Query Type ANY
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[56]), 0x8001u);  // QU question (first query), class IN

  discovered_broker_delete(db);
}

TEST_F(TestLwMdnsSend, SendAnyQueryOnHostnameWorks)
{
  const uint8_t kHostname[] = {
      13, 116, 101, 115, 116, 45,  104, 111, 115, 116, 110, 97, 109, 101,  // test-hostname
      5,  108, 111, 99,  97,  108, 0                                       // local
  };

  auto db = discovered_broker_new(monitor_ref_, "Test Service Instance", "");
  db->platform_data.srv_record_received = true;
  memcpy(db->platform_data.wire_host_name, kHostname, sizeof kHostname);
  lwmdns_send_any_query_on_hostname(db);

  EXPECT_EQ(etcpal_sendto_fake.call_count, kFakeNetints.size());

  // Check the data
  // DNS header: 12 bytes, query name test-hostname.local, 21 bytes, Query fields 4 bytes
  ASSERT_EQ(sent_data_.size(), 37u);
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[2]), 0u);   // DNS header flags should be all 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[4]), 1u);   // Question count: 1
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[6]), 0u);   // Answer count: 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[8]), 0u);   // Authority count: 0
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[10]), 0u);  // Additional count: 0

  EXPECT_EQ(std::memcmp(&sent_data_[12], kHostname, sizeof kHostname), 0);
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[33]), 255u);     // Query Type ANY
  EXPECT_EQ(etcpal_unpack_u16b(&sent_data_[35]), 0x8001u);  // QU question (first query), class IN

  discovered_broker_delete(db);
}
