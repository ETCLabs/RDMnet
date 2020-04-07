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

#include "rdmnet/controller.h"

#include "gtest/gtest.h"
#include "fff.h"

FAKE_VOID_FUNC(handle_controller_connected, rdmnet_controller_t, rdmnet_client_scope_t,
               const RdmnetClientConnectedInfo*, void*);
FAKE_VOID_FUNC(handle_controller_connect_failed, rdmnet_controller_t, rdmnet_client_scope_t,
               const RdmnetClientConnectFailedInfo*, void*);
FAKE_VOID_FUNC(handle_controller_disconnected, rdmnet_controller_t, rdmnet_client_scope_t,
               const RdmnetClientDisconnectedInfo*, void*);
FAKE_VOID_FUNC(handle_client_list_update_received, rdmnet_controller_t, rdmnet_client_scope_t, client_list_action_t,
               const RptClientList*, void*);
FAKE_VOID_FUNC(handle_rdm_response_received, rdmnet_controller_t, rdmnet_client_scope_t, const RdmnetRdmResponse*,
               void*);
FAKE_VOID_FUNC(handle_status_received, rdmnet_controller_t, rdmnet_client_scope_t, const RdmnetRptStatus*, void*);
FAKE_VOID_FUNC(handle_responder_ids_received, rdmnet_controller_t, rdmnet_client_scope_t,
               const RdmnetDynamicUidAssignmentList*, void*);

FAKE_VOID_FUNC(handle_rdm_command_received, rdmnet_controller_t, rdmnet_client_scope_t, const RdmnetRdmCommand*,
               RdmnetSyncRdmResponse*, void*);
FAKE_VOID_FUNC(handle_llrp_rdm_command_received, rdmnet_controller_t, const LlrpRdmCommand*, RdmnetSyncRdmResponse*,
               void*);

class TestControllerApi : public testing::Test
{
protected:
  static constexpr uint16_t kTestManufId = 0x1234;

  void ResetLocalFakes()
  {
    RESET_FAKE(handle_controller_connected);
    RESET_FAKE(handle_controller_connect_failed);
    RESET_FAKE(handle_controller_disconnected);
    RESET_FAKE(handle_client_list_update_received);
    RESET_FAKE(handle_rdm_response_received);
    RESET_FAKE(handle_status_received);
    RESET_FAKE(handle_responder_ids_received);
    RESET_FAKE(handle_rdm_response_received);
    RESET_FAKE(handle_llrp_rdm_command_received);
  }

  void SetUp() override
  {
    ResetLocalFakes();
    ASSERT_EQ(rdmnet_init(nullptr, nullptr), kEtcPalErrOk);

    rdmnet_controller_set_callbacks(&config_, handle_controller_connected, handle_controller_connect_failed,
                                    handle_controller_disconnected, handle_client_list_update_received,
                                    handle_rdm_response_received, handle_status_received, handle_responder_ids_received,
                                    nullptr);
  }

  void TearDown() override { rdmnet_deinit(); }

  std::string test_labels_{"Test"};
  RdmnetControllerConfig config_{RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT_VALUES(kTestManufId)};
};

TEST_F(TestControllerApi, Placeholder)
{
  rdmnet_controller_t handle;
  EXPECT_EQ(rdmnet_controller_create(&config_, &handle), kEtcPalErrOk);
}
