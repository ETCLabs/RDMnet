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

#include "rdm/cpp/uid.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QStandardItem>
END_INCLUDE_QT_HEADERS()

enum EditorWidgetType
{
  kComboBox,
  kButton,
  kDefault
};

enum SupportedDeviceFeature
{
  kNoSupport = 0x0,
  kResetDevice = 0x1,
  kIdentifyDevice = 0x2
};

inline SupportedDeviceFeature operator|(SupportedDeviceFeature a, SupportedDeviceFeature b)
{
  return static_cast<SupportedDeviceFeature>(static_cast<int>(a) | static_cast<int>(b));
}

inline SupportedDeviceFeature operator|=(SupportedDeviceFeature& a, SupportedDeviceFeature b)
{
  return a = (a | b);
}

inline SupportedDeviceFeature operator&(SupportedDeviceFeature a, SupportedDeviceFeature b)
{
  return static_cast<SupportedDeviceFeature>(static_cast<int>(a) & static_cast<int>(b));
}

Q_DECLARE_METATYPE(SupportedDeviceFeature)

class RDMnetNetworkItem : public QStandardItem
{
public:
  static const int RDMnetNetworkItemType = QStandardItem::UserType;
  static const int EditorWidgetTypeRole = Qt::UserRole + 1;
  static const int PersonalityNumberRole = Qt::UserRole + 2;
  static const int PersonalityDescriptionListRole = Qt::UserRole + 3;
  static const int ScopeDataRole = Qt::UserRole + 4;
  static const int CallbackObjectRole = Qt::UserRole + 5;
  static const int CallbackSlotRole = Qt::UserRole + 6;
  static const int ClientManuRole = Qt::UserRole + 7;
  static const int ClientDevRole = Qt::UserRole + 8;
  static const int ScopeSlotRole = Qt::UserRole + 9;
  static const int DisplayNameIndexRole = Qt::UserRole + 10;
  static const int StaticIPv4DataRole = Qt::UserRole + 11;
  static const int StaticIPv6DataRole = Qt::UserRole + 12;

protected:
  SupportedDeviceFeature supportedFeatures{kNoSupport};

  bool children_search_running_{false};

  QString* personalityDescriptions{nullptr};
  uint8_t  numberOfDescriptionsFound{0};
  uint8_t  totalNumberOfDescriptions{0};

  bool device_reset_{false};
  bool device_identifying_{false};

  bool rowHasSearchingStatusItem(int row);

public:
  RDMnetNetworkItem();
  explicit RDMnetNetworkItem(const QVariant& data);
  RDMnetNetworkItem(const QVariant& data, int role);
  virtual ~RDMnetNetworkItem();

  virtual int type() const override;

  bool childrenSearchRunning() const;
  bool supportsFeature(SupportedDeviceFeature feature) const;

  void enableChildrenSearch();
  void disableChildrenSearch();
  void enableFeature(SupportedDeviceFeature feature);
  // alsoRemoveFromThis: For every child row removed, remove the child item from alsoRemoveFromThis as well.
  void completelyRemoveChildren(int row, int count = 1, std::vector<class PropertyItem*>* alsoRemoveFromThis = NULL);
  void disableAllChildItems();

  virtual rdm::Uid uid() const { return rdm::Uid{}; }
  virtual bool     hasValidProperties(void) const;

  bool        initiatePersonalityDescriptionSearch(uint8_t numberOfPersonalities);
  void        personalityDescriptionFound(uint8_t personality, uint16_t footprint, const QString& description);
  bool        allPersonalityDescriptionsFound();
  QStringList personalityDescriptionList();
  QString     personalityDescriptionAt(int i);
  void        setDeviceWasReset(bool reset);
  void        setDeviceIdentifying(bool identifying);
  bool        identifying();

  std::vector<class PropertyItem*> properties;
};
