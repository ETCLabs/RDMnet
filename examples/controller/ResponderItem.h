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

#include "RDMnetNetworkItem.h"

class ResponderItem : public RDMnetNetworkItem
{
public:
  static const int ResponderItemType = QStandardItem::UserType + 5;

  // ResponderItem(uint16_t man, uint32_t dev) { m_Man = man; m_Dev = dev; m_HaveInfo = false; };
  ResponderItem(uint16_t man, uint32_t dev);

  virtual uint16_t getMan(void) const { return m_Man; };
  virtual uint32_t getDev(void) const { return m_Dev; };

  virtual int type() const override;

  bool operator==(const ResponderItem &other) { return (m_Man == other.m_Man) && (m_Dev == other.m_Dev); }

private:
  uint16_t m_Man;  // RDM Manufacturer ID
  uint32_t m_Dev;  // RDM Device ID
};
