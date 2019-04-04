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

#include "RDMnetClientItem.h"

#include "rdmnet/client.h"

const char *RDMnetClientItem::clientType2String(rpt_client_type_t type)
{
  switch (type)
  {
    case kRPTClientTypeDevice:
      return "Device";
    case kRPTClientTypeController:
      return "Controller";
    case kRPTClientTypeUnknown:
    default:
      return "Unknown Client Type";
  }
}

RDMnetClientItem::RDMnetClientItem(const ClientEntryData &entry)
    : RDMnetNetworkItem(QString("%0 | Manu: 0x%2 | ID: 0x%3")
                            .arg(QString(clientType2String(get_rpt_client_entry_data(&entry)->client_type)))
                            .arg(QString::number(get_rpt_client_entry_data(&entry)->client_uid.manu, 16))
                            .arg(QString::number(get_rpt_client_entry_data(&entry)->client_uid.id, 16)))
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

void RDMnetClientItem::setScopeSlot(const QString &scope, uint16_t slot)
{
  for (auto &i : scope_slots_)
  {
    if (i.second == slot)
    {
      removeScopeSlot(i.first);
      break;
    }
  }

  scope_slots_[scope] = slot;
}

uint16_t RDMnetClientItem::getScopeSlot(const QString &scope)
{
  return scope_slots_[scope];
}

void RDMnetClientItem::removeScopeSlot(const QString &scope)
{
  scope_slots_.erase(scope_slots_.find(scope));
}
