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
#include <vector>

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
    // Reset the fakes
    RESET_FAKE(rdmnet_client_connected);
    RESET_FAKE(rdmnet_client_disconnected);

    RDMNET_CORE_DISCOVERY_DO_FOR_ALL_FAKES(RESET_FAKE);
    RDMNET_CORE_CONNECTION_DO_FOR_ALL_FAKES(RESET_FAKE);

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

// Test the rdmnet_rpt_client_create() function in valid and invalid scenarios
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
  rdmnet_client_t handle_1;

  // Invalid arguments
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(LWPA_INVALID, rdmnet_rpt_client_create(NULL, NULL));
  ASSERT_EQ(LWPA_INVALID, rdmnet_rpt_client_create(&config, NULL));
  ASSERT_EQ(LWPA_INVALID, rdmnet_rpt_client_create(NULL, &handle_1));

  // Valid config, but core is not initialized
  rdmnet_core_initialized_fake.return_val = false;
  ASSERT_EQ(LWPA_NOTINIT, rdmnet_rpt_client_create(&config, &handle_1));

  // Valid create with one scope
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(LWPA_OK, rdmnet_rpt_client_create(&config, &handle_1));

  // Valid create with 100 different scopes
  std::vector<RdmnetScopeConfig> lots_of_scopes(100, scope_1_);
  for (size_t i = 0; i < 100; ++i)
  {
    strcat(lots_of_scopes[i].scope, std::to_string(i).c_str());
  }
  config.scope_list = lots_of_scopes.data();
  config.num_scopes = 100;
  rdmnet_client_t handle_2;
  ASSERT_EQ(LWPA_OK, rdmnet_rpt_client_create(&config, &handle_2));
}

// Test that the rdmnet_rpt_client_create() function has the correct side-effects
TEST_F(TestRdmnetClient, create_sideeffects)
{
  RdmnetRptClientConfig config = {kRPTClientTypeController,
                                  false,
                                  {},
                                  {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
                                  &scope_1_,
                                  1,
                                  callbacks_,
                                  this};
  rdmnet_client_t handle_1;

  // Create a client with one scope, dynamic discovery
  rdmnet_core_initialized_fake.return_val = true;
  ASSERT_EQ(LWPA_OK, rdmnet_rpt_client_create(&config, &handle_1));
  ASSERT_EQ(rdmnetdisc_start_monitoring_fake.call_count, 1u);
  ASSERT_EQ(rdmnet_connect_fake.call_count, 0u);

  RESET_FAKE(rdmnetdisc_start_monitoring);
  RESET_FAKE(rdmnet_connect);

  // Create another client with one scope and a static broker address
  RdmnetRptClientConfig config_2 = config;
  config_2.scope_list = &scope_2_;
  ASSERT_EQ(LWPA_OK, rdmnet_rpt_client_create(&config_2, &handle_1));
  ASSERT_EQ(rdmnetdisc_start_monitoring_fake.call_count, 0u);
  ASSERT_EQ(rdmnet_connect_fake.call_count, 1u);
}
