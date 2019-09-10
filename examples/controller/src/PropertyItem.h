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

#include "RDMnetClientItem.h"
#include "PropertyValueItem.h"

class PropertyItem : public RDMnetNetworkItem
{
public:
  static const int PropertyItemType = QStandardItem::UserType + 6;

  PropertyItem(const QString& fullName, const QString& displayText);
  virtual ~PropertyItem();

  virtual int type() const override;

  PropertyValueItem* getValueItem();
  void setValueItem(PropertyValueItem* item, bool deleteItemArgumentIfCopied = true);

  QString getFullName();

protected:
  PropertyValueItem* m_ValueItem;
  QString m_FullName;
};
