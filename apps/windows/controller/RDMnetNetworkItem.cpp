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

#include "RDMnetNetworkItem.h"
#include "SearchingStatusItem.h"

bool RDMnetNetworkItem::rowHasSearchingStatusItem(int row)
{
  QStandardItem *current = this->child(row);

  if (current != NULL)
  {
    return (current->type() == SearchingStatusItem::SearchingStatusItemType);
  }

  return false;
}

RDMnetNetworkItem::RDMnetNetworkItem()
    : children_search_running_(false)
    , supports_reset_device_(false)
    , device_reset_(false)
    , personalityDescriptions(NULL)
    , numberOfDescriptionsFound(0)
    , totalNumberOfDescriptions(0)
{
  setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

  setData(EditorWidgetType::kDefault, EditorWidgetTypeRole);
}

RDMnetNetworkItem::RDMnetNetworkItem(const QVariant &data)
    : QStandardItem()
    , children_search_running_(false)
    , supports_reset_device_(false)
    , device_reset_(false)
    , personalityDescriptions(NULL)
    , numberOfDescriptionsFound(0)
    , totalNumberOfDescriptions(0)
{
  setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

  if (data.type() == QVariant::Bool)
  {
    setData(data.toBool() ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
  }
  else
  {
    setData(data, Qt::DisplayRole);
  }

  setData(EditorWidgetType::kDefault, EditorWidgetTypeRole);
}

RDMnetNetworkItem::RDMnetNetworkItem(const QVariant &data, int role)
    : QStandardItem()
    , children_search_running_(false)
    , supports_reset_device_(false)
    , device_reset_(false)
    , personalityDescriptions(NULL)
    , numberOfDescriptionsFound(0)
    , totalNumberOfDescriptions(0)
{
  setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

  if ((role == Qt::CheckStateRole) && (data.type() == QVariant::Type::Bool))
  {
    setData(data.toBool() ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
  }
  else
  {
    setData(data, role);
  }

  setData(EditorWidgetType::kDefault, EditorWidgetTypeRole);
}

RDMnetNetworkItem::~RDMnetNetworkItem()
{
  delete[] personalityDescriptions;
}

int RDMnetNetworkItem::type() const
{
  return RDMnetNetworkItemType;
}

bool RDMnetNetworkItem::childrenSearchRunning() const
{
  return children_search_running_;
}

bool RDMnetNetworkItem::supportsResetDevice() const
{
  return supports_reset_device_;
}

void RDMnetNetworkItem::enableChildrenSearch()
{
  if (!children_search_running_ /* && (this->rowCount() == 0)*/)
  {
    children_search_running_ = true;

    this->appendRow(new SearchingStatusItem());
  }
}

void RDMnetNetworkItem::disableChildrenSearch()
{
  if (children_search_running_)
  {
    int currentRow = 0;

    while (currentRow < this->rowCount())
    {
      if (this->rowHasSearchingStatusItem(currentRow))
      {
        this->removeRow(currentRow);
      }
      else
      {
        ++currentRow;
      }
    }

    children_search_running_ = false;
  }
}

void RDMnetNetworkItem::enableResetDevice()
{
  supports_reset_device_ = true;
}

void RDMnetNetworkItem::completelyRemoveChildren(int row, int count)
{
  for (int i = row; i < (row + count); ++i)
  {
    RDMnetNetworkItem *c = dynamic_cast<RDMnetNetworkItem *>(child(i));

    if (c != NULL)
    {
      c->completelyRemoveChildren(0, c->rowCount());
    }
  }

  removeRows(row, count);
}

void RDMnetNetworkItem::disableAllChildItems()
{
  for (int i = 0; i < rowCount(); ++i)
  {
    for (int j = 0; j < columnCount(); ++j)
    {
      RDMnetNetworkItem *c = dynamic_cast<RDMnetNetworkItem *>(child(i, j));

      if (c != NULL)
      {
        c->disableAllChildItems();
        c->setEnabled(false);
      }
    }
  }
}

bool RDMnetNetworkItem::hasValidProperties(void) const
{
  return !device_reset_;
}

bool RDMnetNetworkItem::initiatePersonalityDescriptionSearch(uint8_t numberOfPersonalities)
{
  if (personalityDescriptions == NULL)
  {
    totalNumberOfDescriptions = numberOfPersonalities;
    personalityDescriptions = new QString[numberOfPersonalities];
    return true;
  }

  return false;
}

void RDMnetNetworkItem::personalityDescriptionFound(uint8_t personality, uint16_t /*footprint*/, const QString &description)
{
  if (personality <= totalNumberOfDescriptions)
  {
    personalityDescriptions[personality - 1] = description;
    ++numberOfDescriptionsFound;
  }
}

bool RDMnetNetworkItem::allPersonalityDescriptionsFound()
{
  return (numberOfDescriptionsFound >= totalNumberOfDescriptions) && (personalityDescriptions != NULL);
}

QStringList RDMnetNetworkItem::personalityDescriptionList()
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

QString RDMnetNetworkItem::personalityDescriptionAt(int i)
{
  return personalityDescriptions[i];
}

void RDMnetNetworkItem::setDeviceWasReset(bool reset)
{
  device_reset_ = reset;
}
