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

#include "rdmnet/cpp/common.h"

#include "rdmnet_mock/common.h"
#include "gtest/gtest.h"

class TestCommon : public testing::Test
{
protected:
  TestCommon() { rdmnet_mock_common_reset(); }
};

TEST_F(TestCommon, InitFails)
{
  rdmnet_init_fake.return_val = kEtcPalErrSys;
  EXPECT_EQ(rdmnet::Init(), kEtcPalErrSys);
}

TEST_F(TestCommon, InitNoArgs)
{
  EXPECT_TRUE(rdmnet::Init());
  EXPECT_EQ(rdmnet_init_fake.arg0_val, nullptr);
  EXPECT_EQ(rdmnet_init_fake.arg1_val, nullptr);
}

TEST_F(TestCommon, InitLogParamsNoNetints)
{
  EtcPalLogParams params;
  EXPECT_TRUE(rdmnet::Init(&params));
  EXPECT_EQ(rdmnet_init_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_init_fake.arg0_val, &params);
  EXPECT_EQ(rdmnet_init_fake.arg1_val, nullptr);
}

TEST_F(TestCommon, InitNetintsNoLogParams)
{
  // clang-format off
  const std::vector<RdmnetMcastNetintId> netints = {
    {
      kEtcPalIpTypeV4,
      1
    },
    {
      kEtcPalIpTypeV6,
      2
    }
  };
  // clang-format on

  rdmnet_init_fake.custom_fake = [](const EtcPalLogParams* params, const RdmnetNetintConfig* config) {
    EXPECT_EQ(params, nullptr);
    EXPECT_NE(config, nullptr);
    EXPECT_EQ(config->num_netints, 2u);
    EXPECT_NE(config->netints, nullptr);
    EXPECT_EQ(config->netints[0].ip_type, kEtcPalIpTypeV4);
    EXPECT_EQ(config->netints[0].index, 1u);
    EXPECT_EQ(config->netints[1].ip_type, kEtcPalIpTypeV6);
    EXPECT_EQ(config->netints[1].index, 2u);
    return kEtcPalErrOk;
  };
  EXPECT_TRUE(rdmnet::Init(nullptr, netints));
  EXPECT_EQ(rdmnet_init_fake.call_count, 1u);
}
