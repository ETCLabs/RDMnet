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

#include "RDMnetControllerGUI.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QPalette>
END_INCLUDE_QT_HEADERS()

#include <functional>
#include "RDMnetNetworkModel.h"
#include "NetworkDetailsProxyModel.h"
#include "SimpleNetworkProxyModel.h"
#include "PropertyEditorsDelegate.h"
#include "PropertyItem.h"
#include "LogWindowGUI.h"
#include "SendCommandGUI.h"
#include "AboutGUI.h"
#include "etcpal/version.h"
#include "rdmnet/version.h"

void RDMnetControllerGUI::HandleAddBrokerByIP(QString scope, const etcpal::SockAddr& addr)
{
  emit addBrokerByIPActivated(scope, addr);
}

RDMnetControllerGUI* RDMnetControllerGUI::MakeRDMnetControllerGUI()
{
  RDMnetControllerGUI* gui = new RDMnetControllerGUI;
  QHeaderView*         networkTreeHeaderView = NULL;

  gui->log_ = std::make_unique<ControllerLog>();

  rdmnet::Init(gui->log_->logger());

  gui->main_network_model_ =
      RDMnetNetworkModel::MakeRDMnetNetworkModel(gui->rdmnet_controller_, gui->log_->logger());  // makeTestModel();
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

  connect(gui->ui.networkTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, gui,
          &RDMnetControllerGUI::networkTreeViewSelectionChanged);

  connect(gui->ui.addBrokerByScopeButton, &QPushButton::clicked, gui, &RDMnetControllerGUI::addScopeTriggered);
  connect(gui->ui.newScopeNameEdit, &QLineEdit::returnPressed, gui, &RDMnetControllerGUI::addScopeTriggered);
  connect(gui, &RDMnetControllerGUI::addScopeActivated, gui->main_network_model_,
          &RDMnetNetworkModel::addScopeToMonitor);

  connect(gui->ui.removeSelectedBrokerButton, &QPushButton::clicked, gui,
          &RDMnetControllerGUI::removeSelectedBrokerTriggered);
  connect(gui, &RDMnetControllerGUI::removeSelectedBrokerActivated, gui->main_network_model_,
          &RDMnetNetworkModel::removeBroker);

  connect(gui->ui.removeAllBrokersButton, &QPushButton::clicked, gui, &RDMnetControllerGUI::removeAllBrokersTriggered);
  connect(gui, &RDMnetControllerGUI::removeAllBrokersActivated, gui->main_network_model_,
          &RDMnetNetworkModel::removeAllBrokers);

  connect(gui->ui.resetDeviceButton, &QPushButton::clicked, gui, &RDMnetControllerGUI::resetDeviceTriggered);
  connect(gui->ui.identifyDeviceButton, &QPushButton::clicked, gui, &RDMnetControllerGUI::identifyDeviceTriggered);
  connect(gui, &RDMnetControllerGUI::featureActivated, gui->main_network_model_, &RDMnetNetworkModel::activateFeature);

  connect(gui->ui.networkTreeView, &QTreeView::expanded, gui->simple_net_proxy_,
          &SimpleNetworkProxyModel::directChildrenRevealed);
  connect(gui->simple_net_proxy_, &SimpleNetworkProxyModel::expanded, gui->main_network_model_,
          &RDMnetNetworkModel::directChildrenRevealed);

  connect(gui->ui.moreBrokerSettingsButton, &QPushButton::clicked, gui,
          &RDMnetControllerGUI::openBrokerStaticAddDialog);

  connect(gui, &RDMnetControllerGUI::addBrokerByIPActivated, gui->main_network_model_,
          &RDMnetNetworkModel::addBrokerByIP);

  connect(gui->main_network_model_, &RDMnetNetworkModel::brokerItemTextUpdated, gui,
          &RDMnetControllerGUI::processBrokerItemTextUpdate);

  connect(gui->main_network_model_, &RDMnetNetworkModel::featureSupportChanged, gui,
          &RDMnetControllerGUI::processFeatureSupportChange);

  connect(gui->main_network_model_, &RDMnetNetworkModel::expandNewItem, gui, &RDMnetControllerGUI::expandNewItem);

  connect(gui->main_network_model_, &RDMnetNetworkModel::identifyChanged, gui, &RDMnetControllerGUI::identifyChanged);

  connect(gui->ui.actionLogWindow, &QAction::triggered, gui, &RDMnetControllerGUI::openLogWindowDialog);
  connect(gui->ui.actionExit, &QAction::triggered, gui, &RDMnetControllerGUI::exitApplication);
  connect(gui->ui.actionAbout, &QAction::triggered, gui, &RDMnetControllerGUI::openAboutDialog);

  connect(gui->ui.sendCommandsButton, &QPushButton::clicked, gui, &RDMnetControllerGUI::openSendCommandDialog);

  gui->main_network_model_->addScopeToMonitor(E133_DEFAULT_SCOPE);

  return gui;
}

void RDMnetControllerGUI::Shutdown()
{
  if (net_details_proxy_)
  {
    net_details_proxy_->deleteLater();
  }

  if (simple_net_proxy_)
  {
    simple_net_proxy_->deleteLater();
  }

  if (main_network_model_)
  {
    main_network_model_->Shutdown();
    main_network_model_->deleteLater();
  }

  rdmnet::Deinit();
}

void RDMnetControllerGUI::networkTreeViewSelectionChanged(const QItemSelection& selected,
                                                          const QItemSelection& /*deselected*/)
{
  if (!selected.indexes().isEmpty())
  {
    QModelIndex selectedIndex = selected.indexes().first();

    if (selectedIndex.isValid())
    {
      QModelIndex        sourceIndex, proxyIndex;
      QStandardItem*     selectedItem = NULL;
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
  if (log_->getNumberOfCustomLogOutputStreams() == 0)
  {
    LogWindowGUI* logWindowDialog = new LogWindowGUI(this, log_->file_name(), log_->HasFileError());

    connect(logWindowDialog, &QObject::destroyed,
            std::bind(&ControllerLog::removeCustomOutputStream, log_.get(), logWindowDialog));

    logWindowDialog->setAttribute(Qt::WA_DeleteOnClose);
    logWindowDialog->setWindowTitle(tr("Log Window"));
    logWindowDialog->resize(QSize(static_cast<int>(logWindowDialog->width() * 1.2), logWindowDialog->height()));
    logWindowDialog->show();

    log_->addCustomOutputStream(logWindowDialog);
  }
}

void RDMnetControllerGUI::openAboutDialog()
{
  AboutGUI* aboutDialog = new AboutGUI(this, QT_VERSION_STR, RDMNET_VERSION_STRING, ETCPAL_VERSION_STRING);
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

void RDMnetControllerGUI::openSendCommandDialog()
{
  if (currently_selected_network_item_ != NULL)
  {
    SendCommandGUI dialog(this, currently_selected_network_item_, main_network_model_);
    dialog.exec();
  }
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
