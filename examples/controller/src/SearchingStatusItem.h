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
#include <QStandardItem>
END_INCLUDE_QT_HEADERS()

class SearchingStatusItem : public QStandardItem
{
public:
  static const int SearchingStatusItemType = QStandardItem::UserType + 1;

private:
  bool allowDataChanges;
  bool searchInitiated;

public:
  SearchingStatusItem();
  virtual ~SearchingStatusItem();

  void setSearchInitiated(bool value) { searchInitiated = value; }
  bool wasSearchInitiated() { return searchInitiated; }

  virtual int type() const override;

  void setData(const QVariant& value, int role = Qt::UserRole + 1) Q_DECL_OVERRIDE;
};
