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

#include "broker_util.h"

#include <limits>
#include "gmock/gmock.h"

// MATCHER_P generates unreferenced formal parameter warnings for the hidden result_listener
// parameter, which we don't care about.
#ifdef _MSC_VER
#pragma warning(disable : 4100)
#endif

TEST(TestClientHandleGenerator, GeneratesSequentialHandles)
{
  ClientHandleGenerator generator;
  EXPECT_EQ(generator.GetClientHandle(), 0);
  EXPECT_EQ(generator.GetClientHandle(), 1);
  EXPECT_EQ(generator.GetClientHandle(), 2);
}

TEST(TestClientHandleGenerator, SetNextHandleWorks)
{
  ClientHandleGenerator generator;
  EXPECT_EQ(generator.GetClientHandle(), 0);
  generator.SetNextHandle(5);
  EXPECT_EQ(generator.GetClientHandle(), 5);
  EXPECT_EQ(generator.GetClientHandle(), 6);
}

TEST(TestClientHandleGenerator, HandlesWraparound)
{
  ClientHandleGenerator generator;
  generator.SetNextHandle(std::numeric_limits<BrokerClient::Handle>::max());
  EXPECT_EQ(generator.GetClientHandle(), std::numeric_limits<BrokerClient::Handle>::max());
  EXPECT_EQ(generator.GetClientHandle(), 0);
}

TEST(TestClientHandleGenerator, SkipsHandlesInUse)
{
  ClientHandleGenerator generator;

  // The handle "0" is simulated to be in use
  generator.SetValueInUseFunc([](BrokerClient::Handle handle) { return (handle == 0 ? true : false); });

  generator.SetNextHandle(std::numeric_limits<BrokerClient::Handle>::max());
  EXPECT_EQ(generator.GetClientHandle(), std::numeric_limits<BrokerClient::Handle>::max());
  // We should wrap around to 1 instead of 0
  EXPECT_EQ(generator.GetClientHandle(), 1);
}

class MockBrokerClient : public BrokerClient
{
public:
  MockBrokerClient() : BrokerClient(0, 0) {}

  MOCK_METHOD(bool, Push, (const etcpal::Uuid& sender_cid, const BrokerMessage& msg), (override));
};

using testing::_;
using testing::Return;
using testing::StrictMock;

TEST(TestClientDestroyAction, DefaultResolvesToNoAction)
{
  ClientDestroyAction          action;
  StrictMock<MockBrokerClient> client;  // Fail the test if any methods are called

  action.Apply(rdm::Uid(0x6574, 0x12345678), etcpal::Uuid::OsPreferred(), client);
}

MATCHER_P(IsConnectReplyContainingStatus, status, "")
{
  return (arg.vector == VECTOR_BROKER_CONNECT_REPLY && BROKER_GET_CONNECT_REPLY_MSG(&arg)->connect_status == status);
}

TEST(TestClientDestroyAction, PushesConnectReply)
{
  auto             action = ClientDestroyAction::SendConnectReply(kRdmnetConnectCapacityExceeded);
  MockBrokerClient client;

  auto cid = etcpal::Uuid::OsPreferred();
  EXPECT_CALL(client, Push(cid, IsConnectReplyContainingStatus(kRdmnetConnectCapacityExceeded))).WillOnce(Return(true));
  action.Apply(rdm::Uid(0x6574, 0x12345678), cid, client);
}

MATCHER_P(IsDisconnectContainingReason, reason, "")
{
  return (arg.vector == VECTOR_BROKER_DISCONNECT && BROKER_GET_DISCONNECT_MSG(&arg)->disconnect_reason == reason);
}

TEST(TestClientDestroyAction, PushesDisconnect)
{
  auto             action = ClientDestroyAction::SendDisconnect(kRdmnetDisconnectShutdown);
  MockBrokerClient client;

  auto cid = etcpal::Uuid::OsPreferred();
  EXPECT_CALL(client, Push(cid, IsDisconnectContainingReason(kRdmnetDisconnectShutdown))).WillOnce(Return(true));
  action.Apply(rdm::Uid(0x6574, 0x12345678), cid, client);
}

TEST(TestClientDestroyAction, MarksSocketInvalid)
{
  auto             action = ClientDestroyAction::MarkSocketInvalid();
  MockBrokerClient client;
  client.socket = (etcpal_socket_t)20;

  action.Apply(rdm::Uid{}, etcpal::Uuid{}, client);
  EXPECT_EQ(client.socket, ETCPAL_SOCKET_INVALID);
}
