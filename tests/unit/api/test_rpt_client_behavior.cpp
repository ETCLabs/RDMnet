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
#include <map>
#include <utility>

#include "gtest/gtest.h"
#include "rdmnet/client.h"

#include "rdmnet_mock/core.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/discovery.h"

#include "rdmnet_client_fake_callbacks.h"

extern "C" {
static rdmnet_conn_t next_conn_handle;
static rdmnet_conn_t last_conn_handle;
static RdmnetConnectionConfig last_conn_config;

static etcpal_error_t create_conn_and_save_config(const RdmnetConnectionConfig* config, rdmnet_conn_t* handle)
{
  *handle = next_conn_handle++;
  last_conn_handle = *handle;
  last_conn_config = *config;
  return kEtcPalErrOk;
}

static const rdmnet_scope_monitor_t last_monitor_handle = reinterpret_cast<rdmnet_scope_monitor_t>(0xdead);
static RdmnetScopeMonitorConfig last_monitor_config;

static etcpal_error_t start_monitoring_and_save_config(const RdmnetScopeMonitorConfig* config,
                                                       rdmnet_scope_monitor_t* handle, int* platform_specific_error)
{
  RDMNET_UNUSED_ARG(platform_specific_error);
  *handle = last_monitor_handle;
  last_monitor_config = *config;
  return kEtcPalErrOk;
}

static RdmnetClientConnectFailedInfo client_connect_failed_info;

// Just save the info pointed to by the struct
static void custom_connect_failed_cb(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                     const RdmnetClientConnectFailedInfo* info, void* context)
{
  RDMNET_UNUSED_ARG(handle);
  RDMNET_UNUSED_ARG(scope_handle);
  client_connect_failed_info = *info;
  RDMNET_UNUSED_ARG(context);
}

static RdmnetClientDisconnectedInfo client_disconn_info;

// Just save the info pointed to by the struct
static void custom_disconnected_cb(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                   const RdmnetClientDisconnectedInfo* info, void* context)
{
  RDMNET_UNUSED_ARG(handle);
  RDMNET_UNUSED_ARG(scope_handle);
  client_disconn_info = *info;
  RDMNET_UNUSED_ARG(context);
}

static EtcPalSockAddr last_connect_addr;

// Just save the info pointed to by the struct
static etcpal_error_t connect_and_save_address(rdmnet_conn_t handle, const EtcPalSockAddr* remote_addr,
                                               const ClientConnectMsg* connect_data)
{
  RDMNET_UNUSED_ARG(handle);
  last_connect_addr = *remote_addr;
  RDMNET_UNUSED_ARG(connect_data);
  return kEtcPalErrOk;
}
}

class TestRptClientBehavior : public testing::Test
{
protected:
  rdmnet_client_t client_handle_;
  rdmnet_client_scope_t scope_handle_;
  RptClientCallbacks rpt_callbacks_{};
  RdmnetRptClientConfig default_rpt_config_{};

  void SetUp() override
  {
    // Reset the fakes
    RDMNET_CLIENT_CALLBACKS_DO_FOR_ALL_FAKES(RESET_FAKE);
    rdmnet_mock_core_reset();
    rdmnet_connection_create_fake.custom_fake = create_conn_and_save_config;
    rdmnet_connect_fake.return_val = kEtcPalErrOk;
    rdmnet_disc_start_monitoring_fake.return_val = kEtcPalErrOk;

    // Init
    ASSERT_EQ(kEtcPalErrOk, rdmnet_client_init(NULL));
    ASSERT_EQ(rdmnet_core_init_fake.call_count, 1u);

    rpt_callbacks_.connected = rdmnet_client_connected;
    rpt_callbacks_.connect_failed = rdmnet_client_connect_failed;
    rpt_callbacks_.disconnected = rdmnet_client_disconnected;
    rpt_callbacks_.broker_msg_received = rdmnet_client_broker_msg_received;
    rpt_callbacks_.msg_received = rpt_client_msg_received;

    RPT_CLIENT_CONFIG_INIT(&default_rpt_config_, 0x6574);
    default_rpt_config_.type = kRPTClientTypeController;
    default_rpt_config_.cid = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
    default_rpt_config_.callbacks = rpt_callbacks_;
    default_rpt_config_.callback_context = this;

    // Create client
    ASSERT_EQ(kEtcPalErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &client_handle_));
  }

  void TearDown() override
  {
    ASSERT_EQ(kEtcPalErrOk, rdmnet_client_destroy(client_handle_, kRdmnetDisconnectShutdown));
    rdmnet_client_deinit();
  }
};

class TestDynamicRptClientBehavior : public TestRptClientBehavior
{
protected:
  RdmnetScopeConfig default_dynamic_scope_{};

  std::vector<BrokerListenAddr> listen_addrs_;
  RdmnetBrokerDiscInfo discovered_broker_;

  void SetUp() override
  {
    TestRptClientBehavior::SetUp();

    rdmnet_safe_strncpy(default_dynamic_scope_.scope, "default", E133_SCOPE_STRING_PADDED_LENGTH);
    default_dynamic_scope_.has_static_broker_addr = false;

    // Construct our listen addresses
    BrokerListenAddr listen_addr{};
    ETCPAL_IP_SET_V4_ADDRESS(&listen_addr.addr, 0x0a650101);
    listen_addrs_.push_back(listen_addr);
    ETCPAL_IP_SET_V4_ADDRESS(&listen_addr.addr, 0xc0a80101);
    listen_addrs_.push_back(listen_addr);
    const std::array<uint8_t, 16> v6_addr{0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xbb};
    ETCPAL_IP_SET_V6_ADDRESS(&listen_addr.addr, v6_addr.data());
    listen_addrs_.push_back(listen_addr);

    for (size_t i = 0; i < listen_addrs_.size() - 1; ++i)
    {
      listen_addrs_.at(i).next = &listen_addrs_.at(i + 1);
    }

    discovered_broker_.port = 8888;
    rdmnet_safe_strncpy(discovered_broker_.scope, default_dynamic_scope_.scope, E133_SCOPE_STRING_PADDED_LENGTH);
    discovered_broker_.listen_addr_list = listen_addrs_.data();
  }

  void ConnectAndVerify()
  {
    rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;

    ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle_, &default_dynamic_scope_, &scope_handle_));

    EXPECT_EQ(rdmnet_connection_create_fake.call_count, 1u);
    EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

    rdmnet_connect_fake.return_val = kEtcPalErrOk;
    last_monitor_config.callbacks.broker_found(last_monitor_handle, &discovered_broker_,
                                               last_monitor_config.callback_context);
    EXPECT_EQ(rdmnet_connect_fake.call_count, 1u);

    RdmnetConnectedInfo connected_info{};
    connected_info.broker_uid = {20, 40};
    connected_info.client_uid = {1, 2};
    connected_info.connected_addr = {8888, listen_addrs_[0].addr};
    last_conn_config.callbacks.connected(last_conn_handle, &connected_info, last_conn_config.callback_context);

    EXPECT_EQ(rdmnet_client_connected_fake.call_count, 1u);
  }
};

class TestStaticRptClientBehavior : public TestRptClientBehavior
{
protected:
  RdmnetScopeConfig default_static_scope_{};

  void SetUp() override
  {
    TestRptClientBehavior::SetUp();

    rdmnet_safe_strncpy(default_static_scope_.scope, "not_default", E133_SCOPE_STRING_PADDED_LENGTH);
    default_static_scope_.has_static_broker_addr = true;
    ETCPAL_IP_SET_V4_ADDRESS(&default_static_scope_.static_broker_addr.ip, 0x0a650101);
    default_static_scope_.static_broker_addr.port = 8888;
  }

  void ConnectAndVerify()
  {
    ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle_, &default_static_scope_, &scope_handle_));

    EXPECT_EQ(rdmnet_connection_create_fake.call_count, 1u);
    EXPECT_EQ(rdmnet_connect_fake.call_count, 1u);

    RdmnetConnectedInfo connected_info{};
    connected_info.broker_uid = {20, 40};
    connected_info.client_uid = {1, 2};
    connected_info.connected_addr = default_static_scope_.static_broker_addr;
    last_conn_config.callbacks.connected(last_conn_handle, &connected_info, last_conn_config.callback_context);

    EXPECT_EQ(rdmnet_client_connected_fake.call_count, 1u);
  }
};

// Test that the rdmnet_client_add_scope() function has the correct side-effects with respect to
// discovery and connections
TEST_F(TestDynamicRptClientBehavior, AddScopeHasCorrectSideEffects)
{
  // Add a scope with default settings
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle_, &default_dynamic_scope_, &scope_handle));

  // Make sure the correct underlying functions were called
  ASSERT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);
  ASSERT_EQ(rdmnet_connect_fake.call_count, 0u);
}

TEST_F(TestStaticRptClientBehavior, AddScopeHasCorrectSideEffects)
{
  // Add a scope with a static broker address
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle_, &default_static_scope_, &scope_handle));
  ASSERT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 0u);
  ASSERT_EQ(rdmnet_connect_fake.call_count, 1u);
}

TEST_F(TestDynamicRptClientBehavior, DiscoveryErrorsHandled)
{
  rdmnet_disc_start_monitoring_fake.return_val = kEtcPalErrSys;

  rdmnet_client_scope_t scope_handle;
  EXPECT_EQ(kEtcPalErrSys, rdmnet_client_add_scope(client_handle_, &default_dynamic_scope_, &scope_handle));
}

TEST_F(TestDynamicRptClientBehavior, ConnectionErrorsHandled)
{
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;

  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle_, &default_dynamic_scope_, &scope_handle));
  ASSERT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

  rdmnet_connect_fake.return_val = kEtcPalErrSys;
  last_monitor_config.callbacks.broker_found(last_monitor_handle, &discovered_broker_,
                                             last_monitor_config.callback_context);
  // Make sure it tries all possible listen addresses before giving up
  EXPECT_EQ(rdmnet_connect_fake.call_count, listen_addrs_.size());
}

TEST_F(TestDynamicRptClientBehavior, ReconnectionErrorsHandled)
{
  ConnectAndVerify();

  RESET_FAKE(rdmnet_connect);
  rdmnet_connect_fake.return_val = kEtcPalErrSys;
  rdmnet_client_disconnected_fake.custom_fake = custom_disconnected_cb;

  RdmnetDisconnectedInfo disconn_info{};
  disconn_info.event = kRdmnetDisconnectAbruptClose;
  disconn_info.socket_err = kEtcPalErrConnReset;
  last_conn_config.callbacks.disconnected(last_conn_handle, &disconn_info, last_conn_config.callback_context);

  // Make sure it tries all possible listen addresses, then reports an error.
  EXPECT_EQ(rdmnet_connect_fake.call_count, listen_addrs_.size());
  EXPECT_EQ(rdmnet_client_disconnected_fake.call_count, 1u);
  EXPECT_EQ(client_disconn_info.event, kRdmnetDisconnectAbruptClose);
  EXPECT_EQ(client_disconn_info.will_retry, true);
}

TEST_F(TestDynamicRptClientBehavior, ClientRetriesOnConnectFail)
{
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;
  rdmnet_connect_fake.custom_fake = connect_and_save_address;
  rdmnet_client_connect_failed_fake.custom_fake = custom_connect_failed_cb;

  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle_, &default_dynamic_scope_, &scope_handle_));

  EXPECT_EQ(rdmnet_connection_create_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

  rdmnet_connect_fake.return_val = kEtcPalErrOk;
  last_monitor_config.callbacks.broker_found(last_monitor_handle, &discovered_broker_,
                                             last_monitor_config.callback_context);
  EXPECT_EQ(rdmnet_connect_fake.call_count, 1u);
  EXPECT_EQ(last_connect_addr.ip, listen_addrs_.at(0).addr);
  EXPECT_EQ(last_connect_addr.port, discovered_broker_.port);

  RESET_FAKE(rdmnet_connect);
  rdmnet_connect_fake.custom_fake = connect_and_save_address;

  RdmnetConnectFailedInfo failed_info{};
  failed_info.event = kRdmnetConnectFailTcpLevel;
  failed_info.socket_err = kEtcPalErrTimedOut;
  last_conn_config.callbacks.connect_failed(last_conn_handle, &failed_info, last_conn_config.callback_context);

  EXPECT_EQ(rdmnet_client_connect_failed_fake.call_count, 1u);
  EXPECT_TRUE(client_connect_failed_info.will_retry);
  EXPECT_EQ(rdmnet_connect_fake.call_count, 1u);
  // The retry should use the next Broker listen address in the list.
  EXPECT_EQ(last_connect_addr.ip, listen_addrs_.at(1).addr);
  EXPECT_EQ(last_connect_addr.port, discovered_broker_.port);
}

TEST_F(TestStaticRptClientBehavior, ConnectionErrorsHandled)
{
  rdmnet_connect_fake.return_val = kEtcPalErrSys;

  rdmnet_client_scope_t scope_handle;
  EXPECT_EQ(kEtcPalErrSys, rdmnet_client_add_scope(client_handle_, &default_static_scope_, &scope_handle));
}

TEST_F(TestStaticRptClientBehavior, SuccessfulConnectionReported)
{
  ConnectAndVerify();
}

TEST_F(TestStaticRptClientBehavior, ClientRetriesOnDisconnect)
{
  ConnectAndVerify();

  RESET_FAKE(rdmnet_connect);
  rdmnet_client_disconnected_fake.custom_fake = custom_disconnected_cb;

  // Simulate a disconnect for a reason that requires a retry
  RdmnetDisconnectedInfo disconn_info{};
  disconn_info.event = kRdmnetDisconnectGracefulRemoteInitiated;
  disconn_info.rdmnet_reason = kRdmnetDisconnectShutdown;
  disconn_info.socket_err = kEtcPalErrOk;
  last_conn_config.callbacks.disconnected(last_conn_handle, &disconn_info, last_conn_config.callback_context);

  EXPECT_EQ(rdmnet_client_disconnected_fake.call_count, 1u);
  EXPECT_TRUE(client_disconn_info.will_retry);
  EXPECT_GE(rdmnet_connect_fake.call_count, 1u);
}

TEST_F(TestStaticRptClientBehavior, ClientRetriesOnConnectFail)
{
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle_, &default_static_scope_, &scope_handle_));

  EXPECT_EQ(rdmnet_connection_create_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_connect_fake.call_count, 1u);

  RESET_FAKE(rdmnet_connect);
  rdmnet_client_connect_failed_fake.custom_fake = custom_connect_failed_cb;

  RdmnetConnectFailedInfo failed_info{};
  failed_info.event = kRdmnetConnectFailTcpLevel;
  failed_info.socket_err = kEtcPalErrTimedOut;
  last_conn_config.callbacks.connect_failed(last_conn_handle, &failed_info, last_conn_config.callback_context);

  EXPECT_EQ(rdmnet_client_connect_failed_fake.call_count, 1u);
  EXPECT_TRUE(client_connect_failed_info.will_retry);
  EXPECT_GE(rdmnet_connect_fake.call_count, 1u);
}

TEST_F(TestStaticRptClientBehavior, ClientDoesNotRetryOnFatalConnectFail)
{
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle_, &default_static_scope_, &scope_handle_));

  EXPECT_EQ(rdmnet_connection_create_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_connect_fake.call_count, 1u);

  RESET_FAKE(rdmnet_connect);
  rdmnet_client_connect_failed_fake.custom_fake = custom_connect_failed_cb;

  RdmnetConnectFailedInfo failed_info{};
  failed_info.event = kRdmnetConnectFailRejected;
  failed_info.rdmnet_reason = kRdmnetConnectScopeMismatch;
  last_conn_config.callbacks.connect_failed(last_conn_handle, &failed_info, last_conn_config.callback_context);

  EXPECT_EQ(rdmnet_client_connect_failed_fake.call_count, 1u);
  EXPECT_FALSE(client_connect_failed_info.will_retry);
  EXPECT_EQ(rdmnet_connect_fake.call_count, 0u);
}
