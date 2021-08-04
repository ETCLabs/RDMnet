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

#include "rdmnet/core/llrp_manager.h"

#include <cstdint>
#include <functional>
#include <random>
#include <set>
#include "gtest/gtest.h"
#include "fff.h"
#include "etcpal/cpp/mutex.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "rdm/cpp/uid.h"
#include "rdmnet_mock/core/common.h"
#include "rdmnet/core/mcast.h"
#include "rdmnet/core/opts.h"
#include "fake_mcast.h"
#include "mock_llrp_network.h"

extern "C" {
FAKE_VOID_FUNC(managercb_target_discovered, RCLlrpManager*, const LlrpDiscoveredTarget*);
FAKE_VOID_FUNC(managercb_rdm_response_received, RCLlrpManager*, const LlrpRdmResponse*);
FAKE_VOID_FUNC(managercb_discovery_finished, RCLlrpManager*);
FAKE_VOID_FUNC(managercb_destroyed, RCLlrpManager*);
}

class TestLlrpManager;
static TestLlrpManager* test_instance{nullptr};

class TestLlrpManager : public testing::Test
{
public:
  // Callback interface from the C library which allows unit tests to easily add capture state to
  // lambdas that are assigned to these variables.
  std::function<void(RCLlrpManager*, const LlrpDiscoveredTarget*)> target_discovered_cb;
  std::function<void(RCLlrpManager*, const LlrpRdmResponse*)>      rdm_response_received_cb;
  std::function<void(RCLlrpManager*)>                              discovery_finished_cb;
  std::function<void(RCLlrpManager*)>                              destroyed_cb;

  MockLlrpNetwork llrp_network;

protected:
  RCLlrpManager              manager_;
  etcpal::Mutex              manager_lock_;
  std::random_device         r_;
  std::default_random_engine rand_engine_{r_()};

  void SetUp() override
  {
    test_instance = this;

    RESET_FAKE(managercb_target_discovered);
    RESET_FAKE(managercb_rdm_response_received);
    RESET_FAKE(managercb_discovery_finished);
    RESET_FAKE(managercb_destroyed);

    rdmnet_mock_core_reset_and_init();
    etcpal_reset_all_fakes();
    SetUpFakeMcastEnvironment();

    HookFakes();

    manager_.cid = etcpal::Uuid::FromString("48eaee88-2d5e-43d4-b0e9-7a9d5977ae9d").get();
    manager_.uid = rdm::Uid::FromString("e574:a686dee7").get();
    manager_.netint.index = 1;
    manager_.netint.ip_type = kEtcPalIpTypeV4;
    manager_.callbacks.target_discovered = managercb_target_discovered;
    manager_.callbacks.rdm_response_received = managercb_rdm_response_received;
    manager_.callbacks.discovery_finished = managercb_discovery_finished;
    manager_.callbacks.destroyed = managercb_destroyed;
    manager_.lock = &manager_lock_.get();

    ASSERT_EQ(kEtcPalErrOk, rc_llrp_module_init());
    ASSERT_EQ(kEtcPalErrOk, rc_llrp_manager_module_init());
    ASSERT_EQ(kEtcPalErrOk, rc_llrp_manager_register(&manager_));

    llrp_network.SetNetint(manager_.netint);
  }

  void TearDown() override
  {
    rc_llrp_manager_unregister(&manager_);
    rc_llrp_manager_module_deinit();
    rc_llrp_module_deinit();

    test_instance = nullptr;
  }

  void HookFakes()
  {
    // Hook fakes
    managercb_target_discovered_fake.custom_fake = [](RCLlrpManager* manager, const LlrpDiscoveredTarget* target) {
      if (test_instance->target_discovered_cb)
        test_instance->target_discovered_cb(manager, target);
    };
    managercb_rdm_response_received_fake.custom_fake = [](RCLlrpManager* manager, const LlrpRdmResponse* response) {
      if (test_instance->rdm_response_received_cb)
        test_instance->rdm_response_received_cb(manager, response);
    };
    managercb_discovery_finished_fake.custom_fake = [](RCLlrpManager* manager) {
      if (test_instance->discovery_finished_cb)
        test_instance->discovery_finished_cb(manager);
    };
    managercb_destroyed_fake.custom_fake = [](RCLlrpManager* manager) {
      if (test_instance->destroyed_cb)
        test_instance->destroyed_cb(manager);
    };

    etcpal_sendto_fake.custom_fake = [](etcpal_socket_t, const void* message, size_t length, int,
                                        const EtcPalSockAddr* dest_addr) {
      test_instance->llrp_network.HandleMessageSent(reinterpret_cast<const uint8_t*>(message), length, *dest_addr);
      return static_cast<int>(length);
    };
  }

  void TestDiscovering1000Responders(const std::set<rdm::Uid> responder_uids, int lossiness)
  {
    llrp_network.SetLossiness(lossiness);
    for (const rdm::Uid& responder_uid : responder_uids)
      llrp_network.AddTarget(responder_uid);

    std::set<rdm::Uid> responders_discovered;
    target_discovered_cb = [&](RCLlrpManager*, const LlrpDiscoveredTarget* target) {
      ASSERT_TRUE(target != nullptr);
      responders_discovered.insert(target->uid);
    };
    discovery_finished_cb = [&](RCLlrpManager*) { EXPECT_GE(llrp_network.elapsed_time_ms(), 8000); };

    rc_llrp_manager_start_discovery(&manager_, 0);

    while (managercb_discovery_finished_fake.call_count == 0)
      llrp_network.AdvanceTimeAndTick();

    // Heuristic: To discover 1000 responders, we should need at least 5 sub-ranges with 3 probe
    // requests each, plus an initial full-range probe request
    EXPECT_GE(llrp_network.num_probe_requests_received(), 16);
    EXPECT_GE(llrp_network.num_consecutive_clean_probe_requests(), 3);
    EXPECT_EQ(responder_uids.size(), responders_discovered.size());  // redundant with next call, but makes better logs
    EXPECT_EQ(responder_uids, responders_discovered);
    EXPECT_EQ(managercb_target_discovered_fake.call_count, 1000u);
    EXPECT_EQ(managercb_discovery_finished_fake.call_count, 1u);
  }
};

TEST_F(TestLlrpManager, DestroyedCalledOnUnregister)
{
  rc_llrp_manager_unregister(&manager_);
  rc_llrp_manager_module_tick();

  EXPECT_EQ(managercb_destroyed_fake.call_count, 1u);
  EXPECT_EQ(managercb_destroyed_fake.arg0_val, &manager_);
}

TEST_F(TestLlrpManager, SendsThreeTimesWhenNoTargetPresent)
{
  discovery_finished_cb = [&](RCLlrpManager*) { EXPECT_GE(llrp_network.elapsed_time_ms(), 6000); };

  rc_llrp_manager_start_discovery(&manager_, 0);

  // Tick forward 65 * 100ms = 6.5 seconds (3x LLRP_TIMEOUT plus some extra padding).
  for (int i = 0; i < 65; ++i)
    llrp_network.AdvanceTimeAndTick();

  EXPECT_EQ(llrp_network.num_probe_requests_received(), 3);
  EXPECT_EQ(llrp_network.num_consecutive_clean_probe_requests(), 3);
  EXPECT_EQ(managercb_target_discovered_fake.call_count, 0u);
  EXPECT_EQ(managercb_discovery_finished_fake.call_count, 1u);
}

TEST_F(TestLlrpManager, DiscoversSingleResponder)
{
  rdm::Uid responder_uid{0x6574, 0x12345678};
  llrp_network.AddTarget(responder_uid);

  target_discovered_cb = [&](RCLlrpManager*, const LlrpDiscoveredTarget* target) {
    ASSERT_TRUE(target != nullptr);
    EXPECT_EQ(target->uid, responder_uid);
  };
  discovery_finished_cb = [&](RCLlrpManager*) { EXPECT_GE(llrp_network.elapsed_time_ms(), 8000); };

  rc_llrp_manager_start_discovery(&manager_, 0);

  // Tick forward 85 * 100ms = 8.5 seconds (4x LLRP_TIMEOUT plus some extra padding).
  for (int i = 0; i < 85; ++i)
    llrp_network.AdvanceTimeAndTick();

  EXPECT_EQ(llrp_network.num_probe_requests_received(), 4);
  EXPECT_EQ(llrp_network.num_consecutive_clean_probe_requests(), 3);
  EXPECT_EQ(managercb_target_discovered_fake.call_count, 1u);
  EXPECT_EQ(managercb_discovery_finished_fake.call_count, 1u);
}

TEST_F(TestLlrpManager, DiscoversResponderThatDoesntRespondAtFirst)
{
  rdm::Uid responder_uid{0x6574, 0x12345678};
  llrp_network.AddTarget(responder_uid);

  target_discovered_cb = [&](RCLlrpManager*, const LlrpDiscoveredTarget* target) {
    ASSERT_TRUE(target != nullptr);
    EXPECT_EQ(target->uid, responder_uid);
  };
  discovery_finished_cb = [&](RCLlrpManager*) { EXPECT_GE(llrp_network.elapsed_time_ms(), 12000); };

  llrp_network.DontRespondToProbeRequests(2);
  rc_llrp_manager_start_discovery(&manager_, 0);

  // Tick forward 130 * 100ms = 13 seconds (6x LLRP_TIMEOUT plus some extra padding).
  for (int i = 0; i < 130; ++i)
    llrp_network.AdvanceTimeAndTick();

  EXPECT_EQ(llrp_network.num_probe_requests_received(), 6);
  EXPECT_EQ(llrp_network.num_consecutive_clean_probe_requests(), 3);
  EXPECT_EQ(managercb_target_discovered_fake.call_count, 1u);
  EXPECT_EQ(managercb_discovery_finished_fake.call_count, 1u);
}

class TestLlrpManagerAtScale : public TestLlrpManager, public testing::WithParamInterface<int>
{
};

TEST_P(TestLlrpManagerAtScale, Discovers1000DistributedResponders)
{
  std::set<rdm::Uid>                      responders;
  std::uniform_int_distribution<uint32_t> device_id_distribution(0);
  std::uniform_int_distribution<uint16_t> manu_id_distribution(0x1, 0x7fff);

  // Populate responders with UIDs distributed across the manufacturer and device ID range
  while (responders.size() < 1000)
  {
    responders.insert({manu_id_distribution(rand_engine_), device_id_distribution(rand_engine_)});
  }

  TestDiscovering1000Responders(responders, GetParam());
}

TEST_P(TestLlrpManagerAtScale, Discovers1000EtcResponders)
{
  std::set<rdm::Uid>                      responders;
  std::uniform_int_distribution<uint32_t> device_id_distribution(0);

  // Populate responders with UIDs containing ETC's dynamic manufacturer id and device IDs
  // distributed across the range
  while (responders.size() < 1000)
  {
    responders.insert({0x6574, device_id_distribution(rand_engine_)});
  }

  TestDiscovering1000Responders(responders, GetParam());
}

INSTANTIATE_TEST_SUITE_P(DefaultLossinessValues,
                         TestLlrpManagerAtScale,
                         testing::Values(0, 20, 40, 60, 80, 90),
                         [](const testing::TestParamInfo<TestLlrpManagerAtScale::ParamType>& info) {
                           return "Lossiness" + std::to_string(info.param);
                         });
