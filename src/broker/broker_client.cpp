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

#include "rdmnet/broker/client.h"

#include <cassert>
#include "rdmnet/common/connection.h"
#include "rdmnet/broker.h"

bool BrokerClient::Push(const LwpaUuid &sender_cid, const BrokerMessage &msg)
{
  if (broker_msgs_.size() < max_q_size_)
    return false;

  return PushPostSizeCheck(sender_cid, msg);
}

bool BrokerClient::PushPostSizeCheck(const LwpaUuid &sender_cid, const BrokerMessage &msg)
{
  MessageRef to_push;
  bool res = false;

  switch (msg.vector)
  {
    case VECTOR_BROKER_CONNECT_REPLY:
      to_push.data = std::make_unique<uint8_t[]>(CONNECT_REPLY_FULL_MSG_SIZE);
      if (to_push.data)
      {
        to_push.size = pack_connect_reply(to_push.data.get(), CONNECT_REPLY_FULL_MSG_SIZE, &sender_cid,
                                          get_connect_reply_msg(&msg));
        if (to_push.size)
        {
          broker_msgs_.push(std::move(to_push));
          res = true;
        }
      }
      break;
    case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
    case VECTOR_BROKER_CLIENT_ADD:
    case VECTOR_BROKER_CLIENT_REMOVE:
    case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
    {
      size_t bufsize = bufsize_client_list(get_client_list(&msg)->client_entry_list);
      to_push.data = std::make_unique<uint8_t[]>(bufsize);
      if (to_push.data)
      {
        to_push.size = pack_client_list(to_push.data.get(), bufsize, &sender_cid, msg.vector,
                                        get_client_list(&msg)->client_entry_list);
        if (to_push.size)
        {
          broker_msgs_.push(std::move(to_push));
          res = true;
        }
      }
      break;
    }
    default:
      break;
  }
  return res;
}

bool RPTClient::Push(const LwpaUuid &sender_cid, const BrokerMessage &msg)
{
  if (broker_msgs_.size() + status_msgs_.size() >= max_q_size_)
    return false;

  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

bool RPTClient::PushPostSizeCheck(const LwpaUuid &sender_cid, const RptHeader &header, const RptStatusMsg &msg)
{
  bool res = false;
  MessageRef to_push;

  size_t bufsize = bufsize_rpt_status(&msg);
  to_push.data = std::make_unique<uint8_t[]>(bufsize);
  if (to_push.data)
  {
    to_push.size = pack_rpt_status(to_push.data.get(), bufsize, &sender_cid, &header, &msg);
    if (to_push.size)
    {
      status_msgs_.push(std::move(to_push));
      res = true;
    }
  }
  return res;
}

bool RPTController::Push(int /*from_conn*/, const LwpaUuid &sender_cid, const RptMessage &msg)
{
  bool res = false;
  if ((status_msgs_.size() + broker_msgs_.size() + rpt_msgs_.size()) >= max_q_size_)
  {
    return res;
  }

  switch (msg.vector)
  {
    case VECTOR_RPT_STATUS:
      res = RPTClient::PushPostSizeCheck(sender_cid, msg.header, *get_rpt_status_msg(&msg));
      break;

    case VECTOR_RPT_NOTIFICATION:
    {
      MessageRef to_push;
      size_t bufsize = bufsize_rpt_notification(get_rdm_cmd_list(&msg)->list);
      to_push.data = std::make_unique<uint8_t[]>(bufsize);
      if (to_push.data)
      {
        to_push.size =
            pack_rpt_notification(to_push.data.get(), bufsize, &sender_cid, &msg.header, get_rdm_cmd_list(&msg)->list);
        if (to_push.size)
        {
          rpt_msgs_.push(std::move(to_push));
          res = true;
        }
      }
    }
    break;

    default:
      break;
  }
  return res;
}

bool RPTController::Push(const LwpaUuid &sender_cid, const BrokerMessage &msg)
{
  if ((status_msgs_.size() + broker_msgs_.size() + rpt_msgs_.size()) >= max_q_size_)
  {
    return false;
  }
  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

bool RPTController::Push(const LwpaUuid &sender_cid, const RptHeader &header, const RptStatusMsg &msg)
{
  if (broker_msgs_.size() + status_msgs_.size() >= max_q_size_)
    return false;

  return PushPostSizeCheck(sender_cid, header, msg);
}

bool RPTController::Send()
{
  MessageRef *msg = nullptr;
  std::queue<MessageRef> *q = nullptr;

  // Broker messages are first priority, then status messages, then RPT
  // messages.
  if (!broker_msgs_.empty())
  {
    q = &broker_msgs_;
    msg = &broker_msgs_.front();
  }
  else if (!status_msgs_.empty())
  {
    q = &status_msgs_;
    msg = &status_msgs_.front();
  }
  else if (!rpt_msgs_.empty())
  {
    q = &rpt_msgs_;
    msg = &rpt_msgs_.front();
  }

  // Try to send the message.
  if (msg && q)
  {
    int res = rdmnet_send(conn_, &msg->data.get()[msg->size_sent], msg->size - msg->size_sent);
    if (res >= 0)
    {
      msg->size_sent += res;
      if (msg->size_sent >= msg->size)
      {
        // We are done with this message.
        q->pop();
      }
      return true;
    }
  }
  return false;
}

bool RPTDevice::Push(int from_conn, const LwpaUuid &sender_cid, const RptMessage &msg)
{
  bool res = false;
  if ((status_msgs_.size() + broker_msgs_.size() + rpt_msgs_total_size_) >= max_q_size_)
  {
    return res;
  }

  switch (msg.vector)
  {
    case VECTOR_RPT_STATUS:
      res = RPTClient::PushPostSizeCheck(sender_cid, msg.header, *get_rpt_status_msg(&msg));
      break;

    case VECTOR_RPT_REQUEST:
    {
      MessageRef to_push;
      size_t bufsize = bufsize_rpt_request(&(get_rdm_cmd_list(&msg)->list->msg));
      to_push.data = std::make_unique<uint8_t[]>(bufsize);
      if (to_push.data)
      {
        to_push.size = pack_rpt_request(to_push.data.get(), bufsize, &sender_cid, &msg.header,
                                        &(get_rdm_cmd_list(&msg)->list->msg));
        if (to_push.size)
        {
          rpt_msgs_[from_conn].push(std::move(to_push));
          ++rpt_msgs_total_size_;
          res = true;
        }
      }
    }
    break;

    default:
      break;
  }
  return res;
}

bool RPTDevice::Push(const LwpaUuid &sender_cid, const BrokerMessage &msg)
{
  if ((status_msgs_.size() + broker_msgs_.size() + rpt_msgs_total_size_) >= max_q_size_)
  {
    return false;
  }
  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

bool RPTDevice::Send()
{
  MessageRef *msg = nullptr;
  std::queue<MessageRef> *q = nullptr;
  bool is_rpt = false;

  // We should never push a status message to a Device.
  assert(status_msgs_.empty());

  // Broker messages are first priority, then RPT messages.
  if (!broker_msgs_.empty())
  {
    q = &broker_msgs_;
    msg = &broker_msgs_.front();
  }
  else if (!rpt_msgs_.empty())
  {
    // Fair scheduler - we iterate through the controller map in order,
    // starting from the last controller serviced.
    auto con_pair = rpt_msgs_.upper_bound(last_controller_serviced_);
    if (con_pair == rpt_msgs_.end())
      con_pair = rpt_msgs_.begin();
    while (con_pair->first != last_controller_serviced_)
    {
      if (!con_pair->second.empty())
        // Found a controller ready to service.
        break;

      if (++con_pair == rpt_msgs_.end())
        con_pair = rpt_msgs_.begin();
    }
    if (!con_pair->second.empty())
    {
      q = &con_pair->second;
      msg = &con_pair->second.front();
      last_controller_serviced_ = con_pair->first;
    }
  }

  // Try to send the message.
  if (msg && q)
  {
    int res = rdmnet_send(conn_, &msg->data.get()[msg->size_sent], msg->size - msg->size_sent);
    if (res >= 0)
    {
      msg->size_sent += res;
      if (msg->size_sent >= msg->size)
      {
        // We are done with this message.
        q->pop();
      }
      return true;
    }
    else if (res != LWPA_WOULDBLOCK)
    {
      // Error in sending. If this is an RPT message, delete the reference to
      // this controller (and clear out the queue)
      if (is_rpt)
        rpt_msgs_.erase(last_controller_serviced_);
    }
  }
  return false;
}
