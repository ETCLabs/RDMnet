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
#include <map>

#include "gtest/gtest.h"
#include "rdmnet/client.h"

#include "rdmnet_mock/core.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/discovery.h"

#include "rdmnet_client_fake_callbacks.h"

extern "C" {
rdmnet_conn_t next_handle;
rdmnet_conn_t last_handle;
RdmnetConnectionConfig last_conn_config;

etcpal_error_t create_conn_and_save_config(const RdmnetConnectionConfig* config, rdmnet_conn_t* handle)
{
  *handle = next_handle++;
  last_handle = *handle;
  last_conn_config = *config;
  return kEtcPalErrOk;
}
}

class TestClientBehavior : public testing::Test
{
protected:
  RptClientCallbacks rpt_callbacks_{};
  RdmnetRptClientConfig default_rpt_config_{};
  RdmnetScopeConfig default_dynamic_scope_{};
  RdmnetScopeConfig default_static_scope_{};

  TestClientBehavior()
  {
    rpt_callbacks_.connected = rdmnet_client_connected;
    rpt_callbacks_.disconnected = rdmnet_client_disconnected;
    rpt_callbacks_.broker_msg_received = rdmnet_client_broker_msg_received;
    rpt_callbacks_.msg_received = rpt_client_msg_received;

    RPT_CLIENT_CONFIG_INIT(&default_rpt_config_, 0x6574);
    default_rpt_config_.type = kRPTClientTypeController;
    default_rpt_config_.cid = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
    default_rpt_config_.callbacks = rpt_callbacks_;
    default_rpt_config_.callback_context = this;

    rdmnet_safe_strncpy(default_dynamic_scope_.scope, "default", E133_SCOPE_STRING_PADDED_LENGTH);
    default_dynamic_scope_.has_static_broker_addr = false;

    rdmnet_safe_strncpy(default_static_scope_.scope, "not_default", E133_SCOPE_STRING_PADDED_LENGTH);
    default_static_scope_.has_static_broker_addr = true;
    ETCPAL_IP_SET_V4_ADDRESS(&default_static_scope_.static_broker_addr.ip, 0x0a650101);
    default_static_scope_.static_broker_addr.port = 8888;
  }

  void SetUp() override
  {
    // Reset the fakes
    RDMNET_CLIENT_CALLBACKS_DO_FOR_ALL_FAKES(RESET_FAKE);
    rdmnet_mock_core_reset();

    // Init
    ASSERT_EQ(kEtcPalErrOk, rdmnet_client_init(NULL));
    ASSERT_EQ(rdmnet_core_init_fake.call_count, 1u);
  }

  void TearDown() override { rdmnet_client_deinit(); }
};

// Test that the rdmnet_client_add_scope() function has the correct side-effects with respect to
// discovery and connections
TEST_F(TestClientBehavior, add_scope_has_correct_side_effects)
{
  // Create a new client
  rdmnet_client_t client_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &client_handle));

  // Add a scope with default settings
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle, &default_dynamic_scope_, &scope_handle));
  // Make sure the correct underlying functions were called
  ASSERT_EQ(rdmnetdisc_start_monitoring_fake.call_count, 1u);
  ASSERT_EQ(rdmnet_connect_fake.call_count, 0u);

  RESET_FAKE(rdmnetdisc_start_monitoring);
  RESET_FAKE(rdmnet_connect);

  // Create another client with one scope and a static broker address
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(client_handle, &default_static_scope_, &scope_handle));
  ASSERT_EQ(rdmnetdisc_start_monitoring_fake.call_count, 0u);
  ASSERT_EQ(rdmnet_connect_fake.call_count, 1u);
}

TEST_F(TestClientBehavior, successful_connection_reported)
{
  rdmnet_connection_create_fake.custom_fake = create_conn_and_save_config;

  rdmnet_client_t handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle));

  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(handle, &default_static_scope_, &scope_handle));

  EXPECT_EQ(rdmnet_connection_create_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_connect_fake.call_count, 1u);

  RdmnetConnectedInfo connected_info{};
  connected_info.broker_uid = {20, 40};
  connected_info.client_uid = {1, 2};
  connected_info.connected_addr = default_static_scope_.static_broker_addr;
  last_conn_config.callbacks.connected(last_handle, &connected_info, last_conn_config.callback_context);

  EXPECT_EQ(rdmnet_client_connected_fake.call_count, 1u);
}
