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

#include "rdmnet/cpp/ept_client.h"

#include "gmock/gmock.h"
#include "rdmnet_mock/common.h"

class MockEptClientNotifyHandler : public rdmnet::EptClient::NotifyHandler
{
  MOCK_METHOD(void,
              HandleConnectedToBroker,
              (rdmnet::EptClient::Handle          handle,
               rdmnet::ScopeHandle                scope_handle,
               const rdmnet::ClientConnectedInfo& info),
              (override));
  MOCK_METHOD(void,
              HandleBrokerConnectFailed,
              (rdmnet::EptClient::Handle              handle,
               rdmnet::ScopeHandle                    scope_handle,
               const rdmnet::ClientConnectFailedInfo& info),
              (override));
  MOCK_METHOD(void,
              HandleDisconnectedFromBroker,
              (rdmnet::EptClient::Handle             handle,
               rdmnet::ScopeHandle                   scope_handle,
               const rdmnet::ClientDisconnectedInfo& info),
              (override));
  MOCK_METHOD(void,
              HandleClientListUpdate,
              (rdmnet::EptClient::Handle    handle,
               rdmnet::ScopeHandle          scope_handle,
               client_list_action_t         list_action,
               const rdmnet::EptClientList& list),
              (override));
  MOCK_METHOD(rdmnet::EptResponseAction,
              HandleEptData,
              (rdmnet::EptClient::Handle handle, rdmnet::ScopeHandle scope_handle, const rdmnet::EptData& data),
              (override));
  MOCK_METHOD(void,
              HandleEptStatus,
              (rdmnet::EptClient::Handle handle, rdmnet::ScopeHandle scope_handle, const rdmnet::EptStatus& status),
              (override));
};

class TestCppEptClientApi : public testing::Test
{
protected:
  void SetUp() override
  {
    rdmnet_mock_common_reset();
    ASSERT_EQ(rdmnet::Init(), kEtcPalErrOk);
  }

  void TearDown() override { rdmnet::Deinit(); }
};

TEST_F(TestCppEptClientApi, Startup)
{
  // MockEptClientNotifyHandler notify;
  // rdmnet::EptClient          ept_client;

  // std::vector<rdmnet::EptSubProtocol> protocols;
  // protocols.push_back(rdmnet::EptSubProtocol(0x6574, 0xdddd, "Test Protocol"));
  // rdmnet::EptClient::Settings settings(etcpal::Uuid::OsPreferred(), protocols);

  // EXPECT_EQ(ept_client.Startup(notify, settings), kEtcPalErrOk);
}
