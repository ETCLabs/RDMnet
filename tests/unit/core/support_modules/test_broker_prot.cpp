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
#include "gtest/gtest.h"
#include "rdmnet/core/broker_prot.h"

class TestBrokerProt : public ::testing::Test
{
};

TEST_F(TestBrokerProt, MessageIdentMacrosWork)
{
  BrokerMessage bmsg;

  // Verify the test and get macros for each Broker message type.

  bmsg.vector = VECTOR_BROKER_CONNECT;
  ASSERT_TRUE(IS_CLIENT_CONNECT_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_CONNECT_REPLY;
  ASSERT_TRUE(IS_CONNECT_REPLY_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ENTRY_UPDATE;
  ASSERT_TRUE(IS_CLIENT_ENTRY_UPDATE_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_REDIRECT_V4;
  ASSERT_TRUE(IS_CLIENT_REDIRECT_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_REDIRECT_V6;
  ASSERT_TRUE(IS_CLIENT_REDIRECT_MSG(&bmsg));
  bmsg.vector = VECTOR_BROKER_CONNECTED_CLIENT_LIST;
  ASSERT_TRUE(IS_CLIENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ADD;
  ASSERT_TRUE(IS_CLIENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_REMOVE;
  ASSERT_TRUE(IS_CLIENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ENTRY_CHANGE;
  ASSERT_TRUE(IS_CLIENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_REQUEST_DYNAMIC_UIDS;
  ASSERT_TRUE(IS_REQUEST_DYNAMIC_UID_ASSIGNMENT(&bmsg));
  bmsg.vector = VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS;
  ASSERT_TRUE(IS_DYNAMIC_UID_ASSIGNMENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST;
  ASSERT_TRUE(IS_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(&bmsg));
  bmsg.vector = VECTOR_BROKER_DISCONNECT;
  ASSERT_TRUE(IS_DISCONNECT_MSG(&bmsg));

  ASSERT_EQ(GET_CLIENT_CONNECT_MSG(&bmsg), &bmsg.data.client_connect);
  ASSERT_EQ(GET_CONNECT_REPLY_MSG(&bmsg), &bmsg.data.connect_reply);
  ASSERT_EQ(GET_CLIENT_ENTRY_UPDATE_MSG(&bmsg), &bmsg.data.client_entry_update);
  ASSERT_EQ(GET_CLIENT_REDIRECT_MSG(&bmsg), &bmsg.data.client_redirect);
  ASSERT_EQ(GET_CLIENT_LIST(&bmsg), &bmsg.data.client_list);
  ASSERT_EQ(GET_DYNAMIC_UID_REQUEST_LIST(&bmsg), &bmsg.data.dynamic_uid_request_list);
  ASSERT_EQ(GET_DYNAMIC_UID_ASSIGNMENT_LIST(&bmsg), &bmsg.data.dynamic_uid_assignment_list);
  ASSERT_EQ(GET_FETCH_DYNAMIC_UID_ASSIGNMENT_LIST(&bmsg), &bmsg.data.fetch_uid_assignment_list);
  ASSERT_EQ(GET_DISCONNECT_MSG(&bmsg), &bmsg.data.disconnect);
}

TEST_F(TestBrokerProt, MessageStringMacrosWork)
{
  ClientConnectMsg ccmsg;

  // Set default scope
  CLIENT_CONNECT_MSG_SET_DEFAULT_SCOPE(&ccmsg);
  ASSERT_STREQ(ccmsg.scope, E133_DEFAULT_SCOPE);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(E133_DEFAULT_SCOPE); i < E133_SCOPE_STRING_PADDED_LENGTH; ++i)
  {
    ASSERT_EQ(ccmsg.scope[i], '\0');
  }

  // Set custom scope within length requirements
  constexpr char test_scope[] = u8"照明让我感觉很好";
  CLIENT_CONNECT_MSG_SET_SCOPE(&ccmsg, test_scope);
  ASSERT_STREQ(ccmsg.scope, test_scope);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(test_scope); i < E133_SCOPE_STRING_PADDED_LENGTH; ++i)
  {
    ASSERT_EQ(ccmsg.scope[i], '\0');
  }

  // Set custom scope outside length requirements
  constexpr char scope_too_long[] = "longlonglonglonglonglonglonglonglonglonglonglonglonglonglonglon";
  constexpr char scope_truncated[] = "longlonglonglonglonglonglonglonglonglonglonglonglonglonglonglo";
  CLIENT_CONNECT_MSG_SET_SCOPE(&ccmsg, scope_too_long);
  ASSERT_STREQ(ccmsg.scope, scope_truncated);

  // Set default search domain
  CLIENT_CONNECT_MSG_SET_DEFAULT_SEARCH_DOMAIN(&ccmsg);
  ASSERT_STREQ(ccmsg.search_domain, E133_DEFAULT_DOMAIN);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(E133_DEFAULT_DOMAIN); i < E133_DOMAIN_STRING_PADDED_LENGTH; ++i)
  {
    ASSERT_EQ(ccmsg.search_domain[i], '\0');
  }

  // Set custom search domain within length requirements
  constexpr char test_domain[] = "test.pepperoni.pizza.";
  CLIENT_CONNECT_MSG_SET_SEARCH_DOMAIN(&ccmsg, test_domain);
  ASSERT_STREQ(ccmsg.search_domain, test_domain);
  for (size_t i = strlen(test_domain); i < E133_DOMAIN_STRING_PADDED_LENGTH; ++i)
  {
    ASSERT_EQ(ccmsg.search_domain[i], '\0');
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
  CLIENT_CONNECT_MSG_SET_SEARCH_DOMAIN(&ccmsg, domain_too_long);
  ASSERT_STREQ(ccmsg.search_domain, domain_truncated);
}

TEST_F(TestBrokerProt, CodeToStringFunctionsWork)
{
  EXPECT_TRUE(rdmnet_connect_status_to_string(kRdmnetConnectOk) != nullptr);
  EXPECT_TRUE(rdmnet_connect_status_to_string(static_cast<rdmnet_connect_status_t>(INT_MAX)) == nullptr);
  EXPECT_TRUE(rdmnet_connect_status_to_string(static_cast<rdmnet_connect_status_t>(-1)) == nullptr);

  EXPECT_TRUE(rdmnet_disconnect_reason_to_string(kRdmnetDisconnectShutdown) != nullptr);
  EXPECT_TRUE(rdmnet_disconnect_reason_to_string(static_cast<rdmnet_disconnect_reason_t>(INT_MAX)) == nullptr);
  EXPECT_TRUE(rdmnet_disconnect_reason_to_string(static_cast<rdmnet_disconnect_reason_t>(-1)) == nullptr);

  EXPECT_TRUE(rdmnet_dynamic_uid_status_to_string(kDynamicUidStatusOk) != nullptr);
  EXPECT_TRUE(rdmnet_dynamic_uid_status_to_string(static_cast<dynamic_uid_status_t>(INT_MAX)) == nullptr);
  EXPECT_TRUE(rdmnet_dynamic_uid_status_to_string(static_cast<dynamic_uid_status_t>(-1)) == nullptr);
}
