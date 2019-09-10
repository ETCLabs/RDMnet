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

#include "EndpointItem.h"

// EndpointItem::EndpointItem()
//{
//}

EndpointItem::EndpointItem(const RdmUid& parent_uid, uint16_t endpoint, uint8_t type)
    : RDMnetNetworkItem(QString("%0 (%1)").arg(endpoint).arg(
          (endpoint == 0) ? "Default Responder" : ((type == E137_7_ENDPOINT_TYPE_VIRTUAL) ? "virtual" : "physical")))
    , parent_uid_(parent_uid)
    , endpoint_(endpoint)
    , type_(type)
{
}

EndpointItem::~EndpointItem()
{
}

int EndpointItem::type() const
{
  return EndpointItemType;
}
