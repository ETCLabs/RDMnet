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
#include "BrokerItem.h"

BrokerItem::BrokerItem(const QString& scope, rdmnet_client_scope_t scope_handle,
                       const StaticBrokerConfig& static_broker /* = StaticBrokerConfig() */)
    : RDMnetNetworkItem(), scope_(scope), scope_handle_(scope_handle), static_broker_(static_broker)
{
  updateText();
}

BrokerItem::~BrokerItem()
{
}

int BrokerItem::type() const
{
  return BrokerItemType;
}

void BrokerItem::setConnected(bool connected, const etcpal::SockAddr& broker_addr)
{
  connected_ = connected;
  if (connected)
  {
    broker_addr_ = broker_addr;
  }
  updateText();
}

void BrokerItem::updateText()
{
  etcpal::SockAddr address;

  if (connected_)
    address = broker_addr_;
  else if (static_broker_.valid)
    address = static_broker_.addr;

  if (address.ip().IsValid())
  {
    setText(QString("Broker for scope \"%1\" at %2").arg(scope_, QString::fromStdString(address.ToString())));
  }
  else
  {
    setText(QString("Broker for scope \"%1\"").arg(scope_));
  }
}
