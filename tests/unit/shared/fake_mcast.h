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

// fake_mcast.h: Set up some fake values to be returned from the RDMnet core mcast module, which
// the LLRP modules use.

#ifndef FAKE_MCAST_H_
#define FAKE_MCAST_H_

#include <vector>
#include "etcpal/cpp/inet.h"
#include "rdmnet/common.h"

extern const std::vector<EtcPalMcastNetintId> kFakeNetints;
extern const etcpal::MacAddr                  kLowestMacAddr;

void SetUpFakeMcastEnvironment();

#endif  // FAKE_MCAST_H_
