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

#include "SearchingStatusItem.h"

SearchingStatusItem::SearchingStatusItem()
{
  allowDataChanges = true;

  this->setData(QObject::tr("Searching..."), Qt::DisplayRole);
  this->setFlags(Qt::NoItemFlags);

  allowDataChanges = false;

  searchInitiated = false;
}

SearchingStatusItem::~SearchingStatusItem()
{
}

int SearchingStatusItem::type() const
{
  return SearchingStatusItemType;
}

void SearchingStatusItem::setData(const QVariant& value, int role)
{
  if (allowDataChanges)
  {
    QStandardItem::setData(value, role);
  }
}
