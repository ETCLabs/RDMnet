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

// Test how an RPT client handles connection, disconnection and reconnection to
// dynamically-discovered and statically-configured brokers.

#include "rdmnet/core/client.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "etcpal/common.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/mutex.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "rdmnet_mock/core/broker_prot.h"
#include "rdmnet_mock/core/common.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/rpt_prot.h"
#include "rdmnet_mock/discovery.h"
#include "rdmnet_client_fake_callbacks.h"
#include "gtest/gtest.h"

struct SavedScopeMonitorConfig
{
  std::string                 scope;
  std::string                 domain;
  RdmnetScopeMonitorCallbacks callbacks;

  SavedScopeMonitorConfig() = default;
  SavedScopeMonitorConfig(const RdmnetScopeMonitorConfig& monitor)
      : scope(monitor.scope), domain(monitor.domain), callbacks(monitor.callbacks)
  {
  }
};

static intptr_t                   last_monitor_handle = 0xdead;
static SavedScopeMonitorConfig    last_monitor_config;
static std::vector<RCConnection*> conns_registered;

rdmnet_scope_monitor_t GetLastMonitorHandle()
{
  return reinterpret_cast<rdmnet_scope_monitor_t>(last_monitor_handle);
}

extern "C" {
static etcpal_error_t start_monitoring_and_save_config(const RdmnetScopeMonitorConfig* config,
                                                       rdmnet_scope_monitor_t*         handle,
                                                       int*                            platform_specific_error)
{
  ETCPAL_UNUSED_ARG(platform_specific_error);
  *handle = reinterpret_cast<rdmnet_scope_monitor_t>(++last_monitor_handle);
  last_monitor_config = *config;
  return kEtcPalErrOk;
}

static etcpal_error_t register_and_save_conn(RCConnection* conn)
{
  conns_registered.push_back(conn);
  return kEtcPalErrOk;
}

static RdmnetClientConnectedInfo client_connected_info;

static void custom_connected_cb(RCClient*                        client,
                                rdmnet_client_scope_t            scope_handle,
                                const RdmnetClientConnectedInfo* info)
{
  ETCPAL_UNUSED_ARG(client);
  ETCPAL_UNUSED_ARG(scope_handle);
  client_connected_info = *info;
}

static RdmnetClientConnectFailedInfo client_connect_failed_info;

// Just save the info pointed to by the struct
static void custom_connect_failed_cb(RCClient*                            client,
                                     rdmnet_client_scope_t                scope_handle,
                                     const RdmnetClientConnectFailedInfo* info)
{
  ETCPAL_UNUSED_ARG(client);
  ETCPAL_UNUSED_ARG(scope_handle);
  client_connect_failed_info = *info;
}

static RdmnetClientDisconnectedInfo client_disconn_info;

// Just save the info pointed to by the struct
static void custom_disconnected_cb(RCClient*                           client,
                                   rdmnet_client_scope_t               scope_handle,
                                   const RdmnetClientDisconnectedInfo* info)
{
  ETCPAL_UNUSED_ARG(client);
  ETCPAL_UNUSED_ARG(scope_handle);
  client_disconn_info = *info;
}

static EtcPalSockAddr last_connect_addr;

// Just save the info pointed to by the struct
static etcpal_error_t connect_and_save_address(RCConnection*                 conn,
                                               const EtcPalSockAddr*         remote_addr,
                                               const BrokerClientConnectMsg* connect_data)
{
  ETCPAL_UNUSED_ARG(conn);
  last_connect_addr = *remote_addr;
  ETCPAL_UNUSED_ARG(connect_data);
  return kEtcPalErrOk;
}
}

class TestRptClientConnectionHandling : public testing::Test
{
protected:
  RCClient                client_{};
  etcpal::Mutex           client_lock_;
  RCClientCommonCallbacks common_callbacks_{};
  RCRptClientCallbacks    rpt_callbacks_{};

  RdmnetScopeConfig     default_dynamic_scope_{};
  rdmnet_client_scope_t dynamic_scope_handle_{RDMNET_CLIENT_SCOPE_INVALID};
  RdmnetScopeConfig     default_static_scope_{};
  rdmnet_client_scope_t static_scope_handle_{RDMNET_CLIENT_SCOPE_INVALID};

  std::vector<EtcPalIpAddr> listen_addrs_;
  RdmnetBrokerDiscInfo      discovered_broker_;

  void SetUp() override
  {
    // Reset the fakes
    rc_client_callbacks_reset_all_fakes();
    rdmnet_mock_core_reset_and_init();
    rc_broker_prot_reset_all_fakes();
    rc_rpt_prot_reset_all_fakes();
    rc_connection_reset_all_fakes();
    rdmnet_disc_reset_all_fakes();
    etcpal_reset_all_fakes();

    conns_registered.clear();
    rc_conn_register_fake.custom_fake = register_and_save_conn;

    RDMNET_CLIENT_SET_DEFAULT_SCOPE(&default_dynamic_scope_);
    auto static_broker = etcpal::SockAddr(etcpal::IpAddr::FromString("10.101.1.1"), 8888);
    RDMNET_CLIENT_SET_STATIC_SCOPE(&default_static_scope_, "not default", static_broker.get());

    // Construct our listen addresses
    listen_addrs_.push_back(etcpal::IpAddr::FromString("10.101.1.1").get());
    listen_addrs_.push_back(etcpal::IpAddr::FromString("192.168.1.1").get());
    listen_addrs_.push_back(etcpal::IpAddr::FromString("2001:db8::aabb").get());

    discovered_broker_.port = 8888;
    discovered_broker_.scope = default_dynamic_scope_.scope;
    discovered_broker_.listen_addrs = listen_addrs_.data();
    discovered_broker_.num_listen_addrs = listen_addrs_.size();
    discovered_broker_.service_instance_name = "Test Service Instance Name";

    client_.lock = &client_lock_.get();
    client_.type = kClientProtocolRPT;
    client_.cid = etcpal::Uuid::FromString("01b638ac-be34-40a7-988c-cc62d2fbb3b0").get();
    client_.callbacks = kClientFakeCommonCallbacks;
    RC_RPT_CLIENT_DATA(&client_)->type = kRPTClientTypeController;
    RDMNET_INIT_DYNAMIC_UID_REQUEST(&(RC_RPT_CLIENT_DATA(&client_)->uid), 0x6574);
    RC_RPT_CLIENT_DATA(&client_)->callbacks = kClientFakeRptCallbacks;

    // Create client
    ASSERT_EQ(kEtcPalErrOk, rc_client_module_init(nullptr));
    ASSERT_EQ(kEtcPalErrOk, rc_rpt_client_register(&client_, false));
  }

  void TearDown() override
  {
    rc_client_unregister(&client_, kRdmnetDisconnectShutdown);
    for (auto conn : conns_registered)
      conn->callbacks.destroyed(conn);

    rc_client_module_deinit();
  }

  void ConnectAndVerifyDynamic()
  {
    RESET_FAKE(rdmnet_disc_start_monitoring);
    RESET_FAKE(rc_conn_register);
    RESET_FAKE(rc_conn_connect);
    RESET_FAKE(rc_client_connected);
    rc_conn_register_fake.custom_fake = register_and_save_conn;
    rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;
    size_t next_conn_index = conns_registered.size();

    ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_dynamic_scope_, &dynamic_scope_handle_));

    EXPECT_EQ(rc_conn_register_fake.call_count, 1u);
    EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

    rc_conn_connect_fake.return_val = kEtcPalErrOk;
    last_monitor_config.callbacks.broker_found(GetLastMonitorHandle(), &discovered_broker_,
                                               last_monitor_config.callbacks.context);
    EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);

    rc_client_connected_fake.custom_fake = custom_connected_cb;

    RCConnectedInfo connected_info{};
    connected_info.broker_cid = etcpal::Uuid::FromString("500a4ae0-527d-45db-a37c-7fecd0c01f81").get();
    connected_info.broker_uid = {20, 40};
    connected_info.client_uid = {1, 2};
    connected_info.connected_addr = {8888, listen_addrs_[0]};
    conns_registered[next_conn_index]->callbacks.connected(conns_registered[next_conn_index], &connected_info);

    EXPECT_EQ(rc_client_connected_fake.call_count, 1u);
    EXPECT_EQ(client_connected_info.broker_addr, connected_info.connected_addr);
    EXPECT_STREQ(client_connected_info.broker_name, discovered_broker_.service_instance_name);
    EXPECT_EQ(client_connected_info.broker_cid, connected_info.broker_cid);
    EXPECT_EQ(client_connected_info.broker_uid, connected_info.broker_uid);
  }

  void ConnectAndVerifyStatic()
  {
    RESET_FAKE(rc_conn_register);
    RESET_FAKE(rc_conn_connect);
    RESET_FAKE(rc_client_connected);
    rc_conn_register_fake.custom_fake = register_and_save_conn;
    size_t next_conn_index = conns_registered.size();

    ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_static_scope_, &static_scope_handle_));

    EXPECT_EQ(rc_conn_register_fake.call_count, 1u);
    EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);

    rc_client_connected_fake.custom_fake = custom_connected_cb;

    RCConnectedInfo connected_info{};
    connected_info.broker_cid = etcpal::Uuid::FromString("500a4ae0-527d-45db-a37c-7fecd0c01f81").get();
    connected_info.broker_uid = {20, 40};
    connected_info.client_uid = {1, 2};
    connected_info.connected_addr = default_static_scope_.static_broker_addr;
    conns_registered[next_conn_index]->callbacks.connected(conns_registered[next_conn_index], &connected_info);

    EXPECT_EQ(rc_client_connected_fake.call_count, 1u);
    EXPECT_EQ(client_connected_info.broker_addr, connected_info.connected_addr);
    EXPECT_EQ(client_connected_info.broker_cid, connected_info.broker_cid);
    EXPECT_EQ(client_connected_info.broker_uid, connected_info.broker_uid);
  }
};

class TestDynamicRptClientConnectionHandling : public TestRptClientConnectionHandling
{
protected:
  void ConnectAndVerify() { ConnectAndVerifyDynamic(); }
};

// Test that the rdmnet_client_add_scope() function has the correct side-effects with respect to
// discovery and connections
TEST_F(TestDynamicRptClientConnectionHandling, AddScopeHasCorrectSideEffects)
{
  // Add a scope with default settings
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_dynamic_scope_, &scope_handle));

  // Make sure the correct underlying functions were called
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 0u);
}

TEST_F(TestDynamicRptClientConnectionHandling, HandlesDiscoveryErrors)
{
  rdmnet_disc_start_monitoring_fake.return_val = kEtcPalErrSys;

  rdmnet_client_scope_t scope_handle;
  EXPECT_EQ(kEtcPalErrSys, rc_client_add_scope(&client_, &default_dynamic_scope_, &scope_handle));
}

TEST_F(TestDynamicRptClientConnectionHandling, HandlesConnectionErrors)
{
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;

  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_dynamic_scope_, &scope_handle));
  ASSERT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

  rc_conn_connect_fake.return_val = kEtcPalErrSys;
  last_monitor_config.callbacks.broker_found(GetLastMonitorHandle(), &discovered_broker_,
                                             last_monitor_config.callbacks.context);
  // Make sure it tries all possible listen addresses before giving up
  EXPECT_EQ(rc_conn_connect_fake.call_count, listen_addrs_.size());
}

TEST_F(TestDynamicRptClientConnectionHandling, HandlesBrokerUpdated)
{
  ConnectAndVerify();

  RESET_FAKE(rc_conn_connect);
  rc_conn_connect_fake.custom_fake = connect_and_save_address;

  auto new_addr = etcpal::IpAddr::FromString("10.101.50.60").get();
  listen_addrs_.clear();
  listen_addrs_.push_back(new_addr);
  discovered_broker_.listen_addrs = listen_addrs_.data();
  discovered_broker_.num_listen_addrs = listen_addrs_.size();
  last_monitor_config.callbacks.broker_updated(GetLastMonitorHandle(), &discovered_broker_,
                                               last_monitor_config.callbacks.context);

  RCDisconnectedInfo disconn_info{};
  disconn_info.event = kRdmnetDisconnectAbruptClose;
  disconn_info.socket_err = kEtcPalErrConnReset;
  conns_registered[0]->callbacks.disconnected(conns_registered[0], &disconn_info);

  EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);
  // The retry should use the new Broker listen address.
  EXPECT_EQ(last_connect_addr.ip, new_addr);
  EXPECT_EQ(last_connect_addr.port, discovered_broker_.port);
}

TEST_F(TestDynamicRptClientConnectionHandling, HandlesReconnectionErrors)
{
  ConnectAndVerify();

  RESET_FAKE(rc_conn_connect);
  rc_conn_connect_fake.return_val = kEtcPalErrSys;
  rc_client_disconnected_fake.custom_fake = custom_disconnected_cb;

  RCDisconnectedInfo disconn_info{};
  disconn_info.event = kRdmnetDisconnectAbruptClose;
  disconn_info.socket_err = kEtcPalErrConnReset;
  conns_registered[0]->callbacks.disconnected(conns_registered[0], &disconn_info);

  // Make sure it tries all possible listen addresses, then reports an error.
  EXPECT_EQ(rc_conn_connect_fake.call_count, listen_addrs_.size());
  EXPECT_EQ(rc_client_disconnected_fake.call_count, 1u);
  EXPECT_EQ(client_disconn_info.event, kRdmnetDisconnectAbruptClose);
  EXPECT_EQ(client_disconn_info.will_retry, true);
}

TEST_F(TestDynamicRptClientConnectionHandling, ClientRetriesOnConnectFail)
{
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;
  rc_conn_register_fake.custom_fake = register_and_save_conn;
  rc_conn_connect_fake.custom_fake = connect_and_save_address;
  rc_client_connect_failed_fake.custom_fake = custom_connect_failed_cb;

  ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_dynamic_scope_, &dynamic_scope_handle_));

  EXPECT_EQ(rc_conn_register_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

  rc_conn_connect_fake.return_val = kEtcPalErrOk;
  last_monitor_config.callbacks.broker_found(GetLastMonitorHandle(), &discovered_broker_,
                                             last_monitor_config.callbacks.context);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);
  EXPECT_EQ(last_connect_addr.ip, listen_addrs_.at(0));
  EXPECT_EQ(last_connect_addr.port, discovered_broker_.port);

  RESET_FAKE(rc_conn_connect);
  rc_conn_connect_fake.custom_fake = connect_and_save_address;

  RCConnectFailedInfo failed_info{};
  failed_info.event = kRdmnetConnectFailTcpLevel;
  failed_info.socket_err = kEtcPalErrTimedOut;
  conns_registered[0]->callbacks.connect_failed(conns_registered[0], &failed_info);

  EXPECT_EQ(rc_client_connect_failed_fake.call_count, 1u);
  EXPECT_TRUE(client_connect_failed_info.will_retry);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);
  // The retry should use the next Broker listen address in the list.
  EXPECT_EQ(last_connect_addr.ip, listen_addrs_.at(1));
  EXPECT_EQ(last_connect_addr.port, discovered_broker_.port);
}

TEST_F(TestDynamicRptClientConnectionHandling, ScopeStillExistsOnFatalConnectFail)
{
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;
  rc_conn_register_fake.custom_fake = register_and_save_conn;
  rc_conn_connect_fake.custom_fake = connect_and_save_address;
  rc_client_connect_failed_fake.custom_fake = custom_connect_failed_cb;

  ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_dynamic_scope_, &dynamic_scope_handle_));

  EXPECT_EQ(rc_conn_register_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

  rc_conn_connect_fake.return_val = kEtcPalErrOk;
  last_monitor_config.callbacks.broker_found(GetLastMonitorHandle(), &discovered_broker_,
                                             last_monitor_config.callbacks.context);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);
  EXPECT_EQ(last_connect_addr.ip, listen_addrs_.at(0));
  EXPECT_EQ(last_connect_addr.port, discovered_broker_.port);

  RESET_FAKE(rc_conn_connect);
  rc_conn_connect_fake.custom_fake = connect_and_save_address;

  RCConnectFailedInfo failed_info{};
  failed_info.event = kRdmnetConnectFailRejected;
  failed_info.rdmnet_reason = kRdmnetConnectInvalidUid;
  conns_registered[0]->callbacks.connect_failed(conns_registered[0], &failed_info);

  EXPECT_EQ(rc_client_connect_failed_fake.call_count, 1u);
  EXPECT_FALSE(client_connect_failed_info.will_retry);
  EXPECT_EQ(client_connect_failed_info.event, kRdmnetConnectFailRejected);
  EXPECT_EQ(client_connect_failed_info.rdmnet_reason, kRdmnetConnectInvalidUid);

  EXPECT_EQ(rc_conn_connect_fake.call_count, 0u);
  EXPECT_EQ(rc_conn_unregister_fake.call_count, 0u);

  char           scope_buf[E133_SCOPE_STRING_PADDED_LENGTH];
  EtcPalSockAddr static_broker_addr;
  EXPECT_EQ(rc_client_get_scope(&client_, dynamic_scope_handle_, scope_buf, &static_broker_addr), kEtcPalErrOk);
  EXPECT_STREQ(scope_buf, default_dynamic_scope_.scope);
  EXPECT_TRUE(ETCPAL_IP_IS_INVALID(&static_broker_addr.ip));

  default_dynamic_scope_.scope = "Changed Test Scope";
  EXPECT_EQ(rc_client_change_scope(&client_, dynamic_scope_handle_, &default_dynamic_scope_,
                                   kRdmnetDisconnectUserReconfigure),
            kEtcPalErrOk);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 2u);
}

TEST_F(TestDynamicRptClientConnectionHandling, ChangeScopeHasCorrectSideEffects)
{
  ConnectAndVerify();

  RESET_FAKE(rdmnet_disc_start_monitoring);
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;

  default_dynamic_scope_.scope = "Changed Test Scope";
  ASSERT_EQ(rc_client_change_scope(&client_, dynamic_scope_handle_, &default_dynamic_scope_,
                                   kRdmnetDisconnectUserReconfigure),
            kEtcPalErrOk);

  EXPECT_EQ(rc_conn_disconnect_fake.call_count, 1u);
  EXPECT_EQ(rc_conn_disconnect_fake.arg1_val, kRdmnetDisconnectUserReconfigure);

  EXPECT_EQ(rdmnet_disc_stop_monitoring_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);
  EXPECT_STREQ(last_monitor_config.scope.c_str(), default_dynamic_scope_.scope);

  char             new_scope[E133_SCOPE_STRING_PADDED_LENGTH];
  etcpal::SockAddr new_static_broker_addr;
  ASSERT_EQ(rc_client_get_scope(&client_, dynamic_scope_handle_, new_scope, &new_static_broker_addr.get()),
            kEtcPalErrOk);
  EXPECT_STREQ(new_scope, "Changed Test Scope");
  EXPECT_FALSE(new_static_broker_addr.IsValid());
}

TEST_F(TestDynamicRptClientConnectionHandling, ReportsDisconnectAfterScopeChange)
{
  ConnectAndVerify();

  RESET_FAKE(rc_conn_connect);
  RESET_FAKE(rdmnet_disc_start_monitoring);
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;

  default_dynamic_scope_.scope = "Changed Test Scope";
  rc_client_change_scope(&client_, dynamic_scope_handle_, &default_dynamic_scope_, kRdmnetDisconnectUserReconfigure);

  EXPECT_EQ(rc_conn_disconnect_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_stop_monitoring_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

  rc_client_disconnected_fake.custom_fake = [](RCClient*, rdmnet_client_scope_t,
                                               const RdmnetClientDisconnectedInfo* rdmnet_disconn_info) {
    EXPECT_EQ(rdmnet_disconn_info->event, kRdmnetDisconnectGracefulLocalInitiated);
    EXPECT_EQ(rdmnet_disconn_info->will_retry, true);
  };

  RCDisconnectedInfo disconn_info{};
  disconn_info.event = kRdmnetDisconnectGracefulLocalInitiated;
  conns_registered[0]->callbacks.disconnected(conns_registered[0], &disconn_info);

  EXPECT_EQ(rc_client_disconnected_fake.call_count, 1u);

  // The client should not attempt reconnection
  EXPECT_EQ(rc_conn_reconnect_fake.call_count, 0u);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 0u);
}

TEST_F(TestDynamicRptClientConnectionHandling, ChangeScopeToStaticHasCorrectSideEffects)
{
  ConnectAndVerify();

  static const std::string      kNewScopeStr{"Changed Test Scope"};
  static const etcpal::SockAddr kNewStaticAddr{etcpal::IpAddr::FromString("10.101.1.1"), 8000};

  RESET_FAKE(rdmnet_disc_start_monitoring);

  default_dynamic_scope_.scope = kNewScopeStr.c_str();
  default_dynamic_scope_.static_broker_addr = kNewStaticAddr.get();
  rc_client_change_scope(&client_, dynamic_scope_handle_, &default_dynamic_scope_, kRdmnetDisconnectUserReconfigure);

  rc_conn_reconnect_fake.custom_fake = [](RCConnection*, const EtcPalSockAddr* broker_addr,
                                          const BrokerClientConnectMsg* connect_msg,
                                          rdmnet_disconnect_reason_t    disconnect_reason) {
    EXPECT_EQ(*broker_addr, kNewStaticAddr);
    EXPECT_STREQ(connect_msg->scope, kNewScopeStr.c_str());
    EXPECT_EQ(disconnect_reason, kRdmnetDisconnectUserReconfigure);
    return kEtcPalErrOk;
  };

  EXPECT_EQ(rc_conn_reconnect_fake.call_count, 1u);

  EXPECT_EQ(rdmnet_disc_stop_monitoring_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 0u);

  char             new_scope[E133_SCOPE_STRING_PADDED_LENGTH];
  etcpal::SockAddr new_static_broker_addr;
  ASSERT_EQ(rc_client_get_scope(&client_, dynamic_scope_handle_, new_scope, &new_static_broker_addr.get()),
            kEtcPalErrOk);
  EXPECT_EQ(new_scope, kNewScopeStr);
  EXPECT_EQ(new_static_broker_addr, kNewStaticAddr);
}

class TestStaticRptClientConnectionHandling : public TestRptClientConnectionHandling
{
protected:
  void ConnectAndVerify() { ConnectAndVerifyStatic(); }
};

TEST_F(TestStaticRptClientConnectionHandling, AddScopeHasCorrectSideEffects)
{
  // Add a scope with a static broker address
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_static_scope_, &scope_handle));
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 0u);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);
}

TEST_F(TestStaticRptClientConnectionHandling, HandlesConnectionErrors)
{
  rc_conn_connect_fake.return_val = kEtcPalErrSys;

  rdmnet_client_scope_t scope_handle;
  EXPECT_EQ(kEtcPalErrSys, rc_client_add_scope(&client_, &default_static_scope_, &scope_handle));
}

TEST_F(TestStaticRptClientConnectionHandling, ReportsSuccessfulConnection)
{
  ConnectAndVerify();
}

TEST_F(TestStaticRptClientConnectionHandling, ClientRetriesOnDisconnect)
{
  ConnectAndVerify();

  RESET_FAKE(rc_conn_connect);
  rc_client_disconnected_fake.custom_fake = custom_disconnected_cb;

  // Simulate a disconnect for a reason that requires a retry
  RCDisconnectedInfo disconn_info{};
  disconn_info.event = kRdmnetDisconnectGracefulRemoteInitiated;
  disconn_info.rdmnet_reason = kRdmnetDisconnectShutdown;
  disconn_info.socket_err = kEtcPalErrOk;
  conns_registered[0]->callbacks.disconnected(conns_registered[0], &disconn_info);

  EXPECT_EQ(rc_client_disconnected_fake.call_count, 1u);
  EXPECT_TRUE(client_disconn_info.will_retry);
  EXPECT_GE(rc_conn_connect_fake.call_count, 1u);
}

TEST_F(TestStaticRptClientConnectionHandling, ClientRetriesOnConnectFail)
{
  rc_conn_register_fake.custom_fake = register_and_save_conn;

  ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_static_scope_, &static_scope_handle_));

  EXPECT_EQ(rc_conn_register_fake.call_count, 1u);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);

  RESET_FAKE(rc_conn_connect);
  rc_client_connect_failed_fake.custom_fake = custom_connect_failed_cb;

  RCConnectFailedInfo failed_info{};
  failed_info.event = kRdmnetConnectFailTcpLevel;
  failed_info.socket_err = kEtcPalErrTimedOut;
  conns_registered[0]->callbacks.connect_failed(conns_registered[0], &failed_info);

  EXPECT_EQ(rc_client_connect_failed_fake.call_count, 1u);
  EXPECT_TRUE(client_connect_failed_info.will_retry);
  EXPECT_GE(rc_conn_connect_fake.call_count, 1u);
}

TEST_F(TestStaticRptClientConnectionHandling, ClientDoesNotRetryOnFatalConnectFail)
{
  rc_conn_register_fake.custom_fake = register_and_save_conn;

  ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_static_scope_, &static_scope_handle_));

  EXPECT_EQ(rc_conn_register_fake.call_count, 1u);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);

  RESET_FAKE(rc_conn_connect);
  rc_client_connect_failed_fake.custom_fake = custom_connect_failed_cb;

  RCConnectFailedInfo failed_info{};
  failed_info.event = kRdmnetConnectFailRejected;
  failed_info.rdmnet_reason = kRdmnetConnectScopeMismatch;
  conns_registered[0]->callbacks.connect_failed(conns_registered[0], &failed_info);

  EXPECT_EQ(rc_client_connect_failed_fake.call_count, 1u);
  EXPECT_FALSE(client_connect_failed_info.will_retry);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 0u);
}

class TestMultipleScopeRptClientConnectionHandling : public TestRptClientConnectionHandling
{
protected:
  void ConnectAndVerify()
  {
    ConnectAndVerifyStatic();
    ConnectAndVerifyDynamic();
  }
};

TEST_F(TestMultipleScopeRptClientConnectionHandling, ChangeDomainHasCorrectSideEffects)
{
  ConnectAndVerify();

  RESET_FAKE(rdmnet_disc_start_monitoring);
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;

  ASSERT_EQ(rc_client_change_search_domain(&client_, "new-domain.com", kRdmnetDisconnectUserReconfigure), kEtcPalErrOk);

  // We should only disconnect from the dynamic scope
  EXPECT_EQ(rc_conn_disconnect_fake.call_count, 1u);
  EXPECT_EQ(rc_conn_disconnect_fake.arg0_val, conns_registered[1]);
  EXPECT_EQ(rc_conn_disconnect_fake.arg1_val, kRdmnetDisconnectUserReconfigure);

  EXPECT_EQ(rdmnet_disc_stop_monitoring_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);
  EXPECT_EQ(last_monitor_config.domain, "new-domain.com");
}

TEST_F(TestMultipleScopeRptClientConnectionHandling, ReportsDisconnectAfterDomainChange)
{
  ConnectAndVerify();

  RESET_FAKE(rc_conn_connect);
  RESET_FAKE(rdmnet_disc_start_monitoring);
  rdmnet_disc_start_monitoring_fake.custom_fake = start_monitoring_and_save_config;

  ASSERT_EQ(rc_client_change_search_domain(&client_, "new-domain.com", kRdmnetDisconnectUserReconfigure), kEtcPalErrOk);

  EXPECT_EQ(rc_conn_disconnect_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_stop_monitoring_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_start_monitoring_fake.call_count, 1u);

  rc_client_disconnected_fake.custom_fake = [](RCClient*, rdmnet_client_scope_t,
                                               const RdmnetClientDisconnectedInfo* rdmnet_disconn_info) {
    EXPECT_EQ(rdmnet_disconn_info->event, kRdmnetDisconnectGracefulLocalInitiated);
    EXPECT_EQ(rdmnet_disconn_info->will_retry, true);
  };

  RCDisconnectedInfo disconn_info{};
  disconn_info.event = kRdmnetDisconnectGracefulLocalInitiated;
  conns_registered[1]->callbacks.disconnected(conns_registered[1], &disconn_info);

  EXPECT_EQ(rc_client_disconnected_fake.call_count, 1u);

  // The client should not attempt reconnection
  EXPECT_EQ(rc_conn_reconnect_fake.call_count, 0u);
  EXPECT_EQ(rc_conn_connect_fake.call_count, 0u);
}
