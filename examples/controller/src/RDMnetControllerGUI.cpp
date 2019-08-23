/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "RDMnetControllerGUI.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QPalette>
END_INCLUDE_QT_HEADERS()

#include "RDMnetNetworkModel.h"
#include "NetworkDetailsProxyModel.h"
#include "SimpleNetworkProxyModel.h"
#include "PropertyEditorsDelegate.h"
#include "PropertyItem.h"
#include "LogWindowGUI.h"
#include "AboutGUI.h"

#include "rdmnet/version.h"

RDMnetControllerGUI::~RDMnetControllerGUI()
{
  if (net_details_proxy_ != nullptr)
  {
    delete net_details_proxy_;
  }

  if (simple_net_proxy_ != nullptr)
  {
    delete simple_net_proxy_;
  }

  if (main_network_model_ != nullptr)
  {
    delete main_network_model_;
  }

  if (rdmnet_library_ != nullptr)
  {
    delete rdmnet_library_;
  }

  if (log_ != nullptr)
  {
    delete log_;
  }
}

void RDMnetControllerGUI::handleAddBrokerByIP(QString scope, const LwpaSockaddr& addr)
{
  emit addBrokerByIPActivated(scope, addr);
}

RDMnetControllerGUI* RDMnetControllerGUI::makeRDMnetControllerGUI()
{
  RDMnetControllerGUI* gui = new RDMnetControllerGUI;
  QHeaderView* networkTreeHeaderView = NULL;

  gui->log_ = new ControllerLog("RDMnetController.log");
  gui->rdmnet_library_ = new RDMnetLibWrapper(gui->log_);

  gui->main_network_model_ =
      RDMnetNetworkModel::makeRDMnetNetworkModel(gui->rdmnet_library_, gui->log_);  // makeTestModel();
  gui->simple_net_proxy_ = new SimpleNetworkProxyModel;
  gui->net_details_proxy_ = new NetworkDetailsProxyModel;

  gui->simple_net_proxy_->setSourceModel(gui->main_network_model_);
  gui->net_details_proxy_->setSourceModel(gui->main_network_model_);

  gui->ui.networkTreeView->setModel(gui->simple_net_proxy_);
  gui->ui.detailsTreeView->setModel(gui->net_details_proxy_);

  gui->ui.detailsTreeView->header()->resizeSection(0, 200);
  gui->ui.detailsTreeView->setItemDelegate(new PropertyEditorsDelegate());
  gui->ui.detailsTreeView->setSortingEnabled(true);
  gui->ui.detailsTreeView->sortByColumn(0, Qt::SortOrder::AscendingOrder);

  gui->setWindowTitle(tr("RDMnet Controller GUI"));

  networkTreeHeaderView = gui->ui.networkTreeView->header();
  networkTreeHeaderView->hideSection(1);
  networkTreeHeaderView->setSectionResizeMode(0, QHeaderView::Fixed);

  qRegisterMetaType<SupportedDeviceFeature>("SupportedDeviceFeature");

  connect(gui->ui.networkTreeView->selectionModel(),
          SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), gui,
          SLOT(networkTreeViewSelectionChanged(const QItemSelection&, const QItemSelection&)));

  connect(gui->ui.addBrokerByScopeButton, SIGNAL(clicked()), gui, SLOT(addScopeTriggered()));
  connect(gui->ui.newScopeNameEdit, SIGNAL(returnPressed()), gui, SLOT(addScopeTriggered()));
  connect(gui, SIGNAL(addScopeActivated(QString)), gui->main_network_model_, SLOT(addScopeToMonitor(QString)));

  connect(gui->ui.removeSelectedBrokerButton, SIGNAL(clicked()), gui, SLOT(removeSelectedBrokerTriggered()));
  connect(gui, SIGNAL(removeSelectedBrokerActivated(BrokerItem*)), gui->main_network_model_,
          SLOT(removeBroker(BrokerItem*)));

  connect(gui->ui.removeAllBrokersButton, SIGNAL(clicked()), gui, SLOT(removeAllBrokersTriggered()));
  connect(gui, SIGNAL(removeAllBrokersActivated()), gui->main_network_model_, SLOT(removeAllBrokers()));

  connect(gui->ui.resetDeviceButton, SIGNAL(clicked()), gui, SLOT(resetDeviceTriggered()));
  connect(gui->ui.identifyDeviceButton, SIGNAL(clicked()), gui, SLOT(identifyDeviceTriggered()));
  connect(gui, SIGNAL(featureActivated(RDMnetNetworkItem*, SupportedDeviceFeature)), gui->main_network_model_,
          SLOT(activateFeature(RDMnetNetworkItem*, SupportedDeviceFeature)));

  connect(gui->ui.networkTreeView, SIGNAL(expanded(const QModelIndex&)), gui->simple_net_proxy_,
          SLOT(directChildrenRevealed(const QModelIndex&)));
  connect(gui->simple_net_proxy_, SIGNAL(expanded(const QModelIndex&)), gui->main_network_model_,
          SLOT(directChildrenRevealed(const QModelIndex&)));

  connect(gui->ui.moreBrokerSettingsButton, SIGNAL(clicked()), gui, SLOT(openBrokerStaticAddDialog()));

  connect(gui, SIGNAL(addBrokerByIPActivated(QString, const LwpaSockaddr&)), gui->main_network_model_,
          SLOT(addBrokerByIP(QString, const LwpaSockaddr&)));

  connect(gui->main_network_model_, SIGNAL(brokerItemTextUpdated(const BrokerItem*)), gui,
          SLOT(processBrokerItemTextUpdate(const BrokerItem*)));

  connect(gui->main_network_model_,
          SIGNAL(featureSupportChanged(const class RDMnetNetworkItem*, SupportedDeviceFeature)), gui,
          SLOT(processFeatureSupportChange(const class RDMnetNetworkItem*, SupportedDeviceFeature)));

  connect(gui->main_network_model_, SIGNAL(expandNewItem(const QModelIndex&, int)), gui,
          SLOT(expandNewItem(const QModelIndex&, int)));

  connect(gui->main_network_model_, SIGNAL(identifyChanged(const RDMnetNetworkItem*, bool)), gui,
          SLOT(identifyChanged(const RDMnetNetworkItem*, bool)));

  connect(gui->ui.actionLogWindow, SIGNAL(triggered()), gui, SLOT(openLogWindowDialog()));
  connect(gui->ui.actionExit, SIGNAL(triggered()), gui, SLOT(exitApplication()));
  connect(gui->ui.actionAbout, SIGNAL(triggered()), gui, SLOT(openAboutDialog()));

  gui->main_network_model_->addScopeToMonitor(E133_DEFAULT_SCOPE);

  return gui;
}

void RDMnetControllerGUI::networkTreeViewSelectionChanged(const QItemSelection& selected,
                                                          const QItemSelection& /*deselected*/)
{
  if (!selected.indexes().isEmpty())
  {
    QModelIndex selectedIndex = selected.indexes().first();

    if (selectedIndex.isValid())
    {
      QModelIndex sourceIndex, proxyIndex;
      QStandardItem* selectedItem = NULL;
      RDMnetNetworkItem* netItem = NULL;

      sourceIndex = simple_net_proxy_->mapToSource(selectedIndex);
      selectedItem = main_network_model_->itemFromIndex(sourceIndex);
      ui.detailsTreeView->clearSelection();
      net_details_proxy_->setCurrentParentItem(selectedItem);

      proxyIndex = net_details_proxy_->mapFromSource(sourceIndex);
      ui.detailsTreeView->setRootIndex(proxyIndex);

      netItem = dynamic_cast<RDMnetNetworkItem*>(selectedItem);

      if (netItem != NULL)
      {
        currently_selected_network_item_ = netItem;
        ui.resetDeviceButton->setEnabled(netItem->supportsFeature(kResetDevice));
        ui.identifyDeviceButton->setEnabled(netItem->supportsFeature(kIdentifyDevice));

        identifyChanged(netItem, netItem->identifying());
      }

      if (selectedItem->type() == BrokerItem::BrokerItemType)
      {
        currently_selected_broker_item_ = dynamic_cast<BrokerItem*>(selectedItem);
        ui.removeSelectedBrokerButton->setEnabled(true);
      }
      else
      {
        currently_selected_broker_item_ = NULL;
        ui.removeSelectedBrokerButton->setEnabled(false);
      }

      ui.currentSelectionLabel->setText(selectedItem->text());
    }
    else
    {
      currently_selected_broker_item_ = NULL;
      ui.removeSelectedBrokerButton->setEnabled(false);
      ui.currentSelectionLabel->setText(QString(""));
    }
  }
  else
  {
    currently_selected_broker_item_ = NULL;
    ui.removeSelectedBrokerButton->setEnabled(false);
    ui.currentSelectionLabel->setText(QString(""));
  }
}

void RDMnetControllerGUI::addScopeTriggered()
{
  emit addScopeActivated(ui.newScopeNameEdit->text());
  ui.newScopeNameEdit->clear();
}

void RDMnetControllerGUI::removeSelectedBrokerTriggered()
{
  if (currently_selected_broker_item_ != NULL)
  {
    if (net_details_proxy_->currentParentIsChildOfOrEqualTo(currently_selected_broker_item_))
    {
      ui.detailsTreeView->clearSelection();

      net_details_proxy_->setFilterEnabled(false);
      net_details_proxy_->setCurrentParentItem(NULL);
    }

    emit removeSelectedBrokerActivated(currently_selected_broker_item_);

    net_details_proxy_->setFilterEnabled(true);
    net_details_proxy_->invalidate();
  }
}

void RDMnetControllerGUI::removeAllBrokersTriggered()
{
  emit removeAllBrokersActivated();

  ui.networkTreeView->clearSelection();
  ui.detailsTreeView->clearSelection();
  ui.detailsTreeView->reset();

  net_details_proxy_->setFilterEnabled(false);
  net_details_proxy_->setCurrentParentItem(NULL);

  currently_selected_broker_item_ = NULL;

  net_details_proxy_->setFilterEnabled(true);
  net_details_proxy_->invalidate();

  ui.currentSelectionLabel->setText(QString(""));
}

void RDMnetControllerGUI::resetDeviceTriggered()
{
  if (currently_selected_network_item_ != NULL)
  {
    emit featureActivated(currently_selected_network_item_, kResetDevice);
  }
}

void RDMnetControllerGUI::identifyDeviceTriggered()
{
  if (currently_selected_network_item_ != NULL)
  {
    emit featureActivated(currently_selected_network_item_, kIdentifyDevice);
  }
}

void RDMnetControllerGUI::openBrokerStaticAddDialog()
{
  BrokerStaticAddGUI* brokerStaticAddDialog = new BrokerStaticAddGUI(this, this);
  brokerStaticAddDialog->setAttribute(Qt::WA_DeleteOnClose);
  brokerStaticAddDialog->setWindowModality(Qt::WindowModal);
  brokerStaticAddDialog->setWindowTitle(tr("Add Broker by Static IP"));
  brokerStaticAddDialog->show();
}

void RDMnetControllerGUI::openLogWindowDialog()
{
  if (main_network_model_ != NULL)
  {
    if (main_network_model_->getNumberOfCustomLogOutputStreams() == 0)
    {
      LogWindowGUI* logWindowDialog = new LogWindowGUI(this);
      logWindowDialog->setAttribute(Qt::WA_DeleteOnClose);
      logWindowDialog->setWindowTitle(tr("Log Window"));
      logWindowDialog->resize(QSize(static_cast<int>(logWindowDialog->width() * 1.2), logWindowDialog->height()));
      logWindowDialog->show();

      log_->addCustomOutputStream(logWindowDialog);
    }
  }
}

void RDMnetControllerGUI::openAboutDialog()
{
  AboutGUI* aboutDialog = new AboutGUI(this, QT_VERSION_STR, RDMNET_VERSION_STRING);
  aboutDialog->setAttribute(Qt::WA_DeleteOnClose);
  aboutDialog->setWindowModality(Qt::WindowModal);
  aboutDialog->setWindowTitle(tr("About"));
  aboutDialog->setFixedSize(QSize(410, aboutDialog->size().height()));
  aboutDialog->show();
}

void RDMnetControllerGUI::processBrokerItemTextUpdate(const BrokerItem* item)
{
  if (item != NULL)
  {
    if (item == currently_selected_broker_item_)
    {
      ui.currentSelectionLabel->setText(item->text());
    }
  }
}

void RDMnetControllerGUI::processFeatureSupportChange(const RDMnetNetworkItem* item, SupportedDeviceFeature feature)
{
  if (currently_selected_network_item_ != NULL)
  {
    if (currently_selected_network_item_ == item)
    {
      if (feature & kResetDevice)
      {
        ui.resetDeviceButton->setEnabled(item->supportsFeature(kResetDevice) && item->isEnabled());
      }

      if (feature & kIdentifyDevice)
      {
        ui.identifyDeviceButton->setEnabled(item->supportsFeature(kIdentifyDevice) && item->isEnabled());
      }
    }
  }
}

void RDMnetControllerGUI::expandNewItem(const QModelIndex& index, int type)
{
  switch (type)
  {
    case PropertyItem::PropertyItemType:
      ui.detailsTreeView->expand(net_details_proxy_->mapFromSource(index));
      break;
    default:
      ui.networkTreeView->expand(simple_net_proxy_->mapFromSource(index));
  }
}

void RDMnetControllerGUI::identifyChanged(const RDMnetNetworkItem* item, bool identify)
{
  if (currently_selected_network_item_ != NULL)
  {
    if (currently_selected_network_item_ == item)
    {
      ui.identifyDeviceButton->setStyleSheet(identify ? "QPushButton {background-color: red}" : "");
      ui.identifyDeviceButton->setText(identify ? "Stop Identifying" : "Identify Device");
    }
  }
}

void RDMnetControllerGUI::exitApplication()
{
  QApplication::quit();
}

RDMnetControllerGUI::RDMnetControllerGUI(QWidget* parent)
    : QMainWindow(parent)
    , main_network_model_(NULL)
    , simple_net_proxy_(NULL)
    , net_details_proxy_(NULL)
    , currently_selected_broker_item_(NULL)
    , currently_selected_network_item_(NULL)
{
  ui.setupUi(this);
}
