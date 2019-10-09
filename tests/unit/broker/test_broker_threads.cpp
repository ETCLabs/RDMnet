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
#include "gmock/gmock.h"
#include "broker_threads.h"

class MockBrokerThreadNotify : public BrokerThreadNotify
{
  MOCK_METHOD(bool, NewConnection, (etcpal_socket_t new_sock, const EtcPalSockaddr& remote_addr), (override));
  MOCK_METHOD(bool, ServiceClients, (), (override));
};

class TestBrokerThreads : public testing::Test
{
protected:
  BrokerThreadManager thread_mgr_;
  MockBrokerThreadNotify notify_;

  TestBrokerThreads() { thread_mgr_.SetNotify(&notify_); }
};
