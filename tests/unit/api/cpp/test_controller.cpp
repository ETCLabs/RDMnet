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

#include "rdmnet/cpp/controller.h"

#include "gmock/gmock.h"
#include "rdmnet_mock/common.h"
#include "rdmnet_mock/controller.h"

class MockControllerNotifyHandler : public rdmnet::Controller::NotifyHandler
{
  MOCK_METHOD(void,
              HandleConnectedToBroker,
              (rdmnet::Controller::Handle         controller_handle,
               rdmnet::ScopeHandle                scope_handle,
               const rdmnet::ClientConnectedInfo& info),
              (override));
  MOCK_METHOD(void,
              HandleBrokerConnectFailed,
              (rdmnet::Controller::Handle             controller_handle,
               rdmnet::ScopeHandle                    scope_handle,
               const rdmnet::ClientConnectFailedInfo& info),
              (override));
  MOCK_METHOD(void,
              HandleDisconnectedFromBroker,
              (rdmnet::Controller::Handle            controller_handle,
               rdmnet::ScopeHandle                   scope_handle,
               const rdmnet::ClientDisconnectedInfo& info),
              (override));
  MOCK_METHOD(void,
              HandleClientListUpdate,
              (rdmnet::Controller::Handle   controller_handle,
               rdmnet::ScopeHandle          scope_handle,
               client_list_action_t         list_action,
               const rdmnet::RptClientList& list),
              (override));
  MOCK_METHOD(void,
              HandleRdmResponse,
              (rdmnet::Controller::Handle controller_handle,
               rdmnet::ScopeHandle        scope_handle,
               const rdmnet::RdmResponse& resp),
              (override));
  MOCK_METHOD(void,
              HandleRptStatus,
              (rdmnet::Controller::Handle controller_handle,
               rdmnet::ScopeHandle        scope_handle,
               const rdmnet::RptStatus&   status),
              (override));
};

class MockControllerRdmHandler : public rdmnet::Controller::RdmCommandHandler
{
  MOCK_METHOD(rdmnet::RdmResponseAction,
              HandleRdmCommand,
              (rdmnet::Controller::Handle controller_handle,
               rdmnet::ScopeHandle        scope_handle,
               const rdmnet::RdmCommand&  cmd),
              (override));
  MOCK_METHOD(rdmnet::RdmResponseAction,
              HandleLlrpRdmCommand,
              (rdmnet::Controller::Handle controller_handle, const rdmnet::llrp::RdmCommand& cmd),
              (override));
};

constexpr rdmnet::Controller::Handle kControllerHandle{1};

class TestCppControllerApi : public testing::Test
{
protected:
  MockControllerNotifyHandler notify_;
  MockControllerRdmHandler    rdm_handler_;
  rdmnet::Controller::RdmData rdm_data_{1u, 2u, "Test", "Test", "Test", "Test"};

  rdmnet::Controller controller_;

  void SetUp() override
  {
    rdmnet_mock_common_reset();
    rdmnet_controller_reset_all_fakes();
    ASSERT_EQ(rdmnet::Init(), kEtcPalErrOk);
  }

  void TearDown() override { rdmnet::Deinit(); }

  void StartControllerDefault()
  {
    rdmnet_controller_create_fake.custom_fake = [](const RdmnetControllerConfig*, rdmnet_controller_t* handle) {
      *handle = kControllerHandle.value();
      return kEtcPalErrOk;
    };
    ASSERT_TRUE(
        controller_.Startup(notify_, rdmnet::Controller::Settings(etcpal::Uuid::OsPreferred(), 0x6574), rdm_data_));
    ASSERT_EQ(controller_.handle(), kControllerHandle);
  }
};

TEST_F(TestCppControllerApi, StartupWithRdmData)
{
  EXPECT_TRUE(
      controller_.Startup(notify_, rdmnet::Controller::Settings(etcpal::Uuid::OsPreferred(), 0x6574), rdm_data_));
}

TEST_F(TestCppControllerApi, StartupWithRdmHandler)
{
  EXPECT_TRUE(
      controller_.Startup(notify_, rdmnet::Controller::Settings(etcpal::Uuid::OsPreferred(), 0x6574), rdm_handler_));
}

TEST_F(TestCppControllerApi, AddScopeStringOverloadWorks)
{
  static constexpr rdmnet::ScopeHandle kScopeHandle{2};
  static constexpr char                kScopeName[] = "Test Scope Name";

  StartControllerDefault();

  rdmnet_controller_add_scope_fake.custom_fake = [](rdmnet_controller_t    controller_handle, const RdmnetScopeConfig*,
                                                    rdmnet_client_scope_t* scope_handle) {
    EXPECT_EQ(controller_handle, kControllerHandle.value());
    EXPECT_NE(scope_handle, nullptr);
    *scope_handle = kScopeHandle.value();
    return kEtcPalErrOk;
  };
  auto scope_handle = controller_.AddScope(kScopeName);
  EXPECT_TRUE(scope_handle);
  EXPECT_EQ(*scope_handle, kScopeHandle);
}

TEST_F(TestCppControllerApi, AddScopeStringOverloadFailsOnError)
{
  StartControllerDefault();

  rdmnet_controller_add_scope_fake.return_val = kEtcPalErrSys;
  auto scope_handle = controller_.AddScope("Test Scope");
  EXPECT_FALSE(scope_handle);
  EXPECT_EQ(scope_handle.error_code(), kEtcPalErrSys);
}

TEST_F(TestCppControllerApi, SendRdmCommandFailsOnError)
{
  StartControllerDefault();

  rdmnet_controller_send_rdm_command_fake.return_val = kEtcPalErrSys;
  auto seq_num =
      controller_.SendRdmCommand(rdmnet::ScopeHandle(1), rdmnet::DestinationAddr::ToDefaultResponder(0x6574, 0x1234),
                                 kRdmnetCCGetCommand, E120_SUPPORTED_PARAMETERS);
  EXPECT_FALSE(seq_num);
  EXPECT_EQ(seq_num.error_code(), kEtcPalErrSys);
}

TEST_F(TestCppControllerApi, SendGetCommandFailsOnError)
{
  StartControllerDefault();

  rdmnet_controller_send_get_command_fake.return_val = kEtcPalErrSys;
  auto seq_num = controller_.SendGetCommand(
      rdmnet::ScopeHandle(1), rdmnet::DestinationAddr::ToDefaultResponder(0x6574, 0x1234), E120_SUPPORTED_PARAMETERS);
  EXPECT_FALSE(seq_num);
  EXPECT_EQ(seq_num.error_code(), kEtcPalErrSys);
}

TEST_F(TestCppControllerApi, SendSetCommandFailsOnError)
{
  StartControllerDefault();

  rdmnet_controller_send_set_command_fake.return_val = kEtcPalErrSys;
  auto seq_num = controller_.SendSetCommand(
      rdmnet::ScopeHandle(1), rdmnet::DestinationAddr::ToDefaultResponder(0x6574, 0x1234), E120_RESET_DEVICE);
  EXPECT_FALSE(seq_num);
  EXPECT_EQ(seq_num.error_code(), kEtcPalErrSys);
}
