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

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "rdmnet/client.h"

#include "rdmnet_mock/core.h"
#include "rdmnet_mock/private/core.h"

#include "rdmnet_client_fake_callbacks.h"

class TestRptClientApi : public testing::Test
{
protected:
  RdmnetScopeConfig default_dynamic_scope_{};
  RptClientCallbacks rpt_callbacks_{};
  RdmnetRptClientConfig default_rpt_config_{};

  TestRptClientApi()
  {
    RDMNET_CLIENT_SET_DEFAULT_SCOPE(&default_dynamic_scope_);

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
    RDMNET_CLIENT_CALLBACKS_DO_FOR_ALL_FAKES(RESET_FAKE);
    rdmnet_mock_core_reset();

    // Init
    ASSERT_EQ(kEtcPalErrOk, rdmnet_client_init(nullptr, nullptr));
    ASSERT_EQ(rdmnet_core_init_fake.call_count, 1u);
  }

  void TearDown() override { rdmnet_client_deinit(); }
};

// Test the rdmnet_rpt_client_create() function in valid and invalid scenarios
TEST_F(TestRptClientApi, ClientCreateInvalidCallsFail)
{
  rdmnet_client_t handle;

  // Invalid arguments
  EXPECT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_create(NULL, NULL));
  EXPECT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_create(&default_rpt_config_, NULL));
  EXPECT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_create(NULL, &handle));

  // Valid config, but core is not initialized
  rdmnet_core_initialized_fake.return_val = false;
  EXPECT_EQ(kEtcPalErrNotInit, rdmnet_rpt_client_create(&default_rpt_config_, &handle));
}

TEST_F(TestRptClientApi, ClientCreateValidCallsSucceed)
{
  rdmnet_client_t handle_1;

  // Valid create with one scope
  ASSERT_EQ(kEtcPalErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle_1));

  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(handle_1, &default_dynamic_scope_, &scope_handle));

  // Valid create with 100 different scopes
  rdmnet_client_t handle_2;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle_2));

  for (size_t i = 0; i < 100; ++i)
  {
    std::string scope_str = E133_DEFAULT_SCOPE + std::to_string(i);
    RdmnetScopeConfig tmp_scope;
    RDMNET_CLIENT_SET_SCOPE(&tmp_scope, scope_str.c_str());
    rdmnet_client_scope_t tmp_handle;
    ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(handle_2, &tmp_scope, &tmp_handle));
  }
}

TEST_F(TestRptClientApi, SendRdmCommandInvalidCallsFail)
{
  rdmnet_client_t handle;
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle));
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(handle, &default_dynamic_scope_, &scope_handle));

  RdmnetLocalRdmCommand cmd;
  uint32_t seq_num = 0;

  // Core not initialized
  rdmnet_core_initialized_fake.return_val = false;
  EXPECT_EQ(kEtcPalErrNotInit, rdmnet_rpt_client_send_rdm_command(handle, scope_handle, &cmd, &seq_num));

  // Invalid parameters
  rdmnet_core_initialized_fake.return_val = true;
  EXPECT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_rdm_command(RDMNET_CLIENT_INVALID, scope_handle, &cmd, &seq_num));
  EXPECT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_rdm_command(handle, RDMNET_CLIENT_SCOPE_INVALID, &cmd, &seq_num));
  EXPECT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_rdm_command(handle, scope_handle, NULL, &seq_num));
}

TEST_F(TestRptClientApi, SendRdmResponseInvalidCallsFail)
{
  rdmnet_client_t handle;
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle));
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(handle, &default_dynamic_scope_, &scope_handle));

  RdmnetLocalRdmResponse resp;

  // Core not initialized
  rdmnet_core_initialized_fake.return_val = false;
  ASSERT_EQ(kEtcPalErrNotInit, rdmnet_rpt_client_send_rdm_response(handle, scope_handle, &resp));

  // Invalid parameters
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_rdm_response(RDMNET_CLIENT_INVALID, scope_handle, &resp));
  ASSERT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_rdm_response(handle, RDMNET_CLIENT_SCOPE_INVALID, &resp));
  ASSERT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_rdm_response(handle, scope_handle, NULL));
}

TEST_F(TestRptClientApi, SendStatusInvalidCallsFail)
{
  rdmnet_client_t handle;
  rdmnet_client_scope_t scope_handle;
  ASSERT_EQ(kEtcPalErrOk, rdmnet_rpt_client_create(&default_rpt_config_, &handle));
  ASSERT_EQ(kEtcPalErrOk, rdmnet_client_add_scope(handle, &default_dynamic_scope_, &scope_handle));

  LocalRptStatus status;

  // Core not initialized
  rdmnet_core_initialized_fake.return_val = false;
  ASSERT_EQ(kEtcPalErrNotInit, rdmnet_rpt_client_send_status(handle, scope_handle, &status));

  // Invalid parameters
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_status(RDMNET_CLIENT_INVALID, scope_handle, &status));
  ASSERT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_status(handle, RDMNET_CLIENT_SCOPE_INVALID, &status));
  ASSERT_EQ(kEtcPalErrInvalid, rdmnet_rpt_client_send_status(handle, scope_handle, NULL));
}
