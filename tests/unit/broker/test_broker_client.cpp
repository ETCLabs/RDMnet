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

#include "broker_client.h"

#include <cstring>
#include <memory>
#include "gmock/gmock.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/pack.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/timer.h"
#include "etcpal_mock/socket.h"
#include "rdm/cpp/uid.h"

class TestBaseBrokerClient : public testing::Test
{
protected:
  static constexpr BrokerClient::Handle kClientHandle = 0;
  static constexpr etcpal_socket_t      kClientSocket = static_cast<etcpal_socket_t>(0);
  size_t                                kMaxQSize = 20;

  std::unique_ptr<BrokerClient> client_;
  etcpal::Uuid                  broker_cid_ = etcpal::Uuid::OsPreferred();

  TestBaseBrokerClient()
  {
    etcpal_reset_all_fakes();
    // Client can't be a direct member because we must reset EtcPal before it is constructed.
    // Mainly so the timers work out correctly.
    client_ = std::make_unique<BrokerClient>(kClientHandle, kClientSocket, kMaxQSize);
    client_->addr = etcpal::SockAddr(etcpal::IpAddr::FromString("10.101.20.30"), 45000);
  }
};

TEST_F(TestBaseBrokerClient, SendsBrokerMessage)
{
  BrokerMessage msg{};
  msg.vector = VECTOR_BROKER_CONNECT_REPLY;

  EXPECT_EQ(client_->Push(broker_cid_, msg), BrokerClient::PushResult::Ok);
  EXPECT_TRUE(client_->Send(broker_cid_));
  EXPECT_EQ(etcpal_send_fake.call_count, 1u);
}

// Generic/unknown clients should send periodic heartbeat messages.
TEST_F(TestBaseBrokerClient, SendsHeartbeat)
{
  // Advance time so that the heartbeat send interval has passed
  etcpal_getms_fake.return_val = (E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000) + 500;

  etcpal_send_fake.custom_fake = [](etcpal_socket_t /*socket*/, const void* data, size_t size, int /*flags*/) {
    EXPECT_EQ(size, 44u);
    EXPECT_EQ(etcpal_unpack_u16b(&(reinterpret_cast<const uint8_t*>(data))[42]), VECTOR_BROKER_NULL);
    return 44;
  };

  EXPECT_TRUE(client_->Send(broker_cid_));
  EXPECT_EQ(etcpal_send_fake.call_count, 1u);
}

TEST_F(TestBaseBrokerClient, HandlesHeartbeatTimeout)
{
  // Advance time so that the heartbeat timeout has passed.
  etcpal_getms_fake.return_val = (E133_HEARTBEAT_TIMEOUT_SEC * 1000) + 500;
  EXPECT_TRUE(client_->TcpConnExpired());
}

TEST_F(TestBaseBrokerClient, HonorsMaxQSize)
{
  BrokerMessage msg{};
  msg.vector = VECTOR_BROKER_CLIENT_ADD;

  RdmnetRptClientEntry entry{};

  BROKER_GET_CLIENT_LIST(&msg)->client_protocol = kClientProtocolRPT;
  RdmnetRptClientList* rpt_list = BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&msg));
  rpt_list->client_entries = &entry;
  rpt_list->num_client_entries = 1;

  for (size_t i = 0; i < kMaxQSize; ++i)
  {
    ASSERT_EQ(client_->Push(broker_cid_, msg), BrokerClient::PushResult::Ok) << "Failed on iteration " << i;
  }
  EXPECT_EQ(client_->Push(broker_cid_, msg), BrokerClient::PushResult::QueueFull);
}

TEST_F(TestBaseBrokerClient, MaxQSizeInfinite)
{
  // Max Q Size of 0 should mean infinite
  client_->max_q_size = 0;

  BrokerMessage msg{};
  msg.vector = VECTOR_BROKER_CLIENT_ADD;

  RdmnetRptClientEntry entry{};

  BROKER_GET_CLIENT_LIST(&msg)->client_protocol = kClientProtocolRPT;
  RdmnetRptClientList* rpt_list = BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&msg));
  rpt_list->client_entries = &entry;
  rpt_list->num_client_entries = 1;

  for (size_t i = 0; i < 1000; ++i)
  {
    ASSERT_EQ(client_->Push(broker_cid_, msg), BrokerClient::PushResult::Ok) << "Failed on iteration " << i;
  }
}

TEST_F(TestBaseBrokerClient, TransfersInformationToRptController)
{
  RdmnetRptClientEntry client_entry{etcpal::Uuid::OsPreferred().get(), rdm::Uid(0x6574, 0x12345678).get(),
                                    kRPTClientTypeController, etcpal::Uuid::OsPreferred().get()};
  RPTController        controller(40, client_entry, *client_);

  EXPECT_EQ(controller.cid, client_entry.cid);
  EXPECT_EQ(controller.client_protocol, kClientProtocolRPT);
  EXPECT_EQ(controller.addr, client_->addr);
  EXPECT_EQ(controller.handle, kClientHandle);
  EXPECT_EQ(controller.socket, kClientSocket);
  EXPECT_EQ(controller.max_q_size, 40u);
  EXPECT_EQ(controller.uid, client_entry.uid);
  EXPECT_EQ(controller.client_type, client_entry.type);
  EXPECT_EQ(controller.binding_cid, client_entry.binding_cid);
}

TEST_F(TestBaseBrokerClient, TransfersInformationToRptDevice)
{
  RdmnetRptClientEntry client_entry{etcpal::Uuid::OsPreferred().get(), rdm::Uid(0x6574, 0x12345678).get(),
                                    kRPTClientTypeDevice, etcpal::Uuid::OsPreferred().get()};
  RPTDevice            device(40, client_entry, *client_);

  EXPECT_EQ(device.cid, client_entry.cid);
  EXPECT_EQ(device.client_protocol, kClientProtocolRPT);
  EXPECT_EQ(device.addr, client_->addr);
  EXPECT_EQ(device.handle, kClientHandle);
  EXPECT_EQ(device.socket, kClientSocket);
  EXPECT_EQ(device.max_q_size, 40u);
  EXPECT_EQ(device.uid, client_entry.uid);
  EXPECT_EQ(device.client_type, client_entry.type);
  EXPECT_EQ(device.binding_cid, client_entry.binding_cid);
}

class TestBrokerClientRptController : public testing::Test
{
protected:
  static constexpr BrokerClient::Handle kClientHandle = 0;
  static constexpr etcpal_socket_t      kClientSocket = static_cast<etcpal_socket_t>(0);
  static constexpr size_t               kMaxQSize = 10;

  RdmnetRptClientEntry           client_entry_{etcpal::Uuid::OsPreferred().get(), rdm::Uid(0x6574, 0x12345678).get(),
                                     kRPTClientTypeController, etcpal::Uuid::OsPreferred().get()};
  std::unique_ptr<RPTController> controller_;
  etcpal::Uuid                   broker_cid_ = etcpal::Uuid::OsPreferred();

  RptHeader            rpt_header_{};
  RptStatusMsg         status_msg_{};
  RdmBuffer            rdm_buf_{};
  RptMessage           request_{};
  RdmnetRptClientEntry rpt_client_entry_{};
  BrokerMessage        broker_msg_{};
  BrokerClient::Handle sending_controller_handle_{static_cast<BrokerClient::Handle>(1)};

  TestBrokerClientRptController()
  {
    etcpal_reset_all_fakes();

    rdm_buf_.data_len = RDM_MIN_BYTES;
    request_.vector = VECTOR_RPT_REQUEST;
    RPT_GET_RDM_BUF_LIST(&request_)->rdm_buffers = &rdm_buf_;
    RPT_GET_RDM_BUF_LIST(&request_)->num_rdm_buffers = 1;

    broker_msg_.vector = VECTOR_BROKER_CLIENT_ADD;
    BROKER_GET_CLIENT_LIST(&broker_msg_)->client_protocol = kClientProtocolRPT;
    RdmnetRptClientList* rpt_list = BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&broker_msg_));
    rpt_list->client_entries = &rpt_client_entry_;
    rpt_list->num_client_entries = 1;

    BrokerClient bc(kClientHandle, kClientSocket);
    controller_ = std::make_unique<RPTController>(kMaxQSize, client_entry_, bc);
  }
};

// Controllers should send periodic heartbeat messages.
TEST_F(TestBrokerClientRptController, SendsHeartbeat)
{
  // Advance time so that the heartbeat send interval has passed
  etcpal_getms_fake.return_val = (E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000) + 500;

  etcpal_send_fake.custom_fake = [](etcpal_socket_t /*socket*/, const void* data, size_t size, int /*flags*/) {
    EXPECT_EQ(size, 44u);
    EXPECT_EQ(etcpal_unpack_u16b(&(reinterpret_cast<const uint8_t*>(data))[42]), VECTOR_BROKER_NULL);
    return 44;
  };

  EXPECT_TRUE(controller_->Send(broker_cid_));
  EXPECT_EQ(etcpal_send_fake.call_count, 1u);
}

TEST_F(TestBrokerClientRptController, HandlesHeartbeatTimeout)
{
  // Advance time so that the heartbeat timeout has passed.
  etcpal_getms_fake.return_val = (E133_HEARTBEAT_TIMEOUT_SEC * 1000) + 500;
  EXPECT_TRUE(controller_->TcpConnExpired());
}

TEST_F(TestBrokerClientRptController, HonorsMaxQSize)
{
  for (size_t i = 0; i < kMaxQSize; ++i)
  {
    if (i % 3 == 0)
    {
      ASSERT_EQ(controller_->Push(broker_cid_, broker_msg_), BrokerClient::PushResult::Ok)
          << "Failed on iteration " << i;
    }
    else if (i % 3 == 1)
    {
      ASSERT_EQ(controller_->Push(broker_cid_, rpt_header_, status_msg_), BrokerClient::PushResult::Ok)
          << "Failed on iteration " << i;
    }
    else
    {
      ASSERT_EQ(controller_->Push(sending_controller_handle_, broker_cid_, request_), BrokerClient::PushResult::Ok)
          << "Failed on iteration " << i;
    }
  }

  EXPECT_EQ(controller_->Push(broker_cid_, broker_msg_), BrokerClient::PushResult::QueueFull);
  EXPECT_EQ(controller_->Push(broker_cid_, rpt_header_, status_msg_), BrokerClient::PushResult::QueueFull);
  EXPECT_EQ(controller_->Push(sending_controller_handle_, broker_cid_, request_), BrokerClient::PushResult::QueueFull);
}

TEST_F(TestBrokerClientRptController, InfiniteMaxQSize)
{
  // Max Q Size of 0 should mean infinite
  controller_->max_q_size = 0;

  for (size_t i = 0; i < 1000u; ++i)
  {
    if (i % 3 == 0)
    {
      ASSERT_EQ(controller_->Push(broker_cid_, broker_msg_), BrokerClient::PushResult::Ok)
          << "Failed on iteration " << i;
    }
    else if (i % 3 == 1)
    {
      ASSERT_EQ(controller_->Push(broker_cid_, rpt_header_, status_msg_), BrokerClient::PushResult::Ok)
          << "Failed on iteration " << i;
    }
    else
    {
      ASSERT_EQ(controller_->Push(sending_controller_handle_, broker_cid_, request_), BrokerClient::PushResult::Ok)
          << "Failed on iteration " << i;
    }
  }
}

class TestBrokerClientRptDevice : public testing::Test
{
public:
  static constexpr etcpal_socket_t kClientSocket = static_cast<etcpal_socket_t>(0);

protected:
  static constexpr BrokerClient::Handle kClientHandle = 0;
  static constexpr size_t               kMaxQSize = 50;

  const rdm::Uid kDeviceUid{0x6574, 0x12345678};

  RdmnetRptClientEntry       client_entry_{etcpal::Uuid::OsPreferred().get(), kDeviceUid.get(), kRPTClientTypeDevice,
                                     etcpal::Uuid::OsPreferred().get()};
  std::unique_ptr<RPTDevice> device_;
  etcpal::Uuid               broker_cid_ = etcpal::Uuid::OsPreferred();

  RdmBuffer            rdm_buf_{};
  RptMessage           request_{};
  RdmnetRptClientEntry rpt_client_entry_{};
  BrokerMessage        broker_msg_{};

  TestBrokerClientRptDevice()
  {
    etcpal_reset_all_fakes();

    rdm_buf_.data_len = RDM_MIN_BYTES;
    request_.vector = VECTOR_RPT_REQUEST;
    RPT_GET_RDM_BUF_LIST(&request_)->rdm_buffers = &rdm_buf_;
    RPT_GET_RDM_BUF_LIST(&request_)->num_rdm_buffers = 1;

    broker_msg_.vector = VECTOR_BROKER_CLIENT_ADD;
    BROKER_GET_CLIENT_LIST(&broker_msg_)->client_protocol = kClientProtocolRPT;
    RdmnetRptClientList* rpt_list = BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&broker_msg_));
    rpt_list->client_entries = &rpt_client_entry_;
    rpt_list->num_client_entries = 1;

    BrokerClient bc(kClientHandle, kClientSocket);
    device_ = std::make_unique<RPTDevice>(kMaxQSize, client_entry_, bc);
  }
};

// Devices should send periodic heartbeat messages.
TEST_F(TestBrokerClientRptDevice, SendsHeartbeat)
{
  // Advance time so that the heartbeat send interval has passed
  etcpal_getms_fake.return_val = (E133_TCP_HEARTBEAT_INTERVAL_SEC * 1000) + 500;

  etcpal_send_fake.custom_fake = [](etcpal_socket_t /*socket*/, const void* data, size_t size, int /*flags*/) {
    EXPECT_EQ(size, 44u);
    EXPECT_EQ(etcpal_unpack_u16b(&(reinterpret_cast<const uint8_t*>(data))[42]), VECTOR_BROKER_NULL);
    return 44;
  };

  EXPECT_TRUE(device_->Send(broker_cid_));
  EXPECT_EQ(etcpal_send_fake.call_count, 1u);
}

TEST_F(TestBrokerClientRptDevice, HandlesHeartbeatTimeout)
{
  // Advance time so that the heartbeat timeout has passed.
  etcpal_getms_fake.return_val = (E133_HEARTBEAT_TIMEOUT_SEC * 1000) + 500;
  EXPECT_TRUE(device_->TcpConnExpired());
}

TEST_F(TestBrokerClientRptDevice, HonorsMaxQSize)
{
  for (size_t i = 0; i < kMaxQSize; ++i)
  {
    if (i % 2 == 0)
    {
      ASSERT_EQ(device_->Push(broker_cid_, broker_msg_), BrokerClient::PushResult::Ok) << "Failed on iteration " << i;
    }
    else
    {
      ASSERT_EQ(device_->Push(static_cast<BrokerClient::Handle>(kClientHandle + i), broker_cid_, request_),
                BrokerClient::PushResult::Ok)
          << "Failed on iteration " << i;
    }
  }

  EXPECT_EQ(device_->Push(broker_cid_, broker_msg_), BrokerClient::PushResult::QueueFull);
  EXPECT_EQ(device_->Push(kClientHandle + 1, broker_cid_, request_), BrokerClient::PushResult::QueueFull);
}

TEST_F(TestBrokerClientRptDevice, InfiniteMaxQSize)
{
  // Max Q Size of 0 should mean infinite
  device_->max_q_size = 0;

  for (size_t i = 0; i < 1000u; ++i)
  {
    if (i % 2 == 0)
    {
      ASSERT_EQ(device_->Push(broker_cid_, broker_msg_), BrokerClient::PushResult::Ok) << "Failed on iteration " << i;
    }
    else
    {
      ASSERT_EQ(device_->Push(static_cast<BrokerClient::Handle>(kClientHandle + i), broker_cid_, request_),
                BrokerClient::PushResult::Ok)
          << "Failed on iteration " << i;
    }
  }
}

// Helper function and data for the FairScheduler test.

static const etcpal::Uuid kController1Cid({1, 2, 3, 4, 5, 6, 7, 8});
static const etcpal::Uuid kController2Cid({1, 2, 3, 4, 255, 254, 253, 252});
static const etcpal::Uuid kController3Cid({255, 254, 253, 252, 251, 250, 249, 248});

// Sends the next queue message from an RPTDevice and verifies that the buffer given to
// etcpal_send() contains a given controller's CID. The CIDs are predefined globally because
// the fake function pointer must be stateless.
template <size_t Controller>
void SendAndVerify(RPTDevice* device, const etcpal::Uuid& broker_cid)
{
  static_assert(Controller > 0 && Controller < 4);
  static const etcpal::Uuid* controllers[3] = {&kController1Cid, &kController2Cid, &kController3Cid};

  SCOPED_TRACE(std::string("While verifying CID for controller ") + std::to_string(Controller));

  RESET_FAKE(etcpal_send);
  etcpal_send_fake.custom_fake = [](etcpal_socket_t socket, const void* data, size_t size, int /*flags*/) {
    EXPECT_EQ(socket, TestBrokerClientRptDevice::kClientSocket);
    EXPECT_EQ(std::memcmp(&(reinterpret_cast<const uint8_t*>(data))[23], controllers[Controller - 1]->data(), 16), 0);
    return (int)size;
  };
  EXPECT_TRUE(device->Send(broker_cid));
  EXPECT_EQ(etcpal_send_fake.call_count, 1u);
}

TEST_F(TestBrokerClientRptDevice, FairScheduler)
{
  RptMessage request{};
  request.vector = VECTOR_RPT_REQUEST;

  request.header.dest_uid = kDeviceUid.get();
  request.header.dest_endpoint_id = E133_NULL_ENDPOINT;
  request.header.source_uid = RdmUid{0x6574, 1};
  request.header.source_endpoint_id = E133_NULL_ENDPOINT;
  request.header.seqnum = 1;

  // A dummy RdmBuffer, the packing code doesn't care about the contents
  RdmBuffer rdm{{}, 100};
  RPT_GET_RDM_BUF_LIST(&request)->rdm_buffers = &rdm;
  RPT_GET_RDM_BUF_LIST(&request)->num_rdm_buffers = 1;

  // Push 10 requests from controller 1
  const auto controller_1_handle = kClientHandle + 1;
  for (size_t i = 0; i < 10; ++i)
  {
    EXPECT_EQ(device_->Push(controller_1_handle, kController1Cid, request), BrokerClient::PushResult::Ok);
    ++request.header.seqnum;
  }

  // Push 1 request from controller 2
  const auto controller_2_handle = kClientHandle + 2;
  request.header.source_uid = RdmUid{0x6574, 2};
  request.header.seqnum = 1;
  EXPECT_EQ(device_->Push(controller_2_handle, kController2Cid, request), BrokerClient::PushResult::Ok);

  // Push 2 requests from controller 3
  const auto controller_3_handle = kClientHandle + 3;
  request.header.source_uid = RdmUid{0x6574, 3};
  request.header.seqnum = 1;
  EXPECT_EQ(device_->Push(controller_3_handle, kController3Cid, request), BrokerClient::PushResult::Ok);
  ++request.header.seqnum;
  EXPECT_EQ(device_->Push(controller_3_handle, kController3Cid, request), BrokerClient::PushResult::Ok);

  // We have 10 messages from controller 1, 1 from controller 2, and 2 from controller 3.
  // The order should be 1, 2, 3, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1.
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<2>(device_.get(), broker_cid_);
  SendAndVerify<3>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<3>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
  SendAndVerify<1>(device_.get(), broker_cid_);
}
