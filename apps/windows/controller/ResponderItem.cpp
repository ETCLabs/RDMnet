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

#include "ResponderItem.h"

// ResponderItem::ResponderItem()
//{
//}

ResponderItem::ResponderItem(uint16_t man, uint32_t dev)
    : RDMnetNetworkItem(QString("Manu: 0x%0 | ID: 0x%1").arg(QString::number(man, 16)).arg(QString::number(dev, 16)))
{
  m_Man = man;
  m_Dev = dev;
  personalityDescriptions = NULL;
  numberOfDescriptionsFound = 0;
  totalNumberOfDescriptions = 0;
  device_reset_ = false;
}

ResponderItem::~ResponderItem()
{
  delete[] personalityDescriptions;
}

bool ResponderItem::hasValidProperties() const
{
  return !device_reset_;
}

bool ResponderItem::initiatePersonalityDescriptionSearch(uint8_t numberOfPersonalities)
{
  if (personalityDescriptions == NULL)
  {
    totalNumberOfDescriptions = numberOfPersonalities;
    personalityDescriptions = new QString[numberOfPersonalities];
    return true;
  }

  return false;
}

void ResponderItem::personalityDescriptionFound(uint8_t personality, uint16_t /*footprint*/, const QString &description)
{
  if (personality <= totalNumberOfDescriptions)
  {
    personalityDescriptions[personality - 1] = description;
    ++numberOfDescriptionsFound;
  }
}

bool ResponderItem::allPersonalityDescriptionsFound()
{
  return (numberOfDescriptionsFound >= totalNumberOfDescriptions) && (personalityDescriptions != NULL);
}

QStringList ResponderItem::personalityDescriptionList()
{
  QStringList result;

  if (allPersonalityDescriptionsFound())
  {
    for (int i = 0; i < totalNumberOfDescriptions; ++i)
    {
      result.push_back(personalityDescriptions[i]);
    }
  }

  return result;
}

QString ResponderItem::personalityDescriptionAt(int i)
{
  return personalityDescriptions[i];
}

void ResponderItem::setDeviceWasReset(bool reset)
{
  device_reset_ = reset;
}

int ResponderItem::type() const
{
  return ResponderItemType;
}
