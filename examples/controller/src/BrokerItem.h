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

#pragma once

#include "etcpal/cpp/inet.h"
#include "rdmnet/client.h"
#include "RDMnetNetworkItem.h"
#include "RDMnetClientItem.h"
#include "ControllerUtils.h"

class BrokerItem : public RDMnetNetworkItem
{
public:
  static const int BrokerItemType = QStandardItem::UserType + 2;

  BrokerItem(const QString& scope, rdmnet_client_scope_t scope_handle,
             const StaticBrokerConfig& static_broker = StaticBrokerConfig());
  virtual ~BrokerItem();

  virtual int type() const override;
  rdmnet_client_scope_t scope_handle() const { return scope_handle_; }

  void setScope(const QString& scope) { scope_ = scope; }
  QString scope() const { return scope_; }

  void setConnected(bool connected, const etcpal::SockAddr& broker_addr = etcpal::SockAddr());
  bool connected() const { return connected_; }

  std::vector<RDMnetClientItem*> rdmnet_clients_;

protected:
  void updateText();

private:
  QString scope_;
  rdmnet_client_scope_t scope_handle_{RDMNET_CLIENT_SCOPE_INVALID};
  StaticBrokerConfig static_broker_;
  etcpal::SockAddr broker_addr_{};
  bool connected_{false};
};
