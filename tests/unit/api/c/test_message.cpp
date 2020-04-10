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

#include "rdmnet/message.h"

#include <array>
#include <cstring>
#include "gtest/gtest.h"

TEST(TestMessageApi, Placeholder)
{
  const std::array<uint8_t, 4> kTestData{0x00, 0x01, 0x02, 0x03};

  RdmnetRdmCommand cmd{
      {0x1234, 0x56789abc}, 1, 0x12345678, {0}, kTestData.data(), static_cast<uint8_t>(kTestData.size())};

  RdmnetSavedRdmCommand saved_cmd;
  ASSERT_EQ(rdmnet_save_rdm_command(&cmd, &saved_cmd), kEtcPalErrOk);

  // TODO test other members
  ASSERT_EQ(saved_cmd.data_len, cmd.data_len);
  EXPECT_EQ(0, std::memcmp(saved_cmd.data, kTestData.data(), kTestData.size()));
}
