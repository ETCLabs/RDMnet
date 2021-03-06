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

#include "RDMnetClientItem.h"

RDMnetClientItem::RDMnetClientItem(const rdmnet::RptClientEntry& entry, bool is_me)
    : RDMnetNetworkItem(QString("%0%1 | Manu: 0x%2 | ID: 0x%3")
                            .arg(QString::fromStdString(entry.TypeToString()))
                            .arg(is_me ? QString::fromUtf8(" (me)") : QString())
                            .arg(QString::number(entry.uid.manufacturer_id(), 16))
                            .arg(QString::number(entry.uid.device_id(), 16)))
    , entry_(entry)
{
}

RDMnetClientItem::~RDMnetClientItem()
{
}

int RDMnetClientItem::type() const
{
  return RDMnetClientItemType;
}

void RDMnetClientItem::setScopeSlot(const QString& scope, uint16_t slot)
{
  for (auto& i : scope_slots_)
  {
    if (i.second == slot)
    {
      removeScopeSlot(i.first);
      break;
    }
  }

  scope_slots_[scope] = slot;
}

uint16_t RDMnetClientItem::getScopeSlot(const QString& scope)
{
  return scope_slots_[scope];
}

void RDMnetClientItem::removeScopeSlot(const QString& scope)
{
  scope_slots_.erase(scope_slots_.find(scope));
}
