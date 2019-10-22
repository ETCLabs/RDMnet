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

class MockRdmnetConnInterface : public RdmnetConnInterface
{
public:
  MOCK_METHOD(etcpal::Result, Startup, (const etcpal::Uuid& cid, const EtcPalLogParams* log_params), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(void, SetNotify, (RdmnetConnNotify * notify), (override));
  MOCK_METHOD(etcpal::Result, CreateNewConnectionForSocket,
              (etcpal_socket_t sock, const EtcPalSockaddr& addr, rdmnet_conn_t& new_handle), (override));
  MOCK_METHOD(void, DestroyConnection, (rdmnet_conn_t handle, SendDisconnect send_disconnect), (override));
  MOCK_METHOD(etcpal::Result, SetBlocking, (rdmnet_conn_t handle, bool blocking), (override));
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
  MOCK_METHOD(etcpal::Result, RegisterBroker, (const rdmnet::BrokerSettings& settings), (override));
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

    ON_CALL(*mock_conn_, Startup(_, _)).WillByDefault(Return(etcpal::Result::Ok()));
    ON_CALL(*mock_socket_mgr_, Startup()).WillByDefault(Return(true));
    ON_CALL(*mock_threads_, AddListenThread(_)).WillByDefault(Return(true));
    ON_CALL(*mock_threads_, AddClientServiceThread()).WillByDefault(Return(true));
    ON_CALL(*mock_disc_, RegisterBroker(_)).WillByDefault(Return(etcpal::Result::Ok()));
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
  explicit_interfaces.listen_macs = {{0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 2}, {0, 0, 0, 0, 0, 3}};
  EXPECT_FALSE(StartBrokerWithMockComponents(explicit_interfaces));
}

// The broker should not start if it cannot start a client service thread.
TEST_F(TestBrokerCore, DoesNotStartWhenClientServiceThreadFails)
{
  EXPECT_CALL(*mock_threads_, AddClientServiceThread()).WillOnce(Return(false));
  EXPECT_FALSE(StartBrokerWithMockComponents(DefaultBrokerSettings()));
}
