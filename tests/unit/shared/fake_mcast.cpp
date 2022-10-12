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

#include "fake_mcast.h"

#include "rdmnet_mock/core/mcast.h"
#include "gtest/gtest.h"

// clang-format off
const std::vector<EtcPalMcastNetintId> kFakeNetints = {
  { kEtcPalIpTypeV4, 1 },
  { kEtcPalIpTypeV6, 1 },
  { kEtcPalIpTypeV6, 2 },
};
// clang-format on

const etcpal::MacAddr kLowestMacAddr = etcpal::MacAddr::FromString("00:c0:16:a8:ec:82");

void SetUpFakeMcastEnvironment()
{
  rc_mcast_get_netint_array_fake.custom_fake = [](const EtcPalMcastNetintId** array) {
    EXPECT_TRUE(array);
    *array = kFakeNetints.data();
    return kFakeNetints.size();
  };
  rc_mcast_netint_is_valid_fake.custom_fake = [](const EtcPalMcastNetintId* id) {
    EXPECT_TRUE(id);
    return (std::find(kFakeNetints.begin(), kFakeNetints.end(), *id) != kFakeNetints.end());
  };
  rc_mcast_get_lowest_mac_addr_fake.return_val = &kLowestMacAddr.get();
  // Add other fakes as needed
}
