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

#include "broker_client.h"

#include "rdmnet/cpp/broker.h"
#include "rdmnet/core/broker_prot.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/opts.h"

bool BrokerClient::Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (max_q_size != 0 && broker_msgs_.size() >= max_q_size)
    return false;

  return PushPostSizeCheck(sender_cid, msg);
}

bool BrokerClient::PushPostSizeCheck(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  MessageRef to_push;
  bool       res = false;

  switch (msg.vector)
  {
    case VECTOR_BROKER_CONNECT_REPLY:
      to_push.data.reset(new uint8_t[BROKER_CONNECT_REPLY_FULL_MSG_SIZE]);
      if (to_push.data)
      {
        to_push.size = rc_broker_pack_connect_reply(to_push.data.get(), BROKER_CONNECT_REPLY_FULL_MSG_SIZE,
                                                    &sender_cid.get(), BROKER_GET_CONNECT_REPLY_MSG(&msg));
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
    case VECTOR_BROKER_CLIENT_ENTRY_CHANGE: {
      if (BROKER_GET_CLIENT_LIST(&msg)->client_protocol == kClientProtocolRPT)
      {
        const RdmnetRptClientList* rpt_list = BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&msg));
        size_t                     bufsize = rc_broker_get_rpt_client_list_buffer_size(rpt_list->num_client_entries);
        to_push.data.reset(new uint8_t[bufsize]);
        if (to_push.data)
        {
          to_push.size = rc_broker_pack_rpt_client_list(to_push.data.get(), bufsize, &sender_cid.get(), msg.vector,
                                                        rpt_list->client_entries, rpt_list->num_client_entries);
          if (to_push.size)
          {
            broker_msgs_.push(std::move(to_push));
            res = true;
          }
        }
      }
      // else TODO EPT
      break;
    }
    default:
      break;
  }
  return res;
}

bool BrokerClient::SendNull(const etcpal::Uuid& broker_cid)
{
  auto   send_buf = std::unique_ptr<uint8_t[]>(new uint8_t[BROKER_NULL_FULL_MSG_SIZE]);
  size_t send_size = rc_broker_pack_null(send_buf.get(), BROKER_NULL_FULL_MSG_SIZE, &broker_cid.get());
  return (etcpal_send(socket, send_buf.get(), send_size, 0) >= 0);
}

bool BrokerClient::Send(const etcpal::Uuid& broker_cid)
{
  // Try to send the next broker protocol message.
  if (!broker_msgs_.empty())
  {
    MessageRef& msg = broker_msgs_.front();
    int         res = etcpal_send(socket, &msg.data.get()[msg.size_sent], msg.size - msg.size_sent, 0);
    if (res >= 0)
    {
      msg.size_sent += res;
      if (msg.size_sent >= msg.size)
      {
        // We are done with this message.
        broker_msgs_.pop();
      }
      return true;
    }
  }
  else if (send_timer_.IsExpired())
  {
    if (SendNull(broker_cid))
    {
      send_timer_.Reset();
      return true;
    }
  }
  return false;
}

bool RPTClient::Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (broker_msgs_.size() + status_msgs_.size() >= max_q_size)
    return false;

  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

bool RPTClient::PushPostSizeCheck(const etcpal::Uuid& sender_cid, const RptHeader& header, const RptStatusMsg& msg)
{
  bool       res = false;
  MessageRef to_push;

  size_t bufsize = rc_rpt_get_status_buffer_size(&msg);
  to_push.data.reset(new uint8_t[bufsize]);
  if (to_push.data)
  {
    to_push.size = rc_rpt_pack_status(to_push.data.get(), bufsize, &sender_cid.get(), &header, &msg);
    if (to_push.size)
    {
      status_msgs_.push(std::move(to_push));
      res = true;
    }
  }
  return res;
}

bool RPTController::Push(BrokerClient::Handle /*from_client*/, const etcpal::Uuid& sender_cid, const RptMessage& msg)
{
  bool res = false;
  if (max_q_size != 0 && (status_msgs_.size() + broker_msgs_.size() + rpt_msgs_.size()) >= max_q_size)
  {
    return res;
  }

  switch (msg.vector)
  {
    case VECTOR_RPT_REQUEST:  // Controllers should respond to requests like devices do.
    {
      MessageRef to_push;
      size_t     bufsize = rc_rpt_get_request_buffer_size(RPT_GET_RDM_BUF_LIST(&msg)->rdm_buffers);
      to_push.data.reset(new uint8_t[bufsize]);
      if (to_push.data)
      {
        to_push.size = rc_rpt_pack_request(to_push.data.get(), bufsize, &sender_cid.get(), &msg.header,
                                           RPT_GET_RDM_BUF_LIST(&msg)->rdm_buffers);
        if (to_push.size)
        {
          rpt_msgs_.push(std::move(to_push));
          res = true;
        }
      }
    }
    break;

    case VECTOR_RPT_STATUS:
      res = RPTClient::PushPostSizeCheck(sender_cid, msg.header, *RPT_GET_STATUS_MSG(&msg));
      break;

    case VECTOR_RPT_NOTIFICATION: {
      MessageRef       to_push;
      const RdmBuffer* buffers = RPT_GET_RDM_BUF_LIST(&msg)->rdm_buffers;
      const size_t     num_buffers = RPT_GET_RDM_BUF_LIST(&msg)->num_rdm_buffers;

      size_t bufsize = rc_rpt_get_notification_buffer_size(buffers, num_buffers);
      to_push.data.reset(new uint8_t[bufsize]);
      if (to_push.data)
      {
        to_push.size =
            rc_rpt_pack_notification(to_push.data.get(), bufsize, &sender_cid.get(), &msg.header, buffers, num_buffers);
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

bool RPTController::Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (max_q_size != 0 && (status_msgs_.size() + broker_msgs_.size() + rpt_msgs_.size()) >= max_q_size)
  {
    return false;
  }
  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

bool RPTController::Push(const etcpal::Uuid& sender_cid, const RptHeader& header, const RptStatusMsg& msg)
{
  if (max_q_size != 0 && (status_msgs_.size() + broker_msgs_.size() + rpt_msgs_.size()) >= max_q_size)
    return false;

  return PushPostSizeCheck(sender_cid, header, msg);
}

bool RPTController::Send(const etcpal::Uuid& broker_cid)
{
  MessageRef*             msg = nullptr;
  std::queue<MessageRef>* q = nullptr;

  // Broker messages are first priority, then status messages, then RPT messages.
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
    int res = etcpal_send(socket, &msg->data.get()[msg->size_sent], msg->size - msg->size_sent, 0);
    if (res >= 0)
    {
      msg->size_sent += res;
      if (msg->size_sent >= msg->size)
      {
        // We are done with this message.
        q->pop();
        send_timer_.Reset();
      }
      return true;
    }
  }
  else if (send_timer_.IsExpired())
  {
    if (SendNull(broker_cid))
    {
      send_timer_.Reset();
      return true;
    }
  }
  return false;
}

bool RPTDevice::Push(BrokerClient::Handle from_client, const etcpal::Uuid& sender_cid, const RptMessage& msg)
{
  bool res = false;
  if (max_q_size != 0 && (status_msgs_.size() + broker_msgs_.size() + rpt_msgs_total_size_) >= max_q_size)
  {
    return res;
  }

  switch (msg.vector)
  {
    case VECTOR_RPT_STATUS:
      res = RPTClient::PushPostSizeCheck(sender_cid, msg.header, *RPT_GET_STATUS_MSG(&msg));
      break;

    case VECTOR_RPT_REQUEST: {
      MessageRef to_push;
      size_t     bufsize = rc_rpt_get_request_buffer_size(RPT_GET_RDM_BUF_LIST(&msg)->rdm_buffers);
      to_push.data.reset(new uint8_t[bufsize]);
      if (to_push.data)
      {
        to_push.size = rc_rpt_pack_request(to_push.data.get(), bufsize, &sender_cid.get(), &msg.header,
                                           RPT_GET_RDM_BUF_LIST(&msg)->rdm_buffers);
        if (to_push.size)
        {
          rpt_msgs_[from_client].push(std::move(to_push));
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

bool RPTDevice::Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (max_q_size != 0 && (status_msgs_.size() + broker_msgs_.size() + rpt_msgs_total_size_) >= max_q_size)
  {
    return false;
  }
  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

bool RPTDevice::Send(const etcpal::Uuid& broker_cid)
{
  MessageRef*             msg = nullptr;
  std::queue<MessageRef>* q = nullptr;
  bool                    is_rpt = false;

  // We should never push a status message to a Device.
  RDMNET_ASSERT(status_msgs_.empty());

  // Broker messages are first priority, then RPT messages.
  if (!broker_msgs_.empty())
  {
    q = &broker_msgs_;
    msg = &broker_msgs_.front();
  }
  else if (!rpt_msgs_.empty())
  {
    // Fair scheduler - we iterate through the controller map in order, starting from the last
    // controller serviced.
    auto con_pair = rpt_msgs_.upper_bound(last_controller_serviced_);
    if (con_pair == rpt_msgs_.end())
      con_pair = rpt_msgs_.begin();
    while (con_pair->first != last_controller_serviced_)
    {
      if (!con_pair->second.empty())
      {
        // Found a controller ready to service.
        break;
      }

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
    int res = etcpal_send(socket, &msg->data.get()[msg->size_sent], msg->size - msg->size_sent, 0);
    if (res >= 0)
    {
      msg->size_sent += res;
      if (msg->size_sent >= msg->size)
      {
        // We are done with this message.
        send_timer_.Reset();
        q->pop();
      }
      return true;
    }
    else if (res != kEtcPalErrWouldBlock)
    {
      // Error in sending. If this is an RPT message, delete the reference to this controller (and
      // clear out the queue)
      if (is_rpt)
        rpt_msgs_.erase(last_controller_serviced_);
    }
  }
  else if (send_timer_.IsExpired())
  {
    if (SendNull(broker_cid))
    {
      send_timer_.Reset();
      return true;
    }
  }
  return false;
}
