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
#include <array>
#include "etcpal/common.h"
#include "etcpal_mock/socket.h"
#include "etcpal_mock/timer.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/private/connection.h"
#include "rdmnet_mock/core.h"
#include "rdmnet_mock/private/core.h"
#include "gtest/gtest.h"

extern "C" {
FAKE_VOID_FUNC(conncb_connected, rdmnet_conn_t, const RdmnetConnectedInfo*, void*);
FAKE_VOID_FUNC(conncb_connect_failed, rdmnet_conn_t, const RdmnetConnectFailedInfo*, void*);
FAKE_VOID_FUNC(conncb_disconnected, rdmnet_conn_t, const RdmnetDisconnectedInfo*, void*);
FAKE_VOID_FUNC(conncb_msg_received, rdmnet_conn_t, const RdmnetMessage*, void*);
}

class TestConnection : public testing::Test
{
protected:
  RdmnetConnectionConfig default_config_{
      {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
      {conncb_connected, conncb_connect_failed, conncb_disconnected, conncb_msg_received},
      nullptr};
  rdmnet_conn_t conn_;

  void SetUp() override
  {
    RESET_FAKE(conncb_connected);
    RESET_FAKE(conncb_connect_failed);
    RESET_FAKE(conncb_disconnected);
    RESET_FAKE(conncb_msg_received);

    rdmnet_mock_core_reset_and_init();

    etcpal_socket_reset_all_fakes();
    etcpal_timer_reset_all_fakes();
    etcpal_socket_fake.return_val = kEtcPalErrOk;
    etcpal_setblocking_fake.return_val = kEtcPalErrOk;
    etcpal_connect_fake.return_val = kEtcPalErrInProgress;

    etcpal_poll_add_socket_fake.return_val = kEtcPalErrOk;
    etcpal_poll_wait_fake.return_val = kEtcPalErrTimedOut;

    ASSERT_EQ(kEtcPalErrOk, rdmnet_conn_init());
    ASSERT_EQ(kEtcPalErrOk, rdmnet_connection_create(&default_config_, &conn_));
  }

  void TearDown() override
  {
    rdmnet_connection_destroy(conn_, nullptr);
    rdmnet_conn_deinit();
  }

  void PassTimeAndTick()
  {
    etcpal_getms_fake.return_val += 1000;
    rdmnet_conn_tick();
  }
};

class TestConnectionAlreadyConnected : public TestConnection
{
protected:
  void SetUp() override
  {
    TestConnection::SetUp();

    // This allows us to skip the connection process and go straight to a connected state.
    etcpal_socket_t fake_socket = 0;
    EtcPalSockAddr remote_addr{};
    ASSERT_EQ(kEtcPalErrOk, rdmnet_attach_existing_socket(conn_, fake_socket, &remote_addr));
  }
};

// Need to test the value of disconn_info inside a custom fake, because only the pointer is saved
// (which is invalid after the function returns)
extern "C" void conncb_socket_error(rdmnet_conn_t, const RdmnetDisconnectedInfo* disconn_info, void* context)
{
  ETCPAL_UNUSED_ARG(context);
  EXPECT_EQ(disconn_info->socket_err, kEtcPalErrConnReset);
  EXPECT_EQ(disconn_info->event, kRdmnetDisconnectAbruptClose);
}

TEST_F(TestConnectionAlreadyConnected, DisconnectsOnSocketError)
{
  conncb_disconnected_fake.custom_fake = conncb_socket_error;

  // Simulate an error on a socket, make sure it is marked disconnected.
  rdmnet_socket_error(conn_, kEtcPalErrConnReset);

  ASSERT_EQ(conncb_disconnected_fake.call_count, 1u);
  ASSERT_EQ(conncb_disconnected_fake.arg0_val, conn_);
}

TEST_F(TestConnection, HandlesSocketErrorOnConnect)
{
  EtcPalSockAddr remote_addr;
  ETCPAL_IP_SET_V4_ADDRESS(&remote_addr.ip, 0x0a650101);
  remote_addr.port = 8888;

  BrokerClientConnectMsg connect_msg{};
  ASSERT_EQ(kEtcPalErrOk, rdmnet_connect(conn_, &remote_addr, &connect_msg));

  PassTimeAndTick();
  rdmnet_socket_error(conn_, kEtcPalErrConnRefused);

  EXPECT_EQ(conncb_connect_failed_fake.call_count, 1u);
}

TEST_F(TestConnection, SetsCorrectSocketOptionsIpv4)
{
  EtcPalSockAddr remote_addr;
  ETCPAL_IP_SET_V4_ADDRESS(&remote_addr.ip, 0x0a650101);
  remote_addr.port = 8888;

  BrokerClientConnectMsg connect_msg{};

  ASSERT_EQ(kEtcPalErrOk, rdmnet_connect(conn_, &remote_addr, &connect_msg));
  PassTimeAndTick();

  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  EXPECT_EQ(etcpal_socket_fake.arg0_val, ETCPAL_AF_INET);
  EXPECT_EQ(etcpal_socket_fake.arg1_val, ETCPAL_STREAM);

  EXPECT_EQ(etcpal_setblocking_fake.call_count, 1u);
  EXPECT_EQ(etcpal_setblocking_fake.arg1_val, false);

  EXPECT_EQ(etcpal_connect_fake.call_count, 1u);
}

TEST_F(TestConnection, SetsCorrectSocketOptionsIpv6)
{
  EtcPalSockAddr remote_addr;
  const std::array<uint8_t, 16> v6_data = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                                           0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  ETCPAL_IP_SET_V6_ADDRESS(&remote_addr.ip, v6_data.data());
  remote_addr.port = 8888;

  BrokerClientConnectMsg connect_msg{};

  ASSERT_EQ(kEtcPalErrOk, rdmnet_connect(conn_, &remote_addr, &connect_msg));
  PassTimeAndTick();

  EXPECT_EQ(etcpal_socket_fake.call_count, 1u);
  EXPECT_EQ(etcpal_socket_fake.arg0_val, ETCPAL_AF_INET6);
  EXPECT_EQ(etcpal_socket_fake.arg1_val, ETCPAL_STREAM);

  EXPECT_EQ(etcpal_setblocking_fake.call_count, 1u);
  EXPECT_EQ(etcpal_setblocking_fake.arg1_val, false);

  EXPECT_EQ(etcpal_connect_fake.call_count, 1u);
}

TEST_F(TestConnection, EventToStringFunctionsWork)
{
  EXPECT_TRUE(rdmnet_connect_fail_event_to_string(kRdmnetConnectFailSocketFailure) != nullptr);
  EXPECT_TRUE(rdmnet_connect_fail_event_to_string(static_cast<rdmnet_connect_fail_event_t>(INT_MAX)) == nullptr);
  EXPECT_TRUE(rdmnet_connect_fail_event_to_string(static_cast<rdmnet_connect_fail_event_t>(-1)) == nullptr);

  EXPECT_TRUE(rdmnet_disconnect_event_to_string(kRdmnetDisconnectAbruptClose) != nullptr);
  EXPECT_TRUE(rdmnet_disconnect_event_to_string(static_cast<rdmnet_disconnect_event_t>(INT_MAX)) == nullptr);
  EXPECT_TRUE(rdmnet_disconnect_event_to_string(static_cast<rdmnet_disconnect_event_t>(-1)) == nullptr);
}
