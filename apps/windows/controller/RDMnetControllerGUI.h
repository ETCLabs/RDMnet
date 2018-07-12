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

#include <QMainWindow>
#include <QItemSelection>
#include "ui_RDMnetControllerGUI.h"

#include "BrokerStaticAddGUI.h"

class RDMnetControllerGUI : public QMainWindow, public IHandlesBrokerStaticAdd
{
  Q_OBJECT

public:
  static RDMnetControllerGUI *makeRDMnetControllerGUI();

  ~RDMnetControllerGUI();

  virtual void handleAddBrokerByIP(std::string scope, const LwpaSockaddr &addr);

public slots:

  void networkTreeViewSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
  void detailsTreeViewActivated(const QModelIndex &index);
  void addScopeTriggered();
  void removeSelectedBrokerTriggered();
  void removeAllBrokersTriggered();
  void resetDeviceTriggered();
  void openBrokerStaticAddDialog();
  void processBrokerItemTextUpdate(const class BrokerItem *item);
  void processResetDeviceSupportChanged(const class RDMnetNetworkItem *item);
  void dataChangeTest1(QModelIndex a, QModelIndex b, QVector<int> c);
  void dataChangeTest2(QModelIndex a, QModelIndex b, QVector<int> c);

signals:

  void addScopeActivated(std::string scope);
  void removeSelectedBrokerActivated(class BrokerItem *brokerItem);
  void removeAllBrokersActivated();
  void resetDeviceActivated(class ResponderItem *device);
  void addBrokerByIPActivated(std::string scope, const LwpaSockaddr &addr);

private:
  RDMnetControllerGUI(QWidget *parent = Q_NULLPTR);

  Ui::RDMnetControllerGUIClass ui;

  class RDMnetNetworkModel *main_network_model_;
  class SimpleNetworkProxyModel *simple_net_proxy_;
  class NetworkDetailsProxyModel *net_details_proxy_;
  class BrokerItem *currently_selected_broker_item_;
  class RDMnetNetworkItem *currently_selected_network_item_;
};
