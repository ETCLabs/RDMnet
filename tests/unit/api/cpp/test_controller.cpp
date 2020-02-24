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

#include "rdmnet/cpp/controller.h"

#include "gmock/gmock.h"
#include "rdmnet_mock/common.h"

class MockControllerNotifyHandler : public rdmnet::ControllerNotifyHandler
{
  MOCK_METHOD(void, HandleConnectedToBroker,
              (rdmnet::ControllerHandle handle, rdmnet::ScopeHandle scope_handle,
               const RdmnetClientConnectedInfo& info),
              (override));
  MOCK_METHOD(void, HandleBrokerConnectFailed,
              (rdmnet::ControllerHandle handle, rdmnet::ScopeHandle scope_handle,
               const RdmnetClientConnectFailedInfo& info),
              (override));
  MOCK_METHOD(void, HandleDisconnectedFromBroker,
              (rdmnet::ControllerHandle handle, rdmnet::ScopeHandle scope_handle,
               const RdmnetClientDisconnectedInfo& info),
              (override));
  MOCK_METHOD(void, HandleClientListUpdate,
              (rdmnet::ControllerHandle handle, rdmnet::ScopeHandle scope_handle, client_list_action_t list_action,
               const RptClientList& list),
              (override));
  MOCK_METHOD(void, HandleRdmResponse,
              (rdmnet::ControllerHandle handle, rdmnet::ScopeHandle scope_handle, const rdmnet::RdmResponse& resp),
              (override));
  MOCK_METHOD(void, HandleRptStatus,
              (rdmnet::ControllerHandle handle, rdmnet::ScopeHandle scope_handle, const rdmnet::RptStatus& status),
              (override));
};

class MockControllerRdmHandler : public rdmnet::ControllerRdmCommandHandler
{
  MOCK_METHOD(rdmnet::ResponseAction, HandleRdmCommand,
              (rdmnet::ControllerHandle handle, rdmnet::ScopeHandle scope_handle, const rdmnet::RdmCommand& cmd),
              (override));
  MOCK_METHOD(rdmnet::ResponseAction, HandleLlrpRdmCommand,
              (rdmnet::ControllerHandle handle, const rdmnet::llrp::RdmCommand& cmd), (override));
};

class TestCppControllerApi : public testing::Test
{
protected:
  void SetUp() override
  {
    rdmnet_mock_common_reset();
    ASSERT_EQ(rdmnet::Init(), kEtcPalErrOk);
  }

  void TearDown() override { rdmnet::Deinit(); }
};

TEST_F(TestCppControllerApi, StartupWithRdmData)
{
  MockControllerNotifyHandler notify;
  rdmnet::ControllerRdmData rdm("Test", "Test", "Test", "Test");
  rdmnet::Controller controller;

  EXPECT_EQ(controller.Startup(notify, rdmnet::ControllerSettings(etcpal::Uuid::OsPreferred(), 0x6574), rdm),
            kEtcPalErrOk);
}

TEST_F(TestCppControllerApi, StartupWithRdmHandler)
{
  MockControllerNotifyHandler notify;
  MockControllerRdmHandler rdm;
  rdmnet::Controller controller;

  EXPECT_EQ(controller.Startup(notify, rdmnet::ControllerSettings(etcpal::Uuid::OsPreferred(), 0x6574), rdm),
            kEtcPalErrOk);
}
