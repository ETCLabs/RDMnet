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

#include "monitored_scope.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include "rdmnet/core/util.h"
#include "gtest/gtest.h"
#include "test_disc_common_fakes.h"
#include "test_operators.h"

class TestMonitoredScope : public testing::Test
{
protected:
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

  ScopeMonitorUniquePtr MakeDefaultMonitoredScope()
  {
    return ScopeMonitorUniquePtr(scope_monitor_new(&default_config_));
  }

  RdmnetScopeMonitorConfig default_config_{};
};

TEST_F(TestMonitoredScope, NewInitializesFieldsProperly)
{
  auto ref = MakeDefaultMonitoredScope();

  ASSERT_NE(ref, nullptr);

  EXPECT_EQ(ref->config, default_config_);
  EXPECT_EQ(ref->broker_handle, nullptr);
  EXPECT_EQ(ref->broker_list, nullptr);
  EXPECT_EQ(ref->next, nullptr);
}

TEST_F(TestMonitoredScope, InsertWorks)
{
  auto scope_1 = MakeDefaultMonitoredScope();
  rdmnet_safe_strncpy(scope_1->config.scope, "Test Insert 1 Scope", E133_SCOPE_STRING_PADDED_LENGTH);
  scope_monitor_insert(scope_1.get());

  // We test the presence by using the for_each function.
  EXPECT_TRUE(scope_monitor_ref_is_valid(scope_1.get()));
  scope_monitor_for_each([](RdmnetScopeMonitorRef* ref) { EXPECT_STREQ(ref->config.scope, "Test Insert 1 Scope"); });

  auto scope_2 = MakeDefaultMonitoredScope();
  rdmnet_safe_strncpy(scope_2->config.scope, "Test Insert 2 Scope", E133_SCOPE_STRING_PADDED_LENGTH);
  scope_monitor_insert(scope_2.get());

  EXPECT_TRUE(scope_monitor_ref_is_valid(scope_1.get()));
  EXPECT_TRUE(scope_monitor_ref_is_valid(scope_2.get()));
}

// clang-format off
// These need to be at file scope because we are using stateless lambdas as C function pointers
static std::pair<std::string, bool> scope_names[] = {
  {"Test Scope 1", false},
  {"Test Scope 2", false},
  {"Test Scope 3", false},
  {"Test Scope 4", false}
};
// clang-format on

TEST_F(TestMonitoredScope, ForEachWorks)
{
  std::array<ScopeMonitorUniquePtr, std::size(scope_names)> scopes;
  // Insert each name into the list.
  for (size_t i = 0; i < std::size(scope_names); ++i)
  {
    scope_names[i].second = false;
    scopes[i] = MakeDefaultMonitoredScope();
    rdmnet_safe_strncpy(scopes[i]->config.scope, scope_names[i].first.c_str(), E133_SCOPE_STRING_PADDED_LENGTH);
    scope_monitor_insert(scopes[i].get());
  }

  // Flag each name as we hit it from the for_each function.
  scope_monitor_for_each([](RdmnetScopeMonitorRef* ref) {
    for (auto& name : scope_names)
    {
      if (0 == std::strncmp(ref->config.scope, name.first.c_str(), E133_SCOPE_STRING_PADDED_LENGTH))
        name.second = true;
    }
  });

  // Make sure we've hit each of the names.
  for (const auto& name : scope_names)
    EXPECT_EQ(name.second, true);
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
