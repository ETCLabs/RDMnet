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

// Test the broker's handling of connection and disconnection from clients.

#include "broker_core.h"

#include <set>
#include "gmock/gmock.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/pack.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "etcpal_mock/timer.h"
#include "rdmnet_mock/core/common.h"
#include "test_broker_messages.h"
#include "broker_mocks.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

class TestBrokerCoreConnectHandling : public testing::Test
{
protected:
  BrokerMocks mocks_{BrokerMocks::Nice()};
  BrokerCore  broker_;

  const etcpal::SockAddr kDefaultClientAddr{etcpal::IpAddr::FromString("192.168.20.30"), 49000};
  const etcpal_socket_t  kDefaultClientSocket{(etcpal_socket_t)1};

  std::set<BrokerClient::Handle> connected_clients_;

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    rdmnet_mock_core_reset_and_init();
    ASSERT_TRUE(StartBroker(broker_, DefaultBrokerSettings(), mocks_));
  }

  BrokerClient::Handle AddTcpConn();
};

BrokerClient::Handle TestBrokerCoreConnectHandling::AddTcpConn()
{
  BrokerClient::Handle new_conn_handle;
  EXPECT_CALL(*mocks_.socket_mgr, AddSocket(_, kDefaultClientSocket))
      .WillOnce(DoAll(SaveArg<0>(&new_conn_handle), Return(true)));

  EXPECT_TRUE(mocks_.broker_callbacks->HandleNewConnection(kDefaultClientSocket, kDefaultClientAddr));
  return new_conn_handle;
}

TEST_F(TestBrokerCoreConnectHandling, HandlesConnect)
{
  auto                 client_cid = etcpal::Uuid::OsPreferred();
  BrokerClient::Handle conn_handle = AddTcpConn();
  RdmnetMessage        connect_msg = testmsgs::ClientConnect(client_cid);

  etcpal_send_fake.custom_fake = [](etcpal_socket_t, const void* data, size_t data_size, int) -> int {
    EXPECT_EQ(data_size, static_cast<size_t>(BROKER_CONNECT_REPLY_FULL_MSG_SIZE));
    EXPECT_NE(data, nullptr);

    const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(data);
    EXPECT_EQ(etcpal_unpack_u32b(&byte_data[kRootVectorOffset]), ACN_VECTOR_ROOT_BROKER);
    EXPECT_EQ(etcpal_unpack_u16b(&byte_data[kBrokerVectorOffset]), VECTOR_BROKER_CONNECT_REPLY);
    EXPECT_EQ(etcpal_unpack_u16b(&byte_data[kConnectReplyCodeOffset]), E133_CONNECT_OK);
    return static_cast<int>(data_size);
  };
  mocks_.broker_callbacks->HandleSocketMessageReceived(conn_handle, connect_msg);
  EXPECT_TRUE(mocks_.broker_callbacks->ServiceClients());
  EXPECT_EQ(etcpal_send_fake.call_count, 1u);
  EXPECT_EQ(broker_.GetNumClients(), 1u);

  RESET_FAKE(etcpal_send);
}

TEST_F(TestBrokerCoreConnectHandling, RejectsScopeMismatch)
{
  auto                 client_cid = etcpal::Uuid::OsPreferred();
  BrokerClient::Handle conn_handle = AddTcpConn();
  RdmnetMessage        connect_msg = testmsgs::ClientConnect(client_cid, "Not Default Scope");

  etcpal_send_fake.custom_fake = [](etcpal_socket_t, const void* data, size_t data_size, int) -> int {
    EXPECT_EQ(data_size, static_cast<size_t>(BROKER_CONNECT_REPLY_FULL_MSG_SIZE));
    EXPECT_NE(data, nullptr);

    const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(data);
    EXPECT_EQ(etcpal_unpack_u32b(&byte_data[kRootVectorOffset]), ACN_VECTOR_ROOT_BROKER);
    EXPECT_EQ(etcpal_unpack_u16b(&byte_data[kBrokerVectorOffset]), VECTOR_BROKER_CONNECT_REPLY);
    EXPECT_EQ(etcpal_unpack_u16b(&byte_data[kConnectReplyCodeOffset]), E133_CONNECT_SCOPE_MISMATCH);
    return static_cast<int>(data_size);
  };

  mocks_.broker_callbacks->HandleSocketMessageReceived(conn_handle, connect_msg);
  EXPECT_TRUE(mocks_.broker_callbacks->ServiceClients());
  EXPECT_EQ(etcpal_send_fake.call_count, 1u);

  // Rejected client should be cleaned up
  etcpal_getms_fake.return_val += 1000;
  mocks_.broker_callbacks->ServiceClients();
  EXPECT_EQ(broker_.GetNumClients(), 0u);
}

TEST_F(TestBrokerCoreConnectHandling, HandlesRemoveUidOnDisconnect)
{
  auto                 client_cid = etcpal::Uuid::OsPreferred();
  BrokerClient::Handle conn_handle = AddTcpConn();
  RdmnetMessage        connect_msg = testmsgs::ClientConnect(client_cid);
  RdmnetMessage        disconnect_msg = testmsgs::ClientDisconnect(client_cid, kRdmnetDisconnectShutdown);

  mocks_.broker_callbacks->HandleSocketMessageReceived(conn_handle, connect_msg);
  EXPECT_TRUE(broker_.IsValidControllerDestinationUID(rdm::Uid(0xe574, 0x00000002).get()));

  // Use IsValidControllerDestinationUID to verify that RemoveUid gets called immediately.
  mocks_.broker_callbacks->HandleSocketMessageReceived(conn_handle, disconnect_msg);
  EXPECT_FALSE(broker_.IsValidControllerDestinationUID(rdm::Uid(0xe574, 0x00000002).get()));
}
