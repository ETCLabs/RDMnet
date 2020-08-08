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

#include "lwmdns_recv.h"

#include <cstring>
#include "gtest/gtest.h"
#include "fff.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet_mock/core/mcast.h"
#include "rdmnet_mock/core/common.h"
#include "rdmnet/disc/common.h"
#include "rdmnet/disc/monitored_scope.h"
#include "rdmnet/disc/discovered_broker.h"
#include "lwmdns_common.h"
#include "fake_mcast.h"

RCPolledSocketInfo            recv_socket_info;
static const etcpal::SockAddr recvfrom_addr(etcpal::IpAddr::FromString("192.168.1.1"), 5353);

class TestLwMdnsRecv : public testing::Test
{
protected:
  RdmnetScopeMonitorRef*      monitor_ref_;
  static std::vector<uint8_t> data_to_recv_;

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    rc_mcast_reset_all_fakes();
    rdmnet_mock_core_reset_and_init();
    SetUpFakeMcastEnvironment();
    recv_socket_info = RCPolledSocketInfo{};

    data_to_recv_.clear();
    rc_add_polled_socket_fake.custom_fake = [](etcpal_socket_t, etcpal_poll_events_t events,
                                               RCPolledSocketInfo* socket_info) {
      EXPECT_TRUE(events & ETCPAL_POLL_IN);
      recv_socket_info = *socket_info;
      return kEtcPalErrOk;
    };
    etcpal_recvfrom_fake.custom_fake = [](etcpal_socket_t, void* buffer, size_t length, int, EtcPalSockAddr* address) {
      EXPECT_LE(data_to_recv_.size(), length);
      std::memcpy(buffer, data_to_recv_.data(), data_to_recv_.size());
      *address = recvfrom_addr.get();
      return static_cast<int>(data_to_recv_.size());
    };

    ASSERT_EQ(rdmnet_disc_module_init(nullptr), kEtcPalErrOk);

    ASSERT_GT(rc_add_polled_socket_fake.call_count, 0u);
    ASSERT_NE(recv_socket_info.callback, nullptr);

    RdmnetScopeMonitorConfig config = RDMNET_SCOPE_MONITOR_CONFIG_DEFAULT_INIT;
    monitor_ref_ = scope_monitor_new(&config);
    scope_monitor_insert(monitor_ref_);
  }

  void TearDown() override
  {
    scope_monitor_remove(monitor_ref_);
    scope_monitor_delete(monitor_ref_);
    rdmnet_disc_module_deinit();
  }
};

std::vector<uint8_t> TestLwMdnsRecv::data_to_recv_;

TEST_F(TestLwMdnsRecv, HandlesPtrRecordProperly)
{
  EXPECT_EQ(monitor_ref_->broker_list, nullptr);

  data_to_recv_ = {
      0, 0,        // Transaction ID
      0x84, 0x00,  // Flags: Standard query response, no error
      0, 0,        // Question count: 0
      0, 1,        // Answer count: 1
      0, 0,        // Authority count: 0
      0, 0,        // Additional count: 0

      // Start PTR record
      // Name
      8, 95, 100, 101, 102, 97, 117, 108, 116,  // _default
      4, 95, 115, 117, 98,                      // _sub
      7, 95, 114, 100, 109, 110, 101, 116,      // _rdmnet
      4, 95, 116, 99, 112,                      // _tcp
      5, 108, 111, 99, 97, 108, 0,              // local

      0, 12,                                              // Type: PTR
      0, 1,                                               // class IN, cache flush false
      0, 0, 0, 120,                                       // TTL 120 seconds
      0, 24,                                              // Data length
      21, 84, 101, 115, 116, 32, 83, 101, 114, 118, 105,  //
      99, 101, 32, 73, 110, 115, 116, 97, 110, 99, 101,   // Test Service Instance
      0xc0, 0x1a                                          // Pointer to _rdmnet._tcp.local
  };

  EtcPalPollEvent event{};
  event.events = ETCPAL_POLL_IN;
  recv_socket_info.callback(&event, recv_socket_info.data);

  // We should add a discovered broker to the list
  ASSERT_NE(monitor_ref_->broker_list, nullptr);
  DiscoveredBroker* db = monitor_ref_->broker_list;
  EXPECT_STREQ(db->service_instance_name, "Test Service Instance");
  EXPECT_EQ(db->platform_data.ttl_timer.interval, 120u * 1000u);
}

// A zero-TTL PTR record should remove the broker from the list.
TEST_F(TestLwMdnsRecv, HandlesPtrRecordZeroTTL)
{
  data_to_recv_ = {
      0, 0,        // Transaction ID
      0x84, 0x00,  // Flags: Standard query response, no error
      0, 0,        // Question count: 0
      0, 1,        // Answer count: 1
      0, 0,        // Authority count: 0
      0, 0,        // Additional count: 0

      // Start PTR record
      // Name
      8, 95, 100, 101, 102, 97, 117, 108, 116,  // _default
      4, 95, 115, 117, 98,                      // _sub
      7, 95, 114, 100, 109, 110, 101, 116,      // _rdmnet
      4, 95, 116, 99, 112,                      // _tcp
      5, 108, 111, 99, 97, 108, 0,              // local

      0, 12,                                              // Type: PTR
      0, 1,                                               // class IN, cache flush false
      0, 0, 0, 0,                                         // TTL 0 seconds
      0, 24,                                              // Data length
      21, 84, 101, 115, 116, 32, 83, 101, 114, 118, 105,  //
      99, 101, 32, 73, 110, 115, 116, 97, 110, 99, 101,   // Test Service Instance
      0xc0, 0x1a                                          // Pointer to _rdmnet._tcp.local
  };

  EtcPalPollEvent event{};
  event.events = ETCPAL_POLL_IN;
  recv_socket_info.callback(&event, recv_socket_info.data);

  // Receiving a message with zero TTL, when there are no brokers, should not add one.
  ASSERT_EQ(monitor_ref_->broker_list, nullptr);

  DiscoveredBroker* db = discovered_broker_new(monitor_ref_, "Test Service Instance", "");
  ASSERT_NE(db, nullptr);
  discovered_broker_insert(&monitor_ref_->broker_list, db);
  EXPECT_EQ(db->platform_data.destruction_pending, false);

  recv_socket_info.callback(&event, recv_socket_info.data);

  // The broker should now be marked for destruction.
  EXPECT_EQ(db->platform_data.destruction_pending, true);
}

TEST_F(TestLwMdnsRecv, HandlesMultipleServiceRecordsProperly)
{
  DiscoveredBroker* db = discovered_broker_new(monitor_ref_, "Test Service Instance", "");
  ASSERT_NE(db, nullptr);
  discovered_broker_insert(&monitor_ref_->broker_list, db);

  // A response with a SRV and TXT record in it.
  data_to_recv_ = {
      0, 0,        // Transaction ID
      0x84, 0x00,  // Flags: Standard query response, no error
      0, 0,        // Question count: 0
      0, 2,        // Answer count: 2
      0, 0,        // Authority count: 0
      0, 0,        // Additional count: 0

      // Start SRV record
      // Name
      21, 84, 101, 115, 116, 32, 83, 101, 114, 118, 105,                  //
      99, 101, 32, 73, 110, 115, 116, 97, 110, 99, 101,                   // Test Service Instance
      7, 95, 114, 100, 109, 110, 101, 116,                                // _rdmnet
      4, 95, 116, 99, 112,                                                // _tcp
      5, 108, 111, 99, 97, 108, 0,                                        // local
      0, 33,                                                              // Type: SRV
      0x80, 0x01,                                                         // class IN, cache flush true
      0, 0, 0, 120,                                                       // TTL 120 seconds
      0, 22,                                                              // Data length
      0, 0,                                                               // Priority 0
      0, 0,                                                               // Weight 0
      0x22, 0xb8,                                                         // Port 8888
      13, 116, 101, 115, 116, 45, 104, 111, 115, 116, 110, 97, 109, 101,  // test-hostname
      0xc0, 0x2f,                                                         // Pointer to local

      // Start TXT record
      0xc0, 0x0c,                                  // Pointer to Test Service Instance._rdmnet._tcp.local
      0, 16,                                       // Type: TXT
      0x80, 0x01,                                  // Class IN, cache flush true
      0, 0, 0, 120,                                // TTL 120 seconds
      0, 127,                                      // Data length
      9, 84, 120, 116, 86, 101, 114, 115, 61, 49,  // TxtVers=1
      17, 69, 49, 51, 51, 83, 99, 111, 112, 101, 61, 100, 101, 102, 97, 117, 108, 116,  // E133Scope=default
      10, 69, 49, 51, 51, 86, 101, 114, 115, 61, 49,                                    // E133Vers=1
      36, 67, 73, 68, 61, 54, 56, 50, 52, 98, 55, 98, 101, 49, 102, 98, 53, 52,         //
      99, 98, 53, 57, 56, 102, 48, 100, 50, 49, 54, 98, 55, 55, 101, 54, 55, 99,        //
      97,                                                                   // CID=6824b7be1fb54cb598f0d216b77e67ca
      16, 85, 73, 68, 61, 54, 53, 55, 52, 48, 56, 49, 99, 97, 102, 49, 53,  // UID=6574081caf15
      16, 77, 111, 100, 101, 108, 61, 84, 101, 115, 116, 32, 77, 111, 100, 101, 108,  // Model=Test Model
      16, 77, 97, 110, 117, 102, 61, 84, 101, 115, 116, 32, 77, 97, 110, 117, 102     // Manuf=Test Manuf
  };

  EtcPalPollEvent event{};
  event.events = ETCPAL_POLL_IN;
  recv_socket_info.callback(&event, recv_socket_info.data);

  EXPECT_EQ(db->cid, etcpal::Uuid::FromString("6824b7be1fb54cb598f0d216b77e67ca"));
  EXPECT_EQ(db->uid, rdm::Uid::FromString("6574081caf15"));
  EXPECT_EQ(db->e133_version, 1);
  EXPECT_EQ(db->port, 8888);
  EXPECT_STREQ(db->scope, "default");
  EXPECT_STREQ(db->model, "Test Model");
  EXPECT_STREQ(db->manufacturer, "Test Manuf");
  EXPECT_TRUE(db->platform_data.txt_record_received);
  EXPECT_TRUE(db->platform_data.srv_record_received);
  EXPECT_TRUE(lwmdns_domain_names_equal(db->platform_data.wire_host_name, db->platform_data.wire_host_name,
                                        data_to_recv_.data(), &data_to_recv_.data()[70]));
}

TEST_F(TestLwMdnsRecv, HandlesPtrQueryWithAnswer)
{
  EXPECT_EQ(monitor_ref_->broker_list, nullptr);

  data_to_recv_ = {
      0, 0,        // Transaction ID
      0x84, 0x00,  // Flags: Standard query response, no error
      0, 1,        // Question count: 1
      0, 1,        // Answer count: 1
      0, 0,        // Authority count: 0
      0, 0,        // Additional count: 0

      // Start PTR question
      // Name
      8, 95, 100, 101, 102, 97, 117, 108, 116,  // _default
      4, 95, 115, 117, 98,                      // _sub
      7, 95, 114, 100, 109, 110, 101, 116,      // _rdmnet
      4, 95, 116, 99, 112,                      // _tcp
      5, 108, 111, 99, 97, 108, 0,              // local
      0, 12,                                    // Type: PTR
      0x80, 0x01,                               // class IN, QM question

      // Start PTR record
      0xc0, 0x0c,                                         // Pointer to _default._sub._rdmnet._tcp.local
      0, 12,                                              // Type: PTR
      0, 1,                                               // class IN, cache flush false
      0, 0, 0, 120,                                       // TTL 120 seconds
      0, 24,                                              // Data length
      21, 84, 101, 115, 116, 32, 83, 101, 114, 118, 105,  //
      99, 101, 32, 73, 110, 115, 116, 97, 110, 99, 101,   // Test Service Instance
      0xc0, 0x1a                                          // Pointer to _rdmnet._tcp.local
  };

  EtcPalPollEvent event{};
  event.events = ETCPAL_POLL_IN;
  recv_socket_info.callback(&event, recv_socket_info.data);

  // We should add a discovered broker to the list
  ASSERT_NE(monitor_ref_->broker_list, nullptr);
  DiscoveredBroker* db = monitor_ref_->broker_list;
  EXPECT_STREQ(db->service_instance_name, "Test Service Instance");
  EXPECT_EQ(db->platform_data.ttl_timer.interval, 120u * 1000u);
}
