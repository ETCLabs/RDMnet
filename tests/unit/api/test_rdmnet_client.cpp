/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "fff.h"

#include "rdmnet/client.h"

#include "rdmnet_mock/core.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/discovery.h"
#include "rdmnet_mock/private/core.h"
#include "rdmnet_mock/core/llrp_target.h"
#include "rdmnet/core/util.h"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(rdmnet_client_connected, rdmnet_client_t, rdmnet_client_scope_t, const RdmnetClientConnectedInfo *,
               void *);
FAKE_VOID_FUNC(rdmnet_client_connect_failed, rdmnet_client_t, rdmnet_client_scope_t,
               const RdmnetClientConnectFailedInfo *, void *);
FAKE_VOID_FUNC(rdmnet_client_disconnected, rdmnet_client_t, rdmnet_client_scope_t, const RdmnetClientDisconnectedInfo *,
               void *);
FAKE_VOID_FUNC(rdmnet_client_broker_msg_received, rdmnet_client_t, rdmnet_client_scope_t, const BrokerMessage *,
               void *);
FAKE_VOID_FUNC(rpt_client_msg_received, rdmnet_client_t, rdmnet_client_scope_t, const RptClientMessage *, void *);
FAKE_VOID_FUNC(ept_client_msg_received, rdmnet_client_t, rdmnet_client_scope_t, const EptClientMessage *, void *);

rdmnet_conn_t g_next_conn_handle;

extern "C" lwpa_error_t custom_connection_create(const RdmnetConnectionConfig *config, rdmnet_conn_t *handle)
{
  (void)config;
  *handle = g_next_conn_handle++;
  return kLwpaErrOk;
}

class TestRdmnetClient : public testing::Test
{
protected:
  TestRdmnetClient()
  {
    g_next_conn_handle = 0;

    rdmnet_safe_strncpy(default_dynamic_scope_.scope, "default", E133_SCOPE_STRING_PADDED_LENGTH);
    default_dynamic_scope_.has_static_broker_addr = false;

    rdmnet_safe_strncpy(default_static_scope_.scope, "not_default", E133_SCOPE_STRING_PADDED_LENGTH);
    default_static_scope_.has_static_broker_addr = true;
    LWPA_IP_SET_V4_ADDRESS(&default_static_scope_.static_broker_addr.ip, 0x0a650101);
    default_static_scope_.static_broker_addr.port = 8888;

    rpt_callbacks_.connected = rdmnet_client_connected;
    rpt_callbacks_.disconnected = rdmnet_client_disconnected;
    rpt_callbacks_.broker_msg_received = rdmnet_client_broker_msg_received;
    rpt_callbacks_.msg_received = rpt_client_msg_received;

    RPT_CLIENT_CONFIG_INIT(&default_rpt_config_, 0x6574);
    default_rpt_config_.type = kRPTClientTypeController;
    default_rpt_config_.cid = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
    default_rpt_config_.callbacks = rpt_callbacks_;
    default_rpt_config_.callback_context = this;
  }

  void SetUp() override
  {
    // Reset the fakes
    RESET_FAKE(rdmnet_client_connected);
    RESET_FAKE(rdmnet_client_disconnected);

    rdmnet_mock_core_reset_and_init();

    // Init
    rdmnet_core_initialized_fake.return_val = false;
    rdmnet_core_init_fake.return_val = kLwpaErrOk;
    rdmnet_llrp_target_create_fake.return_val = kLwpaErrOk;
    ASSERT_EQ(kLwpaErrOk, rdmnet_client_init(NULL));
    ASSERT_EQ(rdmnet_core_init_fake.call_count, 1u);
    rdmnet_core_initialized_fake.return_val = true;

    // New connection
    rdmnet_connection_create_fake.custom_fake = custom_connection_create;
  }

  void TearDown() override { rdmnet_client_deinit(); }

  RdmnetScopeConfig default_dynamic_scope_{};
  RdmnetScopeConfig default_static_scope_{};
  RptClientCallbacks rpt_callbacks_{};
  RdmnetRptClientConfig default_rpt_config_{};
};

// Test the rdmnet_rpt_client_create() function in valid and invalid scenarios
TEST_F(TestRdmnetClient, create)
{
  rdmnet_client_t handle_1;

  // Invalid arguments
  EXPECT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_create(NULL, NULL));
  EXPECT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_create(&default_rpt_config_, NULL));
  EXPECT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_create(NULL, &handle_1));

  // Valid config, but core is not initialized
  rdmnet_core_initialized_fake.return_val = false;
  EXPECT_EQ(kLwpaErrNotInit, rdmnet_rpt_client_create(&default_rpt_config_, &handle_1));

  // Valid create with one scope
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(kLwpaErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle_1));

  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kLwpaErrOk, rdmnet_client_add_scope(handle_1, &default_dynamic_scope_, &scope_handle));

  // Valid create with 100 different scopes
  rdmnet_client_t handle_2;
  ASSERT_EQ(kLwpaErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle_2));

  for (size_t i = 0; i < 100; ++i)
  {
    RdmnetScopeConfig tmp_scope = default_dynamic_scope_;
    rdmnet_client_scope_t tmp_handle;
    RDMNET_MSVC_NO_DEP_WRN strcat(tmp_scope.scope, std::to_string(i).c_str());
    ASSERT_EQ(kLwpaErrOk, rdmnet_client_add_scope(handle_2, &tmp_scope, &tmp_handle));
  }
}

// Test that the rdmnet_rpt_client_create() function has the correct side-effects
TEST_F(TestRdmnetClient, add_scope_side_effects)
{
  // Create a new client
  rdmnet_client_t client_handle;
  ASSERT_EQ(kLwpaErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &client_handle));

  // Add a scope with default settings
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kLwpaErrOk, rdmnet_client_add_scope(client_handle, &default_dynamic_scope_, &scope_handle));
  // Make sure the correct underlying functions were called
  ASSERT_EQ(rdmnetdisc_start_monitoring_fake.call_count, 1u);
  ASSERT_EQ(rdmnet_connect_fake.call_count, 0u);

  RESET_FAKE(rdmnetdisc_start_monitoring);
  RESET_FAKE(rdmnet_connect);

  // Create another client with one scope and a static broker address
  ASSERT_EQ(kLwpaErrOk, rdmnet_client_add_scope(client_handle, &default_static_scope_, &scope_handle));
  ASSERT_EQ(rdmnetdisc_start_monitoring_fake.call_count, 0u);
  ASSERT_EQ(rdmnet_connect_fake.call_count, 1u);
}

TEST_F(TestRdmnetClient, send_rdm_command)
{
  rdmnet_client_t handle;
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kLwpaErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle));
  ASSERT_EQ(kLwpaErrOk, rdmnet_client_add_scope(handle, &default_dynamic_scope_, &scope_handle));

  // TODO valid initialization
  LocalRdmCommand cmd;
  cmd.dest_endpoint = 0;
  cmd.dest_uid = {};
  uint32_t seq_num = 0;

  // Core not initialized
  rdmnet_core_initialized_fake.return_val = false;
  EXPECT_EQ(kLwpaErrNotInit, rdmnet_rpt_client_send_rdm_command(handle, scope_handle, &cmd, &seq_num));

  // Invalid parameters
  rdmnet_core_initialized_fake.return_val = true;
  EXPECT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_rdm_command(RDMNET_CLIENT_INVALID, scope_handle, &cmd, &seq_num));
  EXPECT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_rdm_command(handle, RDMNET_CLIENT_SCOPE_INVALID, &cmd, &seq_num));
  EXPECT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_rdm_command(handle, scope_handle, NULL, &seq_num));

  // TODO finish test
}

TEST_F(TestRdmnetClient, send_rdm_response)
{
  rdmnet_client_t handle;
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kLwpaErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle));
  ASSERT_EQ(kLwpaErrOk, rdmnet_client_add_scope(handle, &default_dynamic_scope_, &scope_handle));

  // TODO valid initialization
  LocalRdmResponse resp;

  // Core not initialized
  rdmnet_core_initialized_fake.return_val = false;
  ASSERT_EQ(kLwpaErrNotInit, rdmnet_rpt_client_send_rdm_response(handle, scope_handle, &resp));

  // Invalid parameters
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_rdm_response(RDMNET_CLIENT_INVALID, scope_handle, &resp));
  ASSERT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_rdm_response(handle, RDMNET_CLIENT_SCOPE_INVALID, &resp));
  ASSERT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_rdm_response(handle, scope_handle, NULL));

  // TODO finish test
}

TEST_F(TestRdmnetClient, send_status)
{
  rdmnet_client_t handle;
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kLwpaErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle));
  ASSERT_EQ(kLwpaErrOk, rdmnet_client_add_scope(handle, &default_dynamic_scope_, &scope_handle));

  // TODO valid initialization
  LocalRptStatus status;

  // Core not initialized
  rdmnet_core_initialized_fake.return_val = false;
  ASSERT_EQ(kLwpaErrNotInit, rdmnet_rpt_client_send_status(handle, scope_handle, &status));

  // Invalid parameters
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_status(RDMNET_CLIENT_INVALID, scope_handle, &status));
  ASSERT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_status(handle, RDMNET_CLIENT_SCOPE_INVALID, &status));
  ASSERT_EQ(kLwpaErrInvalid, rdmnet_rpt_client_send_status(handle, scope_handle, NULL));

  // TODO finish test
}
