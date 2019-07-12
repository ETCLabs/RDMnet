/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "NetworkDetailsProxyModel.h"
#include "SearchingStatusItem.h"
#include "PropertyItem.h"

NetworkDetailsProxyModel::NetworkDetailsProxyModel()
{
  sourceNetworkModel = NULL;
  currentParentItem = NULL;
  filterEnabled = true;
  setDynamicSortFilter(true);
}

NetworkDetailsProxyModel::~NetworkDetailsProxyModel()
{
}

void NetworkDetailsProxyModel::setCurrentParentItem(const QStandardItem* item)
{
  currentParentItem = item;
  invalidate();  // invalidateFilter();
}

bool NetworkDetailsProxyModel::currentParentIsChildOfOrEqualTo(const QStandardItem* item)
{
  const QStandardItem* currentItem = currentParentItem;

  if ((item != NULL) && (currentParentItem != NULL))
  {
    while (currentItem != NULL)
    {
      if (currentItem == item)
      {
        return true;
      }

      currentItem = currentItem->parent();
    }
  }

  return false;
}

void NetworkDetailsProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
{
  QSortFilterProxyModel::setSourceModel(sourceModel);

  sourceNetworkModel = dynamic_cast<RDMnetNetworkModel*>(sourceModel);
}

void NetworkDetailsProxyModel::setFilterEnabled(bool setting)
{
  filterEnabled = setting;
}

bool NetworkDetailsProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  if (filterEnabled)
  {
    QModelIndex child = source_parent.child(source_row, 0);

    if (currentParentItem)
    {
      bool isChildOfCurrentParent = false;

      for (QModelIndex i = child.parent(); i.isValid() && !isChildOfCurrentParent; i = i.parent())
      {
        if (i == currentParentItem->index())
        {
          isChildOfCurrentParent = true;
        }
      }

      if (!isChildOfCurrentParent)
      {
        return true;
      }
    }

    if (sourceNetworkModel)
    {
      QStandardItem* childItem = sourceNetworkModel->itemFromIndex(child);

      if (childItem)
      {
        return (childItem->type() == PropertyItem::PropertyItemType);
      }
    }
  }

  return !filterEnabled;
}

bool NetworkDetailsProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
  QVariant leftData = sourceModel()->data(left);
  QVariant rightData = sourceModel()->data(right);

  return (leftData.toString() < rightData.toString());
}
