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

#include "rdmnet/disc/common.h"
#include "rdmnet/disc/platform_api.h"
#include "rdmnet/disc/registered_broker.h"
#include "rdmnet/disc/monitored_scope.h"

#include <cstring>
#include <memory>
#include <string>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/timer.h"
#include "rdmnet_mock/core/common.h"
#include "gtest/gtest.h"
#include "test_disc_common_fakes.h"
#include "rdmnet_config.h"

// Disable <cstring> warnings on Windows/MSVC
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

class TestDiscoveryCommon : public testing::Test
{
protected:
  RdmnetScopeMonitorConfig   default_monitor_config_{};
  RdmnetBrokerRegisterConfig default_register_config_{};
  EtcPalIpAddr               default_listen_addr_;
  bool                       deinitted_during_test_{false};

  void SetUp() override
  {
    etcpal_timer_reset_all_fakes();
    rdmnet_mock_core_reset_and_init();
    TestDiscoveryCommonResetAllFakes();

    ASSERT_EQ(kEtcPalErrOk, rdmnet_disc_module_init(nullptr));

    // Fill default values to make default_monitor_config_ valid.
    default_monitor_config_.scope = E133_DEFAULT_SCOPE;
    default_monitor_config_.domain = E133_DEFAULT_DOMAIN;
    default_monitor_config_.callbacks = {monitorcb_broker_found, monitorcb_broker_updated, monitorcb_broker_lost, this};

    // Fill default values to make default_register_config_ valid.
    default_register_config_.cid = etcpal::Uuid::V4().get();
    default_register_config_.service_instance_name = "Test Broker Service Name";
    default_register_config_.port = 8888;
    default_register_config_.netints = nullptr;
    default_register_config_.num_netints = 0;
    default_register_config_.scope = E133_DEFAULT_SCOPE;
    default_register_config_.model = "Test";
    default_register_config_.manufacturer = "Test";
    default_register_config_.callbacks = {regcb_broker_registered, regcb_broker_register_error,
                                          regcb_other_broker_found, regcb_other_broker_lost, this};
  }

  void TearDown() override
  {
    if (!deinitted_during_test_)
      rdmnet_disc_module_deinit();
  }

  rdmnet_registered_broker_t RegisterBroker()
  {
    rdmnet_registered_broker_t broker_handle;
    EXPECT_EQ(rdmnet_disc_register_broker(&default_register_config_, &broker_handle), kEtcPalErrOk);

    // Advance time past the query timeout, initiating random backoff
    etcpal_getms_fake.return_val += BROKER_REG_QUERY_TIMEOUT + 1000;
    rdmnet_disc_module_tick();

    // Advance time past the random backoff
    etcpal_getms_fake.return_val += BROKER_REG_QUERY_TIMEOUT + 1000;
    rdmnet_disc_module_tick();

    EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 1u);
    EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.arg0_val, broker_handle);
    return broker_handle;
  }
};

// None of the public API functions should return an "Ok" error code if rdmnet_core_init() has not
// been called.
TEST_F(TestDiscoveryCommon, DoesntWorkIfNotInitialized)
{
  rc_initialized_fake.return_val = false;

  rdmnet_scope_monitor_t monitor_handle;
  int                    platform_err;
  EXPECT_EQ(rdmnet_disc_start_monitoring(&default_monitor_config_, &monitor_handle, &platform_err), kEtcPalErrNotInit);

  rdmnet_registered_broker_t broker_handle;
  EXPECT_EQ(rdmnet_disc_register_broker(&default_register_config_, &broker_handle), kEtcPalErrNotInit);
}

TEST_F(TestDiscoveryCommon, StartMonitoringWorksWithNormalArgs)
{
  rdmnet_scope_monitor_t monitor_handle;
  int                    platform_err;

  EXPECT_EQ(rdmnet_disc_start_monitoring(&default_monitor_config_, &monitor_handle, &platform_err), kEtcPalErrOk);
  EXPECT_EQ(rdmnet_disc_platform_start_monitoring_fake.call_count, 1u);
}

#if RDMNET_DYNAMIC_MEM
TEST_F(TestDiscoveryCommon, BrokerRegisterSucceedsUnderNormalConditions)
{
  rdmnet_registered_broker_t broker_handle;
  ASSERT_EQ(rdmnet_disc_register_broker(&default_register_config_, &broker_handle), kEtcPalErrOk);

  // Make sure broker is not registered before query timeout expires
  rdmnet_disc_module_tick();
  ASSERT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 0u);

  // Advance time past the query timeout, initiating random backoff
  etcpal_getms_fake.return_val += BROKER_REG_QUERY_TIMEOUT + 1000;
  rdmnet_disc_module_tick();
  ASSERT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 0u);

  // Advance time past the random backoff
  etcpal_getms_fake.return_val += BROKER_REG_QUERY_TIMEOUT + 1000;
  rdmnet_disc_module_tick();

  EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.arg0_val, broker_handle);
}

TEST_F(TestDiscoveryCommon, BrokerUnregisterCallsPlatformCode)
{
  auto broker_handle = RegisterBroker();

  ASSERT_EQ(rdmnet_disc_platform_unregister_broker_fake.call_count, 0u);

  rdmnet_disc_unregister_broker(broker_handle);
  EXPECT_EQ(rdmnet_disc_platform_unregister_broker_fake.call_count, 1u);
}

TEST_F(TestDiscoveryCommon, BrokerNotRegisteredWhenConflictingBrokersPresent)
{
  rdmnet_registered_broker_t broker_handle;
  ASSERT_EQ(rdmnet_disc_register_broker(&default_register_config_, &broker_handle), kEtcPalErrOk);

  // Add a conflicting broker
  DiscoveredBroker* db = discovered_broker_new(broker_handle->scope_monitor_handle, "Other Test Broker",
                                               "Other Test Broker._rdmnet._tcp.local.");
  discovered_broker_insert(&broker_handle->scope_monitor_handle->broker_list, db);

  rdmnet_disc_module_tick();

  // Advance time past the query timeout
  etcpal_getms_fake.return_val += BROKER_REG_QUERY_TIMEOUT + 100;
  rdmnet_disc_module_tick();

  // Make sure the broker has not been registered
  EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 0u);
}
#endif

TEST_F(TestDiscoveryCommon, DeinitUnmonitorsScope)
{
  rdmnet_scope_monitor_t monitor_handle;
  int                    platform_error;
  ASSERT_EQ(rdmnet_disc_start_monitoring(&default_monitor_config_, &monitor_handle, &platform_error), kEtcPalErrOk);
  ASSERT_EQ(rdmnet_disc_platform_start_monitoring_fake.call_count, 1u);

  rdmnet_disc_module_deinit();
  EXPECT_EQ(rdmnet_disc_platform_stop_monitoring_fake.call_count, 1u);

  scope_monitor_for_each([](RdmnetScopeMonitorRef*) {
    FAIL() << "There were still scope monitor refs in the global list after deinit was called.";
  });

  // Stop the destructor from doing a double deinit
  deinitted_during_test_ = true;
}

#if RDMNET_DYNAMIC_MEM
TEST_F(TestDiscoveryCommon, DeinitUnregistersBrokerIfRegistered)
{
  RegisterBroker();

  rdmnet_disc_module_deinit();
  EXPECT_EQ(rdmnet_disc_platform_unregister_broker_fake.call_count, 1u);

  registered_broker_for_each([](RdmnetBrokerRegisterRef*) {
    FAIL() << "There were still registered brokers in the global list after deinit was called.";
  });

  // Stop the destructor from doing a double deinit
  deinitted_during_test_ = true;
}
#endif
