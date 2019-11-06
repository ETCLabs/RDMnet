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

#include "rdmnet/discovery/common.h"

#include <cstring>
#include <memory>
#include <string>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/timer.h"
#include "gtest/gtest.h"
#include "fff.h"

// Disable <cstring> warnings on Windows/MSVC
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

DEFINE_FFF_GLOBALS;

extern "C" {
// rdmnet_core
FAKE_VALUE_FUNC(bool, rdmnet_core_initialized);

// Platform-specific rdmnet_disc sources
FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_init);
FAKE_VOID_FUNC(rdmnet_disc_platform_deinit);
FAKE_VOID_FUNC(rdmnet_disc_platform_tick);
FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_start_monitoring, const RdmnetScopeMonitorConfig*,
                RdmnetScopeMonitorRef*, int*);
FAKE_VOID_FUNC(rdmnet_disc_platform_stop_monitoring, RdmnetScopeMonitorRef*);
FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_platform_register_broker, const RdmnetBrokerDiscInfo*,
                RdmnetBrokerRegisterRef*, int*);
FAKE_VOID_FUNC(rdmnet_disc_platform_unregister_broker, rdmnet_registered_broker_t);

FAKE_VOID_FUNC(discovered_broker_free_platform_resources, DiscoveredBroker*);
FAKE_VOID_FUNC(scope_monitor_free_platform_resources, RdmnetScopeMonitorRef*);
FAKE_VOID_FUNC(registered_broker_free_platform_resources, RdmnetBrokerRegisterRef*);

// rdmnet_disc callbacks
FAKE_VOID_FUNC(monitorcb_broker_found, rdmnet_scope_monitor_t, const RdmnetBrokerDiscInfo*, void*);
FAKE_VOID_FUNC(monitorcb_broker_lost, rdmnet_scope_monitor_t, const char*, const char*, void*);
FAKE_VOID_FUNC(monitorcb_scope_monitor_error, rdmnet_scope_monitor_t, const char*, int, void*);

FAKE_VOID_FUNC(regcb_broker_registered, rdmnet_registered_broker_t, const char*, void*);
FAKE_VOID_FUNC(regcb_broker_register_error, rdmnet_registered_broker_t, int, void*);
FAKE_VOID_FUNC(regcb_broker_found, rdmnet_registered_broker_t, const RdmnetBrokerDiscInfo*, void*);
FAKE_VOID_FUNC(regcb_broker_lost, rdmnet_registered_broker_t, const char*, const char*, void*);
FAKE_VOID_FUNC(regcb_scope_monitor_error, rdmnet_registered_broker_t, const char*, int, void*);
}

void TestDiscoveryCommonResetAllFakes()
{
  RESET_FAKE(rdmnet_core_initialized);

  RESET_FAKE(rdmnet_disc_platform_init);
  RESET_FAKE(rdmnet_disc_platform_deinit);
  RESET_FAKE(rdmnet_disc_platform_tick);
  RESET_FAKE(rdmnet_disc_platform_start_monitoring);
  RESET_FAKE(rdmnet_disc_platform_stop_monitoring);
  RESET_FAKE(rdmnet_disc_platform_register_broker);
  RESET_FAKE(rdmnet_disc_platform_unregister_broker);

  RESET_FAKE(monitorcb_broker_found);
  RESET_FAKE(monitorcb_broker_lost);
  RESET_FAKE(monitorcb_scope_monitor_error);

  RESET_FAKE(regcb_broker_registered);
  RESET_FAKE(regcb_broker_register_error);
  RESET_FAKE(regcb_broker_found);
  RESET_FAKE(regcb_broker_lost);
  RESET_FAKE(regcb_scope_monitor_error);
}

class TestDiscoveryCommon : public testing::Test
{
protected:
  RdmnetScopeMonitorConfig default_monitor_config_{};
  RdmnetBrokerRegisterConfig default_register_config_{};
  std::unique_ptr<BrokerListenAddr> default_listen_addr_;
  bool deinitted_during_test_{false};

  TestDiscoveryCommon()
  {
    etcpal_timer_reset_all_fakes();
    TestDiscoveryCommonResetAllFakes();
    rdmnet_core_initialized_fake.return_val = true;

    rdmnet_disc_init();

    // Fill default values to make default_monitor_config_ valid.
    std::strcpy(default_monitor_config_.scope, E133_DEFAULT_SCOPE);
    std::strcpy(default_monitor_config_.domain, E133_DEFAULT_DOMAIN);
    default_monitor_config_.callbacks = {monitorcb_broker_found, monitorcb_broker_lost, monitorcb_scope_monitor_error};
    default_monitor_config_.callback_context = this;

    // Fill default values to make default_register_config_ valid.
    default_register_config_.my_info.cid = etcpal::Uuid::V4().get();
    std::strcpy(default_register_config_.my_info.service_name, "Test Broker Service Name");
    default_register_config_.my_info.port = 8888;
    default_listen_addr_ = std::make_unique<BrokerListenAddr>();
    default_listen_addr_->addr = etcpal::IpAddr::FromString("10.101.30.40").get();
    default_listen_addr_->next = nullptr;
    default_register_config_.my_info.listen_addr_list = default_listen_addr_.get();
    std::strcpy(default_register_config_.my_info.scope, E133_DEFAULT_SCOPE);
    std::strcpy(default_register_config_.my_info.model, "Test");
    std::strcpy(default_register_config_.my_info.manufacturer, "Test");
    default_register_config_.callbacks = {regcb_broker_registered, regcb_broker_register_error, regcb_broker_found,
                                          regcb_broker_lost, regcb_scope_monitor_error};
    default_register_config_.callback_context = this;
  }

  ~TestDiscoveryCommon()
  {
    if (!deinitted_during_test_)
      rdmnet_disc_deinit();
  }

  rdmnet_registered_broker_t RegisterBroker()
  {
    rdmnet_registered_broker_t broker_handle;
    EXPECT_EQ(rdmnet_disc_register_broker(&default_register_config_, &broker_handle), kEtcPalErrOk);

    // Advance time past the query timeout
    etcpal_getms_fake.return_val += BROKER_REG_QUERY_TIMEOUT + 1000;
    rdmnet_disc_tick();

    EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 1u);
    EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.arg1_val, broker_handle);
    return broker_handle;
  }
};

TEST_F(TestDiscoveryCommon, InitBrokerInfoTouchesAllFields)
{
  RdmnetBrokerDiscInfo broker_info;
  std::memset(&broker_info, 0xcc, sizeof broker_info);

  rdmnet_disc_init_broker_info(&broker_info);

  EXPECT_TRUE(ETCPAL_UUID_IS_NULL(&broker_info.cid));
  EXPECT_EQ(broker_info.service_name[0], '\0');
  EXPECT_EQ(broker_info.port, 0u);
  EXPECT_EQ(broker_info.listen_addr_list, nullptr);
  EXPECT_EQ(broker_info.scope, std::string(E133_DEFAULT_SCOPE));
  EXPECT_EQ(broker_info.model[0], '\0');
  EXPECT_EQ(broker_info.manufacturer[0], '\0');
}

// None of the public API functions should return an "Ok" error code if rdmnet_core_init() has not
// been called.
TEST_F(TestDiscoveryCommon, DoesntWorkIfNotInitialized)
{
  rdmnet_core_initialized_fake.return_val = false;

  rdmnet_scope_monitor_t monitor_handle;
  int platform_err;
  EXPECT_EQ(rdmnet_disc_start_monitoring(&default_monitor_config_, &monitor_handle, &platform_err), kEtcPalErrNotInit);

  rdmnet_registered_broker_t broker_handle;
  EXPECT_EQ(rdmnet_disc_register_broker(&default_register_config_, &broker_handle), kEtcPalErrNotInit);
}

TEST_F(TestDiscoveryCommon, StartMonitoringWorksWithNormalArgs)
{
  rdmnet_scope_monitor_t monitor_handle;
  int platform_err;

  EXPECT_EQ(rdmnet_disc_start_monitoring(&default_monitor_config_, &monitor_handle, &platform_err), kEtcPalErrOk);
  EXPECT_EQ(rdmnet_disc_platform_start_monitoring_fake.call_count, 1u);
}

TEST_F(TestDiscoveryCommon, BrokerRegisterSucceedsUnderNormalConditions)
{
  rdmnet_registered_broker_t broker_handle;
  ASSERT_EQ(rdmnet_disc_register_broker(&default_register_config_, &broker_handle), kEtcPalErrOk);

  // Make sure broker is not registered before query timeout expires
  rdmnet_disc_tick();
  ASSERT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 0u);

  // Advance time past the query timeout
  etcpal_getms_fake.return_val += BROKER_REG_QUERY_TIMEOUT + 1000;
  rdmnet_disc_tick();

  EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 1u);
  EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.arg1_val, broker_handle);
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
  DiscoveredBroker* db = discovered_broker_new("Other Test Broker", "Other Test Broker._rdmnet._tcp.local.");
  discovered_broker_insert(&broker_handle->scope_monitor_handle->broker_list, db);

  rdmnet_disc_tick();

  // Advance time past the query timeout
  etcpal_getms_fake.return_val += BROKER_REG_QUERY_TIMEOUT + 100;
  rdmnet_disc_tick();

  // Make sure the broker has not been registered
  EXPECT_EQ(rdmnet_disc_platform_register_broker_fake.call_count, 0u);
}

TEST_F(TestDiscoveryCommon, DeinitUnmonitorsScope)
{
  rdmnet_scope_monitor_t monitor_handle;
  int platform_error;
  ASSERT_EQ(rdmnet_disc_start_monitoring(&default_monitor_config_, &monitor_handle, &platform_error), kEtcPalErrOk);
  ASSERT_EQ(rdmnet_disc_platform_start_monitoring_fake.call_count, 1u);

  rdmnet_disc_deinit();
  EXPECT_EQ(rdmnet_disc_platform_stop_monitoring_fake.call_count, 1u);

  // Stop the destructor from doing a double deinit
  deinitted_during_test_ = true;
}

TEST_F(TestDiscoveryCommon, DeinitUnregistersBrokerIfRegistered)
{
  RegisterBroker();

  rdmnet_disc_deinit();
  EXPECT_EQ(rdmnet_disc_platform_unregister_broker_fake.call_count, 1u);

  // Stop the destructor from doing a double deinit
  deinitted_during_test_ = true;
}
