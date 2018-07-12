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

#pragma once

#include "estardm.h"
#include "lwpa_uid.h"
#include "rdmnet/cpputil.h"
#include "ResponderItem.h"

class EndpointItem : public RDMnetNetworkItem
{
public:
  static const int EndpointItemType = QStandardItem::UserType + 4;

  // EndpointItem(uint16_t endpoint = 0, uint8_t type = VIRTUAL_ENDPOINT) {
  // endpoint_ = endpoint; };
  EndpointItem(uint16_t manufacturer, uint32_t parentDeviceID, uint16_t endpoint = 0,
               uint8_t type = E137_7_ENDPOINT_TYPE_VIRTUAL);
  virtual ~EndpointItem();

  virtual int type() const override;

  bool operator==(const EndpointItem &other)
  {
    return (parent_uid_ == other.parent_uid_) && (endpoint_ == other.endpoint_) && (type_ == other.type_);
  }

  LwpaUid parent_uid_;
  uint16_t endpoint_;
  uint8_t type_;
  std::vector<ResponderItem *> devices_;
};
