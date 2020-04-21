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

#include "rdmnet/ept_client.h"

#include "gtest/gtest.h"
#include "fff.h"

FAKE_VOID_FUNC(handle_ept_client_connected, rdmnet_ept_client_t, rdmnet_client_scope_t,
               const RdmnetClientConnectedInfo*, void*);
FAKE_VOID_FUNC(handle_ept_client_connect_failed, rdmnet_ept_client_t, rdmnet_client_scope_t,
               const RdmnetClientConnectFailedInfo*, void*);
FAKE_VOID_FUNC(handle_ept_client_disconnected, rdmnet_ept_client_t, rdmnet_client_scope_t,
               const RdmnetClientDisconnectedInfo*, void*);
FAKE_VOID_FUNC(handle_ept_client_client_list_update_received, rdmnet_ept_client_t, rdmnet_client_scope_t,
               client_list_action_t, const RdmnetEptClientList*, void*);
FAKE_VOID_FUNC(handle_ept_client_data_received, rdmnet_ept_client_t, rdmnet_client_scope_t, const RdmnetEptData*,
               RdmnetSyncEptResponse*, void*);
FAKE_VOID_FUNC(handle_ept_client_status_received, rdmnet_ept_client_t, rdmnet_client_scope_t, const RdmnetEptStatus*,
               void*);

class TestEptClientApi : public testing::Test
{
protected:
  void ResetLocalFakes()
  {
    RESET_FAKE(handle_ept_client_connected);
    RESET_FAKE(handle_ept_client_connect_failed);
    RESET_FAKE(handle_ept_client_disconnected);
    RESET_FAKE(handle_ept_client_client_list_update_received);
    RESET_FAKE(handle_ept_client_data_received);
    RESET_FAKE(handle_ept_client_status_received);
  }

  void SetUp() override
  {
    ResetLocalFakes();
    ASSERT_EQ(rdmnet_init(nullptr, nullptr), kEtcPalErrOk);

    rdmnet_ept_client_set_callbacks(&config_, handle_ept_client_connected, handle_ept_client_connect_failed,
                                    handle_ept_client_disconnected, handle_ept_client_client_list_update_received,
                                    handle_ept_client_data_received, handle_ept_client_status_received, nullptr);
    config_.protocols = &test_prot;
    config_.num_protocols = 1;
  }

  void TearDown() override { rdmnet_deinit(); }

  RdmnetEptClientConfig config_ = RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT;
  RdmnetEptSubProtocol test_prot{0x1234, 1, "Test Protocol"};
};

TEST_F(TestEptClientApi, Placeholder)
{
  rdmnet_ept_client_t handle;
  EXPECT_EQ(rdmnet_ept_client_create(&config_, &handle), kEtcPalErrOk);
}
