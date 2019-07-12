/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#ifndef _RDMNET_CONN_WRAPPER_H_
#define _RDMNET_CONN_WRAPPER_H_

#include "rdmnet/core/connection.h"

class RdmnetConnNotify
{
public:
  virtual void RdmnetConnMsgReceived(rdmnet_conn_t handle, const RdmnetMessage& msg) = 0;
  virtual void RdmnetConnDisconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo& disconn_info) = 0;
};

struct SendDisconnect
{
  SendDisconnect() = default;
  SendDisconnect(rdmnet_disconnect_reason_t reason) : valid(true), reason(reason) {}

  bool valid{false};
  rdmnet_disconnect_reason_t reason;
};

class RdmnetConnInterface
{
public:
  virtual lwpa_error_t Startup(const LwpaUuid& cid, const LwpaLogParams* log_params, RdmnetConnNotify* notify) = 0;
  virtual void Shutdown() = 0;

  virtual lwpa_error_t CreateNewConnectionForSocket(lwpa_socket_t sock, const LwpaSockaddr& addr,
                                                    rdmnet_conn_t& new_handle) = 0;
  virtual void DestroyConnection(rdmnet_conn_t handle, SendDisconnect send_disconnect = SendDisconnect()) = 0;
  virtual lwpa_error_t SetBlocking(rdmnet_conn_t handle, bool blocking) = 0;

  virtual void SocketDataReceived(rdmnet_conn_t handle, const uint8_t* data, size_t data_size) = 0;
  virtual void SocketError(rdmnet_conn_t handle, lwpa_error_t err) = 0;
};

class RdmnetConnWrapper : public RdmnetConnInterface
{
public:
  RdmnetConnWrapper();

  lwpa_error_t Startup(const LwpaUuid& cid, const LwpaLogParams* log_params, RdmnetConnNotify* notify) override;
  void Shutdown() override;

  lwpa_error_t CreateNewConnectionForSocket(lwpa_socket_t sock, const LwpaSockaddr& addr,
                                            rdmnet_conn_t& new_handle) override;
  virtual void DestroyConnection(rdmnet_conn_t handle, SendDisconnect send_disconnect = SendDisconnect()) override;
  virtual lwpa_error_t SetBlocking(rdmnet_conn_t handle, bool blocking) override;

  virtual void SocketDataReceived(rdmnet_conn_t handle, const uint8_t* data, size_t data_size) override;
  virtual void SocketError(rdmnet_conn_t handle, lwpa_error_t err) override;

  void LibNotifyMsgReceived(rdmnet_conn_t handle, const RdmnetMessage* msg);
  void LibNotifyDisconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo* disconn_info);

private:
  RdmnetConnectionConfig new_conn_config_{};
  RdmnetConnNotify* notify_{nullptr};
};

#endif  // _RDMNET_CONN_WRAPPER_H_
