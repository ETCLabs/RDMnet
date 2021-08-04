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

#include "mock_llrp_network.h"

#include <array>
#include <cstring>
#include "etcpal/pack.h"
#include "etcpal_mock/socket.h"
#include "etcpal_mock/timer.h"
#include "rdmnet/core/llrp_manager.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/defs.h"
#include "gtest/gtest.h"

std::array<uint8_t, 83> kProbeReplySkeleton = {
    // UDP preamble
    0x00, 0x10, 0x00, 0x00, 0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00,
    // Root layer PDU
    0xf0, 0x00, 0x43, 0x00, 0x00, 0x00, 0x0a,
    // Source CID placeholder
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // LLRP PDU
    0xf0, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x02,
    // Destination CID placeholder
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Transaction Number Placeholder
    0x00, 0x00, 0x00, 0x00,
    // Probe Reply PDU
    0xf0, 0x00, 0x11, 0x01,
    // UID placeholder
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Hardware address
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // Component type (Non-RDMnet)
    0xff};
static constexpr size_t kRootVectorOffset = 19u;
static constexpr size_t kSourceCidOffset = 23u;
static constexpr size_t kLlrpVectorOffset = 42u;
static constexpr size_t kDestinationCidOffset = 46u;
static constexpr size_t kTransactionNumberOffset = 62u;
static constexpr size_t kUidOffset = 70u;
static constexpr size_t kProbeRequestUpperUidOffset = 76u;

static std::array<uint8_t, 83> GetProbeReply(const etcpal::Uuid& source_cid,
                                             const etcpal::Uuid& dest_cid,
                                             uint32_t            transaction_num,
                                             const rdm::Uid&     uid)
{
  std::array<uint8_t, 83> probe_reply_data = kProbeReplySkeleton;

  std::memcpy(&probe_reply_data[kSourceCidOffset], source_cid.data(), 16);
  std::memcpy(&probe_reply_data[kDestinationCidOffset], dest_cid.data(), 16);
  etcpal_pack_u32b(&probe_reply_data[kTransactionNumberOffset], transaction_num);
  etcpal_pack_u16b(&probe_reply_data[kUidOffset], uid.manufacturer_id());
  etcpal_pack_u32b(&probe_reply_data[kUidOffset + 2], uid.device_id());
  return probe_reply_data;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// MockLlrpTarget
///////////////////////////////////////////////////////////////////////////////////////////////////

MockLlrpTarget::MockLlrpTarget(const rdm::Uid& uid_, const etcpal::Uuid& cid_) : uid(uid_), cid(cid_)
{
}

void MockLlrpTarget::HandleProbeRequest(const LlrpHeader&           header,
                                        int                         current_time_ms,
                                        std::default_random_engine& engine)
{
  // If we get here, it's assumed that we meet the probe request requirements.
  if (!pending_probe_reply_)
  {
    int response_time_ms = current_time_ms + std::uniform_int_distribution<int>(0, 1500)(engine);
    pending_probe_reply_ = PendingProbeReply{response_time_ms, header.sender_cid, header.transaction_number};
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// MockLlrpNetwork
///////////////////////////////////////////////////////////////////////////////////////////////////

void MockLlrpNetwork::AdvanceTimeAndTick(int time_to_advance_ms)
{
  etcpal_getms_fake.return_val += time_to_advance_ms;
  elapsed_time_ms_ += time_to_advance_ms;

  // See if we have any probe replies to send
  for (MockLlrpTarget& target : targets_)
  {
    auto pending_probe_reply = target.pending_probe_reply();
    if (pending_probe_reply && pending_probe_reply->response_time_ms < elapsed_time_ms_)
    {
      auto probe_reply_data = GetProbeReply(target.cid, pending_probe_reply->controller_cid,
                                            pending_probe_reply->transaction_num, target.uid);
      rc_llrp_manager_data_received(probe_reply_data.data(), probe_reply_data.size(), &netint_);
      target.ResetPendingProbeReply();
    }
  }

  rc_llrp_manager_module_tick();
}

void MockLlrpNetwork::AddTarget(uint16_t manu_id, uint32_t device_id, const etcpal::Uuid& cid)
{
  AddTarget(rdm::Uid(manu_id, device_id), cid);
}

void MockLlrpNetwork::AddTarget(const rdm::Uid& uid, const etcpal::Uuid& cid)
{
  targets_.push_back({uid, cid});
}

void MockLlrpNetwork::HandleMessageSent(const uint8_t* message, size_t length, const etcpal::SockAddr& dest_addr)
{
  if (dest_addr.IsV4())
  {
    EXPECT_EQ(dest_addr.v4_data(), 0xeffffa85);
  }
  EXPECT_EQ(dest_addr.port(), 5569);

  // Must check this manually here, because we want to increment the number of probe requests even
  // if there are no targets.
  ASSERT_GT(length, kLlrpVectorOffset + 4);
  if (etcpal_unpack_u32b(&message[kRootVectorOffset]) == ACN_VECTOR_ROOT_LLRP &&
      etcpal_unpack_u32b(&message[kLlrpVectorOffset]) == VECTOR_LLRP_PROBE_REQUEST)
  {
    ++probe_requests_;
  }

  if (dont_respond_count_ > 0)
  {
    --dont_respond_count_;
    return;
  }

  int num_targets_responded = 0;

  LlrpMessageInterest message_interest{};
  message_interest.interested_in_probe_request = true;

  for (MockLlrpTarget& target : targets_)
  {
    message_interest.my_cid = target.cid.get();
    message_interest.my_uid = target.uid.get();

    LlrpMessage msg;
    ASSERT_TRUE(rc_parse_llrp_message(message, length, &message_interest, &msg));

    switch (msg.vector)
    {
      case VECTOR_LLRP_PROBE_REQUEST:
        if (LLRP_MSG_GET_PROBE_REQUEST(&msg)->contains_my_uid)
        {
          bool should_respond = true;

          // Lossiness algorithm: At least kMinimumTargetsToRespond targets always respond to a
          // probe request. Each target above that number has a N% chance to respond, where N is
          // equal to 100 minus the lossiness factor.
          //
          // If lossiness is >= 100, only kMinimumTargetsToRespond targets will ever respond to a
          // probe request. If lossiness <= 0, all targets will always respond to a probe request.
          if (num_targets_responded >= kMinimumTargetsToRespond)
          {
            if (std::uniform_int_distribution<int>(1, 100)(rand_engine_) <= lossiness_)
              should_respond = false;
          }

          if (should_respond)
          {
            // ASSERT_TRUE(LLRP_MSG_GET_PROBE_REQUEST(&msg)->contains_my_uid);
            target.HandleProbeRequest(msg.header, elapsed_time_ms_, rand_engine_);
            ++num_targets_responded;
          }
        }
        break;
      default:
        FAIL() << "Received LLRP message with unknown vector " << msg.vector;
        break;
    }
  }

  if (num_targets_responded == 0)
    ++consec_clean_probe_requests_;
  else
    consec_clean_probe_requests_ = 0;
}
