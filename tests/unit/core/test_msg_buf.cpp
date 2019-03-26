/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

// Test the rdmnet/private/msg_buf.c module

#include <vector>
#include <cstring>

#include "gtest/gtest.h"
#include "rdmnet/private/msg_buf.h"
#include "lwpa_mock/socket.h"
#include "test_msg_buf_input_data.h"

DEFINE_FFF_GLOBALS;

class TestMsgBuf : public testing::Test
{
protected:
  TestMsgBuf()
  {
    LWPA_SOCKET_DO_FOR_ALL_FAKES(RESET_FAKE);

    rdmnet_msg_buf_init(&buf_, NULL);
  }

  RdmnetMsgBuf buf_;
};

// Test parsing a fully-formed RPT Notification PDU
TEST_F(TestMsgBuf, rpt_notification_full)
{
  lwpa_socket_t socket_handle = 1;

  lwpa_recv_fake.custom_fake = [](lwpa_socket_t, void *buffer, size_t, int) -> int {
    memcpy((uint8_t *)buffer, RptNotificationPduFullValid::buf, sizeof(RptNotificationPduFullValid::buf));
    return sizeof(RptNotificationPduFullValid::buf);
  };
  ASSERT_EQ(LWPA_OK, rdmnet_msg_buf_recv(socket_handle, &buf_));

  // Test each field of the parsed message
  RdmnetMessage &msg = buf_.msg;
  ASSERT_EQ(msg.vector, RptNotificationPduFullValid::root_vector);
  ASSERT_EQ(0, lwpa_uuid_cmp(&msg.sender_cid, &RptNotificationPduFullValid::sender_cid));

  RptMessage *rpt = get_rpt_msg(&msg);
  ASSERT_EQ(rpt->vector, RptNotificationPduFullValid::rpt_vector);
  ASSERT_TRUE(rdm_uid_equal(&rpt->header.source_uid, &RptNotificationPduFullValid::rpt_src_uid));
  ASSERT_EQ(rpt->header.source_endpoint_id, RptNotificationPduFullValid::rpt_src_endpoint);
  ASSERT_TRUE(rdm_uid_equal(&rpt->header.dest_uid, &RptNotificationPduFullValid::rpt_dest_uid));
  ASSERT_EQ(rpt->header.dest_endpoint_id, RptNotificationPduFullValid::rpt_dest_endpoint);
  ASSERT_EQ(rpt->header.seqnum, RptNotificationPduFullValid::seq_num);

  RdmCmdList *cmd_list = get_rdm_cmd_list(rpt);
  ASSERT_FALSE(cmd_list->partial);

  RdmCmdListEntry *entry = cmd_list->list;
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