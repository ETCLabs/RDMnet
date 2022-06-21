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

#include "rdmnet/core/rpt_prot.h"

#include <algorithm>
#include <memory>
#include "etcpal_mock/socket.h"
#include "rdmnet_mock/core/common.h"
#include "gtest/gtest.h"
#include "test_data_util.h"
#include "load_test_data.h"

void TestPackStatus(const std::string& file_name)
{
  RdmnetMessage        msg;
  std::vector<uint8_t> msg_bytes;
  ASSERT_TRUE(GetTestFileByBasename(file_name, msg_bytes, msg));

  RptStatusMsg* status = RPT_GET_STATUS_MSG(RDMNET_GET_RPT_MSG(&msg));
  EXPECT_EQ(rc_rpt_get_status_buffer_size(status), msg_bytes.size());

  auto buf = std::make_unique<uint8_t[]>(msg_bytes.size());
  EXPECT_EQ(rc_rpt_pack_status(buf.get(), msg_bytes.size(), &msg.sender_cid, &RDMNET_GET_RPT_MSG(&msg)->header, status),
            msg_bytes.size());
  EXPECT_TRUE(std::equal(msg_bytes.begin(), msg_bytes.end(), buf.get()));
}

void TestSendStatus(const std::string& file_name)
{
  RdmnetMessage        msg;
  std::vector<uint8_t> msg_bytes;
  ASSERT_TRUE(GetTestFileByBasename(file_name, msg_bytes, msg));

  RptStatusMsg* status = RPT_GET_STATUS_MSG(RDMNET_GET_RPT_MSG(&msg));
  EXPECT_EQ(rc_rpt_get_status_buffer_size(status), msg_bytes.size());

  static std::vector<uint8_t> packed_msg;
  packed_msg.clear();

  RESET_FAKE(rc_send);
  rc_send_fake.custom_fake = [](etcpal_socket_t, const void* msg, size_t length, int) {
    const uint8_t* msg_bytes = reinterpret_cast<const uint8_t*>(msg);
    std::transform(msg_bytes, msg_bytes + length, std::back_inserter(packed_msg),
                   [](const uint8_t& byte) { return byte; });
    return (int)length;
  };
  RCConnection conn{};
  EXPECT_EQ(rc_rpt_send_status(&conn, &msg.sender_cid, &RDMNET_GET_RPT_MSG(&msg)->header, status), kEtcPalErrOk);
  EXPECT_EQ(msg_bytes, packed_msg);
}

TEST(TestRptProt, PackRptStatusWithoutString)
{
  TestPackStatus("rpt_status_no_string");
}

TEST(TestRptProt, PackRptStatusStringAbsent)
{
  TestPackStatus("rpt_status_string_absent");
}

TEST(TestRptProt, PackRptStatusMidLengthString)
{
  TestPackStatus("rpt_status_mid_length_string");
}

TEST(TestRptProt, PackRptStatusMaxLengthString)
{
  TestPackStatus("rpt_status_max_length_string");
}

TEST(TestRptProt, SendRptStatusWithoutString)
{
  TestSendStatus("rpt_status_no_string");
}

TEST(TestRptProt, SendRptStatusStringAbsent)
{
  TestSendStatus("rpt_status_string_absent");
}

TEST(TestRptProt, SendRptStatusMidLengthString)
{
  TestSendStatus("rpt_status_mid_length_string");
}

TEST(TestRptProt, SendRptStatusMaxLengthString)
{
  TestSendStatus("rpt_status_max_length_string");
}
