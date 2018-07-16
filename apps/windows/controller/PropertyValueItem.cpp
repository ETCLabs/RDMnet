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

#include "PropertyValueItem.h"

std::map<uint16_t, PIDInfo> PropertyValueItem::pidInfo;

bool PropertyValueItem::pidInfoExists(uint16_t pid)
{
  return pidInfo.find(pid) != pidInfo.end();
}

bool PropertyValueItem::pidSupportsGet(uint16_t pid)
{
  return pidInfo[pid].supportsGet;
}

bool PropertyValueItem::pidSupportsSet(uint16_t pid)
{
  return pidInfo[pid].supportsSet;
}

QVariant::Type PropertyValueItem::pidDataType(uint16_t pid)
{
  return pidInfo[pid].dataType;
}

int32_t PropertyValueItem::pidDataRole(uint16_t pid)
{
  return pidInfo[pid].role;
}

int32_t PropertyValueItem::pidDomainMin(uint16_t pid)
{
  return pidInfo[pid].rangeMin;
}

int32_t PropertyValueItem::pidDomainMax(uint16_t pid)
{
  return pidInfo[pid].rangeMax;
}

int32_t PropertyValueItem::pidMaxBufferSize(uint16_t pid)
{
  return pidInfo[pid].maxBufferSize;
}

QString PropertyValueItem::pidPropertyDisplayName(uint16_t pid, int32_t index)
{
  if ((index < 0) || (index >= pidInfo[pid].propertyDisplayNames.size()))
  {
    return QString();
  }

  return pidInfo[pid].propertyDisplayNames.at(index);
}

void PropertyValueItem::setPIDInfo(uint16_t pid, bool supportsGet, bool supportsSet, QVariant::Type dataType,
                                   int32_t role, PropertyLocation locationOfProperties)
{
  if (pidInfo.count(pid) == 0)  // Only allow writing the first time
  {
    pidInfo[pid].supportsGet = supportsGet;
    pidInfo[pid].supportsSet = supportsSet;
    pidInfo[pid].dataType = dataType;
    pidInfo[pid].role = role;
    pidInfo[pid].includedInDataModel = true;
    pidInfo[pid].locationOfProperties = locationOfProperties;
  }
}

void PropertyValueItem::setPIDInfo(uint16_t pid, bool supportsGet, bool supportsSet, QVariant::Type dataType,
                                   bool includedInDataModel)
{
  if (pidInfo.count(pid) == 0)  // Only allow writing the first time
  {
    pidInfo[pid].supportsGet = supportsGet;
    pidInfo[pid].supportsSet = supportsSet;
    pidInfo[pid].dataType = dataType;
    pidInfo[pid].role = Qt::EditRole;
    pidInfo[pid].includedInDataModel = includedInDataModel;
    pidInfo[pid].locationOfProperties = kResponder;
  }
}

void PropertyValueItem::setPIDNumericDomain(uint16_t pid, int32_t min, int32_t max)
{
  pidInfo[pid].rangeMin = min;
  pidInfo[pid].rangeMax = max;
}

void PropertyValueItem::setPIDMaxBufferSize(uint16_t pid, int32_t size)
{
  pidInfo[pid].maxBufferSize = size;
}

void PropertyValueItem::addPIDPropertyDisplayName(uint16_t pid, QString displayName)
{
  pidInfo[pid].propertyDisplayNames.push_back(displayName);
}

PIDInfoIterator PropertyValueItem::pidsBegin()
{
  return pidInfo.begin();
}

PIDInfoIterator PropertyValueItem::pidsEnd()
{
  return pidInfo.end();
}

PropertyValueItem::PropertyValueItem(const QVariant &value, bool writable) : RDMnetNetworkItem(value)
{
  setEditable(writable);
}

PropertyValueItem::PropertyValueItem(const QVariant &value, int role, bool writable) : RDMnetNetworkItem(value, role)
{
  if ((role == Qt::CheckStateRole) && writable)
  {
    setFlags(flags() | Qt::ItemIsUserCheckable);
  }
  else
  {
    setEditable(writable);
  }
}

PropertyValueItem::~PropertyValueItem()
{
}

void PropertyValueItem::setPID(uint16_t pid)
{
  pid_ = pid;
}

uint16_t PropertyValueItem::getPID()
{
  return pid_;
}

int PropertyValueItem::type() const
{
  return PropertyValueItemType;
}
