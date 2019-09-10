/******************************************************************************
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
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

#pragma once

#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QSortFilterProxyModel>
END_INCLUDE_QT_HEADERS()

#include "RDMnetNetworkModel.h"

class NetworkDetailsProxyModel : public QSortFilterProxyModel
{
  Q_OBJECT

private:
  RDMnetNetworkModel* sourceNetworkModel;

public:
  NetworkDetailsProxyModel();
  ~NetworkDetailsProxyModel();

  void setCurrentParentItem(const QStandardItem* item);

  bool currentParentIsChildOfOrEqualTo(const QStandardItem* item);

  void setSourceModel(QAbstractItemModel* sourceModel) Q_DECL_OVERRIDE;

  void setFilterEnabled(bool setting);

protected:
  virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
  virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
  const QStandardItem* currentParentItem;
  bool filterEnabled;
};
