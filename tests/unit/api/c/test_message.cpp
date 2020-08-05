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

#include "rdmnet/message.h"

#include <array>
#include <cstring>
#include <string>
#include "gtest/gtest.h"
#include "rdmnet_config.h"

void ExpectRdmCommandHeadersEqual(const RdmCommandHeader& header_a, const RdmCommandHeader& header_b)
{
  EXPECT_EQ(header_a.source_uid, header_b.source_uid);
  EXPECT_EQ(header_a.dest_uid, header_b.dest_uid);
  EXPECT_EQ(header_a.transaction_num, header_b.transaction_num);
  EXPECT_EQ(header_a.port_id, header_b.port_id);
  EXPECT_EQ(header_a.subdevice, header_b.subdevice);
  EXPECT_EQ(header_a.command_class, header_b.command_class);
  EXPECT_EQ(header_a.param_id, header_b.param_id);
}

void ExpectRdmResponseHeadersEqual(const RdmResponseHeader& header_a, const RdmResponseHeader& header_b)
{
  EXPECT_EQ(header_a.source_uid, header_b.source_uid);
  EXPECT_EQ(header_a.dest_uid, header_b.dest_uid);
  EXPECT_EQ(header_a.transaction_num, header_b.transaction_num);
  EXPECT_EQ(header_a.resp_type, header_b.resp_type);
  EXPECT_EQ(header_a.msg_count, header_b.msg_count);
  EXPECT_EQ(header_a.subdevice, header_b.subdevice);
  EXPECT_EQ(header_a.command_class, header_b.command_class);
  EXPECT_EQ(header_a.param_id, header_b.param_id);
}

TEST(TestMessageApi, SaveRdmCommandWorks)
{
  const std::array<uint8_t, 4> kTestData{0x00, 0x01, 0x02, 0x03};

  RdmnetRdmCommand cmd{
      {0x1234, 0x56789abc}, 1,
      0x12345678,           {{0x1234, 0x56789abc}, {0x4321, 0xcba98765}, 0x78, 1, 511, kRdmCCGetCommand, 0x8001},
      kTestData.data(),     static_cast<uint8_t>(kTestData.size())};

  RdmnetSavedRdmCommand saved_cmd{};
  ASSERT_EQ(rdmnet_save_rdm_command(&cmd, &saved_cmd), kEtcPalErrOk);

  EXPECT_EQ(saved_cmd.rdmnet_source_uid, cmd.rdmnet_source_uid);
  EXPECT_EQ(saved_cmd.dest_endpoint, cmd.dest_endpoint);
  EXPECT_EQ(saved_cmd.seq_num, cmd.seq_num);
  ExpectRdmCommandHeadersEqual(saved_cmd.rdm_header, cmd.rdm_header);
  ASSERT_EQ(saved_cmd.data_len, cmd.data_len);
  EXPECT_EQ(0, std::memcmp(saved_cmd.data, kTestData.data(), kTestData.size()));
}

TEST(TestMessageApi, SaveRdmResponseWorks)
{
  const std::array<uint8_t, 4> kTestCmdData{0x00, 0x01, 0x02, 0x03};
  const std::array<uint8_t, 8> kTestRespData{0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};

  RdmnetRdmResponse resp{
      {0x1234, 0x56789abc},
      1,
      0x12345678,
      true,
      {{0x1234, 0x56789abc}, {0x4321, 0xcba98765}, 0x78, 1, 511, kRdmCCGetCommand, 0x8001},
      kTestCmdData.data(),
      static_cast<uint8_t>(kTestCmdData.size()),
      {{0x4321, 0xcba98765}, {0x1234, 0x56789abc}, 0x78, kRdmResponseTypeAck, 3, 511, kRdmCCGetCommandResponse, 0x8001},
      kTestRespData.data(),
      kTestRespData.size(),
      false};

  RdmnetSavedRdmResponse saved_resp{};
#if RDMNET_DYNAMIC_MEM
  ASSERT_EQ(rdmnet_save_rdm_response(&resp, &saved_resp), kEtcPalErrOk);

  EXPECT_EQ(saved_resp.rdmnet_source_uid, resp.rdmnet_source_uid);
  EXPECT_EQ(saved_resp.source_endpoint, resp.source_endpoint);
  EXPECT_EQ(saved_resp.seq_num, resp.seq_num);
  EXPECT_EQ(saved_resp.is_response_to_me, resp.is_response_to_me);
  ExpectRdmCommandHeadersEqual(saved_resp.original_cmd_header, resp.original_cmd_header);
  ASSERT_EQ(saved_resp.original_cmd_data_len, resp.original_cmd_data_len);
  EXPECT_EQ(0, std::memcmp(saved_resp.original_cmd_data, kTestCmdData.data(), kTestCmdData.size()));
  ExpectRdmResponseHeadersEqual(saved_resp.rdm_header, resp.rdm_header);
  ASSERT_EQ(saved_resp.rdm_data_len, resp.rdm_data_len);
  EXPECT_EQ(0, std::memcmp(saved_resp.rdm_data, kTestRespData.data(), kTestRespData.size()));

  EXPECT_EQ(rdmnet_free_saved_rdm_response(&saved_resp), kEtcPalErrOk);
#else
  EXPECT_EQ(rdmnet_save_rdm_response(&resp, &saved_resp), kEtcPalErrNotImpl);
#endif
}

TEST(TestMessageApi, AppendToSavedRdmResponseWorks)
{
  // TODO
}

TEST(TestMessageApi, SaveRptStatusWorks)
{
  const std::string status_str = "Something has gone horribly wrong";

  RdmnetRptStatus status{{0x1234, 0x56789abc}, 1, 0x12345678, kRptStatusUnknownVector, status_str.c_str()};

  RdmnetSavedRptStatus saved_status{};
#if RDMNET_DYNAMIC_MEM
  ASSERT_EQ(rdmnet_save_rpt_status(&status, &saved_status), kEtcPalErrOk);

  EXPECT_EQ(saved_status.source_uid, status.source_uid);
  EXPECT_EQ(saved_status.source_endpoint, status.source_endpoint);
  EXPECT_EQ(saved_status.seq_num, status.seq_num);
  EXPECT_EQ(saved_status.status_code, status.status_code);
  EXPECT_STREQ(saved_status.status_string, status.status_string);

  EXPECT_EQ(rdmnet_free_saved_rpt_status(&saved_status), kEtcPalErrOk);
#else
  EXPECT_EQ(rdmnet_save_rpt_status(&status, &saved_status), kEtcPalErrNotImpl);
#endif
}

TEST(TestMessageApi, CopySavedRdmResponseWorks)
{
  // TODO
}

TEST(TestMessageApi, CopySavedRptStatusWorks)
{
  // TODO
}

TEST(TestMessageApi, SaveEptDataWorks)
{
  // TODO
}

TEST(TestMessageApi, SaveEptStatusWorks)
{
  // TODO
}

TEST(TestMessageApi, CopySavedEptDataWorks)
{
  // TODO
}

TEST(TestMessageApi, CopySavedEptStatusWorks)
{
  // TODO
}

TEST(TestMessageApi, SaveLlrpRdmCommandWorks)
{
  const std::array<uint8_t, 4> kTestData{0x00, 0x01, 0x02, 0x03};

  LlrpRdmCommand cmd{{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}},
                     0x12345678,
                     {kEtcPalIpTypeV4, 1},
                     {{0x1234, 0x56789abc}, {0x4321, 0xcba98765}, 0x78, 1, 511, kRdmCCGetCommand, 0x8001},
                     kTestData.data(),
                     static_cast<uint8_t>(kTestData.size())};

  LlrpSavedRdmCommand saved_cmd{};
  ASSERT_EQ(rdmnet_save_llrp_rdm_command(&cmd, &saved_cmd), kEtcPalErrOk);

  EXPECT_EQ(saved_cmd.source_cid, cmd.source_cid);
  EXPECT_EQ(saved_cmd.seq_num, cmd.seq_num);
  EXPECT_EQ(saved_cmd.netint_id.index, cmd.netint_id.index);
  EXPECT_EQ(saved_cmd.netint_id.ip_type, cmd.netint_id.ip_type);
  ExpectRdmCommandHeadersEqual(saved_cmd.rdm_header, cmd.rdm_header);
  ASSERT_EQ(saved_cmd.data_len, cmd.data_len);
  EXPECT_EQ(0, std::memcmp(saved_cmd.data, kTestData.data(), kTestData.size()));
}

TEST(TestMessageApi, SaveLlrpRdmResponseWorks)
{
  const std::array<uint8_t, 8> kTestRespData{0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};

  LlrpRdmResponse resp{
      {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}},
      0x12345678,
      {{0x4321, 0xcba98765}, {0x1234, 0x56789abc}, 0x78, kRdmResponseTypeAck, 3, 511, kRdmCCGetCommandResponse, 0x8001},
      kTestRespData.data(),
      static_cast<uint8_t>(kTestRespData.size())};

  LlrpSavedRdmResponse saved_resp{};
  ASSERT_EQ(rdmnet_save_llrp_rdm_response(&resp, &saved_resp), kEtcPalErrOk);

  EXPECT_EQ(saved_resp.source_cid, resp.source_cid);
  EXPECT_EQ(saved_resp.seq_num, resp.seq_num);
  ExpectRdmResponseHeadersEqual(saved_resp.rdm_header, resp.rdm_header);
  ASSERT_EQ(saved_resp.rdm_data_len, resp.rdm_data_len);
  EXPECT_EQ(0, std::memcmp(saved_resp.rdm_data, kTestRespData.data(), kTestRespData.size()));
}

TEST(TestMessageApi, CopySavedLlrpRdmResponseWorks)
{
  // TODO
}
