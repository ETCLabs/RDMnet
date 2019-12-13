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

#include "broker_threads.h"

#include "gmock/gmock.h"
#include "etcpal_mock/thread.h"
#include "etcpal_mock/socket.h"

using testing::_;
using testing::Return;

class MockBrokerThreadNotify : public BrokerThreadNotify
{
public:
  MOCK_METHOD(bool, HandleNewConnection, (etcpal_socket_t new_sock, const etcpal::SockAddr& remote_addr), (override));
  MOCK_METHOD(bool, ServiceClients, (), (override));
};

class ThreadTestBase : public testing::Test
{
protected:
  testing::StrictMock<MockBrokerThreadNotify> notify_;

  void SetUp() override
  {
    etcpal_thread_reset_all_fakes();
    etcpal_socket_reset_all_fakes();
  }
};

class TestThreadManager : public ThreadTestBase
{
protected:
  BrokerThreadManager thread_mgr_;

  void SetUp() override
  {
    ThreadTestBase::SetUp();
    thread_mgr_.SetNotify(&notify_);
  }
};

TEST_F(TestThreadManager, AddListenThreadNormalWorks)
{
  EXPECT_TRUE(thread_mgr_.AddListenThread(0));
}

class TestListenThread : public ThreadTestBase
{
protected:
  static constexpr uint16_t kTestPort = 8888;
  static constexpr uint32_t kTestIpv4 = 0x0a650203;
  static constexpr etcpal_socket_t kListenSocketVal = 0;
};

TEST_F(TestListenThread, StartCleansUpOnThreadError)
{
  ListenThread lt(kListenSocketVal, &notify_, nullptr);

  etcpal_thread_create_fake.return_val = kEtcPalErrSys;
  EXPECT_FALSE(lt.Start());
  EXPECT_EQ(etcpal_close_fake.call_count, 1u);
  EXPECT_TRUE(lt.terminated());
}

TEST_F(TestListenThread, AcceptResultIsForwarded)
{
  ListenThread lt(kListenSocketVal, &notify_, nullptr);
  ASSERT_TRUE(lt.Start());

  // Tentatively trying this. This is not standard C++ (it's not valid for a function pointer
  // called from C to point to a C++ linkage function, which all lambdas are), but I *think* it is
  // supported by all the compilers we run tests with, and it makes for nicer less-boilerplatey
  // test code.
  etcpal_accept_fake.custom_fake = [](etcpal_socket_t socket, EtcPalSockAddr* accept_addr,
                                      etcpal_socket_t* accept_sock) {
    EXPECT_EQ(socket, 0);
    ETCPAL_IP_SET_V4_ADDRESS(&accept_addr->ip, kTestIpv4);
    accept_addr->port = kTestPort;
    *accept_sock = 1;
    return kEtcPalErrOk;
  };

  const etcpal::SockAddr expected_addr(kTestIpv4, kTestPort);
  EXPECT_CALL(notify_, HandleNewConnection(1, expected_addr)).WillOnce(Return(true));
  lt.ReadSocket();
}

TEST_F(TestListenThread, SocketClosedAfterNotHandled)
{
  ListenThread lt(0, &notify_, nullptr);
  ASSERT_TRUE(lt.Start());

  etcpal_accept_fake.return_val = kEtcPalErrOk;

  EXPECT_CALL(notify_, HandleNewConnection(_, _)).WillOnce(Return(false));
  lt.ReadSocket();
  EXPECT_EQ(etcpal_close_fake.call_count, 1u);
  EXPECT_FALSE(lt.terminated());
}

TEST_F(TestListenThread, AcceptErrorStopsThread)
{
  ListenThread lt(0, &notify_, nullptr);
  lt.Start();
  ASSERT_FALSE(lt.terminated());

  etcpal_accept_fake.return_val = kEtcPalErrNotFound;

  lt.ReadSocket();

  EXPECT_TRUE(lt.terminated());
}
