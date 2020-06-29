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

#include "lwmdns_send.h"

#include "gtest/gtest.h"
#include "fff.h"
#include "etcpal_mock/common.h"
#include "rdmnet_mock/core/mcast.h"
#include "fake_mcast.h"

class TestLwMdnsSend : public testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    rc_mcast_reset_all_fakes();
    SetUpFakeMcastEnvironment();
  }

  void InitModuleWithDefaultConfig() { ASSERT_EQ(lwmdns_send_module_init(nullptr), kEtcPalErrOk); }
};

TEST_F(TestLwMdnsSend, InitWorksWithNoConfig)
{
  ASSERT_EQ(lwmdns_send_module_init(nullptr), kEtcPalErrOk);
  EXPECT_EQ(rc_mcast_get_send_socket_fake.call_count, kFakeNetints.size());

  for (size_t i = 0; i < kFakeNetints.size(); ++i)
  {
    EXPECT_EQ(rc_mcast_get_send_socket_fake.arg1_history[i], E133_MDNS_PORT);
    EXPECT_NE(rc_mcast_get_send_socket_fake.arg2_history[i], nullptr);
  }
}

TEST_F(TestLwMdnsSend, InitWorksWithConfig)
{
  RdmnetNetintConfig netint_config;
  netint_config.netints = kFakeNetints.data();
  netint_config.num_netints = 1;

  rc_mcast_get_send_socket_fake.custom_fake = [](const RdmnetMcastNetintId* netint_id, uint16_t source_port,
                                                 etcpal_socket_t* socket) {
    EXPECT_EQ(netint_id->index, kFakeNetints[0].index);
    EXPECT_EQ(netint_id->ip_type, kFakeNetints[0].ip_type);
    EXPECT_EQ(source_port, E133_MDNS_PORT);
    *socket = (etcpal_socket_t)0;
    return kEtcPalErrOk;
  };
  ASSERT_EQ(lwmdns_send_module_init(&netint_config), kEtcPalErrOk);
  EXPECT_EQ(rc_mcast_get_send_socket_fake.call_count, 1u);
}
