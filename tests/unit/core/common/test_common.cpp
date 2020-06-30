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

#include "rdmnet/core/common.h"

#include <array>
#include <random>
#include <string>
#include <vector>
#include "etcpal_mock/common.h"
#include "rdmnet_mock/core/client.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/llrp.h"
#include "rdmnet_mock/core/llrp_target.h"
#include "rdmnet_mock/core/llrp_manager.h"
#include "rdmnet_mock/disc/common.h"
#include "rdmnet_config.h"
#include "gtest/gtest.h"

struct ModuleFakeFunctionRef
{
  etcpal_error_t&       init_return_val;
  unsigned int&         init_call_count;
  unsigned int&         deinit_call_count;
  std::function<void()> reset_all_fakes{};
  const std::string     module_name;

  template <typename InitFake, typename DeinitFake>
  ModuleFakeFunctionRef(InitFake& init_fake, DeinitFake& deinit_fake, std::function<void()> reset_all, const char* name)
      : init_return_val(init_fake.return_val)
      , init_call_count(init_fake.call_count)
      , deinit_call_count(deinit_fake.call_count)
      , reset_all_fakes(reset_all)
      , module_name(name)
  {
  }
};

class TestCoreCommon : public testing::Test
{
protected:
  // clang-format off
  const std::vector<ModuleFakeFunctionRef> kModuleRefs =
  {
    ModuleFakeFunctionRef(etcpal_init_fake, etcpal_deinit_fake, etcpal_reset_all_fakes, "EtcPal"),
    ModuleFakeFunctionRef(rc_client_module_init_fake, rc_client_module_deinit_fake, rc_client_reset_all_fakes, "RC Client"),
    ModuleFakeFunctionRef(rc_conn_module_init_fake, rc_conn_module_deinit_fake, rc_connection_reset_all_fakes, "RC Connection"),
    ModuleFakeFunctionRef(rc_llrp_module_init_fake, rc_llrp_module_deinit_fake, rc_llrp_reset_all_fakes, "LLRP"),
    ModuleFakeFunctionRef(rc_llrp_manager_module_init_fake, rc_llrp_manager_module_deinit_fake, rc_llrp_manager_reset_all_fakes, "LLRP Manager"),
    ModuleFakeFunctionRef(rc_llrp_target_module_init_fake, rc_llrp_target_module_deinit_fake, rc_llrp_target_reset_all_fakes, "LLRP Target"),
    ModuleFakeFunctionRef(rdmnet_disc_module_init_fake, rdmnet_disc_module_deinit_fake, rdmnet_disc_common_reset_all_fakes, "Discovery"),
  };
  // clang-format on

  TestCoreCommon()
  {
    for (const auto& module_ref : kModuleRefs)
    {
      module_ref.reset_all_fakes();
    }
  }
};

TEST_F(TestCoreCommon, InitWorks)
{
  ASSERT_EQ(rc_init(nullptr, nullptr), kEtcPalErrOk);

  for (const auto& module_ref : kModuleRefs)
  {
#if RDMNET_DYNAMIC_MEM
    EXPECT_EQ(module_ref.init_call_count, 1u) << "Module: " << module_ref.module_name;
#else
    if (module_ref.module_name == "LLRP Manager")
      EXPECT_EQ(module_ref.init_call_count, 0u) << "Module: " << module_ref.module_name;
    else
      EXPECT_EQ(module_ref.init_call_count, 1u) << "Module: " << module_ref.module_name;
#endif
  }

  rc_deinit();
}

TEST_F(TestCoreCommon, DeinitWorks)
{
  ASSERT_EQ(rc_init(nullptr, nullptr), kEtcPalErrOk);

  rc_deinit();

  for (const auto& module_ref : kModuleRefs)
  {
#if RDMNET_DYNAMIC_MEM
    EXPECT_EQ(module_ref.deinit_call_count, 1u) << "Module: " << module_ref.module_name;
#else
    if (module_ref.module_name == "LLRP Manager")
      EXPECT_EQ(module_ref.deinit_call_count, 0u) << "Module: " << module_ref.module_name;
    else
      EXPECT_EQ(module_ref.deinit_call_count, 1u) << "Module: " << module_ref.module_name;
#endif
  }
}

TEST_F(TestCoreCommon, InitFailsGracefullyAndCleansUp)
{
  std::random_device                    rd;
  std::default_random_engine            random_gen(rd());
  std::uniform_int_distribution<size_t> distrib(0, kModuleRefs.size() - 1);

  auto fn_to_fail = distrib(random_gen);
  kModuleRefs[fn_to_fail].init_return_val = kEtcPalErrSys;

  ASSERT_EQ(rc_init(nullptr, nullptr), kEtcPalErrSys);

  for (size_t i = 0; i < kModuleRefs.size(); ++i)
  {
    const auto& ref = kModuleRefs[i];
    // Everything should be either cleaned up or not called, except the failing module, which
    // should have init() called but not deinit().
    if (i == fn_to_fail)
    {
      EXPECT_EQ(ref.init_call_count, 1u) << "Module: " << ref.module_name;
      EXPECT_EQ(ref.deinit_call_count, 0u) << "Module: " << ref.module_name;
    }
    else
    {
      EXPECT_EQ(ref.init_call_count, ref.deinit_call_count) << "Module: " << ref.module_name;
    }
  }
}
