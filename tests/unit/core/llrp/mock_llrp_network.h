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

#ifndef MOCK_LLRP_NETWORK_H_
#define MOCK_LLRP_NETWORK_H_

#include <cstdint>
#include <optional>
#include <random>
#include <set>
#include <vector>
#include "rdm/cpp/uid.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "rdmnet/core/llrp_prot.h"

struct PendingProbeReply
{
  int          response_time_ms;
  etcpal::Uuid controller_cid;
  uint32_t     transaction_num;
};
class MockLlrpTarget
{
public:
  MockLlrpTarget(const rdm::Uid& uid_, const etcpal::Uuid& cid_);

  void HandleProbeRequest(const LlrpHeader& header, int current_time_ms, std::default_random_engine& engine);

  std::optional<PendingProbeReply> pending_probe_reply() const { return pending_probe_reply_; }
  void                             ResetPendingProbeReply() { pending_probe_reply_ = std::nullopt; }

  rdm::Uid     uid;
  etcpal::Uuid cid;

private:
  // If present, a probe reply is pending, to be sent at the value of ms indicated
  std::optional<PendingProbeReply> pending_probe_reply_;
};

class MockLlrpNetwork
{
public:
  void AdvanceTimeAndTick(int time_to_advance_ms = 100);
  void AddTarget(uint16_t manu_id, uint32_t device_id, const etcpal::Uuid& cid = etcpal::Uuid::V4());
  void AddTarget(const rdm::Uid& uid, const etcpal::Uuid& cid = etcpal::Uuid::V4());
  void HandleMessageSent(const uint8_t* message, size_t length, const etcpal::SockAddr& dest_addr);

  void SetNetint(const EtcPalMcastNetintId& netint) { netint_ = netint; }
  void SetLossiness(int lossiness) { lossiness_ = lossiness; }
  void DontRespondToProbeRequests(int num_probe_requests) { dont_respond_count_ = num_probe_requests; }
  void SkipRangeCheck() { skip_range_check_ = true; }  // Simulate targets that don't comply with e1.33 s5.7.3
  int  num_probe_requests_received() const { return probe_requests_; }
  int  num_consecutive_clean_probe_requests() const { return consec_clean_probe_requests_; }
  int  elapsed_time_ms() const { return elapsed_time_ms_; }

private:
  static constexpr int kMinimumTargetsToRespond = 10;

  std::random_device         r_;
  std::default_random_engine rand_engine_{r_()};

  EtcPalMcastNetintId         netint_{};
  int                         lossiness_{0};
  int                         dont_respond_count_{0};
  bool                        skip_range_check_{false};
  int                         probe_requests_{0};
  int                         consec_clean_probe_requests_{0};
  int                         elapsed_time_ms_{0};
  std::vector<MockLlrpTarget> targets_;
};

#endif  // MOCK_LLRP_NETWORK_H_
