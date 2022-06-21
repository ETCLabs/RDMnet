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

// Test the rdmnet/private/msg_buf.c module
// The msg_buf module is the TCP stream parser that parses the RDMnet TCP-based protocols: Broker,
// RPT and EPT. This module works using a small testing library which deserializes a set of golden
// master RDMnet protocol messages which live in tests/data/messages and validates them.

// This test suite makes use of GoogleTest's Value-Parameterized Tests functionality:
// https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#value-parameterized-tests

#include <cassert>
#include <cstring>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "etcpal_mock/common.h"
#include "etcpal_mock/socket.h"
#include "gtest/gtest.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/msg_buf.h"
#include "test_file_manifest.h"
#include "load_test_data.h"
#include "test_data_util.h"

static constexpr size_t kNumRandomIterationsPerMessage = 10;
static constexpr size_t kNumChunksPerMessage = 5;

// If a test fails on a certain file and set of random chunks, you can reproduce the test by
// changing this to 1, and adding the file name and chunk set in the map below.
#define DEBUGGING_TEST_FAILURE 0

#if DEBUGGING_TEST_FAILURE
// clang-format off
const std::unordered_map<std::string, const std::vector<size_t>> kFixedChunkSizes = {
   std::make_pair("C:/git/ETCLabs/RDMnet/tests/data/messages/rpt_connected_client_list.data.txt",
                  std::vector<size_t>({1, 7, 85, 23, 66}))
};
// clang-format on
#endif

// This test fixture is run on each file in the data file manifest; see tests/data
class TestMsgBufParsing : public testing::Test, public testing::WithParamInterface<DataValidationPair>
{
protected:
  TestMsgBufParsing() { rc_msg_buf_init(&buf_); }

  std::vector<std::vector<uint8_t>> DivideIntoRandomChunks(const std::vector<uint8_t>& original, size_t num_chunks);
  std::vector<std::vector<uint8_t>> DivideIntoFixedChunks(const std::vector<uint8_t>& original,
                                                          const std::vector<size_t>&  chunk_sizes);

  std::random_device         dev_;
  std::default_random_engine rng_{dev_()};
  RCMsgBuf                   buf_;
};

// Divide a vector of uint8_t into num_chunks randomly-sized chunks.
std::vector<std::vector<uint8_t>> TestMsgBufParsing::DivideIntoRandomChunks(const std::vector<uint8_t>& original,
                                                                            size_t                      num_chunks)
{
  assert(num_chunks > 0);
  assert(original.size() >= num_chunks);

  std::uniform_int_distribution<> dist(1, static_cast<int>(original.size() - 1));
  std::set<int>                   breakpoints;

  // Generate a set of indexes at which to divide the vector, using a while loop because we might
  // get duplicates
  while (breakpoints.size() < num_chunks - 1)
  {
    breakpoints.insert(dist(rng_));
  }

  std::vector<std::vector<uint8_t>> result;
  result.reserve(num_chunks);

  // Divide the vector between each pair of breakpoints, with the first chunk being between the
  // beginning and the first breakpoint, and the last chunk being between the last breakpoint and
  // the end.
  int  prev_breakpoint = 0;
  auto chunk_begin_iter = original.begin();
  for (auto breakpoint : breakpoints)
  {
    auto chunk_end_iter = chunk_begin_iter + (breakpoint - prev_breakpoint);
    result.push_back(std::vector<uint8_t>(chunk_begin_iter, chunk_end_iter));
    prev_breakpoint = breakpoint;
    chunk_begin_iter = chunk_end_iter;
  }
  result.push_back(std::vector<uint8_t>(chunk_begin_iter, original.end()));
  return result;
}

// Divide a vector of uint8_t into fixed-size chunks specified by chunk_sizes. For debugging failed
// tests only. Very little error checking.
std::vector<std::vector<uint8_t>> TestMsgBufParsing::DivideIntoFixedChunks(const std::vector<uint8_t>& original,
                                                                           const std::vector<size_t>&  chunk_sizes)
{
  std::vector<std::vector<uint8_t>> result;
  result.reserve(chunk_sizes.size());

  auto   begin_iter = original.begin();
  size_t chunks_index = 0;
  while (begin_iter != original.end())
  {
    result.push_back(std::vector<uint8_t>(begin_iter, begin_iter + chunk_sizes[chunks_index]));
    begin_iter += chunk_sizes[chunks_index];
    ++chunks_index;
  }
  return result;
}

// Test parsing the message as one full chunk.
TEST_P(TestMsgBufParsing, ParseMessageInFull)
{
  SCOPED_TRACE(std::string{"While testing input file: "} + GetParam().first);

  std::ifstream test_data_file(GetParam().first);
  auto          test_data = rdmnet::testing::LoadTestData(test_data_file);
  ASSERT_LE(test_data.size(), static_cast<size_t>(RDMNET_RECV_DATA_MAX_SIZE));

  std::memcpy(&buf_.buf[buf_.cur_data_size], test_data.data(), test_data.size());
  buf_.cur_data_size += test_data.size();
  ASSERT_EQ(kEtcPalErrOk, rc_msg_buf_parse_data(&buf_));
  ExpectMessagesEqual(buf_.msg, GetParam().second);
  rc_free_message_resources(&buf_.msg);
}

// Test parsing the message after dividing it into a number of randomly-sized chunks and simulating
// receiving each chunk at discrete times. This simulates the byte-stream nature of TCP. The number
// of chunks is controlled by kNumChunksPerMessage, and this test case re-divides the message
// randomly and iterates a number of times controlled by kNumRandomIterationsPerMessage.
TEST_P(TestMsgBufParsing, ParseMessageInRandomChunks)
{
  SCOPED_TRACE(std::string{"While testing input file: "} + GetParam().first);

  std::ifstream test_data_file(GetParam().first);
  auto          test_data = rdmnet::testing::LoadTestData(test_data_file);

#if !DEBUGGING_TEST_FAILURE
  for (size_t i = 0; i < kNumRandomIterationsPerMessage; ++i)
  {
    SCOPED_TRACE(std::string{"On random chunk iteration "} + std::to_string(i));
#endif

#if DEBUGGING_TEST_FAILURE
    std::vector<std::vector<uint8_t>> chunks;
    auto                              fixed_chunks = kFixedChunkSizes.find(GetParam()->first);
    if (fixed_chunks != kFixedChunkSizes.end())
      chunks = DivideIntoFixedChunks(test_data, fixed_chunks->second);
    else
      chunks = DivideIntoRandomChunks(test_data, kNumChunksPerMessage);
#else
  auto chunks = DivideIntoRandomChunks(test_data, kNumChunksPerMessage);
#endif

    // Assemble some test debugging output and error checking around the chunks.
    std::string error_msg = "Total message length: " + std::to_string(test_data.size()) + "\nRandom chunk sizes: {";
    size_t      chunk_sum = 0;
    for (const auto& chunk : chunks)
    {
      error_msg += std::to_string(chunk.size()) + ' ';
      chunk_sum += chunk.size();
    }
    error_msg.erase(error_msg.length() - 1);  // Trim the last space
    error_msg += '}';
    ASSERT_EQ(chunk_sum, test_data.size()) << "Uh oh, looks like the test has a bug!";
    SCOPED_TRACE("While testing input data:\n" + error_msg);

    // Do the chunked parsing
    for (size_t j = 0; j < kNumChunksPerMessage - 1; ++j)
    {
      std::memcpy(&buf_.buf[buf_.cur_data_size], chunks[j].data(), chunks[j].size());
      buf_.cur_data_size += chunks[j].size();
      ASSERT_EQ(kEtcPalErrNoData, rc_msg_buf_parse_data(&buf_))
          << "While parsing chunk " << j + 1 << " of " << kNumChunksPerMessage;
    }
    std::memcpy(&buf_.buf[buf_.cur_data_size], chunks.back().data(), chunks.back().size());
    buf_.cur_data_size += chunks.back().size();
    ASSERT_EQ(kEtcPalErrOk, rc_msg_buf_parse_data(&buf_))
        << "While parsing chunk " << kNumChunksPerMessage << " of " << kNumChunksPerMessage;

    // Validate the parse result
    SCOPED_TRACE("While validating RdmnetMessage parsed from rc_msg_buf_recv() using ExpectMessagesEqual()");
    ExpectMessagesEqual(buf_.msg, GetParam().second);
    rc_free_message_resources(&buf_.msg);
#if !DEBUGGING_TEST_FAILURE
  }
#endif
}

INSTANTIATE_TEST_SUITE_P(TestValidInputData, TestMsgBufParsing, testing::ValuesIn(kRdmnetTestDataFiles));

class TestMsgBufReceiving : public testing::Test
{
protected:
  static constexpr uint8_t     kTestRecvData[] = {0xD, 0xE, 0xA, 0xD, 0xB, 0xE, 0xE, 0xF};
  static constexpr size_t      kTestRecvDataSize = sizeof(kTestRecvData);
  static constexpr size_t      kRecvBufMaxSize = RC_MSG_BUF_SIZE;
  static const etcpal_socket_t kTestSocket = static_cast<etcpal_socket_t>(0);

  TestMsgBufReceiving()
  {
    etcpal_reset_all_fakes();
    rc_msg_buf_init(&buf_);
  }

  RCMsgBuf buf_;
};

TEST_F(TestMsgBufReceiving, ReceivesOneByteAtATime)
{
  static size_t num_bytes_received = 0u;
  etcpal_recv_fake.custom_fake = [](etcpal_socket_t, void* buffer, size_t, int) {
    if (num_bytes_received == kTestRecvDataSize)
      return static_cast<int>(kEtcPalErrWouldBlock);

    reinterpret_cast<uint8_t*>(buffer)[0] = kTestRecvData[num_bytes_received];
    ++num_bytes_received;
    return 1;
  };

  EXPECT_EQ(rc_msg_buf_recv(&buf_, kTestSocket), kEtcPalErrOk);
  EXPECT_EQ(memcmp(buf_.buf, kTestRecvData, kTestRecvDataSize), 0);
  EXPECT_EQ(buf_.cur_data_size, kTestRecvDataSize);
  EXPECT_EQ(etcpal_recv_fake.call_count, kTestRecvDataSize + 1u);
}

TEST_F(TestMsgBufReceiving, ReceivesTwoBytesAtATime)
{
  static size_t num_bytes_received = 0u;
  etcpal_recv_fake.custom_fake = [](etcpal_socket_t, void* buffer, size_t, int) {
    if (num_bytes_received == kTestRecvDataSize)
      return static_cast<int>(kEtcPalErrWouldBlock);

    reinterpret_cast<uint8_t*>(buffer)[0] = kTestRecvData[num_bytes_received];
    reinterpret_cast<uint8_t*>(buffer)[1] = kTestRecvData[num_bytes_received + 1];
    num_bytes_received += 2;
    return 2;
  };

  EXPECT_EQ(rc_msg_buf_recv(&buf_, kTestSocket), kEtcPalErrOk);
  EXPECT_EQ(memcmp(buf_.buf, kTestRecvData, kTestRecvDataSize), 0);
  EXPECT_EQ(buf_.cur_data_size, kTestRecvDataSize);
  EXPECT_EQ(etcpal_recv_fake.call_count, (kTestRecvDataSize / 2u) + 1u);
}

TEST_F(TestMsgBufReceiving, ReceivesZeroBytes)
{
  etcpal_recv_fake.return_val = kEtcPalErrWouldBlock;
  EXPECT_EQ(rc_msg_buf_recv(&buf_, kTestSocket), kEtcPalErrWouldBlock);
  EXPECT_EQ(etcpal_recv_fake.call_count, 1u);
}

TEST_F(TestMsgBufReceiving, ReceivesUntilBufferIsFull)
{
  etcpal_recv_fake.return_val = 1;
  EXPECT_EQ(rc_msg_buf_recv(&buf_, kTestSocket), kEtcPalErrOk);
  EXPECT_EQ(buf_.cur_data_size, kRecvBufMaxSize);
  EXPECT_EQ(etcpal_recv_fake.call_count, kRecvBufMaxSize);
}

TEST_F(TestMsgBufReceiving, AvoidsReceiveIfBufferAlreadyFull)
{
  etcpal_recv_fake.return_val = 1;
  buf_.cur_data_size = kRecvBufMaxSize;
  EXPECT_EQ(rc_msg_buf_recv(&buf_, kTestSocket), kEtcPalErrWouldBlock);
  EXPECT_EQ(buf_.cur_data_size, kRecvBufMaxSize);
  EXPECT_EQ(etcpal_recv_fake.call_count, 0u);
}
