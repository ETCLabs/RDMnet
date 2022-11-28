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
#include "rdmnet/core/common.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/opts.h"

bool BrokerClient::HasRoomToPush()
{
  return (max_q_size_ == kLimitlessQueueSize) || (broker_msgs_.size() < max_q_size_);
}

ClientPushResult BrokerClient::Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (marked_for_destruction_)
    return ClientPushResult::Error;
  if (!HasRoomToPush())
    return ClientPushResult::QueueFull;

  return PushPostSizeCheck(sender_cid, msg);
}

bool BrokerClient::Send(const etcpal::Uuid& broker_cid)
{
  // Try to send the next broker protocol message.
  if (!broker_msgs_.empty())
  {
    MessageRef& msg = broker_msgs_.front();
    auto        msg_data = msg.data.get();
    if (!RDMNET_ASSERT_VERIFY(msg_data))
      return false;

    int res = rc_send(socket_, &msg_data[msg.size_sent], msg.size - msg.size_sent, 0);
    if (res >= 0)
    {
      msg.size_sent += res;
      if (msg.size_sent >= msg.size)
      {
        // We are done with this message.
        broker_msgs_.pop_front();
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

void BrokerClient::MarkForDestruction(const etcpal::Uuid&        broker_cid,
                                      const rdm::Uid&            broker_uid,
                                      const ClientDestroyAction& destroy_action)
{
  // Clear out the existing queue
  ClearAllQueues();
  ApplyDestroyAction(broker_cid, broker_uid, destroy_action);
  marked_for_destruction_ = true;
}

ClientPushResult BrokerClient::PushPostSizeCheck(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (marked_for_destruction_)
    return ClientPushResult::Error;

  ClientPushResult res = ClientPushResult::Error;

  switch (msg.vector)
  {
    case VECTOR_BROKER_CONNECT_REPLY: {
      MessageRef to_push(BROKER_CONNECT_REPLY_FULL_MSG_SIZE);
      if (to_push.data)
      {
        to_push.size = rc_broker_pack_connect_reply(to_push.data.get(), BROKER_CONNECT_REPLY_FULL_MSG_SIZE,
                                                    &sender_cid.get(), BROKER_GET_CONNECT_REPLY_MSG(&msg));
        if (to_push.size)
        {
          broker_msgs_.push_back(std::move(to_push));
          res = ClientPushResult::Ok;
        }
      }
    }
    break;
    case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
    case VECTOR_BROKER_CLIENT_ADD:
    case VECTOR_BROKER_CLIENT_REMOVE:
    case VECTOR_BROKER_CLIENT_ENTRY_CHANGE: {
      auto client_list = BROKER_GET_CLIENT_LIST(&msg);
      if (!RDMNET_ASSERT_VERIFY(client_list))
        return ClientPushResult::Error;

      if (client_list->client_protocol == kClientProtocolRPT)
      {
        const RdmnetRptClientList* rpt_list = BROKER_GET_RPT_CLIENT_LIST(client_list);
        if (!RDMNET_ASSERT_VERIFY(rpt_list))
          return ClientPushResult::Error;

        size_t     bufsize = rc_broker_get_rpt_client_list_buffer_size(rpt_list->num_client_entries);
        MessageRef to_push(bufsize);
        if (to_push.data)
        {
          to_push.size = rc_broker_pack_rpt_client_list(to_push.data.get(), bufsize, &sender_cid.get(), msg.vector,
                                                        rpt_list->client_entries, rpt_list->num_client_entries);
          if (to_push.size)
          {
            broker_msgs_.push_back(std::move(to_push));
            res = ClientPushResult::Ok;
          }
        }
      }
      // else TODO EPT
      break;
    }
    case VECTOR_BROKER_DISCONNECT: {
      MessageRef to_push(BROKER_DISCONNECT_FULL_MSG_SIZE);
      if (to_push.data)
      {
        to_push.size = rc_broker_pack_disconnect(to_push.data.get(), BROKER_DISCONNECT_FULL_MSG_SIZE, &sender_cid.get(),
                                                 BROKER_GET_DISCONNECT_MSG(&msg));
        if (to_push.size)
        {
          broker_msgs_.push_back(std::move(to_push));
          res = ClientPushResult::Ok;
        }
      }
    }
    break;
    default:
      break;
  }
  return res;
}

bool BrokerClient::SendNull(const etcpal::Uuid& broker_cid)
{
  auto   send_buf = std::unique_ptr<uint8_t[]>(new uint8_t[BROKER_NULL_FULL_MSG_SIZE]);
  size_t send_size = rc_broker_pack_null(send_buf.get(), BROKER_NULL_FULL_MSG_SIZE, &broker_cid.get());
  return (rc_send(socket_, send_buf.get(), send_size, 0) >= 0);
}

void BrokerClient::ApplyDestroyAction(const etcpal::Uuid&        broker_cid,
                                      const rdm::Uid&            broker_uid,
                                      const ClientDestroyAction& destroy_action)
{
  switch (destroy_action.action())
  {
    case ClientDestroyAction::Action::SendConnectReply: {
      BrokerMessage msg{};
      msg.vector = VECTOR_BROKER_CONNECT_REPLY;

      auto connect_reply_msg = BROKER_GET_CONNECT_REPLY_MSG(&msg);
      if (!RDMNET_ASSERT_VERIFY(connect_reply_msg))
        return;

      connect_reply_msg->broker_uid = broker_uid.get();
      connect_reply_msg->connect_status = destroy_action.connect_status();
      connect_reply_msg->e133_version = E133_VERSION;

      Push(broker_cid, msg);
    }
    break;
    case ClientDestroyAction::Action::SendDisconnect: {
      BrokerMessage msg{};
      msg.vector = VECTOR_BROKER_DISCONNECT;

      auto get_disconnect_msg = BROKER_GET_DISCONNECT_MSG(&msg);
      if (!RDMNET_ASSERT_VERIFY(get_disconnect_msg))
        return;

      get_disconnect_msg->disconnect_reason = destroy_action.disconnect_reason();

      Push(broker_cid, msg);
    }
    break;
    case ClientDestroyAction::Action::MarkSocketInvalid:
      socket_ = ETCPAL_SOCKET_INVALID;
      break;
    default:
      break;
  }
}

bool RPTClient::HasRoomToPush()
{
  return (broker_msgs_.size() + status_msgs_.size()) < max_q_size_;
}

ClientPushResult RPTClient::Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (marked_for_destruction_)
    return ClientPushResult::Error;
  if (!HasRoomToPush())
    return ClientPushResult::QueueFull;

  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

ClientPushResult RPTClient::PushPostSizeCheck(const etcpal::Uuid& sender_cid,
                                              const RptHeader&    header,
                                              const RptStatusMsg& msg)
{
  ClientPushResult res = ClientPushResult::Error;

  size_t     bufsize = rc_rpt_get_status_buffer_size(&msg);
  MessageRef to_push(bufsize);
  if (to_push.data)
  {
    to_push.size = rc_rpt_pack_status(to_push.data.get(), bufsize, &sender_cid.get(), &header, &msg);
    if (to_push.size)
    {
      status_msgs_.push_back(std::move(to_push));
      res = ClientPushResult::Ok;
    }
  }
  return res;
}

void RPTClient::ClearAllQueues()
{
  broker_msgs_.clear();
  status_msgs_.clear();
}

bool RPTController::HasRoomToPush()
{
  return ((max_q_size_ == kLimitlessQueueSize) ||
          (status_msgs_.size() + broker_msgs_.size() + rpt_msgs_.size()) < max_q_size_);
}

ClientPushResult RPTController::Push(BrokerClient::Handle /*from_client*/,
                                     const etcpal::Uuid& sender_cid,
                                     const RptMessage&   msg)
{
  if (marked_for_destruction_)
    return ClientPushResult::Error;
  if (!HasRoomToPush())
    return ClientPushResult::QueueFull;

  ClientPushResult res = ClientPushResult::Error;

  switch (msg.vector)
  {
    case VECTOR_RPT_REQUEST: {
      auto rdm_buf_list = RPT_GET_RDM_BUF_LIST(&msg);
      if (!RDMNET_ASSERT_VERIFY(rdm_buf_list))
        return ClientPushResult::Error;

      size_t     bufsize = rc_rpt_get_request_buffer_size(rdm_buf_list->rdm_buffers);
      MessageRef to_push(bufsize);
      if (to_push.data)
      {
        to_push.size =
            rc_rpt_pack_request(to_push.data.get(), bufsize, &sender_cid.get(), &msg.header, rdm_buf_list->rdm_buffers);
        if (to_push.size)
        {
          rpt_msgs_.push_back(std::move(to_push));
          res = ClientPushResult::Ok;
        }
      }
    }
    break;

    case VECTOR_RPT_STATUS:
      auto status_msg = RPT_GET_STATUS_MSG(&msg);
      if (!RDMNET_ASSERT_VERIFY(status_msg))
        return ClientPushResult::Error;

      res = RPTClient::PushPostSizeCheck(sender_cid, msg.header, *status_msg);
      break;

    case VECTOR_RPT_NOTIFICATION: {
      auto rdm_buf_list = RPT_GET_RDM_BUF_LIST(&msg);
      if (!RDMNET_ASSERT_VERIFY(rdm_buf_list))
        return ClientPushResult::Error;

      const RdmBuffer* buffers = rdm_buf_list->rdm_buffers;
      const size_t     num_buffers = rdm_buf_list->num_rdm_buffers;

      size_t     bufsize = rc_rpt_get_notification_buffer_size(buffers, num_buffers);
      MessageRef to_push(bufsize);
      if (to_push.data)
      {
        to_push.size =
            rc_rpt_pack_notification(to_push.data.get(), bufsize, &sender_cid.get(), &msg.header, buffers, num_buffers);
        if (to_push.size)
        {
          rpt_msgs_.push_back(std::move(to_push));
          res = ClientPushResult::Ok;
        }
      }
    }
    break;

    default:
      break;
  }
  return res;
}

ClientPushResult RPTController::Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (marked_for_destruction_)
    return ClientPushResult::Error;
  if (!HasRoomToPush())
    return ClientPushResult::QueueFull;

  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

ClientPushResult RPTController::Push(const etcpal::Uuid& sender_cid, const RptHeader& header, const RptStatusMsg& msg)
{
  if (marked_for_destruction_)
    return ClientPushResult::Error;
  if (!HasRoomToPush())
    return ClientPushResult::QueueFull;

  return PushPostSizeCheck(sender_cid, header, msg);
}

bool RPTController::Send(const etcpal::Uuid& broker_cid)
{
  MessageRef*             msg = nullptr;
  std::deque<MessageRef>* q = nullptr;

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
    int res = rc_send(socket_, &msg->data.get()[msg->size_sent], msg->size - msg->size_sent, 0);
    if (res >= 0)
    {
      msg->size_sent += res;
      if (msg->size_sent >= msg->size)
      {
        // We are done with this message.
        q->pop_front();
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

void RPTController::ClearAllQueues()
{
  rpt_msgs_.clear();
  status_msgs_.clear();
  broker_msgs_.clear();
}

bool RPTDevice::HasRoomToPush()
{
  return ((max_q_size_ == kLimitlessQueueSize) ||
          (status_msgs_.size() + broker_msgs_.size() + rpt_msgs_.size()) < max_q_size_);
}

ClientPushResult RPTDevice::Push(BrokerClient::Handle from_client,
                                 const etcpal::Uuid&  sender_cid,
                                 const RptMessage&    msg)
{
  if (marked_for_destruction_)
    return ClientPushResult::Error;
  if (!HasRoomToPush())
    return ClientPushResult::QueueFull;

  ClientPushResult res = ClientPushResult::Error;

  switch (msg.vector)
  {
    case VECTOR_RPT_STATUS:
      auto status_msg = RPT_GET_STATUS_MSG(&msg);
      if (!RDMNET_ASSERT_VERIFY(status_msg))
        return ClientPushResult::Error;

      res = RPTClient::PushPostSizeCheck(sender_cid, msg.header, *status_msg);
      break;

    case VECTOR_RPT_REQUEST: {
      auto rdm_buf_list = RPT_GET_RDM_BUF_LIST(&msg);
      if (!RDMNET_ASSERT_VERIFY(rdm_buf_list))
        return ClientPushResult::Error;

      size_t     bufsize = rc_rpt_get_request_buffer_size(rdm_buf_list->rdm_buffers);
      MessageRef to_push(bufsize);
      if (to_push.data)
      {
        to_push.size =
            rc_rpt_pack_request(to_push.data.get(), bufsize, &sender_cid.get(), &msg.header, rdm_buf_list->rdm_buffers);
        if (to_push.size)
        {
          rpt_msgs_.push_back(from_client, std::move(to_push));
          res = ClientPushResult::Ok;
        }
      }
    }
    break;

    default:
      break;
  }
  return res;
}

ClientPushResult RPTDevice::Push(const etcpal::Uuid& sender_cid, const BrokerMessage& msg)
{
  if (marked_for_destruction_)
    return ClientPushResult::Error;
  if (!HasRoomToPush())
    return ClientPushResult::QueueFull;

  return BrokerClient::PushPostSizeCheck(sender_cid, msg);
}

bool RPTDevice::Send(const etcpal::Uuid& broker_cid)
{
  MessageRef* msg = nullptr;
  bool        is_rpt = false;

  // We should never push a status message to a Device.
  RDMNET_ASSERT(status_msgs_.empty());

  // Broker messages are first priority, then RPT messages.
  if (!broker_msgs_.empty())
  {
    msg = &broker_msgs_.front();
  }
  else if (!rpt_msgs_.empty())
  {
    is_rpt = true;
    msg = rpt_msgs_.front();
  }

  // Try to send the message.
  if (msg)
  {
    auto msg_data = msg->data.get();
    if (!RDMNET_ASSERT_VERIFY(msg_data))
      return false;

    int res = rc_send(socket_, &msg_data[msg->size_sent], msg->size - msg->size_sent, 0);
    if (res >= 0)
    {
      msg->size_sent += res;
      if (msg->size_sent >= msg->size)
      {
        // We are done with this message.
        send_timer_.Reset();
        if (is_rpt)
          rpt_msgs_.pop_front();
        else
          broker_msgs_.pop_front();
      }
      return true;
    }
    else if (res != kEtcPalErrWouldBlock)
    {
      // Error in sending. If this is an RPT message, delete the reference to this controller (and
      // clear out the queue)
      if (is_rpt)
        rpt_msgs_.RemoveCurrentController();
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

void RPTDevice::ClearAllQueues()
{
  rpt_msgs_.clear();
  status_msgs_.clear();
  broker_msgs_.clear();
}

bool RPTDevice::RptMsgQ::empty() const
{
  return total_msg_count_ == 0;
}

MessageRef* RPTDevice::RptMsgQ::front()
{
  // Fair scheduler - we iterate through the controller map in order, starting from the last
  // controller serviced.
  auto con_pair = rpt_msgs_.upper_bound(current_controller_);
  if (con_pair == rpt_msgs_.end())
    con_pair = rpt_msgs_.begin();

  if (!RDMNET_ASSERT_VERIFY(con_pair != rpt_msgs_.end()))
    return nullptr;

  while (con_pair->first != current_controller_)
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
    current_controller_ = con_pair->first;
    return &con_pair->second.front();
  }
  return nullptr;
}

void RPTDevice::RptMsgQ::pop_front()
{
  if (current_controller_ != kInvalidHandle)
  {
    rpt_msgs_[current_controller_].pop_front();
    --total_msg_count_;
  }
}

void RPTDevice::RptMsgQ::push_back(Handle controller, MessageRef&& value)
{
  rpt_msgs_[controller].push_back(std::move(value));
  ++total_msg_count_;
}

size_t RPTDevice::RptMsgQ::size() const
{
  return total_msg_count_;
}

void RPTDevice::RptMsgQ::RemoveCurrentController()
{
  auto controller_pair = rpt_msgs_.find(current_controller_);
  if (controller_pair != rpt_msgs_.end())
  {
    total_msg_count_ -= controller_pair->second.size();
    rpt_msgs_.erase(controller_pair);
  }
}

void RPTDevice::RptMsgQ::clear()
{
  rpt_msgs_.clear();
  total_msg_count_ = 0;
  current_controller_ = kInvalidHandle;
}
