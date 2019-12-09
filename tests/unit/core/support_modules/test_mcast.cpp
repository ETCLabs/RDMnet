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

#include "rdmnet/private/mcast.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include "etcpal/cpp/inet.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "etcpal_mock/socket.h"
#include "gtest/gtest.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

class TestMcast : public ::testing::Test
{
protected:
  std::vector<EtcPalNetintInfo> netint_arr_;
  bool initted_in_test_{true};

  TestMcast()
  {
    etcpal_reset_all_fakes();

    EtcPalNetintInfo iface;

    // Interface 1
    iface.index = 1;
    iface.addr = etcpal::IpAddr::FromString("10.101.1.20").get();
    iface.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
    iface.mac = etcpal::MacAddr::FromString("10:00:00:00:00:01").get();
    std::strcpy(iface.name, "if1");
    std::strcpy(iface.friendly_name, "Interface 1");
    netint_arr_.push_back(iface);

    // Interface 2
    iface.index = 2;
    iface.addr = etcpal::IpAddr::FromString("fe80::1:2:3:4").get();
    iface.mask = etcpal::IpAddr::NetmaskV6(64).get();
    iface.mac = etcpal::MacAddr::FromString("00:00:00:00:00:02").get();
    std::strcpy(iface.name, "if2");
    std::strcpy(iface.friendly_name, "Interface 2");
    netint_arr_.push_back(iface);

    // Interface 3
    iface.index = 3;
    iface.addr = etcpal::IpAddr::FromString("192.168.30.4").get();
    iface.mask = etcpal::IpAddr::FromString("255.255.255.0").get();
    iface.mac = etcpal::MacAddr::FromString("00:10:00:00:00:01").get();
    std::strcpy(iface.name, "if3");
    std::strcpy(iface.friendly_name, "Interface 3");
    netint_arr_.push_back(iface);

    etcpal_netint_get_interfaces_fake.return_val = netint_arr_.data();
    etcpal_netint_get_num_interfaces_fake.return_val = netint_arr_.size();
  }

  ~TestMcast()
  {
    if (initted_in_test_)
      rdmnet_mcast_deinit();
  }
};

TEST_F(TestMcast, InitWorksWithNoConfig)
{
  ASSERT_EQ(kEtcPalErrOk, rdmnet_mcast_init(nullptr));
  // Make sure any test sockets have been cleaned up. Init should not leave any sockets open.
  EXPECT_EQ(etcpal_socket_fake.call_count, etcpal_close_fake.call_count);
}

TEST_F(TestMcast, InitWorksWithConfigProvided)
{
  // Create a config that only specifies interface 1
  RdmnetMcastNetintId interface_1;
  interface_1.index = 1;
  interface_1.ip_type = kEtcPalIpTypeV4;

  RdmnetNetintConfig config;
  config.num_netints = 1;
  config.netint_arr = &interface_1;

  ASSERT_EQ(kEtcPalErrOk, rdmnet_mcast_init(&config));
  // Make sure any test sockets have been cleaned up. Init should not leave any sockets open.
  EXPECT_EQ(etcpal_socket_fake.call_count, etcpal_close_fake.call_count);
}

TEST_F(TestMcast, InvalidConfigFails)
{
  initted_in_test_ = false;

  RdmnetMcastNetintId interface_id;
  RdmnetNetintConfig config;
  config.netint_arr = nullptr;
  config.num_netints = 0;

  EXPECT_NE(kEtcPalErrOk, rdmnet_mcast_init(&config));

  config.netint_arr = &interface_id;
  EXPECT_NE(kEtcPalErrOk, rdmnet_mcast_init(&config));

  config.netint_arr = nullptr;
  config.num_netints = 1;
  EXPECT_NE(kEtcPalErrOk, rdmnet_mcast_init(&config));
}

TEST_F(TestMcast, LowestHardwareAddrIsCorrect)
{
  ASSERT_EQ(kEtcPalErrOk, rdmnet_mcast_init(nullptr));

  // The lowest address
  EtcPalMacAddr lowest_mac =
      std::min_element(netint_arr_.begin(), netint_arr_.end(),
                       [](const EtcPalNetintInfo& a, const EtcPalNetintInfo& b) { return a.mac < b.mac; })
          ->mac;
  EXPECT_EQ(*(rdmnet_get_lowest_mac_addr()), lowest_mac);
}

// Test that we report the correct number of interfaces when not providing a config.
TEST_F(TestMcast, ReportsCorrectNumberOfInterfacesWithNoConfig)
{
  ASSERT_EQ(kEtcPalErrOk, rdmnet_mcast_init(nullptr));

  const RdmnetMcastNetintId* netint_array;
  ASSERT_EQ(netint_arr_.size(), rdmnet_get_mcast_netint_array(&netint_array));

  for (size_t i = 0; i < netint_arr_.size(); ++i)
  {
    EXPECT_TRUE(rdmnet_mcast_netint_is_valid(&netint_array[i]));
    // Make sure each interface in the returned array corresponds to one of our system interfaces.
    EXPECT_NE(std::find_if(netint_arr_.begin(), netint_arr_.end(),
                           [&](const EtcPalNetintInfo& info) {
                             return (info.addr.type == netint_array[i].ip_type && info.index == netint_array[i].index);
                           }),
              netint_arr_.end());
  }
}

TEST_F(TestMcast, ReportsCorrectNumberOfInterfacesWithConfig)
{
  // Create a config that only specifies interface 1
  RdmnetMcastNetintId interface_1;
  interface_1.index = 1;
  interface_1.ip_type = kEtcPalIpTypeV4;

  RdmnetNetintConfig config;
  config.num_netints = 1;
  config.netint_arr = &interface_1;

  ASSERT_EQ(kEtcPalErrOk, rdmnet_mcast_init(&config));

  const RdmnetMcastNetintId* netint_array;
  ASSERT_EQ(1, rdmnet_get_mcast_netint_array(&netint_array));
  EXPECT_EQ(netint_array[0].index, interface_1.index);
  EXPECT_EQ(netint_array[0].ip_type, interface_1.ip_type);
  EXPECT_TRUE(rdmnet_mcast_netint_is_valid(&interface_1));
}

TEST_F(TestMcast, SendSocketsAreRefcounted)
{
  ASSERT_EQ(kEtcPalErrOk, rdmnet_mcast_init(nullptr));
  etcpal_socket_reset_all_fakes();

  RdmnetMcastNetintId interface_1;
  interface_1.index = 1;
  interface_1.ip_type = kEtcPalIpTypeV4;

  etcpal_socket_t socket;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_get_mcast_send_socket(&interface_1, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  ASSERT_EQ(kEtcPalErrOk, rdmnet_get_mcast_send_socket(&interface_1, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  rdmnet_release_mcast_send_socket(&interface_1);
  EXPECT_EQ(etcpal_close_fake.call_count, 0u);
  rdmnet_release_mcast_send_socket(&interface_1);
  EXPECT_EQ(etcpal_close_fake.call_count, 1u);
}
