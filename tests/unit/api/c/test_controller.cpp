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

#include "rdmnet/controller.h"

#include <string>
#include "etcpal/cpp/uuid.h"
#include "rdmnet_mock/core/common.h"
#include "rdmnet_mock/core/client.h"
#include "gtest/gtest.h"
#include "fff.h"

FAKE_VOID_FUNC(handle_controller_connected,
               rdmnet_controller_t,
               rdmnet_client_scope_t,
               const RdmnetClientConnectedInfo*,
               void*);
FAKE_VOID_FUNC(handle_controller_connect_failed,
               rdmnet_controller_t,
               rdmnet_client_scope_t,
               const RdmnetClientConnectFailedInfo*,
               void*);
FAKE_VOID_FUNC(handle_controller_disconnected,
               rdmnet_controller_t,
               rdmnet_client_scope_t,
               const RdmnetClientDisconnectedInfo*,
               void*);
FAKE_VOID_FUNC(handle_controller_client_list_update_received,
               rdmnet_controller_t,
               rdmnet_client_scope_t,
               client_list_action_t,
               const RdmnetRptClientList*,
               void*);
FAKE_VOID_FUNC(handle_controller_rdm_response_received,
               rdmnet_controller_t,
               rdmnet_client_scope_t,
               const RdmnetRdmResponse*,
               void*);
FAKE_VOID_FUNC(handle_controller_status_received,
               rdmnet_controller_t,
               rdmnet_client_scope_t,
               const RdmnetRptStatus*,
               void*);
FAKE_VOID_FUNC(handle_controller_responder_ids_received,
               rdmnet_controller_t,
               rdmnet_client_scope_t,
               const RdmnetDynamicUidAssignmentList*,
               void*);

FAKE_VOID_FUNC(handle_controller_rdm_command_received,
               rdmnet_controller_t,
               rdmnet_client_scope_t,
               const RdmnetRdmCommand*,
               RdmnetSyncRdmResponse*,
               void*);
FAKE_VOID_FUNC(handle_controller_llrp_rdm_command_received,
               rdmnet_controller_t,
               const LlrpRdmCommand*,
               RdmnetSyncRdmResponse*,
               void*);

class TestControllerApi;

static TestControllerApi* current_test_fixture{nullptr};

class TestControllerApi : public testing::Test
{
public:
  RdmnetControllerConfig config = RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT(kTestManufId);

protected:
  static constexpr uint16_t kTestManufId = 0x1234;
  RdmnetControllerRdmData   rdm_data_ = {1u,
                                       2u,
                                       "Test Manufacturer Label",
                                       "Test Device Model Description",
                                       "Test Software Version Label",
                                       "Test Device Label",
                                       3u,
                                       true};

  void ResetLocalFakes()
  {
    RESET_FAKE(handle_controller_connected);
    RESET_FAKE(handle_controller_connect_failed);
    RESET_FAKE(handle_controller_disconnected);
    RESET_FAKE(handle_controller_client_list_update_received);
    RESET_FAKE(handle_controller_rdm_response_received);
    RESET_FAKE(handle_controller_status_received);
    RESET_FAKE(handle_controller_responder_ids_received);
    RESET_FAKE(handle_controller_rdm_response_received);
    RESET_FAKE(handle_controller_llrp_rdm_command_received);
  }

  void SetUp() override
  {
    current_test_fixture = this;

    ResetLocalFakes();
    rdmnet_mock_core_reset();
    ASSERT_EQ(rdmnet_init(nullptr, nullptr), kEtcPalErrOk);

    config.cid = etcpal::Uuid::FromString("cef3f6dc-c42d-4f39-884e-ee106029dbb8").get();
    rdmnet_controller_set_callbacks(&config, handle_controller_connected, handle_controller_connect_failed,
                                    handle_controller_disconnected, handle_controller_client_list_update_received,
                                    handle_controller_rdm_response_received, handle_controller_status_received,
                                    handle_controller_responder_ids_received, nullptr);
  }

  void TearDown() override
  {
    rdmnet_deinit();
    current_test_fixture = nullptr;
  }

  std::string test_labels_{"Test"};
};

// TODO duplicate this test for RDM handler
TEST_F(TestControllerApi, CreateRegistersClientCorrectly)
{
  rc_rpt_client_register_fake.custom_fake = [](RCClient* client, bool create_llrp_target,
                                               const EtcPalMcastNetintId* llrp_netints, size_t num_llrp_netints) {
    EXPECT_NE(client->lock, nullptr);
    EXPECT_EQ(client->type, kClientProtocolRPT);
    EXPECT_EQ(client->cid, current_test_fixture->config.cid);
    EXPECT_EQ(RC_RPT_CLIENT_DATA(client)->type, kRPTClientTypeController);
    EXPECT_EQ(RC_RPT_CLIENT_DATA(client)->uid, current_test_fixture->config.uid);
    if (current_test_fixture->config.search_domain)
      EXPECT_STREQ(client->search_domain, current_test_fixture->config.search_domain);
    else
      EXPECT_STREQ(client->search_domain, "");
    EXPECT_EQ(client->sync_resp_buf, nullptr);

    EXPECT_FALSE(create_llrp_target);
    EXPECT_EQ(llrp_netints, nullptr);
    EXPECT_EQ(num_llrp_netints, 0u);

    return kEtcPalErrOk;
  };

  config.rdm_data = rdm_data_;

  rdmnet_controller_t handle;
  EXPECT_EQ(rdmnet_controller_create(&config, &handle), kEtcPalErrOk);
}
