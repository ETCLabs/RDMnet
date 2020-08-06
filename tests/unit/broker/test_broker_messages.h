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

#ifndef TEST_BROKER_MESSAGES_H_
#define TEST_BROKER_MESSAGES_H_

#include <string>
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/broker_prot.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

constexpr size_t kRootVectorOffset = 19;
constexpr size_t kBrokerVectorOffset = 42;
constexpr size_t kConnectReplyCodeOffset = 44;
constexpr size_t kDisconnectCodeOffset = 44;

namespace testmsgs
{
inline RdmnetMessage ClientConnect(const etcpal::Uuid& cid, std::string scope = E133_DEFAULT_SCOPE)
{
  RdmnetMessage connect_msg;
  connect_msg.vector = ACN_VECTOR_ROOT_BROKER;
  connect_msg.sender_cid = cid.get();

  BrokerMessage* broker_msg = RDMNET_GET_BROKER_MSG(&connect_msg);
  broker_msg->vector = VECTOR_BROKER_CONNECT;

  BrokerClientConnectMsg* client_connect = BROKER_GET_CLIENT_CONNECT_MSG(broker_msg);
  strcpy(client_connect->scope, scope.c_str());
  client_connect->e133_version = E133_VERSION;
  strcpy(client_connect->search_domain, E133_DEFAULT_DOMAIN);
  client_connect->connect_flags = 0;

  client_connect->client_entry.client_protocol = kClientProtocolRPT;
  RdmnetRptClientEntry* rpt_entry = GET_RPT_CLIENT_ENTRY(&client_connect->client_entry);
  rpt_entry->cid = cid.get();
  rpt_entry->binding_cid = etcpal::Uuid().get();
  rpt_entry->uid = rdm::Uid::DynamicUidRequest(0x6574).get();
  rpt_entry->type = kRPTClientTypeController;

  return connect_msg;
}

inline RdmnetMessage ClientDisconnect(const etcpal::Uuid& cid, rdmnet_disconnect_reason_t disconnect_reason)
{
  RdmnetMessage disconnect_msg;
  disconnect_msg.vector = ACN_VECTOR_ROOT_BROKER;
  disconnect_msg.sender_cid = cid.get();

  BrokerMessage* broker_msg = RDMNET_GET_BROKER_MSG(&disconnect_msg);
  broker_msg->vector = VECTOR_BROKER_DISCONNECT;

  BrokerDisconnectMsg* client_disconnect = BROKER_GET_DISCONNECT_MSG(broker_msg);
  client_disconnect->disconnect_reason = disconnect_reason;

  return disconnect_msg;
}

inline RdmnetMessage Null(const etcpal::Uuid& cid)
{
  RdmnetMessage null_msg;
  null_msg.vector = ACN_VECTOR_ROOT_BROKER;
  null_msg.sender_cid = cid.get();

  BrokerMessage* broker_msg = RDMNET_GET_BROKER_MSG(&null_msg);
  broker_msg->vector = VECTOR_BROKER_NULL;

  return null_msg;
}

inline RdmnetMessage FetchClientList(const etcpal::Uuid& cid)
{
  RdmnetMessage fcl_msg;
  fcl_msg.vector = ACN_VECTOR_ROOT_BROKER;
  fcl_msg.sender_cid = cid.get();

  BrokerMessage* broker_msg = RDMNET_GET_BROKER_MSG(&fcl_msg);
  broker_msg->vector = VECTOR_BROKER_FETCH_CLIENT_LIST;

  return fcl_msg;
}

};  // namespace testmsgs

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif  // BROKER_MESSAGES_H_
