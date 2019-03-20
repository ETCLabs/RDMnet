/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include <string>

#include "gtest/gtest.h"
#include "fff.h"

#include "rdmnet/client.h"

#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/discovery.h"
#include "rdmnet_mock/core.h"
#include "rdmnet/core/util.h"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(rdmnet_client_connected, rdmnet_client_t, const char *, void *);
FAKE_VOID_FUNC(rdmnet_client_disconnected, rdmnet_client_t, const char *, void *);

class TestRdmnetClient : public testing::Test
{
protected:
  TestRdmnetClient()
  {
    rdmnet_safe_strncpy(scope_1_.scope, "default", E133_SCOPE_STRING_PADDED_LENGTH);
    scope_1_.has_static_broker_addr = false;

    rdmnet_safe_strncpy(scope_2_.scope, "not_default", E133_SCOPE_STRING_PADDED_LENGTH);
    scope_2_.has_static_broker_addr = true;
    lwpaip_set_v4_address(&scope_2_.static_broker_addr.ip, 0x0a650101);
    scope_2_.static_broker_addr.port = 8888;

    callbacks_.connected = rdmnet_client_connected;
    callbacks_.disconnected = rdmnet_client_disconnected;
  }

  RdmnetScopeConfig scope_1_;
  RdmnetScopeConfig scope_2_;
  RdmnetClientCallbacks callbacks_;
};

TEST_F(TestRdmnetClient, create)
{
  // The config we'll be using
  RdmnetRptClientConfig config = {kRPTClientTypeController,
                                  false,
                                  {},
                                  {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
                                  &scope_1_,
                                  1,
                                  callbacks_,
                                  this};
  rdmnet_client_t handle;

  // Invalid arguments
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(LWPA_INVALID, rdmnet_rpt_client_create(NULL, NULL));
  ASSERT_EQ(LWPA_INVALID, rdmnet_rpt_client_create(&config, NULL));
  ASSERT_EQ(LWPA_INVALID, rdmnet_rpt_client_create(NULL, &handle));

  // Valid config, but core is not initialized
  rdmnet_core_initialized_fake.return_val = false;
  ASSERT_EQ(LWPA_NOTINIT, rdmnet_rpt_client_create(&config, &handle));
}
