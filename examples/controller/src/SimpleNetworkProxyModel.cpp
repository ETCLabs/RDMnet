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

#include "SimpleNetworkProxyModel.h"
#include "PropertyItem.h"

void SimpleNetworkProxyModel::directChildrenRevealed(const QModelIndex& parentIndex)
{
  emit expanded(mapToSource(parentIndex));
}

SimpleNetworkProxyModel::SimpleNetworkProxyModel() : sourceNetworkModel(NULL)
{
  setDynamicSortFilter(true);
}

SimpleNetworkProxyModel::~SimpleNetworkProxyModel()
{
}

// QModelIndex SimpleNetworkProxyModel::mapToSource(const QModelIndex &proxyIndex) const
//{
//
//}
//
// QModelIndex SimpleNetworkProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
//{
//
//}
//
// QModelIndex SimpleNetworkProxyModel::index(int row, int column, const QModelIndex &parent) const
//{
//
//}
//
// QModelIndex SimpleNetworkProxyModel::parent(const QModelIndex &child) const
//{
//
//}
//
// int SimpleNetworkProxyModel::rowCount(const QModelIndex &parent) const
//{
//
//}
//
// int SimpleNetworkProxyModel::columnCount(const QModelIndex &parent) const
//{
//	return QSortFilterProxyModel::columnCount(parent);// 1;
//}
//
// bool SimpleNetworkProxyModel::hasChildren(const QModelIndex &parent) const
//{
//	return QSortFilterProxyModel::hasChildren(parent);// return true;
//}

void SimpleNetworkProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
{
  QSortFilterProxyModel::setSourceModel(sourceModel);

  sourceNetworkModel = dynamic_cast<RDMnetNetworkModel*>(sourceModel);
}

bool SimpleNetworkProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  if (source_parent.isValid())
  {
    if (sourceNetworkModel)
    {
      QStandardItem* item = sourceNetworkModel->itemFromIndex(source_parent);

      if (item)
      {
        QStandardItem* child = item->child(source_row);

        if (child)
        {
          if (child->type() == PropertyItem::PropertyItemType)
            return false;
        }
      }
    }
  }

  return true;
}
