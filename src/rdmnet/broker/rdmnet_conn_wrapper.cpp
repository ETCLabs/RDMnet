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

#include "rdmnet_conn_wrapper.h"

extern "C" {

void conncb_disconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo* disconn_info, void* context)
{
  RdmnetConnWrapper* conn_wrapper = static_cast<RdmnetConnWrapper*>(context);
  if (conn_wrapper)
  {
    conn_wrapper->LibNotifyDisconnected(handle, disconn_info);
  }
}

void conncb_msg_received(rdmnet_conn_t handle, const RdmnetMessage* msg, void* context)
{
  RdmnetConnWrapper* conn_wrapper = static_cast<RdmnetConnWrapper*>(context);
  if (conn_wrapper)
  {
    conn_wrapper->LibNotifyMsgReceived(handle, msg);
  }
}
}

RdmnetConnWrapper::RdmnetConnWrapper()
{
  new_conn_config_.callback_context = this;

  // We are only interested in some of the callbacks.
  new_conn_config_.callbacks.connected = nullptr;
  new_conn_config_.callbacks.connect_failed = nullptr;
  new_conn_config_.callbacks.disconnected = conncb_disconnected;
  new_conn_config_.callbacks.msg_received = conncb_msg_received;
}

etcpal::Result RdmnetConnWrapper::Startup(const etcpal::Uuid& cid, const EtcPalLogParams* log_params)
{
  new_conn_config_.local_cid = cid.get();
  return rdmnet_core_init(log_params);
}

void RdmnetConnWrapper::Shutdown()
{
  rdmnet_core_deinit();
}

etcpal::Result RdmnetConnWrapper::CreateNewConnectionForSocket(etcpal_socket_t sock, const etcpal::SockAddr& addr,
                                                               rdmnet_conn_t& new_handle)
{
  etcpal_error_t create_res = rdmnet_connection_create(&new_conn_config_, &new_handle);
  if (create_res == kEtcPalErrOk)
  {
    create_res = rdmnet_attach_existing_socket(new_handle, sock, &addr.get());
    if (create_res != kEtcPalErrOk)
    {
      rdmnet_connection_destroy(new_handle, nullptr);
    }
  }
  return create_res;
}

void RdmnetConnWrapper::DestroyConnection(rdmnet_conn_t handle, SendDisconnect send_disconnect)
{
  rdmnet_connection_destroy(handle, send_disconnect.valid ? &send_disconnect.reason : nullptr);
}

etcpal::Result RdmnetConnWrapper::SetBlocking(rdmnet_conn_t handle, bool blocking)
{
  return rdmnet_set_blocking(handle, blocking);
}

void RdmnetConnWrapper::SocketDataReceived(rdmnet_conn_t handle, const uint8_t* data, size_t data_size)
{
  rdmnet_socket_data_received(handle, data, data_size);
}

void RdmnetConnWrapper::SocketError(rdmnet_conn_t handle, etcpal_error_t err)
{
  rdmnet_socket_error(handle, err);
}

void RdmnetConnWrapper::LibNotifyMsgReceived(rdmnet_conn_t handle, const RdmnetMessage* msg)
{
  if (notify_ && msg)
  {
    notify_->HandleRdmnetConnMsgReceived(handle, *msg);
  }
}

void RdmnetConnWrapper::LibNotifyDisconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo* disconn_info)
{
  if (notify_ && disconn_info)
  {
    notify_->HandleRdmnetConnDisconnected(handle, *disconn_info);
  }
}
