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

// Test the BrokerCore class in various startup and shutdown conditions.

#include "broker_core.h"

#include "gmock/gmock.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "rdmnet_mock/core/common.h"
#include "broker_mocks.h"

using testing::_;
using testing::Return;

class TestBrokerCoreStartup : public testing::Test
{
protected:
  BrokerMocks mocks_{BrokerMocks::Nice()};
  BrokerCore  broker_;

  TestBrokerCoreStartup()
  {
    etcpal_reset_all_fakes();
    rdmnet_mock_core_reset_and_init();
  }

  etcpal::Error StartBroker(const rdmnet::Broker::Settings& settings)
  {
    return ::StartBroker(broker_, settings, mocks_);
  }
};

// The broker should start if all dependent operations succeed. These are set up to succeed by the
// default mock setup functions defined in the test fixture constructor.
TEST_F(TestBrokerCoreStartup, StartsUnderNormalConditions)
{
  EXPECT_TRUE(StartBroker(DefaultBrokerSettings()));
}

// The broker should not start if it is given an invalid settings struct.
TEST_F(TestBrokerCoreStartup, DoesNotStartWithInvalidSettings)
{
  rdmnet::Broker::Settings settings;  // A default-constructed settings is invalid
  EXPECT_FALSE(StartBroker(settings));
}

// The broker should not start if RDMnet has not been initialized
TEST_F(TestBrokerCoreStartup, DoesNotStartWhenRdmnetIsNotInitialized)
{
  rc_initialized_fake.return_val = false;
  EXPECT_EQ(StartBroker(DefaultBrokerSettings()), kEtcPalErrNotInit);
}

// The broker should not start if we specify listening on all interface (default behavior), and
// starting the single listen thread fails.
TEST_F(TestBrokerCoreStartup, DoesNotStartWhenSingleListenThreadFails)
{
  EXPECT_CALL(*mocks_.threads, AddListenThread(_)).WillOnce(Return(kEtcPalErrSys));
  EXPECT_FALSE(StartBroker(DefaultBrokerSettings()));
}

// The broker should not start if we specify explicit interfaces to listen on, and starting the
// thread for each interface fails.
TEST_F(TestBrokerCoreStartup, DoesNotStartWhenAllListenThreadsFail)
{
  EXPECT_CALL(*mocks_.threads, AddListenThread(_)).WillRepeatedly(Return(kEtcPalErrSys));

  auto explicit_interfaces = DefaultBrokerSettings();
  explicit_interfaces.listen_interfaces = {"netint 1", "netint 2", "netint 3"};

  EXPECT_FALSE(StartBroker(explicit_interfaces));
}

// The broker should not start if it cannot start a client service thread.
TEST_F(TestBrokerCoreStartup, DoesNotStartWhenClientServiceThreadFails)
{
  EXPECT_CALL(*mocks_.threads, AddClientServiceThread()).WillOnce(Return(kEtcPalErrSys));
  EXPECT_FALSE(StartBroker(DefaultBrokerSettings()));
}

// When no explicit listen interfaces are specified, the broker should create a single IPv6 socket
// and bind it to in6addr_any with the V6ONLY option disabled.
TEST_F(TestBrokerCoreStartup, SingleSocketWhenListeningOnAllInterfaces)
{
  static int v6only_call_count;  // Lambdas used must be stateless so this must be static
  v6only_call_count = 0;

  etcpal_setsockopt_fake.custom_fake = [](etcpal_socket_t, int level, int option, const void* value,
                                          size_t value_size) {
    if (level == ETCPAL_IPPROTO_IPV6 && option == ETCPAL_IPV6_V6ONLY)
    {
      EXPECT_EQ(value_size, sizeof(int));
      EXPECT_EQ(*reinterpret_cast<const int*>(value), 0);
      ++v6only_call_count;
    }
    return kEtcPalErrOk;
  };
  etcpal_bind_fake.custom_fake = [](etcpal_socket_t, const EtcPalSockAddr* addr) {
    EXPECT_TRUE(ETCPAL_IP_IS_V6(&addr->ip));
    EXPECT_TRUE(etcpal_ip_is_wildcard(&addr->ip));
    EXPECT_EQ(addr->port, 0u);
    return kEtcPalErrOk;
  };

  ASSERT_TRUE(StartBroker(DefaultBrokerSettings()));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  EXPECT_EQ(etcpal_socket_fake.arg0_val, ETCPAL_AF_INET6);
  EXPECT_EQ(etcpal_socket_fake.arg1_val, ETCPAL_SOCK_STREAM);
  EXPECT_EQ(v6only_call_count, 1);
  EXPECT_EQ(etcpal_listen_fake.call_count, 1u);
}

// When explicit listen interfaces are specified, the broker should create a socket per interface
// with the appropriate IP protocol and bind it to the interface IP address.
// TEST_F(TestBrokerCoreStartup, IndividualSocketsWhenListeningOnMultipleInterfaces)
//{
//  static int v6only_call_count;  // Lambdas used must be stateless so this must be static
//  v6only_call_count = 0;
//
//  auto settings = DefaultBrokerSettings();
//  settings.listen_addrs.insert(etcpal::IpAddr::FromString("10.101.20.30"));
//  settings.listen_addrs.insert(etcpal::IpAddr::FromString("fe80::1234:5678:9abc:def0"));
//
//  // In this situation, we should be setting the V6ONLY socket option to true for V6 sockets only.
//  etcpal_setsockopt_fake.custom_fake = [](etcpal_socket_t, int level, int option, const void* value,
//                                          size_t value_size) -> etcpal_error_t {
//    if (option == ETCPAL_IPV6_V6ONLY)
//    {
//      EXPECT_EQ(level, ETCPAL_IPPROTO_IPV6);
//      EXPECT_EQ(value_size, sizeof(int));
//      EXPECT_EQ(*reinterpret_cast<const int*>(value), 1);
//      ++v6only_call_count;
//    }
//    return kEtcPalErrOk;
//  };
//
//  etcpal_getsockname_fake.custom_fake = [](etcpal_socket_t, EtcPalSockAddr* addr) {
//    addr->ip = etcpal::IpAddr::FromString("10.101.20.30").get();
//    addr->port = 8888;
//    return kEtcPalErrOk;
//  };
//
//  etcpal_bind_fake.custom_fake = [](etcpal_socket_t, const EtcPalSockAddr* addr) {
//    EXPECT_FALSE(etcpal_ip_is_wildcard(&addr->ip));
//    if (etcpal_bind_fake.call_count == 1u)
//      EXPECT_EQ(addr->port, 0);
//    else
//      EXPECT_EQ(addr->port, 8888);
//    return kEtcPalErrOk;
//  };
//
//  ASSERT_TRUE(StartBrokerWithMockComponents(settings));
//
//  EXPECT_EQ(etcpal_socket_fake.call_count, 2u);
//  if (etcpal_socket_fake.arg0_history[0] == ETCPAL_AF_INET)
//    EXPECT_EQ(etcpal_socket_fake.arg0_history[1], ETCPAL_AF_INET6);
//  else if (etcpal_socket_fake.arg0_history[0] == ETCPAL_AF_INET6)
//    EXPECT_EQ(etcpal_socket_fake.arg0_history[1], ETCPAL_AF_INET);
//  else
//    ADD_FAILURE() << "etcpal_socket called with invalid protocol argument";
//
//  // Should only be called once, for the IPv6 socket.
//  EXPECT_EQ(v6only_call_count, 1);
//  EXPECT_EQ(etcpal_bind_fake.call_count, 2u);
//  EXPECT_EQ(etcpal_listen_fake.call_count, 2u);
//}
