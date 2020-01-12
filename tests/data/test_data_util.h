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

#ifndef TEST_DATA_UTIL_H_
#define TEST_DATA_UTIL_H_

#include <string>
#include <cstring>
#include "rdmnet/core/message.h"
#include "gtest/gtest.h"

inline void ExpectRptClientEntriesEqual(const RptClientEntry& a, const RptClientEntry& b)
{
  EXPECT_EQ(a.cid, b.cid);
  EXPECT_EQ(a.uid, b.uid);
  EXPECT_EQ(a.type, b.type);
  EXPECT_EQ(a.binding_cid, b.binding_cid);
}

inline void ExpectEptClientEntriesEqual(const EptClientEntry& a, const EptClientEntry& b)
{
  EXPECT_EQ(a.cid, b.cid);
  EXPECT_EQ(a.more_coming, b.more_coming);
  EXPECT_EQ(a.num_protocols, b.num_protocols);
  if (a.num_protocols == b.num_protocols)
  {
    if (a.protocol_list && b.protocol_list)
    {
      for (size_t i = 0; i < a.num_protocols; ++i)
      {
        EXPECT_EQ(a.protocol_list[i].protocol_vector, b.protocol_list[i].protocol_vector)
            << "While comparing index " << i;
        EXPECT_STREQ(a.protocol_list[i].protocol_string, b.protocol_list[i].protocol_string)
            << "While comparing index " << i;
      }
    }
    else if (!a.protocol_list && b.protocol_list)
    {
      // No comparison to make
    }
    else
    {
      ADD_FAILURE() << "Null/not-null mismatch between protocol_list entries; a was " << a.protocol_list << ", b was "
                    << b.protocol_list;
    }
  }
}

inline void ExpectClientEntriesEqual(const ClientEntry& a, const ClientEntry& b)
{
  EXPECT_EQ(a.client_protocol, b.client_protocol);
  if (a.client_protocol == b.client_protocol)
  {
    switch (a.client_protocol)
    {
      case kClientProtocolRPT:
        ExpectRptClientEntriesEqual(a.data.rpt, b.data.rpt);
        break;
      case kClientProtocolEPT:
        ExpectEptClientEntriesEqual(a.data.ept, b.data.ept);
        break;
      default:
        ADD_FAILURE() << "Client Entries contained unknown client protocol " << a.client_protocol;
        break;
    }
  }
}

inline void ExpectMessagesEqual(const ClientConnectMsg& a, const ClientConnectMsg& b)
{
  EXPECT_STREQ(a.scope, b.scope);
  EXPECT_EQ(a.e133_version, b.e133_version);
  EXPECT_STREQ(a.search_domain, b.search_domain);
  EXPECT_EQ(a.connect_flags, b.connect_flags);
  ExpectClientEntriesEqual(a.client_entry, b.client_entry);
}

inline void ExpectMessagesEqual(const ConnectReplyMsg& a, const ConnectReplyMsg& b)
{
  EXPECT_EQ(a.connect_status, b.connect_status);
  EXPECT_EQ(a.e133_version, b.e133_version);
  EXPECT_EQ(a.broker_uid, b.broker_uid);
  EXPECT_EQ(a.client_uid, b.client_uid);
}

inline void ExpectMessagesEqual(const ClientEntryUpdateMsg& a, const ClientEntryUpdateMsg& b)
{
  EXPECT_EQ(a.connect_flags, b.connect_flags);
  ExpectClientEntriesEqual(a.client_entry, b.client_entry);
}

inline void ExpectMessagesEqual(const ClientRedirectMsg& a, const ClientRedirectMsg& b)
{
  EXPECT_EQ(a.new_addr, b.new_addr);
}

inline void ExpectMessagesEqual(const ClientList& a, const ClientList& b)
{
  EXPECT_EQ(a.client_protocol, b.client_protocol);
  if (a.client_protocol == b.client_protocol)
  {
    switch (a.client_protocol)
    {
      case kClientProtocolRPT:
        EXPECT_EQ(a.data.rpt.more_coming, b.data.rpt.more_coming);
        EXPECT_EQ(a.data.rpt.num_client_entries, b.data.rpt.num_client_entries);
        if (a.data.rpt.num_client_entries == b.data.rpt.num_client_entries)
        {
          if (a.data.rpt.client_entries && b.data.rpt.client_entries)
          {
            for (size_t i = 0; i < a.data.rpt.num_client_entries; ++i)
            {
              SCOPED_TRACE(std::string{"While evaluating Client Entries at index "} + std::to_string(i));
              ExpectRptClientEntriesEqual(a.data.rpt.client_entries[i], b.data.rpt.client_entries[i]);
            }
          }
          else if (!a.data.rpt.client_entries && b.data.rpt.client_entries)
          {
            // No comparison to make
          }
          else
          {
            ADD_FAILURE() << "Null/not-null mismatch between client entry lists; a was " << a.data.rpt.client_entries
                          << ", b was " << b.data.rpt.client_entries;
          }
        }
        break;
      case kClientProtocolEPT:
        EXPECT_EQ(a.data.ept.more_coming, b.data.ept.more_coming);
        EXPECT_EQ(a.data.ept.num_client_entries, b.data.ept.num_client_entries);
        if (a.data.ept.num_client_entries == b.data.ept.num_client_entries)
        {
          if (a.data.ept.client_entries && b.data.ept.client_entries)
          {
            for (size_t i = 0; i < a.data.ept.num_client_entries; ++i)
            {
              SCOPED_TRACE(std::string{"While evaluating Client Entries at index "} + std::to_string(i));
              ExpectEptClientEntriesEqual(a.data.ept.client_entries[i], b.data.ept.client_entries[i]);
            }
          }
          else if (!a.data.ept.client_entries && b.data.ept.client_entries)
          {
            // No comparison to make
          }
          else
          {
            ADD_FAILURE() << "Null/not-null mismatch between client entry lists; a was " << a.data.ept.client_entries
                          << ", b was " << b.data.ept.client_entries;
          }
        }
        break;
      default:
        ADD_FAILURE() << "Client Lists contained unknown client protocol " << a.client_protocol;
        break;
    }
  }
}

inline void ExpectMessagesEqual(const DynamicUidRequestList& a, const DynamicUidRequestList& b)
{
  EXPECT_EQ(a.num_requests, b.num_requests);
  EXPECT_EQ(a.more_coming, b.more_coming);
  if (a.num_requests == b.num_requests)
  {
    if (a.requests && b.requests)
    {
      for (size_t i = 0; i < a.num_requests; ++i)
      {
        EXPECT_EQ(a.requests[i].manu_id, b.requests[i].manu_id) << "While comparing index " << i;
        EXPECT_EQ(a.requests[i].rid, b.requests[i].rid) << "While comparing index " << i;
      }
    }
    else if (!a.requests && !b.requests)
    {
      // No comparison to make
    }
    else
    {
      ADD_FAILURE() << "Null/not-null mismatch between dynamic UID request lists; a was " << a.requests << ", b was "
                    << b.requests;
    }
  }
}

inline void ExpectMessagesEqual(const DynamicUidAssignmentList& a, const DynamicUidAssignmentList& b)
{
  EXPECT_EQ(a.num_mappings, b.num_mappings);
  EXPECT_EQ(a.more_coming, b.more_coming);
  if (a.num_mappings == b.num_mappings)
  {
    if (a.mappings && b.mappings)
    {
      for (size_t i = 0; i < a.num_mappings; ++i)
      {
        EXPECT_EQ(a.mappings[i].status_code, b.mappings[i].status_code) << "While comparing index " << i;
        EXPECT_EQ(a.mappings[i].uid, b.mappings[i].uid) << "While comparing index " << i;
        EXPECT_EQ(a.mappings[i].rid, b.mappings[i].rid) << "While comparing index " << i;
      }
    }
    else if (!a.mappings && !b.mappings)
    {
      // No comparison to make
    }
    else
    {
      ADD_FAILURE() << "Null/not-null mismatch between dynamic UID mapping lists; a was " << a.mappings << ", b was "
                    << b.mappings;
    }
  }
}

inline void ExpectMessagesEqual(const FetchUidAssignmentList& a, const FetchUidAssignmentList& b)
{
  EXPECT_EQ(a.num_uids, b.num_uids);
  EXPECT_EQ(a.more_coming, b.more_coming);
  if (a.num_uids == b.num_uids)
  {
    if (a.uids && b.uids)
    {
      for (size_t i = 0; i < a.num_uids; ++i)
      {
        EXPECT_EQ(a.uids[i], b.uids[i]) << "While comparing index " << i;
      }
    }
    else if (!a.uids && !b.uids)
    {
      // No comparison to make
    }
    else
    {
      ADD_FAILURE() << "Null/not-null mismatch between UID lists; a was " << a.uids << ", b was " << b.uids;
    }
  }
}

inline void ExpectMessagesEqual(const DisconnectMsg& a, const DisconnectMsg& b)
{
  EXPECT_EQ(a.disconnect_reason, b.disconnect_reason);
}

inline void ExpectMessagesEqual(const BrokerMessage& a, const BrokerMessage& b)
{
  EXPECT_EQ(a.vector, b.vector);
  if (a.vector == b.vector)
  {
    switch (a.vector)
    {
      case VECTOR_BROKER_CONNECT:
        ExpectMessagesEqual(a.data.client_connect, b.data.client_connect);
        break;
      case VECTOR_BROKER_CONNECT_REPLY:
        ExpectMessagesEqual(a.data.connect_reply, b.data.connect_reply);
        break;
      case VECTOR_BROKER_CLIENT_ENTRY_UPDATE:
        ExpectMessagesEqual(a.data.client_entry_update, b.data.client_entry_update);
        break;
      case VECTOR_BROKER_REDIRECT_V4:
      case VECTOR_BROKER_REDIRECT_V6:
        ExpectMessagesEqual(a.data.client_redirect, b.data.client_redirect);
        break;
      case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
      case VECTOR_BROKER_CLIENT_ADD:
      case VECTOR_BROKER_CLIENT_REMOVE:
      case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
        ExpectMessagesEqual(a.data.client_list, b.data.client_list);
        break;
      case VECTOR_BROKER_REQUEST_DYNAMIC_UIDS:
        ExpectMessagesEqual(a.data.dynamic_uid_request_list, b.data.dynamic_uid_request_list);
        break;
      case VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS:
        ExpectMessagesEqual(a.data.dynamic_uid_assignment_list, b.data.dynamic_uid_assignment_list);
        break;
      case VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST:
        ExpectMessagesEqual(a.data.fetch_uid_assignment_list, b.data.fetch_uid_assignment_list);
        break;
      case VECTOR_BROKER_DISCONNECT:
        ExpectMessagesEqual(a.data.disconnect, b.data.disconnect);
        break;
      case VECTOR_BROKER_FETCH_CLIENT_LIST:
      case VECTOR_BROKER_NULL:
        // No data to validate
        break;
      default:
        ADD_FAILURE() << "Broker messages contained unknown vector " << a.vector;
        break;
    }
  }
}

inline void ExpectMessagesEqual(const RdmBufList& a, const RdmBufList& b)
{
  EXPECT_EQ(a.num_rdm_buffers, b.num_rdm_buffers);
  EXPECT_EQ(a.more_coming, b.more_coming);
  if (a.num_rdm_buffers == b.num_rdm_buffers)
  {
    if (a.rdm_buffers && b.rdm_buffers)
    {
      for (size_t i = 0; i < a.num_rdm_buffers; ++i)
      {
        std::vector<uint8_t> a_rdm_data(a.rdm_buffers[i].data, &a.rdm_buffers[i].data[a.rdm_buffers[i].datalen]);
        std::vector<uint8_t> b_rdm_data(b.rdm_buffers[i].data, &b.rdm_buffers[i].data[b.rdm_buffers[i].datalen]);
        EXPECT_EQ(a_rdm_data, b_rdm_data) << "While comparing index " << i;
      }
    }
    else if (!a.rdm_buffers && !b.rdm_buffers)
    {
      // No comparison to make
    }
    else
    {
      ADD_FAILURE() << "Null/not-null mismatch between RDM buffer lists; a was " << a.rdm_buffers << ", b was "
                    << b.rdm_buffers;
    }
  }
}

inline void ExpectMessagesEqual(const RptStatusMsg& a, const RptStatusMsg& b)
{
  EXPECT_EQ(a.status_code, b.status_code);
  if (a.status_string && b.status_string)
  {
    EXPECT_STREQ(a.status_string, b.status_string);
  }
  else if (!a.status_string && !b.status_string)
  {
    // No comparison to make
  }
  else
  {
    ADD_FAILURE() << "Null/not-null mismatch between status strings; a was " << a.status_string << ", b was "
                  << b.status_string;
  }
}

inline void ExpectMessagesEqual(const RptMessage& a, const RptMessage& b)
{
  EXPECT_EQ(a.vector, b.vector);
  EXPECT_EQ(a.header.source_uid, b.header.source_uid);
  EXPECT_EQ(a.header.source_endpoint_id, b.header.source_endpoint_id);
  EXPECT_EQ(a.header.dest_uid, b.header.dest_uid);
  EXPECT_EQ(a.header.dest_endpoint_id, b.header.dest_endpoint_id);
  EXPECT_EQ(a.header.seqnum, b.header.seqnum);

  if (a.vector == b.vector)
  {
    switch (a.vector)
    {
      case VECTOR_RPT_REQUEST:
      case VECTOR_RPT_NOTIFICATION:
        ExpectMessagesEqual(a.data.rdm, b.data.rdm);
        break;
      case VECTOR_RPT_STATUS:
        ExpectMessagesEqual(a.data.status, b.data.status);
        break;
      default:
        ADD_FAILURE() << "RPT messages contained unknown vector " << a.vector;
    }
  }
}

inline void ExpectMessagesEqual(const EptMessage& /* a */, const EptMessage& /* b */)
{
  // TODO
}

inline void ExpectMessagesEqual(const RdmnetMessage& a, const RdmnetMessage& b)
{
  EXPECT_EQ(a.vector, b.vector);
  EXPECT_EQ(a.sender_cid, b.sender_cid);

  if (a.vector == b.vector)
  {
    switch (a.vector)
    {
      case ACN_VECTOR_ROOT_BROKER:
        ExpectMessagesEqual(a.data.broker, b.data.broker);
        break;
      case ACN_VECTOR_ROOT_RPT:
        ExpectMessagesEqual(a.data.rpt, b.data.rpt);
        break;
      case ACN_VECTOR_ROOT_EPT:
        ExpectMessagesEqual(a.data.ept, b.data.ept);
        break;
      default:
        ADD_FAILURE() << "Messages contained unknown root vector " << a.vector;
        break;
    }
  }
}

#endif  // TEST_DATA_UTIL_H_
