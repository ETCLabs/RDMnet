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
  ASSERT_TRUE(is_client_connect_msg(&bmsg));
  bmsg.vector = VECTOR_BROKER_CONNECT_REPLY;
  ASSERT_TRUE(is_connect_reply_msg(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ENTRY_UPDATE;
  ASSERT_TRUE(is_client_entry_update_msg(&bmsg));
  bmsg.vector = VECTOR_BROKER_REDIRECT_V4;
  ASSERT_TRUE(is_client_redirect_msg(&bmsg));
  bmsg.vector = VECTOR_BROKER_REDIRECT_V6;
  ASSERT_TRUE(is_client_redirect_msg(&bmsg));
  bmsg.vector = VECTOR_BROKER_CONNECTED_CLIENT_LIST;
  ASSERT_TRUE(is_client_list(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ADD;
  ASSERT_TRUE(is_client_list(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_REMOVE;
  ASSERT_TRUE(is_client_list(&bmsg));
  bmsg.vector = VECTOR_BROKER_CLIENT_ENTRY_CHANGE;
  ASSERT_TRUE(is_client_list(&bmsg));
  bmsg.vector = VECTOR_BROKER_REQUEST_DYNAMIC_UIDS;
  ASSERT_TRUE(is_request_dynamic_uid_assignment(&bmsg));
  bmsg.vector = VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS;
  ASSERT_TRUE(is_dynamic_uid_assignment_list(&bmsg));
  bmsg.vector = VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST;
  ASSERT_TRUE(is_fetch_dynamic_uid_assignment_list(&bmsg));
  bmsg.vector = VECTOR_BROKER_DISCONNECT;
  ASSERT_TRUE(is_disconnect_msg(&bmsg));

  ASSERT_EQ(get_client_connect_msg(&bmsg), &bmsg.data.client_connect);
  ASSERT_EQ(get_connect_reply_msg(&bmsg), &bmsg.data.connect_reply);
  ASSERT_EQ(get_client_entry_update_msg(&bmsg), &bmsg.data.client_entry_update);
  ASSERT_EQ(get_client_redirect_msg(&bmsg), &bmsg.data.client_redirect);
  ASSERT_EQ(get_client_list(&bmsg), &bmsg.data.client_list);
  ASSERT_EQ(get_dynamic_uid_request_list(&bmsg), &bmsg.data.dynamic_uid_request_list);
  ASSERT_EQ(get_dynamic_uid_assignment_list(&bmsg), &bmsg.data.dynamic_uid_assignment_list);
  ASSERT_EQ(get_fetch_dynamic_uid_assignment_list(&bmsg), &bmsg.data.fetch_uid_assignment_list);
  ASSERT_EQ(get_disconnect_msg(&bmsg), &bmsg.data.disconnect);
}

TEST_F(TestBrokerProt, MessageStringMacrosWork)
{
  ClientConnectMsg ccmsg;

  // Set default scope
  client_connect_msg_set_default_scope(&ccmsg);
  ASSERT_STREQ(ccmsg.scope, E133_DEFAULT_SCOPE);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(E133_DEFAULT_SCOPE); i < E133_SCOPE_STRING_PADDED_LENGTH; ++i)
  {
    ASSERT_EQ(ccmsg.scope[i], '\0');
  }

  // Set custom scope within length requirements
  constexpr char test_scope[] = u8"照明让我感觉很好";
  client_connect_msg_set_scope(&ccmsg, test_scope);
  ASSERT_STREQ(ccmsg.scope, test_scope);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(test_scope); i < E133_SCOPE_STRING_PADDED_LENGTH; ++i)
  {
    ASSERT_EQ(ccmsg.scope[i], '\0');
  }

  // Set custom scope outside length requirements
  constexpr char scope_too_long[] = "longlonglonglonglonglonglonglonglonglonglonglonglonglonglonglon";
  constexpr char scope_truncated[] = "longlonglonglonglonglonglonglonglonglonglonglonglonglonglonglo";
  client_connect_msg_set_scope(&ccmsg, scope_too_long);
  ASSERT_STREQ(ccmsg.scope, scope_truncated);

  // Set default search domain
  client_connect_msg_set_default_search_domain(&ccmsg);
  ASSERT_STREQ(ccmsg.search_domain, E133_DEFAULT_DOMAIN);
  // We should be null-padded to the full length of the array.
  for (size_t i = strlen(E133_DEFAULT_DOMAIN); i < E133_DOMAIN_STRING_PADDED_LENGTH; ++i)
  {
    ASSERT_EQ(ccmsg.search_domain[i], '\0');
  }

  // Set custom search domain within length requirements
  constexpr char test_domain[] = "test.pepperoni.pizza.";
  client_connect_msg_set_search_domain(&ccmsg, test_domain);
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
  client_connect_msg_set_search_domain(&ccmsg, domain_too_long);
  ASSERT_STREQ(ccmsg.search_domain, domain_truncated);
}
