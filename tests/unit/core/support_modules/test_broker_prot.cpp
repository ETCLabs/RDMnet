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

#include "rdmnet/core/broker_prot.h"

#include <cstring>
#include "gtest/gtest.h"
#include "etcpal/cpp/uuid.h"

TEST(TestBrokerProt, MessageIdentMacrosWork)
{
  BrokerMessage bmsg;

  // Verify the test and get macros for each Broker message type.

  bmsg.vector = VECTOR_BROKER_CONNECT;
  EXPECT_TRUE(BROKER_IS_CLIENT_CONNECT_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_CONNECT_REPLY;
  EXPECT_TRUE(BROKER_IS_CONNECT_REPLY_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ENTRY_UPDATE;
  EXPECT_TRUE(BROKER_IS_CLIENT_ENTRY_UPDATE_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_REDIRECT_V4;
  EXPECT_TRUE(BROKER_IS_CLIENT_REDIRECT_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_REDIRECT_V6;
  EXPECT_TRUE(BROKER_IS_CLIENT_REDIRECT_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_CONNECTED_CLIENT_LIST;
  EXPECT_TRUE(BROKER_IS_CLIENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ADD;
  EXPECT_TRUE(BROKER_IS_CLIENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_REMOVE;
  EXPECT_TRUE(BROKER_IS_CLIENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ENTRY_CHANGE;
  EXPECT_TRUE(BROKER_IS_CLIENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_REQUEST_DYNAMIC_UIDS;
  EXPECT_TRUE(BROKER_IS_REQUEST_DYNAMIC_UID_ASSIGNMENT(&bmsg));
  bmsg.vector = VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS;
  EXPECT_TRUE(BROKER_IS_DYNAMIC_UID_ASSIGNMENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST;
  EXPECT_TRUE(BROKER_IS_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_DISCONNECT;
  EXPECT_TRUE(BROKER_IS_DISCONNECT_MSG(&bmsg));

  EXPECT_EQ(BROKER_GET_CLIENT_CONNECT_MSG(&bmsg), &bmsg.data.client_connect);
  EXPECT_EQ(BROKER_GET_CONNECT_REPLY_MSG(&bmsg), &bmsg.data.connect_reply);
  EXPECT_EQ(BROKER_GET_CLIENT_ENTRY_UPDATE_MSG(&bmsg), &bmsg.data.client_entry_update);
  EXPECT_EQ(BROKER_GET_CLIENT_REDIRECT_MSG(&bmsg), &bmsg.data.client_redirect);
  EXPECT_EQ(BROKER_GET_CLIENT_LIST(&bmsg), &bmsg.data.client_list);
  EXPECT_EQ(BROKER_GET_DYNAMIC_UID_REQUEST_LIST(&bmsg), &bmsg.data.dynamic_uid_request_list);
  EXPECT_EQ(BROKER_GET_DYNAMIC_UID_ASSIGNMENT_LIST(&bmsg), &bmsg.data.dynamic_uid_assignment_list);
  EXPECT_EQ(BROKER_GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(&bmsg), &bmsg.data.fetch_uid_assignment_list);
  EXPECT_EQ(BROKER_GET_DISCONNECT_MSG(&bmsg), &bmsg.data.disconnect);
}

TEST(TestBrokerProt, MessageStringMacrosWork)
{
  BrokerClientConnectMsg ccmsg;

  // Set default scope
  BROKER_CLIENT_CONNECT_MSG_SET_DEFAULT_SCOPE(&ccmsg);
  EXPECT_STREQ(ccmsg.scope, E133_DEFAULT_SCOPE);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(E133_DEFAULT_SCOPE); i < E133_SCOPE_STRING_PADDED_LENGTH; ++i)
  {
    EXPECT_EQ(ccmsg.scope[i], '\0');
  }

  // Set custom scope within length requirements
  constexpr char test_scope[] = u8"照明让我感觉很好";
  BROKER_CLIENT_CONNECT_MSG_SET_SCOPE(&ccmsg, test_scope);
  EXPECT_STREQ(ccmsg.scope, test_scope);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(test_scope); i < E133_SCOPE_STRING_PADDED_LENGTH; ++i)
  {
    EXPECT_EQ(ccmsg.scope[i], '\0');
  }

  // Set custom scope outside length requirements
  constexpr char scope_too_long[] = "longlonglonglonglonglonglonglonglonglonglonglonglonglonglonglon";
  constexpr char scope_truncated[] = "longlonglonglonglonglonglonglonglonglonglonglonglonglonglonglo";
  BROKER_CLIENT_CONNECT_MSG_SET_SCOPE(&ccmsg, scope_too_long);
  EXPECT_STREQ(ccmsg.scope, scope_truncated);

  // Set default search domain
  BROKER_CLIENT_CONNECT_MSG_SET_DEFAULT_SEARCH_DOMAIN(&ccmsg);
  EXPECT_STREQ(ccmsg.search_domain, E133_DEFAULT_DOMAIN);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(E133_DEFAULT_DOMAIN); i < E133_DOMAIN_STRING_PADDED_LENGTH; ++i)
  {
    EXPECT_EQ(ccmsg.search_domain[i], '\0');
  }

  // Set custom search domain within length requirements
  constexpr char test_domain[] = "test.pepperoni.pizza.";
  BROKER_CLIENT_CONNECT_MSG_SET_SEARCH_DOMAIN(&ccmsg, test_domain);
  EXPECT_STREQ(ccmsg.search_domain, test_domain);
  for (size_t i = strlen(test_domain); i < E133_DOMAIN_STRING_PADDED_LENGTH; ++i)
  {
    EXPECT_EQ(ccmsg.search_domain[i], '\0');
  }

  // Set custom search domain outside length requirements
  constexpr char domain_too_long[] =
      "this.is.a.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very."
      "very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very."
      "long.domain.";
  constexpr char domain_truncated[] =
      "this.is.a.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very."
      "very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very.very."
      "long.domai";
  BROKER_CLIENT_CONNECT_MSG_SET_SEARCH_DOMAIN(&ccmsg, domain_too_long);
  EXPECT_STREQ(ccmsg.search_domain, domain_truncated);
}

TEST(TestBrokerProt, PackBrokerDisconnectWorks)
{
  const uint8_t kCorrectDisconnectMsg[] = {
      // TCP preamble
      0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00,  // ACN packet identifier
      0x00, 0x00, 0x00, 0x1e,                                                  // PDU block size
      // Root Layer PDU
      0xf0, 0x00, 0x1e,        // Flags and Length
      0x00, 0x00, 0x00, 0x09,  // VECTOR_ROOT_BROKER
      0x9e, 0xfb, 0x97, 0x13, 0x2b, 0x82, 0x41, 0x21, 0x8a, 0xe0, 0x9c, 0xa0, 0x45, 0x08, 0x6f, 0xe6,  // Sender CID
      // Broker PDU
      0xf0, 0x00, 0x07,  // Flags & Length
      0x00, 0x0e,        // VECTOR_BROKER_DISCONNECT
      0x00, 0x04,        // DISCONNECT_SOFTWARE_RESET
  };
  uint8_t buf[BROKER_DISCONNECT_FULL_MSG_SIZE];

  BrokerDisconnectMsg msg_data;
  msg_data.disconnect_reason = kRdmnetDisconnectSoftwareReset;
  size_t size =
      rc_broker_pack_disconnect(buf, BROKER_DISCONNECT_FULL_MSG_SIZE,
                                &etcpal::Uuid::FromString("9efb9713-2b82-4121-8ae0-9ca045086fe6").get(), &msg_data);
  ASSERT_EQ(size, sizeof(kCorrectDisconnectMsg));
  EXPECT_EQ(std::memcmp(buf, kCorrectDisconnectMsg, sizeof(kCorrectDisconnectMsg)), 0);
}
