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

#include "rdmnet/common.h"

#include <climits>
#include "gtest/gtest.h"

TEST(TestCommonApi, EventToStringFunctionsWork)
{
  // out-of-range values should still return a valid "unknown" string
  EXPECT_NE(rdmnet_connect_fail_event_to_string(kRdmnetConnectFailSocketFailure), nullptr);
  EXPECT_NE(rdmnet_connect_fail_event_to_string(static_cast<rdmnet_connect_fail_event_t>(INT_MAX)), nullptr);
  EXPECT_NE(rdmnet_connect_fail_event_to_string(static_cast<rdmnet_connect_fail_event_t>(-1)), nullptr);

  EXPECT_NE(rdmnet_disconnect_event_to_string(kRdmnetDisconnectAbruptClose), nullptr);
  EXPECT_NE(rdmnet_disconnect_event_to_string(static_cast<rdmnet_disconnect_event_t>(INT_MAX)), nullptr);
  EXPECT_NE(rdmnet_disconnect_event_to_string(static_cast<rdmnet_disconnect_event_t>(-1)), nullptr);

  EXPECT_NE(rdmnet_connect_status_to_string(kRdmnetConnectOk), nullptr);
  EXPECT_NE(rdmnet_connect_status_to_string(static_cast<rdmnet_connect_status_t>(INT_MAX)), nullptr);
  EXPECT_NE(rdmnet_connect_status_to_string(static_cast<rdmnet_connect_status_t>(-1)), nullptr);

  EXPECT_NE(rdmnet_disconnect_reason_to_string(kRdmnetDisconnectShutdown), nullptr);
  EXPECT_NE(rdmnet_disconnect_reason_to_string(static_cast<rdmnet_disconnect_reason_t>(INT_MAX)), nullptr);
  EXPECT_NE(rdmnet_disconnect_reason_to_string(static_cast<rdmnet_disconnect_reason_t>(-1)), nullptr);

  EXPECT_NE(rdmnet_dynamic_uid_status_to_string(kRdmnetDynamicUidStatusOk), nullptr);
  EXPECT_NE(rdmnet_dynamic_uid_status_to_string(static_cast<rdmnet_dynamic_uid_status_t>(INT_MAX)), nullptr);
  EXPECT_NE(rdmnet_dynamic_uid_status_to_string(static_cast<rdmnet_dynamic_uid_status_t>(-1)), nullptr);
}
