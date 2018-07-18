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

#include <QStandardItem>

enum EditorWidgetType
{
  EWT_COMBOBOX,
  EWT_DEFAULT
};

class RDMnetNetworkItem : public QStandardItem
{
public:
  static const int RDMnetNetworkItemType = QStandardItem::UserType;
  static const int EditorWidgetTypeRole = Qt::UserRole + 1;

protected:
  bool children_search_running_;
  bool supports_reset_device_;

  QString *personalityDescriptions;
  uint8_t numberOfDescriptionsFound;
  uint8_t totalNumberOfDescriptions;

  bool device_reset_;

  bool rowHasSearchingStatusItem(int row);

public:
  RDMnetNetworkItem();
  RDMnetNetworkItem(const QVariant &data);
  RDMnetNetworkItem(const QVariant &data, int role);
  virtual ~RDMnetNetworkItem();

  virtual int type() const override;

  bool childrenSearchRunning() const;
  bool supportsResetDevice() const;

  void enableChildrenSearch();
  void disableChildrenSearch();
  void enableResetDevice();
  void completelyRemoveChildren(int row, int count = 1);
  void disableAllChildItems();

  virtual uint16_t getMan(void) const { return 0; };
  virtual uint32_t getDev(void) const { return 0; };
  virtual bool hasValidProperties(void) const;

  bool initiatePersonalityDescriptionSearch(uint8_t numberOfPersonalities);
  void personalityDescriptionFound(uint8_t personality, uint16_t footprint, const QString &description);
  bool allPersonalityDescriptionsFound();
  QStringList personalityDescriptionList();
  QString personalityDescriptionAt(int i);
  void setDeviceWasReset(bool reset);

  std::vector<class PropertyItem *> properties;
};
