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

#include "RDMnetNetworkItem.h"
#include "SearchingStatusItem.h"

bool RDMnetNetworkItem::rowHasSearchingStatusItem(int row)
{
  QStandardItem* current = this->child(row);

  if (current != NULL)
  {
    return (current->type() == SearchingStatusItem::SearchingStatusItemType);
  }

  return false;
}

RDMnetNetworkItem::RDMnetNetworkItem()
{
  setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

  setData(EditorWidgetType::kDefault, EditorWidgetTypeRole);
}

RDMnetNetworkItem::RDMnetNetworkItem(const QVariant& data)
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

RDMnetNetworkItem::RDMnetNetworkItem(const QVariant& data, int role)
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

bool RDMnetNetworkItem::supportsFeature(SupportedDeviceFeature feature) const
{
  return supportedFeatures & feature;
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

void RDMnetNetworkItem::enableFeature(SupportedDeviceFeature feature)
{
  supportedFeatures |= feature;
}

void RDMnetNetworkItem::completelyRemoveChildren(int                               row,
                                                 int                               count,
                                                 std::vector<class PropertyItem*>* alsoRemoveFromThis)
{
  for (int i = row; i < (row + count); ++i)
  {
    RDMnetNetworkItem* c = dynamic_cast<RDMnetNetworkItem*>(child(i));

    if (c != NULL)
    {
      c->completelyRemoveChildren(0, c->rowCount(), alsoRemoveFromThis);
    }

    if (alsoRemoveFromThis != NULL)
    {
      class PropertyItem* toRemove = reinterpret_cast<class PropertyItem*>(c);
      alsoRemoveFromThis->erase(std::remove(alsoRemoveFromThis->begin(), alsoRemoveFromThis->end(), toRemove),
                                alsoRemoveFromThis->end());
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
      RDMnetNetworkItem* c = dynamic_cast<RDMnetNetworkItem*>(child(i, j));

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
  if (!personalityDescriptions)
  {
    totalNumberOfDescriptions = numberOfPersonalities;
    personalityDescriptions = new QString[numberOfPersonalities];
    return true;
  }

  return false;
}

void RDMnetNetworkItem::personalityDescriptionFound(uint8_t personality,
                                                    uint16_t /*footprint*/,
                                                    const QString& description)
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

void RDMnetNetworkItem::setDeviceIdentifying(bool identifying)
{
  device_identifying_ = identifying;
}

bool RDMnetNetworkItem::identifying()
{
  return device_identifying_;
}
