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

// Test the rdmnet/private/msg_buf.c module

#include <vector>
#include <cstring>

#include "gtest/gtest.h"
#include "rdmnet/private/msg_buf.h"
#include "test_msg_buf_input_data.h"

class TestMsgBuf : public testing::Test
{
protected:
  TestMsgBuf() { rdmnet_msg_buf_init(&buf_); }

  RdmnetMsgBuf buf_;
};

// Test parsing a fully-formed RPT Notification PDU
TEST_F(TestMsgBuf, RptNotificationFull)
{
  ASSERT_EQ(kEtcPalErrOk,
            rdmnet_msg_buf_recv(&buf_, RptNotificationPduFullValid::buf, sizeof(RptNotificationPduFullValid::buf)));

  // Test each field of the parsed message
  RdmnetMessage& msg = buf_.msg;
  ASSERT_EQ(msg.vector, RptNotificationPduFullValid::root_vector);
  ASSERT_EQ(0, ETCPAL_UUID_CMP(&msg.sender_cid, &RptNotificationPduFullValid::sender_cid));

  RptMessage* rpt = get_rpt_msg(&msg);
  ASSERT_EQ(rpt->vector, RptNotificationPduFullValid::rpt_vector);
  ASSERT_TRUE(RDM_UID_EQUAL(&rpt->header.source_uid, &RptNotificationPduFullValid::rpt_src_uid));
  ASSERT_EQ(rpt->header.source_endpoint_id, RptNotificationPduFullValid::rpt_src_endpoint);
  ASSERT_TRUE(RDM_UID_EQUAL(&rpt->header.dest_uid, &RptNotificationPduFullValid::rpt_dest_uid));
  ASSERT_EQ(rpt->header.dest_endpoint_id, RptNotificationPduFullValid::rpt_dest_endpoint);
  ASSERT_EQ(rpt->header.seqnum, RptNotificationPduFullValid::seq_num);

  RdmBufList* buf_list = get_rdm_buf_list(rpt);
  ASSERT_FALSE(buf_list->more_coming);

  RdmBufListEntry* entry = buf_list->list;
  ASSERT_NE(entry->next, nullptr);
  ASSERT_EQ(entry->msg.datalen, RptNotificationPduFullValid::first_cmd.datalen);
  ASSERT_EQ(0, memcmp(entry->msg.data, RptNotificationPduFullValid::first_cmd.data,
                      RptNotificationPduFullValid::first_cmd.datalen));

  entry = entry->next;
  ASSERT_EQ(nullptr, entry->next);
  ASSERT_EQ(entry->msg.datalen, RptNotificationPduFullValid::second_cmd.datalen);
  ASSERT_EQ(0, memcmp(entry->msg.data, RptNotificationPduFullValid::second_cmd.data,
                      RptNotificationPduFullValid::second_cmd.datalen));
}
