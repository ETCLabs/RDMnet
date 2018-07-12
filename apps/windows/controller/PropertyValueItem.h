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

#include "RDMnetClientItem.h"
#include <map>

#include <QVariant>

enum PropertyLocation
{
  kResponder = 1,
  kEndpoint = 2,
  kDevice = 4,
  kController = 8,
  kBroker = 16
};

inline PropertyLocation operator|(PropertyLocation a, PropertyLocation b)
{
  return static_cast<PropertyLocation>(static_cast<int>(a) | static_cast<int>(b));
}

struct PIDInfo
{
  bool supportsGet;
  bool supportsSet;
  bool includedInDataModel;
  QVariant::Type dataType;
  int32_t role;

  int32_t rangeMin;
  int32_t rangeMax;
  int32_t maxBufferSize;

  QStringList propertyDisplayNames;

  PropertyLocation locationOfProperties;
};

typedef std::map<uint16_t, PIDInfo>::iterator PIDInfoIterator;

class PropertyValueItem : public RDMnetNetworkItem
{
public:
  static const int PropertyValueItemType = QStandardItem::UserType + 7;

  static bool pidSupportsGet(uint16_t pid);
  static bool pidSupportsSet(uint16_t pid);
  static QVariant::Type pidDataType(uint16_t pid);
  static int32_t pidDataRole(uint16_t pid);
  static int32_t pidDomainMin(uint16_t pid);
  static int32_t pidDomainMax(uint16_t pid);
  static int32_t pidMaxBufferSize(uint16_t pid);
  static QString pidPropertyDisplayName(uint16_t pid, int32_t index = 0);

  static void setPIDInfo(uint16_t pid, bool supportsGet, bool supportsSet, QVariant::Type dataType, int32_t role,
                         PropertyLocation locationOfProperties = kResponder);
  static void setPIDInfo(uint16_t pid, bool supportsGet, bool supportsSet, QVariant::Type dataType,
                         bool includedInDataModel = true);
  static void setPIDNumericDomain(uint16_t pid, int32_t min, int32_t max);
  static void setPIDMaxBufferSize(uint16_t pid, int32_t size);
  static void addPIDPropertyDisplayName(uint16_t pid, QString displayName);

  static PIDInfoIterator pidsBegin();
  static PIDInfoIterator pidsEnd();

  PropertyValueItem(const QVariant &value, bool writable);
  PropertyValueItem(const QVariant &value, int role, bool writable);
  virtual ~PropertyValueItem();

  virtual int type() const override;

  void setPID(uint16_t pid);
  uint16_t getPID();

protected:
  static std::map<uint16_t, PIDInfo> pidInfo;

  uint16_t pid_;
};
