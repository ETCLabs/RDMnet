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
#include "BrokerItem.h"

BrokerItem::BrokerItem(const QString&          scope,
                       rdmnet_client_scope_t   scope_handle,
                       const etcpal::SockAddr& static_broker /* = etcpal::SockAddr() */)
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

void BrokerItem::SetConnected(bool connected, const etcpal::SockAddr& broker_addr)
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
  else if (static_broker_.IsValid())
    address = static_broker_;

  if (address.IsValid())
  {
    setText(QString("Broker for scope \"%1\" at %2").arg(scope_, QString::fromStdString(address.ToString())));
  }
  else
  {
    setText(QString("Broker for scope \"%1\"").arg(scope_));
  }
}

std::optional<rdmnet::DestinationAddr> BrokerItem::FindResponder(const rdm::Uid& uid) const
{
  for (const RDMnetClientItem* client : rdmnet_clients_)
  {
    if (client->uid() == uid)
    {
      // This UID represents the default responder
      return rdmnet::DestinationAddr::ToDefaultResponder(uid);
    }
    else
    {
      for (const EndpointItem* endpoint : client->endpoints_)
      {
        for (const ResponderItem* responder : endpoint->responders_)
        {
          if (responder->uid() == uid)
          {
            return rdmnet::DestinationAddr::ToSubResponder(client->uid(), endpoint->id(), responder->uid());
          }
        }
      }
    }
  }
  return std::nullopt;
}
