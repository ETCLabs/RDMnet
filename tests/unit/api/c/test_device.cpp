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

#include "rdmnet/device.h"

#include "gtest/gtest.h"
#include "fff.h"

FAKE_VOID_FUNC(handle_device_connected, rdmnet_device_t, const RdmnetClientConnectedInfo*, void*);
FAKE_VOID_FUNC(handle_device_connect_failed, rdmnet_device_t, const RdmnetClientConnectFailedInfo*, void*);
FAKE_VOID_FUNC(handle_device_disconnected, rdmnet_device_t, const RdmnetClientDisconnectedInfo*, void*);
FAKE_VOID_FUNC(handle_device_rdm_command_received, rdmnet_device_t, const RdmnetRdmCommand*, RdmnetSyncRdmResponse*,
               void*);
FAKE_VOID_FUNC(handle_device_llrp_rdm_command_received, rdmnet_device_t, const LlrpRdmCommand*, RdmnetSyncRdmResponse*,
               void*);
FAKE_VOID_FUNC(handle_device_dynamic_uid_status, rdmnet_device_t, const RdmnetDynamicUidAssignmentList*, void*);

class TestDeviceApi : public testing::Test
{
protected:
  static constexpr uint16_t kTestManufId = 0x1234;

  void ResetLocalFakes()
  {
    RESET_FAKE(handle_device_connected);
    RESET_FAKE(handle_device_connect_failed);
    RESET_FAKE(handle_device_disconnected);
    RESET_FAKE(handle_device_rdm_command_received);
    RESET_FAKE(handle_device_llrp_rdm_command_received);
    RESET_FAKE(handle_device_dynamic_uid_status);
  }

  void SetUp() override
  {
    ResetLocalFakes();
    ASSERT_EQ(rdmnet_init(nullptr, nullptr), kEtcPalErrOk);

    rdmnet_device_set_callbacks(&config_, handle_device_connected, handle_device_connect_failed,
                                handle_device_disconnected, handle_device_rdm_command_received,
                                handle_device_llrp_rdm_command_received, handle_device_dynamic_uid_status, nullptr);
  }

  void TearDown() override { rdmnet_deinit(); }

  RdmnetDeviceConfig config_{RDMNET_DEVICE_CONFIG_DEFAULT_INIT_VALUES(kTestManufId)};
};

TEST_F(TestDeviceApi, Placeholder)
{
  rdmnet_device_t handle;
  EXPECT_EQ(rdmnet_device_create(&config_, &handle), kEtcPalErrOk);
}
