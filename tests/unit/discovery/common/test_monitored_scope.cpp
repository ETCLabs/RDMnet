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

#include "rdmnet/disc/monitored_scope.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include "rdmnet/core/util.h"
#include "rdmnet/disc/discovered_broker.h"
#include "gtest/gtest.h"
#include "test_disc_common_fakes.h"
#include "test_operators.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

class TestMonitoredScope : public testing::Test
{
protected:
  static constexpr const char* kDefaultTestScope = "Test Scope";
  static constexpr const char* kDefaultTestDomain = "Test Domain";

  struct ScopeMonitorDeleter
  {
    void operator()(RdmnetScopeMonitorRef* ref)
    {
      scope_monitor_remove(ref);
      scope_monitor_delete(ref);
    }
  };
  using ScopeMonitorUniquePtr = std::unique_ptr<RdmnetScopeMonitorRef, ScopeMonitorDeleter>;

  TestMonitoredScope() { TestDiscoveryCommonResetAllFakes(); }

  ScopeMonitorUniquePtr MakeMonitoredScope(const RdmnetScopeMonitorConfig& config)
  {
    return ScopeMonitorUniquePtr(scope_monitor_new(&config));
  }

  ScopeMonitorUniquePtr MakeDefaultMonitoredScope() { return MakeMonitoredScope(default_config_); }

  RdmnetScopeMonitorConfig default_config_{};

  void SetUp() override
  {
    ASSERT_EQ(monitored_scope_module_init(), kEtcPalErrOk);
    ASSERT_EQ(discovered_broker_module_init(), kEtcPalErrOk);
    default_config_.scope = kDefaultTestScope;
    default_config_.domain = kDefaultTestDomain;
  }

  void TearDown() override { monitored_scope_module_deinit(); }
};

TEST_F(TestMonitoredScope, NewInitializesFieldsProperly)
{
  auto ref = MakeDefaultMonitoredScope();

  ASSERT_NE(ref, nullptr);

  EXPECT_STREQ(ref->scope, default_config_.scope);
  EXPECT_STREQ(ref->domain, default_config_.domain);
  EXPECT_EQ(ref->broker_handle, nullptr);
  EXPECT_EQ(ref->broker_list, nullptr);
}

TEST_F(TestMonitoredScope, InsertWorks)
{
  auto scope_1 = MakeDefaultMonitoredScope();
  scope_monitor_insert(scope_1.get());

  // We test the presence by using the for_each function.
  EXPECT_TRUE(scope_monitor_ref_is_valid(scope_1.get()));
  scope_monitor_for_each([](RdmnetScopeMonitorRef* ref) { EXPECT_STREQ(ref->scope, kDefaultTestScope); });

  RdmnetScopeMonitorConfig scope_2_config{};
  scope_2_config.scope = "Test Insert 2 Scope";
  scope_2_config.domain = "Test Insert 2 Domain";
  auto scope_2 = MakeMonitoredScope(scope_2_config);
  scope_monitor_insert(scope_2.get());

  EXPECT_TRUE(scope_monitor_ref_is_valid(scope_1.get()));
  EXPECT_TRUE(scope_monitor_ref_is_valid(scope_2.get()));
}

// clang-format off
// These need to be at file scope because we are using stateless lambdas as C function pointers
constexpr size_t kNumScopeNames = 4;
static std::pair<std::string, bool> scope_names[kNumScopeNames] = {
  {"Test Scope 1", false},
  {"Test Scope 2", false},
  {"Test Scope 3", false},
  {"Test Scope 4", false}
};
// clang-format on

TEST_F(TestMonitoredScope, ForEachWorks)
{
  std::array<ScopeMonitorUniquePtr, kNumScopeNames> scopes;
  // Insert each name into the list.
  for (size_t i = 0; i < kNumScopeNames; ++i)
  {
    RdmnetScopeMonitorConfig config{};
    config.scope = scope_names[i].first.c_str();
    scope_names[i].second = false;
    scopes[i] = MakeMonitoredScope(config);
    scope_monitor_insert(scopes[i].get());
  }

  // Flag each name as we hit it from the for_each function.
  scope_monitor_for_each([](RdmnetScopeMonitorRef* ref) {
    for (auto& name : scope_names)
    {
      if (0 == std::strncmp(ref->scope, name.first.c_str(), E133_SCOPE_STRING_PADDED_LENGTH))
        name.second = true;
    }
  });

  // Make sure we've hit each of the names.
  for (const auto& name : scope_names)
    EXPECT_EQ(name.second, true);
}

TEST_F(TestMonitoredScope, FindWorks)
{
  // An array of RdmnetScopeMonitorRef pointers that will automatically remove and delete each one
  // on destruction.
#if RDMNET_DYNAMIC_MEM
  constexpr size_t kNumScopes = 10;
#else
  constexpr size_t kNumScopes = RDMNET_MAX_MONITORED_SCOPES;
#endif
  std::array<ScopeMonitorUniquePtr, kNumScopes> scopes;

  static const void* kContext = reinterpret_cast<const void*>(0x12345678);

  // Fill the array and linked list of DiscoveredScopes
  for (int i = 0; i < kNumScopes; ++i)
  {
    scopes[i] = MakeDefaultMonitoredScope();
    std::strcpy(scopes[i]->scope, std::string("Test Scope " + std::to_string(i)).c_str());
    scope_monitor_insert(scopes[i].get());
  }

  // Find the kNumScopes / 2 scope monitor instance by scope string using a predicate function.
  static const std::string scope_to_find("Test Scope " + std::to_string(kNumScopes / 2));
  auto                     found_scope = scope_monitor_find(
      [](const RdmnetScopeMonitorRef* ref, const void* context) {
        EXPECT_EQ(context, kContext);
        return ref->scope == scope_to_find;
      },
      kContext);
  ASSERT_NE(found_scope, nullptr);
  EXPECT_STREQ(found_scope->scope, scope_to_find.c_str());
}

TEST_F(TestMonitoredScope, FindScopeAndBrokerWorks)
{
  std::vector<ScopeMonitorUniquePtr> scopes;
  constexpr size_t                   kNumScopes = 3;
  constexpr size_t                   kNumBrokersPerScope = 5;

#if !RDMNET_DYNAMIC_MEM
  static_assert(RDMNET_MAX_MONITORED_SCOPES >= kNumScopes);
  static_assert(RDMNET_MAX_DISCOVERED_BROKERS_PER_SCOPE >= kNumBrokersPerScope);
#endif

  for (int i = 0; i < kNumScopes; ++i)
  {
    auto scope = MakeDefaultMonitoredScope();
    std::strcpy(scope->scope, std::string("Test Scope " + std::to_string(i)).c_str());
    for (int j = 0; j < kNumBrokersPerScope; ++j)
    {
      auto db = discovered_broker_new(
          scope.get(), std::string("Test Service Instance " + std::to_string(i * kNumBrokersPerScope + j)).c_str(), "");
      ASSERT_NE(db, nullptr);
      discovered_broker_insert(&scope->broker_list, db);
    }
    scope_monitor_insert(scope.get());
    scopes.push_back(std::move(scope));
  }

  static const void* kContext = reinterpret_cast<const void*>(0x12345678);

  static const std::string service_instance_to_find("Test Service Instance " + std::to_string(8));
  RdmnetScopeMonitorRef*   found_ref = nullptr;
  DiscoveredBroker*        found_db = nullptr;
  ASSERT_TRUE(scope_monitor_and_discovered_broker_find(
      [](const RdmnetScopeMonitorRef*, const DiscoveredBroker* db, const void* context) {
        EXPECT_EQ(context, kContext);
        return db->service_instance_name == service_instance_to_find;
      },
      kContext, &found_ref, &found_db));
  ASSERT_NE(found_ref, nullptr);
  ASSERT_NE(found_db, nullptr);
  EXPECT_STREQ(found_ref->scope, "Test Scope 1");
  EXPECT_STREQ(found_db->service_instance_name, "Test Service Instance 8");
}

// Needs to be at file scope because of C function pointers/stateless lambdas
static int remove_test_call_count = 0;

TEST_F(TestMonitoredScope, RemoveWorks)
{
  remove_test_call_count = 0;

  auto scope_1 = MakeDefaultMonitoredScope();
  scope_monitor_insert(scope_1.get());
  auto scope_2 = MakeDefaultMonitoredScope();
  scope_monitor_insert(scope_2.get());

  scope_monitor_remove(scope_2.get());

  // Only scope_1 should remain in the list
  EXPECT_TRUE(scope_monitor_ref_is_valid(scope_1.get()));
  EXPECT_FALSE(scope_monitor_ref_is_valid(scope_2.get()));
  scope_monitor_for_each([](RdmnetScopeMonitorRef*) { ++remove_test_call_count; });
  EXPECT_EQ(remove_test_call_count, 1);

  scope_monitor_remove(scope_1.get());

  // No brokers should remain in the list
  EXPECT_FALSE(scope_monitor_ref_is_valid(scope_1.get()));
  scope_monitor_for_each([](RdmnetScopeMonitorRef*) { FAIL(); });

  // Need to clean up the resources manually since they've already been removed
  scope_monitor_delete(scope_1.release());
  scope_monitor_delete(scope_2.release());
}

TEST_F(TestMonitoredScope, DeleteAllWorks)
{
  auto scope_1 = MakeDefaultMonitoredScope();
  scope_monitor_insert(scope_1.get());
  auto scope_2 = MakeDefaultMonitoredScope();
  scope_monitor_insert(scope_2.get());

  scope_monitor_delete_all();

  // No brokers should remain in the list
  EXPECT_FALSE(scope_monitor_ref_is_valid(scope_1.get()));
  EXPECT_FALSE(scope_monitor_ref_is_valid(scope_2.get()));
  scope_monitor_for_each([](RdmnetScopeMonitorRef*) { FAIL(); });

  scope_1.release();
  scope_2.release();
}
