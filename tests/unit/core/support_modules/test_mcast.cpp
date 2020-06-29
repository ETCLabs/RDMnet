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

#include "rdmnet/core/mcast.h"

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

class TestMcast : public testing::Test
{
protected:
  std::vector<EtcPalNetintInfo> sys_netints_;
  bool                          initted_in_test_{true};

  TestMcast()
  {
    etcpal_reset_all_fakes();

    EtcPalNetintInfo iface;

    // Interface 1
    iface.index = 1;
    iface.addr = etcpal::IpAddr::FromString("10.101.1.20").get();
    iface.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
    iface.mac = etcpal::MacAddr::FromString("10:00:00:00:00:01").get();
    std::strcpy(iface.id, "if1");
    std::strcpy(iface.friendly_name, "Interface 1");
    sys_netints_.push_back(iface);

    // Interface 2
    iface.index = 2;
    iface.addr = etcpal::IpAddr::FromString("fe80::1:2:3:4").get();
    iface.mask = etcpal::IpAddr::NetmaskV6(64).get();
    iface.mac = etcpal::MacAddr::FromString("00:00:00:00:00:02").get();
    std::strcpy(iface.id, "if2");
    std::strcpy(iface.friendly_name, "Interface 2");
    sys_netints_.push_back(iface);

    // Interface 3
    iface.index = 3;
    iface.addr = etcpal::IpAddr::FromString("192.168.30.4").get();
    iface.mask = etcpal::IpAddr::FromString("255.255.255.0").get();
    iface.mac = etcpal::MacAddr::FromString("00:10:00:00:00:01").get();
    std::strcpy(iface.id, "if3");
    std::strcpy(iface.friendly_name, "Interface 3");
    sys_netints_.push_back(iface);

    etcpal_netint_get_interfaces_fake.return_val = sys_netints_.data();
    etcpal_netint_get_num_interfaces_fake.return_val = sys_netints_.size();
  }

  ~TestMcast()
  {
    if (initted_in_test_)
      rc_mcast_module_deinit();
  }
};

TEST_F(TestMcast, InitWorksWithNoConfig)
{
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(nullptr));
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
  config.netints = &interface_1;

  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(&config));
  // Make sure any test sockets have been cleaned up. Init should not leave any sockets open.
  EXPECT_EQ(etcpal_socket_fake.call_count, etcpal_close_fake.call_count);
}

TEST_F(TestMcast, InvalidConfigFails)
{
  initted_in_test_ = false;

  RdmnetMcastNetintId interface_id;
  RdmnetNetintConfig  config;
  config.netints = nullptr;
  config.num_netints = 0;

  EXPECT_NE(kEtcPalErrOk, rc_mcast_module_init(&config));

  config.netints = &interface_id;
  EXPECT_NE(kEtcPalErrOk, rc_mcast_module_init(&config));

  config.netints = nullptr;
  config.num_netints = 1;
  EXPECT_NE(kEtcPalErrOk, rc_mcast_module_init(&config));
}

TEST_F(TestMcast, LowestHardwareAddrIsCorrect)
{
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(nullptr));

  // The lowest address
  EtcPalMacAddr lowest_mac =
      std::min_element(sys_netints_.begin(), sys_netints_.end(),
                       [](const EtcPalNetintInfo& a, const EtcPalNetintInfo& b) { return a.mac < b.mac; })
          ->mac;
  EXPECT_EQ(*(rc_mcast_get_lowest_mac_addr()), lowest_mac);
}

// Test that we report the correct number of interfaces when not providing a config.
TEST_F(TestMcast, ReportsCorrectNumberOfInterfacesWithNoConfig)
{
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(nullptr));

  const RdmnetMcastNetintId* netint_array;
  ASSERT_EQ(sys_netints_.size(), rc_mcast_get_netint_array(&netint_array));

  for (size_t i = 0; i < sys_netints_.size(); ++i)
  {
    EXPECT_TRUE(rc_mcast_netint_is_valid(&netint_array[i]));
    // Make sure each interface in the returned array corresponds to one of our system interfaces.
    EXPECT_NE(std::find_if(sys_netints_.begin(), sys_netints_.end(),
                           [&](const EtcPalNetintInfo& info) {
                             return (info.addr.type == netint_array[i].ip_type && info.index == netint_array[i].index);
                           }),
              sys_netints_.end());
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
  config.netints = &interface_1;

  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(&config));

  const RdmnetMcastNetintId* netint_array;
  ASSERT_EQ(1u, rc_mcast_get_netint_array(&netint_array));
  EXPECT_EQ(netint_array[0].index, interface_1.index);
  EXPECT_EQ(netint_array[0].ip_type, interface_1.ip_type);
  EXPECT_TRUE(rc_mcast_netint_is_valid(&interface_1));
}

TEST_F(TestMcast, SendSocketsRefcounted)
{
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(nullptr));
  etcpal_socket_reset_all_fakes();
  etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* sock) {
    *sock = (etcpal_socket_t)0;
    return kEtcPalErrOk;
  };

  RdmnetMcastNetintId interface_1;
  interface_1.index = 1;
  interface_1.ip_type = kEtcPalIpTypeV4;

  etcpal_socket_t socket;
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 0, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 0, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  rc_mcast_release_send_socket(&interface_1, 0);
  EXPECT_EQ(etcpal_close_fake.call_count, 0u);
  rc_mcast_release_send_socket(&interface_1, 0);
  EXPECT_EQ(etcpal_close_fake.call_count, 1u);
}

TEST_F(TestMcast, SendSocketsMultiplexedBySourcePort)
{
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(nullptr));
  etcpal_socket_reset_all_fakes();

  RdmnetMcastNetintId interface_1;
  interface_1.index = 1;
  interface_1.ip_type = kEtcPalIpTypeV4;

  etcpal_socket_fake.custom_fake = [](unsigned int af, unsigned int type, etcpal_socket_t* socket) {
    EXPECT_EQ(af, ETCPAL_AF_INET);
    EXPECT_EQ(type, ETCPAL_SOCK_DGRAM);
    *socket = (etcpal_socket_t)0;
    return kEtcPalErrOk;
  };
  etcpal_socket_t socket;
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 0, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  EXPECT_EQ(etcpal_bind_fake.call_count, 0u);

  etcpal_socket_fake.custom_fake = [](unsigned int af, unsigned int type, etcpal_socket_t* socket) {
    EXPECT_EQ(af, ETCPAL_AF_INET);
    EXPECT_EQ(type, ETCPAL_SOCK_DGRAM);
    *socket = (etcpal_socket_t)1;
    return kEtcPalErrOk;
  };
  etcpal_bind_fake.custom_fake = [](etcpal_socket_t id, const EtcPalSockAddr* address) {
    EXPECT_EQ(id, (etcpal_socket_t)1);
    EXPECT_EQ(address->port, 8888);
    EXPECT_TRUE(etcpal_ip_is_wildcard(&address->ip));
    return kEtcPalErrOk;
  };
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 8888, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 2u);
  EXPECT_EQ(etcpal_bind_fake.call_count, 1u);
}

TEST_F(TestMcast, SocketsRefcountedBySourcePort)
{
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(nullptr));
  etcpal_socket_reset_all_fakes();
  etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* sock) {
    *sock = (etcpal_socket_t)0;
    return kEtcPalErrOk;
  };

  RdmnetMcastNetintId interface_1;
  interface_1.index = 1;
  interface_1.ip_type = kEtcPalIpTypeV4;

  etcpal_socket_t socket;
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 0, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 0, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 8888, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 2u);
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 8888, &socket));
  EXPECT_EQ(etcpal_socket_fake.call_count, 2u);

  rc_mcast_release_send_socket(&interface_1, 0);
  EXPECT_EQ(etcpal_close_fake.call_count, 0u);
  rc_mcast_release_send_socket(&interface_1, 0);
  EXPECT_EQ(etcpal_close_fake.call_count, 1u);
  rc_mcast_release_send_socket(&interface_1, 8888);
  EXPECT_EQ(etcpal_close_fake.call_count, 1u);
  rc_mcast_release_send_socket(&interface_1, 8888);
  EXPECT_EQ(etcpal_close_fake.call_count, 2u);
}

TEST_F(TestMcast, SetsReuseAddrWhenSourcePortSpecified)
{
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_module_init(nullptr));
  etcpal_socket_reset_all_fakes();
  etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* sock) {
    *sock = (etcpal_socket_t)0;
    return kEtcPalErrOk;
  };

  RdmnetMcastNetintId interface_1;
  interface_1.index = 1;
  interface_1.ip_type = kEtcPalIpTypeV4;

  static bool reuseaddr_set;
  reuseaddr_set = false;
  etcpal_setsockopt_fake.custom_fake = [](etcpal_socket_t, int level, int option_name, const void* option_value,
                                          size_t option_len) {
    if (level == ETCPAL_SOL_SOCKET && option_name == ETCPAL_SO_REUSEADDR)
    {
      EXPECT_EQ(option_len, sizeof(int));
      EXPECT_EQ(*(const int*)option_value, 1);
      reuseaddr_set = true;
    }
    return kEtcPalErrOk;
  };

  etcpal_socket_t socket;
  ASSERT_EQ(kEtcPalErrOk, rc_mcast_get_send_socket(&interface_1, 8888, &socket));

  EXPECT_TRUE(reuseaddr_set);
}
