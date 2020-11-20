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

#include <array>
#include <cstring>
#include "etcpal/common.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/mutex.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "etcpal_mock/timer.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/core/connection.h"
#include "rdmnet_mock/core/broker_prot.h"
#include "rdmnet_mock/core/message.h"
#include "rdmnet_mock/core/msg_buf.h"
#include "rdmnet_mock/core/common.h"
#include "gtest/gtest.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

extern "C" {
FAKE_VOID_FUNC(conncb_connected, RCConnection*, const RCConnectedInfo*);
FAKE_VOID_FUNC(conncb_connect_failed, RCConnection*, const RCConnectFailedInfo*);
FAKE_VOID_FUNC(conncb_disconnected, RCConnection*, const RCDisconnectedInfo*);
FAKE_VOID_FUNC(conncb_msg_received, RCConnection*, const RdmnetMessage*);
FAKE_VOID_FUNC(conncb_destroyed, RCConnection*);
}

static RCPolledSocketInfo conn_poll_info;

static constexpr const char*  kTestScope = "Test Scope";
static constexpr const char*  kTestDomain = "local.";
static const etcpal::Uuid     kTestLocalCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22");
static const rdm::Uid         kTestLocalUid = rdm::Uid::FromString("6574:82048492");
static const etcpal::Uuid     kTestBrokerCid = etcpal::Uuid::FromString("3569236f-6a14-4db3-815d-e3961d386b72");
static const rdm::Uid         kTestBrokerUid = rdm::Uid::FromString("6574:a0e34807");
static const etcpal::SockAddr kTestRemoteAddrV4(etcpal::IpAddr::FromString("10.101.1.1"), 8888);
static const etcpal::SockAddr kTestRemoteAddrV6(etcpal::IpAddr::FromString("2001:db8::1234:5678"), 8888);

class TestConnection : public testing::Test
{
protected:
  RCConnection                     conn_{};
  etcpal::Mutex                    conn_lock_;
  static constexpr etcpal_socket_t kFakeSocket = 0;

  BrokerClientConnectMsg connect_msg_{};

  void SetUp() override
  {
    RESET_FAKE(conncb_connected);
    RESET_FAKE(conncb_connect_failed);
    RESET_FAKE(conncb_disconnected);
    RESET_FAKE(conncb_msg_received);
    RESET_FAKE(conncb_destroyed);

    rdmnet_mock_core_reset_and_init();
    rc_broker_prot_reset_all_fakes();
    rc_message_reset_all_fakes();
    rc_msg_buf_reset_all_fakes();
    etcpal_reset_all_fakes();

    etcpal_socket_fake.return_val = kEtcPalErrOk;
    etcpal_setblocking_fake.return_val = kEtcPalErrOk;
    etcpal_connect_fake.return_val = kEtcPalErrInProgress;

    etcpal_poll_add_socket_fake.return_val = kEtcPalErrOk;
    etcpal_poll_wait_fake.return_val = kEtcPalErrTimedOut;

    // Fill in the connection information
    conn_.local_cid = etcpal::Uuid::FromString("51077344-7164-487e-88c1-b3146de32d4c").get();
    conn_.lock = &conn_lock_.get();
    conn_.callbacks.connected = conncb_connected;
    conn_.callbacks.connect_failed = conncb_connect_failed;
    conn_.callbacks.disconnected = conncb_disconnected;
    conn_.callbacks.message_received = conncb_msg_received;
    conn_.callbacks.destroyed = conncb_destroyed;
    conn_.is_blocking = true;

    // Fill in the connect message
    std::strcpy(connect_msg_.scope, kTestScope);
    connect_msg_.e133_version = E133_VERSION;
    std::strcpy(connect_msg_.search_domain, kTestDomain);
    connect_msg_.client_entry.client_protocol = kClientProtocolRPT;
    GET_RPT_CLIENT_ENTRY(&connect_msg_.client_entry)->cid = kTestLocalCid.get();
    GET_RPT_CLIENT_ENTRY(&connect_msg_.client_entry)->uid = kTestLocalUid.get();
    GET_RPT_CLIENT_ENTRY(&connect_msg_.client_entry)->type = kRPTClientTypeController;

    // Give the connection its socket value
    etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* socket) {
      *socket = kFakeSocket;
      return kEtcPalErrOk;
    };

    // Set us up to capture the poll info that the connection creates so that we can use it to
    // feed data back to the connection.
    std::memset(&conn_poll_info, 0, sizeof(RCPolledSocketInfo));
    rc_add_polled_socket_fake.custom_fake = [](etcpal_socket_t, etcpal_poll_events_t, RCPolledSocketInfo* info) {
      conn_poll_info = *info;
      return kEtcPalErrOk;
    };

    ASSERT_EQ(kEtcPalErrOk, rc_conn_module_init());
    ASSERT_EQ(kEtcPalErrOk, rc_conn_register(&conn_));
  }

  void TearDown() override
  {
    rc_conn_unregister(&conn_, nullptr);
    rc_conn_module_deinit();
  }

  void PassTimeAndTick(uint32_t time_to_pass = 1000)
  {
    etcpal_getms_fake.return_val += time_to_pass;
    rc_conn_module_tick();
  }
};

void SetValidConnectReply(RdmnetMessage& msg)
{
  msg.vector = ACN_VECTOR_ROOT_BROKER;
  msg.sender_cid = kTestBrokerCid.get();
  RDMNET_GET_BROKER_MSG(&msg)->vector = VECTOR_BROKER_CONNECT_REPLY;
  BrokerConnectReplyMsg* conn_reply = BROKER_GET_CONNECT_REPLY_MSG(RDMNET_GET_BROKER_MSG(&msg));
  conn_reply->broker_uid = kTestBrokerUid.get();
  conn_reply->client_uid = kTestLocalUid.get();
  conn_reply->connect_status = kRdmnetConnectOk;
  conn_reply->e133_version = E133_VERSION;
}

TEST_F(TestConnection, HandlesSocketErrorOnConnect)
{
  ASSERT_EQ(kEtcPalErrOk, rc_conn_connect(&conn_, &kTestRemoteAddrV4.get(), &connect_msg_));

  // Start connection
  PassTimeAndTick();

  EtcPalPollEvent event;
  event.err = kEtcPalErrConnRefused;
  event.events = ETCPAL_POLL_ERR;
  event.socket = kFakeSocket;
  event.user_data = &conn_.poll_info;

  conncb_connect_failed_fake.custom_fake = [](RCConnection*, const RCConnectFailedInfo* failed_info) {
    EXPECT_EQ(failed_info->event, kRdmnetConnectFailTcpLevel);
    EXPECT_EQ(failed_info->socket_err, kEtcPalErrConnRefused);
  };

  conn_.poll_info.callback(&event, conn_.poll_info.data);

  EXPECT_EQ(conncb_connect_failed_fake.call_count, 1u);
}

TEST_F(TestConnection, SetsCorrectSocketOptionsIpv4)
{
  ASSERT_EQ(kEtcPalErrOk, rc_conn_connect(&conn_, &kTestRemoteAddrV4.get(), &connect_msg_));
  PassTimeAndTick();

  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  EXPECT_EQ(etcpal_socket_fake.arg0_val, ETCPAL_AF_INET);
  EXPECT_EQ(etcpal_socket_fake.arg1_val, ETCPAL_SOCK_STREAM);

  EXPECT_EQ(etcpal_setblocking_fake.call_count, 1u);
  EXPECT_EQ(etcpal_setblocking_fake.arg1_val, false);

  EXPECT_EQ(etcpal_connect_fake.call_count, 1u);
}

TEST_F(TestConnection, SetsCorrectSocketOptionsIpv6)
{
  ASSERT_EQ(kEtcPalErrOk, rc_conn_connect(&conn_, &kTestRemoteAddrV6.get(), &connect_msg_));
  PassTimeAndTick();

  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  EXPECT_EQ(etcpal_socket_fake.arg0_val, ETCPAL_AF_INET6);
  EXPECT_EQ(etcpal_socket_fake.arg1_val, ETCPAL_SOCK_STREAM);

  EXPECT_EQ(etcpal_setblocking_fake.call_count, 1u);
  EXPECT_EQ(etcpal_setblocking_fake.arg1_val, false);

  EXPECT_EQ(etcpal_connect_fake.call_count, 1u);
}

TEST_F(TestConnection, ReportsConnectionCorrectly)
{
  ASSERT_EQ(kEtcPalErrOk, rc_conn_connect(&conn_, &kTestRemoteAddrV4.get(), &connect_msg_));
  PassTimeAndTick();

  ASSERT_NE(conn_poll_info.callback, nullptr);

  EtcPalPollEvent event;
  event.events = ETCPAL_POLL_CONNECT;
  event.socket = kFakeSocket;
  conn_poll_info.callback(&event, conn_poll_info.data);

  ASSERT_EQ(rc_broker_send_client_connect_fake.call_count, 1u);

  SetValidConnectReply(conn_.recv_buf.msg);

  etcpal_error_t return_vals[2] = {kEtcPalErrOk, kEtcPalErrNoData};
  SET_RETURN_SEQ(rc_msg_buf_parse_data, return_vals, 2);
  event.events = ETCPAL_POLL_IN;

  conncb_connected_fake.custom_fake = [](RCConnection*, const RCConnectedInfo* conn_info) {
    EXPECT_EQ(conn_info->broker_cid, kTestBrokerCid);
    EXPECT_EQ(conn_info->broker_uid, kTestBrokerUid);
    EXPECT_EQ(conn_info->connected_addr, kTestRemoteAddrV4);
    EXPECT_EQ(conn_info->client_uid, kTestLocalUid);
  };

  conn_poll_info.callback(&event, conn_poll_info.data);

  EXPECT_EQ(conncb_connected_fake.call_count, 1u);
}

TEST_F(TestConnection, DestroyedCalledOnUnregister)
{
  rc_conn_unregister(&conn_, nullptr);
  PassTimeAndTick();

  EXPECT_EQ(conncb_destroyed_fake.call_count, 1u);
  EXPECT_EQ(conncb_destroyed_fake.arg0_val, &conn_);
}

TEST_F(TestConnection, HandlesTimeoutAfterTcpEstablished)
{
  ASSERT_EQ(kEtcPalErrOk, rc_conn_connect(&conn_, &kTestRemoteAddrV4.get(), &connect_msg_));
  PassTimeAndTick();

  ASSERT_NE(conn_poll_info.callback, nullptr);

  EtcPalPollEvent event;
  event.events = ETCPAL_POLL_CONNECT;
  event.socket = kFakeSocket;
  conn_poll_info.callback(&event, conn_poll_info.data);

  ASSERT_EQ(rc_broker_send_client_connect_fake.call_count, 1u);

  PassTimeAndTick((E133_HEARTBEAT_TIMEOUT_SEC * 1000) - 1000);

  EXPECT_EQ(conncb_connect_failed_fake.call_count, 0u);

  conncb_connect_failed_fake.custom_fake = [](RCConnection*, const RCConnectFailedInfo* failed_info) {
    EXPECT_EQ(failed_info->event, kRdmnetConnectFailNoReply);
  };

  PassTimeAndTick(2000);
  EXPECT_EQ(conncb_connect_failed_fake.call_count, 1u);
}

class TestConnectionAlreadyConnected : public TestConnection
{
protected:
  void SetUp() override
  {
    TestConnection::SetUp();

    ASSERT_EQ(kEtcPalErrOk, rc_conn_connect(&conn_, &kTestRemoteAddrV4.get(), &connect_msg_));
    PassTimeAndTick();

    ASSERT_NE(conn_poll_info.callback, nullptr);

    EtcPalPollEvent event;
    event.events = ETCPAL_POLL_CONNECT;
    event.socket = kFakeSocket;
    conn_poll_info.callback(&event, conn_poll_info.data);

    ASSERT_EQ(rc_broker_send_client_connect_fake.call_count, 1u);

    SetValidConnectReply(conn_.recv_buf.msg);

    etcpal_error_t return_vals[2] = {kEtcPalErrOk, kEtcPalErrNoData};
    SET_RETURN_SEQ(rc_msg_buf_parse_data, return_vals, 2);
    event.events = ETCPAL_POLL_IN;

    conn_poll_info.callback(&event, conn_poll_info.data);

    EXPECT_EQ(conncb_connected_fake.call_count, 1u);
  }
};

TEST_F(TestConnectionAlreadyConnected, DisconnectsOnSocketError)
{
  conncb_disconnected_fake.custom_fake = [](RCConnection*, const RCDisconnectedInfo* disconn_info) {
    EXPECT_EQ(disconn_info->socket_err, kEtcPalErrConnReset);
    EXPECT_EQ(disconn_info->event, kRdmnetDisconnectAbruptClose);
  };

  // Simulate an error on a socket, make sure it is marked disconnected.
  EtcPalPollEvent event;
  event.err = kEtcPalErrConnReset;
  event.events = ETCPAL_POLL_ERR;
  event.socket = kFakeSocket;
  event.user_data = &conn_.poll_info;
  conn_.poll_info.callback(&event, conn_.poll_info.data);

  EXPECT_EQ(conncb_disconnected_fake.call_count, 1u);
  EXPECT_EQ(conncb_disconnected_fake.arg0_val, &conn_);
}

TEST_F(TestConnectionAlreadyConnected, MsgBufResetOnDisconnect)
{
  RESET_FAKE(rc_msg_buf_init);

  EtcPalPollEvent event;
  event.err = kEtcPalErrConnReset;
  event.events = ETCPAL_POLL_ERR;
  event.socket = kFakeSocket;
  event.user_data = &conn_.poll_info;
  conn_.poll_info.callback(&event, conn_.poll_info.data);
  ASSERT_EQ(conncb_disconnected_fake.call_count, 1u);

  EXPECT_EQ(rc_msg_buf_init_fake.call_count, 1u);
  EXPECT_EQ(rc_msg_buf_init_fake.arg0_val, &conn_.recv_buf);
}
