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

// Test the broker's sending and receiving messages to/from clients.

#include "broker_core.h"

#include "gmock/gmock.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "etcpal_mock/timer.h"
#include "rdmnet/defs.h"
#include "rdmnet_mock/core/common.h"
#include "broker_mocks.h"
#include "test_broker_messages.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

class TestBrokerCoreMessageHandling : public testing::Test
{
protected:
  BrokerMocks mocks_{BrokerMocks::Nice()};
  BrokerCore  broker_;

  const etcpal::SockAddr kDefaultClientAddr{etcpal::IpAddr::FromString("192.168.20.30"), 49000};
  const etcpal_socket_t  kDefaultClientSocket{(etcpal_socket_t)1};

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    rdmnet_mock_core_reset_and_init();
    ASSERT_TRUE(StartBroker(broker_, DefaultBrokerSettings(), mocks_));
  }

  BrokerClient::Handle AddClient(const etcpal::Uuid& cid);
};

BrokerClient::Handle TestBrokerCoreMessageHandling::AddClient(const etcpal::Uuid& cid)
{
  BrokerClient::Handle new_conn_handle;
  EXPECT_CALL(*mocks_.socket_mgr, AddSocket(_, kDefaultClientSocket))
      .WillOnce(DoAll(SaveArg<0>(&new_conn_handle), Return(true)));

  EXPECT_TRUE(mocks_.broker_callbacks->HandleNewConnection(kDefaultClientSocket, kDefaultClientAddr));

  RdmnetMessage connect_msg = testmsgs::ClientConnect(cid);

  static bool got_connect_reply;
  got_connect_reply = false;

  RESET_FAKE(etcpal_send);
  etcpal_send_fake.custom_fake = [](etcpal_socket_t, const void* data, size_t data_size, int) -> int {
    EXPECT_NE(data, nullptr);
    const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(data);
    if (data_size > kBrokerVectorOffset + 2 &&
        etcpal_unpack_u16b(&byte_data[kBrokerVectorOffset]) == VECTOR_BROKER_CONNECT_REPLY)
    {
      got_connect_reply = true;
      EXPECT_EQ(data_size, static_cast<size_t>(BROKER_CONNECT_REPLY_FULL_MSG_SIZE));

      EXPECT_EQ(etcpal_unpack_u32b(&byte_data[kRootVectorOffset]), ACN_VECTOR_ROOT_BROKER);
      EXPECT_EQ(etcpal_unpack_u16b(&byte_data[kConnectReplyCodeOffset]), E133_CONNECT_OK);
    }
    return (int)data_size;
  };
  mocks_.broker_callbacks->HandleSocketMessageReceived(new_conn_handle, connect_msg);
  EXPECT_TRUE(mocks_.broker_callbacks->ServiceClients());
  EXPECT_TRUE(got_connect_reply);
  RESET_FAKE(etcpal_send);

  return new_conn_handle;
}

TEST_F(TestBrokerCoreMessageHandling, HandlesHeartbeat)
{
  auto client_cid = etcpal::Uuid::OsPreferred();
  auto client_handle = AddClient(client_cid);

  RdmnetMessage null_msg = testmsgs::Null(client_cid);

  EXPECT_CALL(*mocks_.socket_mgr, RemoveSocket(_)).Times(0);

  // pass time and send null messages until a heartbeat timeout
  while (etcpal_getms_fake.return_val < ((E133_HEARTBEAT_TIMEOUT_SEC * 1000) + 1000))
  {
    etcpal_getms_fake.return_val += (E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
    mocks_.broker_callbacks->HandleSocketMessageReceived(client_handle, null_msg);
    mocks_.broker_callbacks->ServiceClients();
  }

  mocks_.broker_callbacks->ServiceClients();
  EXPECT_EQ(broker_.GetNumClients(), 1u);

  testing::Mock::VerifyAndClearExpectations(mocks_.socket_mgr);
}

TEST_F(TestBrokerCoreMessageHandling, InterpretsAllMessageTypesAsHeartbeat)
{
  auto client_cid = etcpal::Uuid::OsPreferred();
  auto client_handle = AddClient(client_cid);

  RdmnetMessage fcl_msg = testmsgs::FetchClientList(client_cid);

  EXPECT_CALL(*mocks_.socket_mgr, RemoveSocket(_)).Times(0);

  // pass time and send null messages until a heartbeat timeout
  while (etcpal_getms_fake.return_val < ((E133_HEARTBEAT_TIMEOUT_SEC * 1000) + 1000))
  {
    etcpal_getms_fake.return_val += (E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000);
    mocks_.broker_callbacks->HandleSocketMessageReceived(client_handle, fcl_msg);
    mocks_.broker_callbacks->ServiceClients();
  }

  mocks_.broker_callbacks->ServiceClients();
  EXPECT_EQ(broker_.GetNumClients(), 1u);

  testing::Mock::VerifyAndClearExpectations(mocks_.socket_mgr);
}

TEST_F(TestBrokerCoreMessageHandling, HandlesNoHeartbeat)
{
  auto client_cid = etcpal::Uuid::OsPreferred();
  auto client_handle = AddClient(client_cid);

  EXPECT_EQ(broker_.GetNumClients(), 1u);

  EXPECT_CALL(*mocks_.socket_mgr, RemoveSocket(client_handle));

  // pass time to past heartbeat timeout
  etcpal_getms_fake.return_val += ((E133_HEARTBEAT_TIMEOUT_SEC * 1000) + 1000);
  mocks_.broker_callbacks->ServiceClients();

  EXPECT_EQ(broker_.GetNumClients(), 0u);
}

TEST_F(TestBrokerCoreMessageHandling, SendsRptClientListOnRequest)
{
  auto client_1_cid = etcpal::Uuid::OsPreferred();
  auto client_2_cid = etcpal::Uuid::OsPreferred();
  auto client_1_handle = AddClient(client_1_cid);
  AddClient(client_2_cid);

  RdmnetMessage fcl_msg = testmsgs::FetchClientList(client_1_cid);

  static bool got_client_list;
  got_client_list = false;

  RESET_FAKE(etcpal_send);
  etcpal_send_fake.custom_fake = [](etcpal_socket_t, const void* data, size_t data_size, int) -> int {
    EXPECT_NE(data, nullptr);
    const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(data);
    if (data_size > kBrokerVectorOffset + 2 &&
        etcpal_unpack_u16b(&byte_data[kBrokerVectorOffset]) == VECTOR_BROKER_CONNECTED_CLIENT_LIST)
    {
      got_client_list = true;
      // There should be two client entries in the list
      EXPECT_EQ(data_size, rc_broker_get_rpt_client_list_buffer_size(2));
      EXPECT_EQ(etcpal_unpack_u32b(&byte_data[kRootVectorOffset]), ACN_VECTOR_ROOT_BROKER);
    }
    return (int)data_size;
  };

  mocks_.broker_callbacks->HandleSocketMessageReceived(client_1_handle, fcl_msg);
  mocks_.broker_callbacks->ServiceClients();

  EXPECT_EQ(got_client_list, true);
}
