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

#ifndef BROKER_MOCKS_H_
#define BROKER_MOCKS_H_

#include <memory>
#include "rdmnet/cpp/broker.h"
#include "broker_socket_manager.h"
#include "broker_threads.h"
#include "broker_discovery.h"
#include "broker_core.h"
#include "gmock/gmock.h"

class MockBrokerSocketManager : public BrokerSocketManager
{
public:
  MOCK_METHOD(bool, Startup, (), (override));
  MOCK_METHOD(bool, Shutdown, (), (override));
  MOCK_METHOD(void, SetNotify, (BrokerSocketNotify * notify), (override));
  MOCK_METHOD(bool, AddSocket, (BrokerClient::Handle conn_handle, etcpal_socket_t sock), (override));
  MOCK_METHOD(void, RemoveSocket, (BrokerClient::Handle conn_handle), (override));
};

class MockBrokerThreadManager : public BrokerThreadInterface
{
public:
  MOCK_METHOD(void, SetNotify, (BrokerThreadNotify * notify), (override));
  MOCK_METHOD(etcpal::Error, AddListenThread, (etcpal_socket_t listen_sock), (override));
  MOCK_METHOD(etcpal::Error, AddClientServiceThread, (), (override));
  MOCK_METHOD(void, StopThreads, (), (override));
};

class MockBrokerDiscoveryManager : public BrokerDiscoveryInterface
{
public:
  MOCK_METHOD(void, SetNotify, (BrokerDiscoveryNotify * notify), (override));
  MOCK_METHOD(etcpal::Error,
              RegisterBroker,
              (const rdmnet::Broker::Settings& settings, const std::vector<unsigned int>& resolved_interface_indexes),
              (override));
  MOCK_METHOD(void, UnregisterBroker, (), (override));
};

class MockBrokerNotifyHandler : public rdmnet::Broker::NotifyHandler
{
public:
  MOCK_METHOD(void, HandleScopeChanged, (const std::string& new_scope), (override));
};

// These raw pointers are meant to be ownership-transferred to a broker instance using
// StartBroker(), so they are not deleted on destruction.
struct BrokerMocks
{
  MockBrokerSocketManager*                 socket_mgr{nullptr};
  MockBrokerThreadManager*                 threads{nullptr};
  MockBrokerDiscoveryManager*              disc{nullptr};
  std::unique_ptr<MockBrokerNotifyHandler> notify{std::make_unique<MockBrokerNotifyHandler>()};

  BrokerComponentNotify* broker_callbacks{nullptr};

  BrokerMocks(MockBrokerSocketManager*    new_socket_mgr,
              MockBrokerThreadManager*    new_threads,
              MockBrokerDiscoveryManager* new_disc)
      : socket_mgr(new_socket_mgr), threads(new_threads), disc(new_disc)
  {
    ON_CALL(*socket_mgr, SetNotify(testing::_)).WillByDefault([&](BrokerSocketNotify* notify) {
      broker_callbacks = static_cast<BrokerComponentNotify*>(notify);
    });

    ON_CALL(*socket_mgr, Startup()).WillByDefault(testing::Return(true));
    ON_CALL(*threads, AddListenThread(testing::_)).WillByDefault(testing::Return(etcpal::Error::Ok()));
    ON_CALL(*threads, AddClientServiceThread()).WillByDefault(testing::Return(etcpal::Error::Ok()));
    ON_CALL(*disc, RegisterBroker(testing::_, testing::_)).WillByDefault(testing::Return(etcpal::Error::Ok()));
  }

  static BrokerMocks Nice()
  {
    return BrokerMocks(new testing::NiceMock<MockBrokerSocketManager>, new testing::NiceMock<MockBrokerThreadManager>,
                       new testing::NiceMock<MockBrokerDiscoveryManager>);
  }

  static BrokerMocks Strict()
  {
    return BrokerMocks(new testing::StrictMock<MockBrokerSocketManager>,
                       new testing::StrictMock<MockBrokerThreadManager>,
                       new testing::StrictMock<MockBrokerDiscoveryManager>);
  }

  static BrokerMocks Normal()
  {
    return BrokerMocks(new MockBrokerSocketManager, new MockBrokerThreadManager, new MockBrokerDiscoveryManager);
  }
};

inline rdmnet::Broker::Settings DefaultBrokerSettings()
{
  return rdmnet::Broker::Settings(etcpal::Uuid::OsPreferred(), 0x6574);
}

inline etcpal::Error StartBroker(BrokerCore& broker, const rdmnet::Broker::Settings& settings, BrokerMocks& mocks)
{
  return broker.Startup(settings, mocks.notify.get(), nullptr,
                        BrokerComponents(std::unique_ptr<BrokerSocketManager>(mocks.socket_mgr),
                                         std::unique_ptr<BrokerThreadInterface>(mocks.threads),
                                         std::unique_ptr<BrokerDiscoveryInterface>(mocks.disc)));
}

#endif  // BROKER_MOCKS_H_
