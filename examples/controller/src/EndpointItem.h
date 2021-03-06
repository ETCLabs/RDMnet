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

#pragma once

#include "rdm/defs.h"
#include "rdm/cpp/uid.h"
#include "ResponderItem.h"

class EndpointItem : public RDMnetNetworkItem
{
public:
  static const int EndpointItemType = QStandardItem::UserType + 4;

  EndpointItem(const rdm::Uid& parent_uid, uint16_t endpoint = 0, uint8_t type = E137_7_ENDPOINT_TYPE_VIRTUAL);
  virtual ~EndpointItem();

  virtual int     type() const override;
  uint16_t        id() const { return endpoint_; }
  const rdm::Uid& parent_uid() const { return parent_uid_; }

  bool operator==(const EndpointItem& other)
  {
    return ((parent_uid_ == other.parent_uid_) && (endpoint_ == other.endpoint_) && (type_ == other.type_));
  }

  std::vector<ResponderItem*> responders_;

private:
  rdm::Uid parent_uid_;
  uint16_t endpoint_;
  uint8_t  type_;
};
