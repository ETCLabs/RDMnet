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

#include "broker_core.h"

#include "gmock/gmock.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"

class MockRdmnetConnInterface : public RdmnetConnInterface
{
public:
  MOCK_METHOD(etcpal::Error, Startup, (const etcpal::Uuid& cid, const EtcPalLogParams* log_params), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(void, SetNotify, (RdmnetConnNotify * notify), (override));
  MOCK_METHOD(etcpal::Error, CreateNewConnectionForSocket,
              (etcpal_socket_t sock, const etcpal::SockAddr& addr, rdmnet_conn_t& new_handle), (override));
  MOCK_METHOD(void, DestroyConnection, (rdmnet_conn_t handle, SendDisconnect send_disconnect), (override));
  MOCK_METHOD(etcpal::Error, SetBlocking, (rdmnet_conn_t handle, bool blocking), (override));
  MOCK_METHOD(void, SocketDataReceived, (rdmnet_conn_t handle, const uint8_t* data, size_t data_size), (override));
  MOCK_METHOD(void, SocketError, (rdmnet_conn_t handle, etcpal_error_t err), (override));
};

class MockBrokerSocketManager : public BrokerSocketManager
{
public:
  MOCK_METHOD(bool, Startup, (), (override));
  MOCK_METHOD(bool, Shutdown, (), (override));
  MOCK_METHOD(void, SetNotify, (BrokerSocketNotify * notify), (override));
  MOCK_METHOD(bool, AddSocket, (rdmnet_conn_t conn_handle, etcpal_socket_t sock), (override));
  MOCK_METHOD(void, RemoveSocket, (rdmnet_conn_t conn_handle), (override));
};

class MockBrokerThreadManager : public BrokerThreadInterface
{
public:
  MOCK_METHOD(void, SetNotify, (BrokerThreadNotify * notify), (override));
  MOCK_METHOD(bool, AddListenThread, (etcpal_socket_t listen_sock), (override));
  MOCK_METHOD(bool, AddClientServiceThread, (), (override));
  MOCK_METHOD(void, StopThreads, (), (override));
};

class MockBrokerDiscoveryManager : public BrokerDiscoveryInterface
{
public:
  MOCK_METHOD(void, SetNotify, (BrokerDiscoveryNotify * notify), (override));
  MOCK_METHOD(etcpal::Error, RegisterBroker, (const rdmnet::BrokerSettings& settings), (override));
  MOCK_METHOD(void, UnregisterBroker, (), (override));
};

class MockBrokerNotify : public rdmnet::BrokerNotify
{
public:
  MOCK_METHOD(void, HandleScopeChanged, (const std::string& new_scope), (override));
};

using testing::_;
using testing::NiceMock;
using testing::Return;

class TestBrokerCore : public testing::Test
{
protected:
  MockRdmnetConnInterface* mock_conn_{nullptr};
  MockBrokerSocketManager* mock_socket_mgr_{nullptr};
  MockBrokerThreadManager* mock_threads_{nullptr};
  MockBrokerDiscoveryManager* mock_disc_{nullptr};

  MockBrokerNotify notify_;

  BrokerCore broker_;
  BrokerComponentNotify* broker_callbacks_{nullptr};

  TestBrokerCore()
  {
    etcpal_reset_all_fakes();

    // Using raw news here, ownership is transferred into the BrokerCore using the BrokerComponents
    // struct
    mock_conn_ = new NiceMock<MockRdmnetConnInterface>;
    mock_socket_mgr_ = new NiceMock<MockBrokerSocketManager>;
    mock_threads_ = new NiceMock<MockBrokerThreadManager>;
    mock_disc_ = new NiceMock<MockBrokerDiscoveryManager>;

    ON_CALL(*mock_conn_, SetNotify(_)).WillByDefault([&](RdmnetConnNotify* notify) {
      broker_callbacks_ = static_cast<BrokerComponentNotify*>(notify);
    });

    ON_CALL(*mock_conn_, Startup(_, _)).WillByDefault(Return(etcpal::Error::Ok()));
    ON_CALL(*mock_socket_mgr_, Startup()).WillByDefault(Return(true));
    ON_CALL(*mock_threads_, AddListenThread(_)).WillByDefault(Return(true));
    ON_CALL(*mock_threads_, AddClientServiceThread()).WillByDefault(Return(true));
    ON_CALL(*mock_disc_, RegisterBroker(_)).WillByDefault(Return(etcpal::Error::Ok()));
  }

  // The way the test fixture is currently architected, this can only be called once per test.
  bool StartBrokerWithMockComponents(const rdmnet::BrokerSettings& settings)
  {
    return broker_.Startup(settings, &notify_, nullptr,
                           BrokerComponents(std::unique_ptr<RdmnetConnInterface>(mock_conn_),
                                            std::unique_ptr<BrokerSocketManager>(mock_socket_mgr_),
                                            std::unique_ptr<BrokerThreadInterface>(mock_threads_),
                                            std::unique_ptr<BrokerDiscoveryInterface>(mock_disc_)));
  }

  rdmnet::BrokerSettings DefaultBrokerSettings() { return rdmnet::BrokerSettings(etcpal::Uuid::OsPreferred(), 0x6574); }
};

// The broker should start if all dependent operations succeed. These are set up to succeed by the
// default mock setup functions defined in the test fixture constructor.
TEST_F(TestBrokerCore, StartsUnderNormalConditions)
{
  EXPECT_TRUE(StartBrokerWithMockComponents(DefaultBrokerSettings()));
}

// The broker should not start if it is given an invalid settings struct.
TEST_F(TestBrokerCore, DoesNotStartWithInvalidSettings)
{
  rdmnet::BrokerSettings settings;  // A default-constructed settings is invalid
  EXPECT_FALSE(StartBrokerWithMockComponents(settings));
}

// The broker should not start if we specify listening on all interface (default behavior), and
// starting the single listen thread fails.
TEST_F(TestBrokerCore, DoesNotStartWhenSingleListenThreadFails)
{
  EXPECT_CALL(*mock_threads_, AddListenThread(_)).WillOnce(Return(false));
  EXPECT_FALSE(StartBrokerWithMockComponents(DefaultBrokerSettings()));
}

// The broker should not start if we specify explicit interfaces to listen on, and starting the
// thread for each interface fails.
TEST_F(TestBrokerCore, DoesNotStartWhenAllListenThreadsFail)
{
  EXPECT_CALL(*mock_threads_, AddListenThread(_)).WillRepeatedly(Return(false));
  rdmnet::BrokerSettings explicit_interfaces(etcpal::Uuid::OsPreferred(), 0x6574);
  explicit_interfaces.listen_macs = {etcpal::MacAddr({0, 0, 0, 0, 0, 1}), etcpal::MacAddr({0, 0, 0, 0, 0, 2}),
                                     etcpal::MacAddr({0, 0, 0, 0, 0, 3})};
  EXPECT_FALSE(StartBrokerWithMockComponents(explicit_interfaces));
}

// The broker should not start if it cannot start a client service thread.
TEST_F(TestBrokerCore, DoesNotStartWhenClientServiceThreadFails)
{
  EXPECT_CALL(*mock_threads_, AddClientServiceThread()).WillOnce(Return(false));
  EXPECT_FALSE(StartBrokerWithMockComponents(DefaultBrokerSettings()));
}

// When no explicit listen interfaces are specified, the broker should create a single IPv6 socket
// and bind it to in6addr_any with the V6ONLY option disabled.
TEST_F(TestBrokerCore, SingleSocketWhenListeningOnAllInterfaces)
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

  ASSERT_TRUE(StartBrokerWithMockComponents(DefaultBrokerSettings()));
  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  EXPECT_EQ(etcpal_socket_fake.arg0_val, ETCPAL_AF_INET6);
  EXPECT_EQ(etcpal_socket_fake.arg1_val, ETCPAL_STREAM);
  EXPECT_EQ(v6only_call_count, 1);
  EXPECT_EQ(etcpal_listen_fake.call_count, 1u);
}

// When explicit listen interfaces are specified, the broker should create a socket per interface
// with the appropriate IP protocol and bind it to the interface IP address.
TEST_F(TestBrokerCore, IndividualSocketsWhenListeningOnMultipleInterfaces)
{
  static int v6only_call_count;  // Lambdas used must be stateless so this must be static
  v6only_call_count = 0;

  auto settings = DefaultBrokerSettings();
  settings.listen_addrs.insert(etcpal::IpAddr::FromString("10.101.20.30"));
  settings.listen_addrs.insert(etcpal::IpAddr::FromString("fe80::1234:5678:9abc:def0"));

  // In this situation, we should be setting the V6ONLY socket option to true for V6 sockets only.
  etcpal_setsockopt_fake.custom_fake = [](etcpal_socket_t, int level, int option, const void* value,
                                          size_t value_size) -> etcpal_error_t {
    if (option == ETCPAL_IPV6_V6ONLY)
    {
      EXPECT_EQ(level, ETCPAL_IPPROTO_IPV6);
      EXPECT_EQ(value_size, sizeof(int));
      EXPECT_EQ(*reinterpret_cast<const int*>(value), 1);
      ++v6only_call_count;
    }
    return kEtcPalErrOk;
  };

  etcpal_getsockname_fake.custom_fake = [](etcpal_socket_t, EtcPalSockAddr* addr) {
    addr->ip = etcpal::IpAddr::FromString("10.101.20.30").get();
    addr->port = 8888;
    return kEtcPalErrOk;
  };

  etcpal_bind_fake.custom_fake = [](etcpal_socket_t, const EtcPalSockAddr* addr) {
    EXPECT_FALSE(etcpal_ip_is_wildcard(&addr->ip));
    if (etcpal_bind_fake.call_count == 1u)
      EXPECT_EQ(addr->port, 0);
    else
      EXPECT_EQ(addr->port, 8888);
    return kEtcPalErrOk;
  };

  ASSERT_TRUE(StartBrokerWithMockComponents(settings));

  EXPECT_EQ(etcpal_socket_fake.call_count, 2u);
  if (etcpal_socket_fake.arg0_history[0] == ETCPAL_AF_INET)
    EXPECT_EQ(etcpal_socket_fake.arg0_history[1], ETCPAL_AF_INET6);
  else if (etcpal_socket_fake.arg0_history[0] == ETCPAL_AF_INET6)
    EXPECT_EQ(etcpal_socket_fake.arg0_history[1], ETCPAL_AF_INET);
  else
    ADD_FAILURE() << "etcpal_socket called with invalid protocol argument";

  // Should only be called once, for the IPv6 socket.
  EXPECT_EQ(v6only_call_count, 1);
  EXPECT_EQ(etcpal_bind_fake.call_count, 2u);
  EXPECT_EQ(etcpal_listen_fake.call_count, 2u);
}

class TestStartedBrokerCore : public TestBrokerCore
{
protected:
  TestStartedBrokerCore() : TestBrokerCore() { StartBrokerWithMockComponents(DefaultBrokerSettings()); }
};

TEST_F(TestStartedBrokerCore, HandlesConnect)
{
}
