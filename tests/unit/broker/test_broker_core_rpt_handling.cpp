/******************************************************************************
 * Copyright 2022 ETC Inc.
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

// Test the broker's sending and receiving RPT messages to/from RPT clients.

#include "broker_core.h"

#include "gmock/gmock.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "etcpal_mock/timer.h"
#include "rdmnet/defs.h"
#include "rdmnet_mock/core/common.h"
#include "broker_mocks.h"
#include "test_broker_messages.h"
#include "test_rdm_commands.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

class TestBrokerCoreRptHandling : public testing::Test
{
protected:
  BrokerMocks mocks_{BrokerMocks::Nice()};
  BrokerCore  broker_;

  const etcpal::SockAddr        kDefaultClientAddr{etcpal::IpAddr::FromString("192.168.20.30"), 49000};
  const etcpal_socket_t         kDefaultClientSocket{(etcpal_socket_t)1};
  static constexpr RdmUid       kTestControllerUid{84, 42};
  static constexpr uint16_t     kTestManu1{0x6574};
  static constexpr uint16_t     kTestManu2{0x7465};
  static constexpr unsigned int kMaxControllerMessages{10u};
  static constexpr unsigned int kMaxDeviceMessages{20u};

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    rdmnet_mock_core_reset_and_init();

    rc_send_fake.custom_fake = [](etcpal_socket_t, const void*, size_t data_size, int) -> int {
      return (int)data_size;
    };

    auto settings = DefaultBrokerSettings();
    settings.limits.controller_messages = kMaxControllerMessages;
    settings.limits.device_messages = kMaxDeviceMessages;
    ASSERT_TRUE(StartBroker(broker_, settings, mocks_));
  }

  BrokerClient::Handle AddClient(const etcpal::Uuid& cid, rpt_client_type_t client_type, uint16_t manu);
  void                 TestMessageLimit(BrokerClient::Handle sender_handle,
                                        const RdmnetMessage& msg,
                                        unsigned int         num_remaining_messages_allowed);
  void                 TestMessageLimitWithHarvest(BrokerClient::Handle sender_handle,
                                                   const RdmnetMessage& msg,
                                                   unsigned int         num_remaining_messages_allowed);
};

BrokerClient::Handle TestBrokerCoreRptHandling::AddClient(const etcpal::Uuid& cid,
                                                          rpt_client_type_t   type,
                                                          uint16_t            manu)
{
  BrokerClient::Handle new_conn_handle;
  EXPECT_CALL(*mocks_.socket_mgr, AddSocket(_, kDefaultClientSocket))
      .WillOnce(DoAll(SaveArg<0>(&new_conn_handle), Return(true)));

  EXPECT_TRUE(mocks_.broker_callbacks->HandleNewConnection(kDefaultClientSocket, kDefaultClientAddr));

  RdmnetMessage connect_msg = testmsgs::ClientConnect(cid, E133_DEFAULT_SCOPE, type, manu);

  mocks_.broker_callbacks->HandleSocketMessageReceived(new_conn_handle, connect_msg);
  EXPECT_TRUE(mocks_.broker_callbacks->ServiceClients());

  return new_conn_handle;
}

void TestBrokerCoreRptHandling::TestMessageLimit(BrokerClient::Handle sender_handle,
                                                 const RdmnetMessage& msg,
                                                 unsigned int         num_remaining_messages_allowed)
{
  static constexpr int kNumRetriesToTest = 3;

  for (unsigned int i = 0u; i < num_remaining_messages_allowed; ++i)
  {
    EXPECT_EQ(mocks_.broker_callbacks->HandleSocketMessageReceived(sender_handle, msg),
              HandleMessageResult::kGetNextMessage);
  }

  for (int i = 0; i < kNumRetriesToTest; ++i)
  {
    EXPECT_EQ(mocks_.broker_callbacks->HandleSocketMessageReceived(sender_handle, msg),
              HandleMessageResult::kRetryLater);
  }
}

void TestBrokerCoreRptHandling::TestMessageLimitWithHarvest(BrokerClient::Handle sender_handle,
                                                            const RdmnetMessage& msg,
                                                            unsigned int         num_remaining_messages_allowed)
{
  TestMessageLimit(sender_handle, msg, num_remaining_messages_allowed);
  EXPECT_TRUE(mocks_.broker_callbacks->ServiceClients());  // Harvest (consume/send) a message from every queue
  TestMessageLimit(sender_handle, msg, 1u);
}

TEST_F(TestBrokerCoreRptHandling, DeviceBroadcastThrottlesAtMaxLimit)
{
  static constexpr int kNumDestinations = 3u;

  for (int i = 0u; i < kNumDestinations; ++i)
    AddClient(etcpal::Uuid::OsPreferred(), kRPTClientTypeDevice, kTestManu1);

  auto sender_handle = AddClient(etcpal::Uuid::OsPreferred(), kRPTClientTypeController, kTestManu1);

  auto test_cmd = TestRdmCommand::GetBroadcast(E120_DEVICE_INFO);
  TestMessageLimitWithHarvest(sender_handle, test_cmd.msg, kMaxDeviceMessages);

  testing::Mock::VerifyAndClearExpectations(mocks_.socket_mgr);
}

TEST_F(TestBrokerCoreRptHandling, ControllerBroadcastThrottlesAtMaxLimit)
{
  static constexpr int kNumDestinations = 3u;

  for (int i = 0u; i < kNumDestinations; ++i)
    AddClient(etcpal::Uuid::OsPreferred(), kRPTClientTypeController, kTestManu1);

  auto sender_handle = AddClient(etcpal::Uuid::OsPreferred(), kRPTClientTypeDevice, kTestManu1);

  auto test_response = TestRdmResponse::GetResponseBroadcast(kTestControllerUid, E120_DEVICE_INFO);
  TestMessageLimitWithHarvest(sender_handle, test_response.msg, kMaxControllerMessages);

  testing::Mock::VerifyAndClearExpectations(mocks_.socket_mgr);
}

TEST_F(TestBrokerCoreRptHandling, DeviceManuBroadcastThrottlesAtMaxLimit)
{
  static constexpr int kNumDestinationsForManu1 = 5u;
  static constexpr int kNumDestinationsForManu2 = 2u;

  for (int i = 0u; i < kNumDestinationsForManu1; ++i)
    AddClient(etcpal::Uuid::OsPreferred(), kRPTClientTypeDevice, kTestManu1);
  for (int i = 0u; i < kNumDestinationsForManu2; ++i)
    AddClient(etcpal::Uuid::OsPreferred(), kRPTClientTypeDevice, kTestManu2);

  auto sender_handle = AddClient(etcpal::Uuid::OsPreferred(), kRPTClientTypeController, kTestManu1);

  // Test manu2 message limit
  auto test_manu2_cmd = TestRdmCommand::GetManuBroadcast(kTestManu2, E120_DEVICE_INFO);
  TestMessageLimitWithHarvest(sender_handle, test_manu2_cmd.msg, kMaxDeviceMessages);

  // Verify no all-device broadcasts can be sent due to manu2 limit
  auto test_all_manu_cmd = TestRdmCommand::GetBroadcast(E120_DEVICE_INFO);
  TestMessageLimit(sender_handle, test_all_manu_cmd.msg, 0u);

  // Test manu1 message limit
  auto test_manu1_cmd = TestRdmCommand::GetManuBroadcast(kTestManu1, E120_DEVICE_INFO);
  TestMessageLimit(sender_handle, test_manu1_cmd.msg, kMaxDeviceMessages);

  // Harvesting a message from each queue should allow one all-device broadcast
  EXPECT_TRUE(mocks_.broker_callbacks->ServiceClients());
  TestMessageLimit(sender_handle, test_all_manu_cmd.msg, 1u);

  testing::Mock::VerifyAndClearExpectations(mocks_.socket_mgr);
}
