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
#include "BrokerItem.h"

#include "lwpa/socket.h"

BrokerItem::BrokerItem(const QString &scope, rdmnet_client_scope_t scope_handle,
                       const StaticBrokerConfig &static_broker /* = StaticBrokerConfig() */)
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

void BrokerItem::setConnected(bool connected, const LwpaSockaddr &broker_addr)
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
  bool have_address = (connected_ || static_broker_.valid);
  LwpaSockaddr address;

  if (connected_)
    address = broker_addr_;
  else if (static_broker_.valid)
    address = static_broker_.addr;

  char addrString[LWPA_INET6_ADDRSTRLEN];
  if (have_address && kLwpaErrOk == lwpa_inet_ntop(&address.ip, addrString, LWPA_INET6_ADDRSTRLEN))
  {
    setText(QString("Broker for scope \"%1\" at %2:%3").arg(scope_, addrString, QString::number(address.port)));
  }
  else
  {
    setText(QString("Broker for scope \"%1\"").arg(scope_));
  }
}
