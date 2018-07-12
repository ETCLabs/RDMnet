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

void RDMnetNetworkItem::updateParentWithLocalChanges(bool hadLocalChangesPreviously)
{
  bool hasLocalChangesCurrently = this->hasLocalChanges();

  if (hasLocalChangesCurrently != hadLocalChangesPreviously)
  {
    QStandardItem *parent = this->parent();

    if (parent != NULL)
    {
      if (parent->type() == RDMnetNetworkItemType)
      {
        RDMnetNetworkItem *castedParent = dynamic_cast<RDMnetNetworkItem *>(parent);

        if (hasLocalChangesCurrently == true)  // hasLocalChanges() just became true.
        {
          castedParent->incrementNumberOfChildrenWithLocalChanges();
        }
        else  // hasLocalChanges() just became false.
        {
          castedParent->decrementNumberOfChildrenWithLocalChanges();
        }
      }
    }
  }
}

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
    : self_has_local_changes_(false)
    , children_search_running_(false)
    , supports_reset_device_(false)
    , num_children_with_local_changes_(0)
{
  setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

  setData(EditorWidgetType::EWT_DEFAULT, EditorWidgetTypeRole);
}

RDMnetNetworkItem::RDMnetNetworkItem(const QVariant &data)
    : QStandardItem()
    , self_has_local_changes_(false)
    , children_search_running_(false)
    , supports_reset_device_(false)
    , num_children_with_local_changes_(0)
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

  setData(EditorWidgetType::EWT_DEFAULT, EditorWidgetTypeRole);
}

RDMnetNetworkItem::RDMnetNetworkItem(const QVariant &data, int role)
    : QStandardItem()
    , self_has_local_changes_(false)
    , children_search_running_(false)
    , supports_reset_device_(false)
    , num_children_with_local_changes_(0)
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

  setData(EditorWidgetType::EWT_DEFAULT, EditorWidgetTypeRole);
}

RDMnetNetworkItem::~RDMnetNetworkItem()
{
}

int RDMnetNetworkItem::type() const
{
  return RDMnetNetworkItemType;
}

bool RDMnetNetworkItem::hasLocalChanges() const
{
  return self_has_local_changes_ || (num_children_with_local_changes_ > 0);
}

bool RDMnetNetworkItem::childrenSearchRunning() const
{
  return children_search_running_;
}

bool RDMnetNetworkItem::supportsResetDevice() const
{
  return supports_reset_device_;
}

int RDMnetNetworkItem::numberOfChildrenWithLocalChanges() const
{
  return num_children_with_local_changes_;
}

void RDMnetNetworkItem::setSelfHasLocalChanges(bool value)
{
  if (self_has_local_changes_ != value)
  {
    bool previousHasLocalChanges = this->hasLocalChanges();

    self_has_local_changes_ = value;

    updateParentWithLocalChanges(previousHasLocalChanges);
  }
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

void RDMnetNetworkItem::incrementNumberOfChildrenWithLocalChanges()
{
  bool previousHasLocalChanges = this->hasLocalChanges();

  ++num_children_with_local_changes_;

  updateParentWithLocalChanges(previousHasLocalChanges);
}

void RDMnetNetworkItem::decrementNumberOfChildrenWithLocalChanges()
{
  if (num_children_with_local_changes_ >= 1)  // Assumes that hasLocalChanges() is true at this point.
  {
    --num_children_with_local_changes_;

    updateParentWithLocalChanges(true);
  }
}
