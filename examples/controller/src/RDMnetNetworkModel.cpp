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

#include "RDMnetNetworkModel.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include "etcpal/cpp/inet.h"
#include "etcpal/pack.h"
#include "etcpal/socket.h"
#include "rdm/responder.h"
#include "rdmnet/version.h"
#include "PropertyItem.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QMessageBox>
END_INCLUDE_QT_HEADERS()

// Unpack an IPv4 or IPv6 address from a byte buffer pointed to by addrData, with type indicated by
// addrType.
// Returns a string representation of the IP address if parsed successfully, empty string otherwise.
static QString UnpackAndParseIPAddress(const uint8_t* addrData, etcpal_iptype_t addrType)
{
  etcpal::IpAddr ip;

  if (addrType == kEtcPalIpTypeV4)
  {
    ip.SetAddress(etcpal_unpack_u32b(addrData));
  }
  else if (addrType == kEtcPalIpTypeV6)
  {
    ip.SetAddress(addrData);
  }

  if (!ip.IsWildcard())
  {
    return QString::fromStdString(ip.ToString());
  }
  else
  {
    return QString();
  }
}

static bool ParseAndPackIPAddress(etcpal_iptype_t addrType, const std::string& ipString, uint8_t* outBuf)
{
  etcpal::IpAddr ip = etcpal::IpAddr::FromString(ipString);

  if (ip.IsValid())
  {
    if (addrType == kEtcPalIpTypeV4)
    {
      etcpal_pack_u32b(outBuf, ip.v4_data());
      return true;
    }
    else if (addrType == kEtcPalIpTypeV6)
    {
      memcpy(outBuf, ip.v6_data(), ETCPAL_IPV6_BYTES);
      return true;
    }
  }

  return false;
}

void AppendRowToItem(QStandardItem* parent, QStandardItem* child)
{
  if (parent && child)
  {
    parent->appendRow(child);

    if (child->columnCount() != 2)
    {
      child->setColumnCount(2);
    }
  }
}

template <typename T>
T* GetNearestParentItemOfType(QStandardItem* child)
{
  T*             parent = nullptr;
  QStandardItem* current = child;

  while (!parent && current)
  {
    current = current->parent();

    if (current)
    {
      parent = dynamic_cast<T*>(current);
    }
  }

  return parent;
}

void RDMnetNetworkModel::addScopeToMonitor(QString scope)
{
  bool scopeAlreadyAdded = false;
  if (scope.length() > 0)
  {
    etcpal::WriteGuard conn_write(conn_lock_);

    for (auto iter = broker_connections_.begin(); (iter != broker_connections_.end()) && !scopeAlreadyAdded; ++iter)
    {
      if (iter->second->scope() == scope)
      {
        scopeAlreadyAdded = true;
      }
    }

    if (scopeAlreadyAdded)
    {
      QMessageBox errorMessageBox;

      errorMessageBox.setText(tr("The broker for the scope \"%1\" has already been added to this tree. "
                                 "Duplicates with the same scope cannot be added.")
                                  .arg(scope));
      errorMessageBox.setIcon(QMessageBox::Icon::Critical);
      errorMessageBox.exec();
    }
    else
    {
      auto new_scope_handle = rdmnet_.AddScope(scope.toStdString());
      if (new_scope_handle)
      {
        BrokerItem* broker = new BrokerItem(scope, *new_scope_handle);
        AppendRowToItem(invisibleRootItem(), broker);
        broker->enableChildrenSearch();

        emit expandNewItem(broker->index(), BrokerItem::BrokerItemType);

        broker_connections_.insert(std::make_pair(*new_scope_handle, broker));
      }
    }
  }
}

void RDMnetNetworkModel::directChildrenRevealed(const QModelIndex& parentIndex)
{
  QStandardItem* item = itemFromIndex(parentIndex);

  if (item)
  {
    for (int i = 0; i < item->rowCount(); ++i)
    {
      QStandardItem* child = item->child(i);

      if (child)
      {
        if (child->type() == SearchingStatusItem::SearchingStatusItemType)
        {
          searchingItemRevealed(dynamic_cast<SearchingStatusItem*>(child));
        }
      }
    }
  }
}

void RDMnetNetworkModel::addBrokerByIP(QString scope, const etcpal::SockAddr& addr)
{
  bool brokerAlreadyAdded = false;
  // bool shouldSendRDMGetResponsesBroadcast = false;
  // std::vector<RdmParamData> resp_data_list;

  {
    etcpal::WriteGuard conn_write(conn_lock_);
    for (const auto& broker_pair : broker_connections_)
    {
      if (broker_pair.second->scope() == scope)
      {
        brokerAlreadyAdded = true;
        break;
      }
    }

    if (brokerAlreadyAdded)
    {
      QMessageBox errorMessageBox;

      errorMessageBox.setText(tr("The broker for the scope \"%1\" has already been added to this "
                                 "tree. Duplicates with the same scope cannot be added.")
                                  .arg(scope));
      errorMessageBox.setIcon(QMessageBox::Icon::Critical);
      errorMessageBox.exec();
    }
    else
    {
      auto new_scope_handle = rdmnet_.AddScope(scope.toStdString(), addr);
      if (new_scope_handle)
      {
        BrokerItem* broker = new BrokerItem(scope, *new_scope_handle, addr);
        AppendRowToItem(invisibleRootItem(), broker);
        broker->enableChildrenSearch();

        emit expandNewItem(broker->index(), BrokerItem::BrokerItemType);

        broker_connections_.insert(std::make_pair(*new_scope_handle, broker));
      }
    }
  }
}

void RDMnetNetworkModel::HandleConnectedToBroker(rdmnet::Controller::Handle /* controller_handle */,
                                                 rdmnet::ScopeHandle                scope_handle,
                                                 const rdmnet::ClientConnectedInfo& info)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  auto broker_itemIter = broker_connections_.find(scope_handle);
  if (broker_itemIter != broker_connections_.end())
  {
    // Update relevant data
    broker_itemIter->second->SetConnected(true, info.broker_addr());
    std::string utf8_scope = broker_itemIter->second->scope().toStdString();

    log_->Info("Connected to broker on scope %s", utf8_scope.c_str());
    rdmnet_.RequestClientList(scope_handle);
  }
}

void RDMnetNetworkModel::HandleBrokerConnectFailed(rdmnet::Controller::Handle /* controller_handle */,
                                                   rdmnet::ScopeHandle                    scope_handle,
                                                   const rdmnet::ClientConnectFailedInfo& info)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  BrokerItem* broker_item = broker_connections_[scope_handle];
  if (broker_item)
  {
    log_->Info("Connection failed to broker on scope %s: %s. %s", broker_item->scope().toStdString().c_str(),
               info.EventToCString(), info.will_retry() ? "Retrying..." : "NOT retrying!");
    if (info.HasSocketErr())
      log_->Info("Socket error: '%s'", info.socket_err().ToCString());
    if (info.HasRdmnetReason())
      log_->Info("Reject reason: '%s'", info.RdmnetReasonToCString());
    // TODO: display user-facing information if this is a fatal connect failure.
  }
}

void RDMnetNetworkModel::HandleDisconnectedFromBroker(rdmnet::Controller::Handle /* controller_handle */,
                                                      rdmnet::ScopeHandle                   scope_handle,
                                                      const rdmnet::ClientDisconnectedInfo& info)
{
  etcpal::WriteGuard conn_write(conn_lock_);

  BrokerItem* broker_item = broker_connections_[scope_handle];
  if (broker_item)
  {
    if (broker_item->connected())
    {
      broker_item->SetConnected(false);

      log_->Info("Disconnected from broker on scope %s: %s. %s", broker_item->scope().toStdString().c_str(),
                 info.EventToCString(), info.will_retry() ? "Retrying..." : "NOT retrying!");
      if (info.HasSocketErr())
        log_->Info("Socket error: '%s'", info.socket_err().ToCString());
      if (info.HasRdmnetReason())
        log_->Info("Disconnect reason: '%s'", info.RdmnetReasonToCString());
      // TODO: display user-facing information if this is a fatal connect failure.

      emit brokerItemTextUpdated(broker_item);

      broker_item->rdmnet_clients_.clear();
      broker_item->completelyRemoveChildren(0, broker_item->rowCount());
      broker_item->enableChildrenSearch();
    }
  }
}

void RDMnetNetworkModel::processAddRdmnetClients(BrokerItem*                                broker_item,
                                                 const std::vector<rdmnet::RptClientEntry>& list)
{
  // Update the Controller's discovered list to match
  if (list.size() > 0)
  {
    broker_item->disableChildrenSearch();
  }

  for (const auto& rpt_entry : list)
  {
    bool              is_me = (rpt_entry.cid == my_cid_);
    RDMnetClientItem* newRDMnetClientItem = new RDMnetClientItem(rpt_entry, is_me);
    bool              itemAlreadyAdded = false;

    for (auto clientIter = broker_item->rdmnet_clients_.begin();
         (clientIter != broker_item->rdmnet_clients_.end()) && !itemAlreadyAdded; ++clientIter)
    {
      if ((*clientIter))
      {
        if ((*newRDMnetClientItem) == (*(*clientIter)))
        {
          itemAlreadyAdded = true;
        }
      }
    }

    if (itemAlreadyAdded)
    {
      delete newRDMnetClientItem;
    }
    else
    {
      AppendRowToItem(broker_item, newRDMnetClientItem);
      broker_item->rdmnet_clients_.push_back(newRDMnetClientItem);

      if (rpt_entry.type != kRPTClientTypeUnknown)
      {
        InitializeRptClientProperties(newRDMnetClientItem, rpt_entry.uid, rpt_entry.type);
        newRDMnetClientItem->enableFeature(kIdentifyDevice);
        emit featureSupportChanged(newRDMnetClientItem, kIdentifyDevice);
      }

      newRDMnetClientItem->enableChildrenSearch();
    }
  }
}

void RDMnetNetworkModel::processRemoveRdmnetClients(BrokerItem*                                broker_item,
                                                    const std::vector<rdmnet::RptClientEntry>& list)
{
  // Update the Controller's discovered list by removing these newly lost
  // clients
  for (int i = broker_item->rowCount() - 1; i >= 0; --i)
  {
    RDMnetClientItem* clientItem = dynamic_cast<RDMnetClientItem*>(broker_item->child(i));

    if (clientItem)
    {
      for (const auto& rpt_entry : list)
      {
        if (rpt_entry.type == clientItem->rpt_type() && rpt_entry.uid == clientItem->uid())
        {
          // Found the match
          broker_item->rdmnet_clients_.erase(
              std::remove(broker_item->rdmnet_clients_.begin(), broker_item->rdmnet_clients_.end(), clientItem),
              broker_item->rdmnet_clients_.end());
          broker_item->completelyRemoveChildren(i);
          break;
        }
      }
    }
  }

  if (broker_item->rowCount() == 0)
  {
    broker_item->enableChildrenSearch();
  }
}

void RDMnetNetworkModel::processNewEndpointList(RDMnetClientItem*                                treeClientItem,
                                                const std::vector<std::pair<uint16_t, uint8_t>>& list)
{
  if (treeClientItem->childrenSearchRunning() && (list.size() > 1))
  {
    treeClientItem->disableChildrenSearch();
  }

  std::vector<EndpointItem*> prev_list = treeClientItem->endpoints_;

  // Save these endpoints here
  for (auto endpoint_id : list)
  {
    if (endpoint_id.first != 0)
    {
      EndpointItem* newEndpointItem = new EndpointItem(treeClientItem->uid(), endpoint_id.first, endpoint_id.second);
      bool          itemAlreadyAdded = false;

      for (auto existing_endpoint = prev_list.begin(); existing_endpoint != prev_list.end(); ++existing_endpoint)
      {
        if ((*newEndpointItem) == **existing_endpoint)
        {
          itemAlreadyAdded = true;
          prev_list.erase(existing_endpoint);
          break;
        }
      }

      if (itemAlreadyAdded)
      {
        delete newEndpointItem;
      }
      else
      {
        AppendRowToItem(treeClientItem, newEndpointItem);
        treeClientItem->endpoints_.push_back(newEndpointItem);
        newEndpointItem->enableChildrenSearch();
      }
    }
  }

  // Now remove the ones that aren't there anymore
  for (int i = treeClientItem->rowCount() - 1; i >= 0; --i)
  {
    EndpointItem* endpointItem = dynamic_cast<EndpointItem*>(treeClientItem->child(i));

    if (endpointItem)
    {
      for (auto removed_endpoint : prev_list)
      {
        if (*removed_endpoint == *endpointItem)
        {
          // Found the match
          treeClientItem->endpoints_.erase(
              std::remove(treeClientItem->endpoints_.begin(), treeClientItem->endpoints_.end(), endpointItem),
              treeClientItem->endpoints_.end());
          treeClientItem->completelyRemoveChildren(i);
          break;
        }
      }
    }
  }

  if (treeClientItem->rowCount() == 0)
  {
    treeClientItem->enableChildrenSearch();
  }
}

void RDMnetNetworkModel::processNewResponderList(EndpointItem* treeEndpointItem, const std::vector<rdm::Uid>& list)
{
  bool somethingWasAdded = false;

  std::vector<ResponderItem*> prev_list = treeEndpointItem->responders_;

  // Save these devices
  for (auto resp_uid : list)
  {
    ResponderItem* newResponderItem = new ResponderItem(resp_uid);
    bool           itemAlreadyAdded = false;

    for (auto existing_responder = prev_list.begin(); existing_responder != prev_list.end(); ++existing_responder)
    {
      if ((*newResponderItem) == (*(*existing_responder)))
      {
        itemAlreadyAdded = true;
        prev_list.erase(existing_responder);
        break;
      }
    }

    if (itemAlreadyAdded)
    {
      delete newResponderItem;
    }
    else
    {
      AppendRowToItem(treeEndpointItem, newResponderItem);
      treeEndpointItem->responders_.push_back(newResponderItem);
      somethingWasAdded = true;

      InitializeResponderProperties(newResponderItem);

      newResponderItem->enableFeature(kIdentifyDevice);
      emit featureSupportChanged(newResponderItem, kIdentifyDevice);
    }
  }

  // Now remove the ones that aren't there anymore
  for (int i = treeEndpointItem->rowCount() - 1; i >= 0; --i)
  {
    ResponderItem* responderItem = dynamic_cast<ResponderItem*>(treeEndpointItem->child(i));

    if (responderItem)
    {
      for (auto removed_responder : prev_list)
      {
        if (*removed_responder == *responderItem)
        {
          // Found the match
          // responderItem->properties.clear();
          // responderItem->removeRows(0, responderItem->rowCount());

          treeEndpointItem->responders_.erase(
              std::remove(treeEndpointItem->responders_.begin(), treeEndpointItem->responders_.end(), responderItem),
              treeEndpointItem->responders_.end());
          treeEndpointItem->completelyRemoveChildren(i);
          break;
        }
      }
    }
  }

  if (somethingWasAdded)
  {
    treeEndpointItem->disableChildrenSearch();
  }
  else if (treeEndpointItem->rowCount() == 0)
  {
    treeEndpointItem->enableChildrenSearch();
  }
}

void RDMnetNetworkModel::processSetPropertyData(RDMnetNetworkItem* parent,
                                                unsigned short     pid,
                                                const QString&     name,
                                                const QVariant&    value,
                                                int                role)
{
  bool enable = value.isValid() || PropertyValueItem::pidStartEnabled(pid);
  bool overrideEnableSet = (role == RDMnetNetworkItem::EditorWidgetTypeRole) &&
                           (static_cast<EditorWidgetType>(value.toInt()) == kButton) &&
                           (PropertyValueItem::pidFlags(pid) & kEnableButtons);

  if (parent)
  {
    if (parent->isEnabled())
    {
      // Check if this property already exists before adding it. If it exists
      // already, then update the existing property.
      for (auto item : parent->properties)
      {
        if (item->getValueItem())
        {
          if ((item->getFullName() == name) && (item->getValueItem()->getPID() == pid))
          {
            item->getValueItem()->setData(value, role);

            item->setEnabled(enable);
            item->getValueItem()->setEnabled((enable && PropertyValueItem::pidSupportsSet(pid)) || overrideEnableSet);

            return;
          }
        }
      }

      // Property doesn't exist, so make a new one.
      PropertyItem*      propertyItem = createPropertyItem(parent, name);
      PropertyValueItem* propertyValueItem = new PropertyValueItem(value, role);

      if (pid == E120_DMX_PERSONALITY)
      {
        propertyValueItem->setData(EditorWidgetType::kComboBox, RDMnetNetworkItem::EditorWidgetTypeRole);
      }

      propertyValueItem->setPID(pid);
      propertyValueItem->setEnabled((enable && PropertyValueItem::pidSupportsSet(pid)) || overrideEnableSet);
      propertyItem->setValueItem(propertyValueItem);
      propertyItem->setEnabled(enable);
      parent->properties.push_back(propertyItem);
    }
  }
}

void RDMnetNetworkModel::processRemovePropertiesInRange(RDMnetNetworkItem*          parent,
                                                        std::vector<PropertyItem*>* properties,
                                                        unsigned short              pid,
                                                        int                         role,
                                                        const QVariant&             min,
                                                        const QVariant&             max)
{
  if (parent)
  {
    if (parent->isEnabled())
    {
      for (int i = parent->rowCount() - 1; i >= 0; --i)
      {
        PropertyItem*      child = dynamic_cast<PropertyItem*>(parent->child(i, 0));
        PropertyValueItem* sibling = dynamic_cast<PropertyValueItem*>(parent->child(i, 1));

        if (child && sibling)
        {
          if (sibling->getPID() == pid)
          {
            QVariant value = sibling->data(role);

            if (value.isValid())
            {
              if ((value >= min) && (value <= max))
              {
                parent->completelyRemoveChildren(i, 1, properties);
              }
            }
          }
        }
      }
    }
  }
}

void RDMnetNetworkModel::processAddPropertyEntry(RDMnetNetworkItem* parent,
                                                 unsigned short     pid,
                                                 const QString&     name,
                                                 int                role)
{
  processSetPropertyData(parent, pid, name, QVariant(), role);
}

void RDMnetNetworkModel::processPropertyButtonClick(const QPersistentModelIndex& propertyIndex)
{
  // Assuming this is SET TCP_COMMS_STATUS for now.
  if (propertyIndex.isValid())
  {
    QString scope = propertyIndex.data(RDMnetNetworkItem::ScopeDataRole).toString();

    uint8_t  maxBuffSize = PropertyValueItem::pidMaxBufferSize(E133_TCP_COMMS_STATUS);
    QVariant manuVariant = propertyIndex.data(RDMnetNetworkItem::ClientManuRole);
    QVariant devVariant = propertyIndex.data(RDMnetNetworkItem::ClientDevRole);

    // TODO Christian, I'm curious if it's possible to get the BrokerItem by moving upward through
    // parent items from the model index instead of finding it by scope string.
    const BrokerItem* broker_item = nullptr;
    {
      etcpal::ReadGuard conn_read(conn_lock_);
      for (const auto& broker_pair : broker_connections_)
      {
        if (broker_pair.second->scope() == scope)
        {
          broker_item = broker_pair.second;
          break;
        }
      }
    }

    if (!broker_item)
    {
      log_->Error("Error: Cannot find broker connection for clicked button.");
    }
    else
    {
      rdm::Uid dest_uid(static_cast<uint16_t>(manuVariant.toUInt()), static_cast<uint32_t>(devVariant.toUInt()));

      SendSetCommand(broker_item, dest_uid, E133_TCP_COMMS_STATUS,
                     reinterpret_cast<const uint8_t*>(scope.toUtf8().constData()),
                     static_cast<uint8_t>(std::min<int>(scope.length(), maxBuffSize)));
    }
  }
  else
  {
    log_->Error("Error: Button clicked on invalid property.");
  }
}

void RDMnetNetworkModel::removeBroker(BrokerItem* broker_item)
{
  bool removeComplete = false;

  rdmnet::ScopeHandle scope_handle = broker_item->scope_handle();
  rdmnet_.RemoveScope(scope_handle, kRdmnetDisconnectUserReconfigure);
  {  // Write lock scope
    etcpal::WriteGuard conn_write(conn_lock_);
    broker_connections_.erase(scope_handle);
  }

  for (int i = invisibleRootItem()->rowCount() - 1; (i >= 0) && !removeComplete; --i)
  {
    BrokerItem* currentItem = dynamic_cast<BrokerItem*>(invisibleRootItem()->child(i));

    if (currentItem)
    {
      if (currentItem->scope_handle() == scope_handle)
      {
        currentItem->completelyRemoveChildren(0, currentItem->rowCount());
        invisibleRootItem()->removeRow(i);
        removeComplete = true;
      }
    }
  }
}

void RDMnetNetworkModel::removeAllBrokers()
{
  {  // Write lock scope
    etcpal::WriteGuard conn_write(conn_lock_);

    auto broker_iter = broker_connections_.begin();
    while (broker_iter != broker_connections_.end())
    {
      rdmnet_.RemoveScope(broker_iter->second->scope_handle(), kRdmnetDisconnectUserReconfigure);
      broker_iter = broker_connections_.erase(broker_iter);
    }
  }

  for (int i = invisibleRootItem()->rowCount() - 1; i >= 0; --i)
  {
    BrokerItem* currentItem = dynamic_cast<BrokerItem*>(invisibleRootItem()->child(i));

    if (currentItem)
    {
      currentItem->completelyRemoveChildren(0, currentItem->rowCount());
      invisibleRootItem()->removeRow(i);
    }
  }
}

void RDMnetNetworkModel::activateFeature(RDMnetNetworkItem* device, SupportedDeviceFeature feature)
{
  if (device)
  {
    if (feature & kResetDevice)
    {
      if (device->hasValidProperties())  // Means device wasn't reset
      {
        device->disableAllChildItems();
        device->setDeviceWasReset(true);
        device->setEnabled(false);

        emit featureSupportChanged(device, kResetDevice | kIdentifyDevice);

        uint8_t data_len = PropertyValueItem::pidMaxBufferSize(E120_RESET_DEVICE);
        auto    data = std::make_unique<uint8_t[]>(data_len);
        memset(data.get(), 0, data_len);
        data[0] = 0xff;  // Default to cold reset

        SendSetCommand(GetNearestParentItemOfType<BrokerItem>(device), device->uid(), E120_RESET_DEVICE, data.get(),
                       data_len);
      }
    }

    if (feature & kIdentifyDevice)
    {
      uint8_t data_len = PropertyValueItem::pidMaxBufferSize(E120_IDENTIFY_DEVICE);
      auto    data = std::make_unique<uint8_t[]>(data_len);
      memset(data.get(), 0, data_len);
      data[0] = device->identifying() ? 0x00 : 0x01;

      SendSetCommand(GetNearestParentItemOfType<BrokerItem>(device), device->uid(), E120_RESET_DEVICE, data.get(),
                     data_len);
    }
  }
}

RDMnetNetworkModel::RDMnetNetworkModel(rdmnet::Controller& library, etcpal::Logger& log) : log_(&log), rdmnet_(library)
{
}

RDMnetNetworkModel* RDMnetNetworkModel::MakeRDMnetNetworkModel(rdmnet::Controller& library, etcpal::Logger& log)
{
  RDMnetNetworkModel* model = new RDMnetNetworkModel(library, log);

  const rdmnet::Controller::RdmData my_rdm_data(kExampleControllerModelId, kExampleControllerSwVersionId, "ETC",
                                                "Example RDMnet Controller", RDMNET_VERSION_STRING,
                                                "Example RDMnet Controller");
  model->rdmnet_.Startup(*model, rdmnet::Controller::Settings(model->my_cid_, 0x6574), my_rdm_data);

  // Initialize GUI-supported PID information
  QString rdmGroupName("RDM");
  QString rdmNetGroupName("RDMnet");

  // Location flags specify where specific property items will be created by default. Exceptions can be made.
  PIDFlags rdmPIDFlags = kLocDevice | kLocController | kLocResponder;
  PIDFlags rdmNetPIDFlags = kLocDevice;

  // E1.20
  // pid, get, set, type, role/included
  // clang-format off

  // SUPPORTED_PARAMETERS
  PropertyValueItem::setPIDInfo(E120_SUPPORTED_PARAMETERS, rdmPIDFlags | kSupportsGet | kExcludeFromModel, QVariant::Type::Invalid);

  // DEVICE_INFO
  PropertyValueItem::setPIDInfo(E120_DEVICE_INFO, rdmPIDFlags | kSupportsGet, QVariant::Type::Invalid);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_INFO,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("RDM Protocol Version")));
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_INFO,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Device Model ID")));
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_INFO,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Product Category")));
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_INFO,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Software Version ID")));
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_INFO,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("DMX512 Footprint")));
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_INFO,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Sub-Device Count")));
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_INFO,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Sensor Count")));

  // DEVICE_MODEL_DESCRIPTION
  PropertyValueItem::setPIDInfo(E120_DEVICE_MODEL_DESCRIPTION,
                                rdmPIDFlags | kSupportsGet,
                                QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_MODEL_DESCRIPTION,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Device Model Description")));

  // MANUFACTURER_LABEL
  PropertyValueItem::setPIDInfo(E120_MANUFACTURER_LABEL,
                                rdmPIDFlags | kSupportsGet,
                                QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_MANUFACTURER_LABEL,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Manufacturer Label")));

  // DEVICE_LABEL
  PropertyValueItem::setPIDInfo(E120_DEVICE_LABEL,
                                rdmPIDFlags | kSupportsGet | kSupportsSet,
                                QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_LABEL,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Device Label")));
  PropertyValueItem::setPIDMaxBufferSize(E120_DEVICE_LABEL, static_cast<uint8_t>(kRdmDeviceLabelMaxLength));

  // SOFTWARE_VERSION_LABEL
  PropertyValueItem::setPIDInfo(E120_SOFTWARE_VERSION_LABEL,
                                rdmPIDFlags | kSupportsGet,
                                QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_SOFTWARE_VERSION_LABEL,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Software Label")));

  // BOOT_SOFTWARE_VERSION_ID
  PropertyValueItem::setPIDInfo(E120_BOOT_SOFTWARE_VERSION_ID,
                                rdmPIDFlags | kSupportsGet,
                                QVariant::Type::Int);
  PropertyValueItem::addPIDPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_ID,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Boot Software ID")));

  // BOOT_SOFTWARE_VERSION_LABEL
  PropertyValueItem::setPIDInfo(E120_BOOT_SOFTWARE_VERSION_LABEL,
                                rdmPIDFlags | kSupportsGet,
                                QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_LABEL,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("Boot Software Label")));

  // DMX_START_ADDRESS
  PropertyValueItem::setPIDInfo(E120_DMX_START_ADDRESS,
                                rdmPIDFlags | kSupportsGet | kSupportsSet,
                                QVariant::Type::Int);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DMX_START_ADDRESS,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("DMX512 Start Address")));
  PropertyValueItem::setPIDNumericDomain(E120_DMX_START_ADDRESS, 1, 512);
  PropertyValueItem::setPIDMaxBufferSize(E120_DMX_START_ADDRESS, 2);

  // IDENTIFY_DEVICE
  PropertyValueItem::setPIDInfo(E120_IDENTIFY_DEVICE,
                                rdmPIDFlags | kSupportsSet | kExcludeFromModel,
                                QVariant::Type::Bool);
  PropertyValueItem::setPIDMaxBufferSize(E120_IDENTIFY_DEVICE, 1);

  // DMX_PERSONALITY
  PropertyValueItem::setPIDInfo(E120_DMX_PERSONALITY,
                                rdmPIDFlags | kSupportsGet | kSupportsSet,
                                QVariant::Type::Char,
                                RDMnetNetworkItem::PersonalityNumberRole);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DMX_PERSONALITY,
                                               QString("%0\\%1").arg(rdmGroupName).arg(tr("DMX512 Personality")));
  PropertyValueItem::setPIDNumericDomain(E120_DMX_PERSONALITY, 1, 255);
  PropertyValueItem::setPIDMaxBufferSize(E120_DMX_PERSONALITY, 1);

  // RESET_DEVICE
  PropertyValueItem::setPIDInfo(E120_RESET_DEVICE,
                                rdmPIDFlags | kSupportsSet | kExcludeFromModel,
                                QVariant::Type::Char);
  PropertyValueItem::setPIDMaxBufferSize(E120_RESET_DEVICE, 1);

  // RDMnet
  // COMPONENT_SCOPE
  PropertyValueItem::setPIDInfo(E133_COMPONENT_SCOPE,
                                rdmNetPIDFlags | kSupportsGet | kSupportsSet,
                                QVariant::Type::Invalid);
  PropertyValueItem::addPIDPropertyDisplayName(E133_COMPONENT_SCOPE,
                                               QString("%0\\%1")
                                               .arg(rdmNetGroupName)
                                               .arg(tr("Component Scope")));
  PropertyValueItem::addPIDPropertyDisplayName(E133_COMPONENT_SCOPE,
                                               QString("%0\\%1")
                                               .arg(rdmNetGroupName)
                                               .arg(tr("Static Broker IPv4 (Leave blank for dynamic)")));
  PropertyValueItem::addPIDPropertyDisplayName(E133_COMPONENT_SCOPE,
                                               QString("%0\\%1")
                                               .arg(rdmNetGroupName)
                                               .arg(tr("Static Broker IPv6 (Leave blank for dynamic)")));
  PropertyValueItem::setPIDMaxBufferSize(E133_COMPONENT_SCOPE,
                                         2 /* Scope Slot */ +
                                         E133_SCOPE_STRING_PADDED_LENGTH /* Scope String */ +
                                         1 /* Static Config Type */ +
                                         4 /* Static IPv4 Address */ +
                                         16 /* Static IPv6 Address */ +
                                         2 /* Static Port */);

  // SEARCH_DOMAIN
  PropertyValueItem::setPIDInfo(E133_SEARCH_DOMAIN,
                                rdmNetPIDFlags | kLocController | kSupportsGet | kSupportsSet,
                                QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E133_SEARCH_DOMAIN,
                                               QString("%0\\%1")
                                               .arg(rdmNetGroupName)
                                               .arg(tr("Search Domain")));
  PropertyValueItem::setPIDMaxBufferSize(E133_SEARCH_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);

  // TCP_COMMS_STATUS
  PropertyValueItem::setPIDInfo(E133_TCP_COMMS_STATUS,
                                rdmNetPIDFlags | kSupportsGet | kEnableButtons,
                                QVariant::Type::Invalid);
  PropertyValueItem::addPIDPropertyDisplayName(
      E133_TCP_COMMS_STATUS, QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Broker IP Address (Current)")));
  PropertyValueItem::addPIDPropertyDisplayName(E133_TCP_COMMS_STATUS,
                                               QString("%0\\%1")
                                               .arg(rdmNetGroupName)
                                               .arg(tr("Unhealthy TCP Events")));
  PropertyValueItem::addPIDPropertyDisplayName(E133_TCP_COMMS_STATUS,
                                               QString("%0\\%1")
                                               .arg(rdmNetGroupName)
                                               .arg(tr("Unhealthy TCP Events\\Reset Counter")));
  PropertyValueItem::setPIDMaxBufferSize(E133_TCP_COMMS_STATUS, E133_SCOPE_STRING_PADDED_LENGTH);

  // clang-format on

  model->setColumnCount(2);
  model->setHeaderData(0, Qt::Orientation::Horizontal, tr("Property"));
  model->setHeaderData(1, Qt::Orientation::Horizontal, tr("Value"));

  qRegisterMetaType<std::vector<rdmnet::RptClientEntry>>("std::vector<rdmnet::RptClientEntry>");
  qRegisterMetaType<std::vector<std::pair<uint16_t, uint8_t>>>("std::vector<std::pair<uint16_t, uint8_t>>");
  qRegisterMetaType<std::vector<rdm::Uid>>("std::vector<rdm::Uid>");
  qRegisterMetaType<std::vector<PropertyItem*>*>("std::vector<PropertyItem*>*");
  qRegisterMetaType<QVector<int>>("QVector<int>");
  qRegisterMetaType<uint16_t>("uint16_t");

  connect(model, &RDMnetNetworkModel::addRdmnetClients, model, &RDMnetNetworkModel::processAddRdmnetClients,
          Qt::AutoConnection);
  connect(model, &RDMnetNetworkModel::removeRdmnetClients, model, &RDMnetNetworkModel::processRemoveRdmnetClients,
          Qt::AutoConnection);
  connect(model, &RDMnetNetworkModel::newEndpointList, model, &RDMnetNetworkModel::processNewEndpointList,
          Qt::AutoConnection);
  connect(model, &RDMnetNetworkModel::newResponderList, model, &RDMnetNetworkModel::processNewResponderList,
          Qt::AutoConnection);
  connect(model, &RDMnetNetworkModel::setPropertyData, model, &RDMnetNetworkModel::processSetPropertyData,
          Qt::AutoConnection);
  connect(model, &RDMnetNetworkModel::removePropertiesInRange, model,
          &RDMnetNetworkModel::processRemovePropertiesInRange, Qt::AutoConnection);
  connect(model, &RDMnetNetworkModel::addPropertyEntry, model, &RDMnetNetworkModel::processAddPropertyEntry,
          Qt::AutoConnection);

  return model;
}

/*
TODO what is this for? Is it needed?
RDMnetNetworkModel* RDMnetNetworkModel::MakeTestModel()
{
  RDMnetNetworkModel* model = new RDMnetNetworkModel(nullptr, nullptr);

  QStandardItem* parentItem = model->invisibleRootItem();

  model->setColumnCount(2);
  model->setHeaderData(0, Qt::Orientation::Horizontal, tr("Name"));
  model->setHeaderData(1, Qt::Orientation::Horizontal, tr("Value"));

  for (int i = 0; i < 4; ++i)
  {
    QStandardItem* item = new RDMnetNetworkItem(QString("item %0").arg(i));
    QStandardItem* item2 = new RDMnetNetworkItem(QString("item2 %0").arg(i));

    AppendRowToItem(parentItem, item);
    parentItem->setChild(parentItem->rowCount() - 1, 1, item2);

    parentItem = item;
  }

  if (parentItem->type() == RDMnetNetworkItem::RDMnetNetworkItemType)
    dynamic_cast<RDMnetNetworkItem*>(parentItem)->enableChildrenSearch();

  return model;
}
*/

void RDMnetNetworkModel::Shutdown()
{
  {  // Write lock scope
    etcpal::WriteGuard conn_write(conn_lock_);

    broker_connections_.clear();
  }

  rdmnet_.Shutdown();

  log_ = nullptr;
}

void RDMnetNetworkModel::searchingItemRevealed(SearchingStatusItem* searchItem)
{
  if (searchItem && !searchItem->wasSearchInitiated())
  {
    // A search item was likely just revealed in the tree, starting a search process.
    QStandardItem* searchItemParent = searchItem->parent();

    if (searchItemParent)
    {
      switch (searchItemParent->type())
      {
        case BrokerItem::BrokerItemType:
          searchItem->setSearchInitiated(true);
          break;

        case RDMnetClientItem::RDMnetClientItemType: {
          RDMnetClientItem* clientItem = dynamic_cast<RDMnetClientItem*>(searchItemParent);
          if (clientItem)
          {
            searchItem->setSearchInitiated(true);
            SendGetCommand(GetNearestParentItemOfType<BrokerItem>(clientItem), clientItem->uid(), E137_7_ENDPOINT_LIST);
          }
          break;
        }

        case EndpointItem::EndpointItemType: {
          EndpointItem* endpointItem = dynamic_cast<EndpointItem*>(searchItemParent);

          if (endpointItem)
          {
            searchItem->setSearchInitiated(true);

            uint8_t cmd_buf[2];
            etcpal_pack_u16b(cmd_buf, endpointItem->id());
            SendGetCommand(GetNearestParentItemOfType<BrokerItem>(endpointItem), endpointItem->parent_uid(),
                           E137_7_ENDPOINT_RESPONDERS, cmd_buf, 2);
          }
          break;
        }
      }
    }
  }
}

bool RDMnetNetworkModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  QStandardItem* item = itemFromIndex(index);
  bool           updateValue = true;
  QVariant       newValue = value;

  if (item)
  {
    if (item->type() == PropertyValueItem::PropertyValueItemType)
    {
      PropertyValueItem* propertyValueItem = dynamic_cast<PropertyValueItem*>(item);
      RDMnetNetworkItem* parentItem = GetNearestParentItemOfType<ResponderItem>(item);

      if (!parentItem)
      {
        parentItem = GetNearestParentItemOfType<RDMnetClientItem>(item);
      }

      if ((propertyValueItem) && (parentItem))
      {
        uint16_t pid = propertyValueItem->getPID();

        if (PropertyValueItem::pidDataRole(pid) == role)  // Then this value should be replicated over the network
        {
          if (((PropertyValueItem::pidDataType(pid) == QVariant::Type::Int) ||
               (PropertyValueItem::pidDataType(pid) == QVariant::Type::Char)) &&
              ((value < PropertyValueItem::pidDomainMin(pid)) || (value > PropertyValueItem::pidDomainMax(pid))))
          {
            // Value is out of range, reset to original value.
            updateValue = false;
          }
          else if (!parentItem->hasValidProperties())
          {
            // User interacted with a dead property that has yet to be removed.
            updateValue = false;
          }
          else
          {
            uint8_t max_buf_size = PropertyValueItem::pidMaxBufferSize(pid);
            auto    data_buf = std::make_unique<uint8_t[]>(max_buf_size);
            memset(data_buf.get(), 0, max_buf_size);

            uint8_t* pack_ptr = data_buf.get();

            // Special cases for certain PIDs
            if (pid == E133_COMPONENT_SCOPE)
            {
              // Scope slot (default to 1)
              etcpal_pack_u16b(pack_ptr, static_cast<uint16_t>(index.data(RDMnetNetworkItem::ScopeSlotRole).toInt()));
              pack_ptr += 2;
            }

            switch (PropertyValueItem::pidDataType(pid))
            {
              case QVariant::Type::Int:
                switch (max_buf_size - (pack_ptr - data_buf.get()))
                {
                  case 2:
                    etcpal_pack_u16b(pack_ptr, static_cast<uint16_t>(value.toInt()));
                    break;
                  case 4:
                    etcpal_pack_u32b(pack_ptr, static_cast<uint32_t>(value.toInt()));
                    break;
                }
                break;
              case QVariant::Type::String: {
                auto qstr = value.toString();
                qstr.truncate(max_buf_size - static_cast<uint8_t>(pack_ptr - data_buf.get()));
                newValue = qstr;
                auto stdstr = qstr.toStdString();
                memcpy(pack_ptr, stdstr.data(), stdstr.length());
                break;
              }
              case QVariant::Type::Bool:
                if (value.toBool())
                  pack_ptr[0] = 1;
                else
                  pack_ptr[0] = 0;
                break;
              case QVariant::Type::Char:
                pack_ptr[0] = static_cast<uint8_t>(value.toInt());
                break;
              default:
                if (pid == E133_COMPONENT_SCOPE)
                {
                  // Obtain the index of the property item display name (identifying the item)
                  int displayNameIndex = index.data(RDMnetNetworkItem::DisplayNameIndexRole).toInt();

                  QVariant scopeString = index.data(RDMnetNetworkItem::ScopeDataRole);
                  QVariant ipv4String = index.data(RDMnetNetworkItem::StaticIPv4DataRole);
                  QVariant ipv6String = index.data(RDMnetNetworkItem::StaticIPv6DataRole);

                  switch (displayNameIndex)
                  {
                    case 0:  // scope
                      scopeString = value;
                      break;
                    case 1:  // ipv4
                      newValue = ipv4String = value;
                      break;
                    case 2:  // ipv6
                      newValue = ipv6String = value;
                      break;
                  }

                  auto qstr = scopeString.toString();
                  qstr.truncate(E133_SCOPE_STRING_PADDED_LENGTH);
                  if (displayNameIndex == 0)
                    newValue = qstr;
                  auto stdstr = qstr.toStdString();
                  memcpy(pack_ptr, stdstr.data(), stdstr.length());
                  pack_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

                  if ((ipv4String.toString().length() > 0) &&
                      ((displayNameIndex != 2) || (ipv6String.toString().length() == 0)))
                  {
                    *pack_ptr = E133_STATIC_CONFIG_IPV4;
                  }
                  else if ((ipv6String.toString().length() > 0) &&
                           ((displayNameIndex != 1) || (ipv4String.toString().length() == 0)))
                  {
                    *pack_ptr = E133_STATIC_CONFIG_IPV6;
                    updateValue = false;  // IPv6 is still in development, so make this read-only for now.
                  }
                  else
                  {
                    *pack_ptr = E133_NO_STATIC_CONFIG;
                  }

                  uint8_t  static_config_type = *pack_ptr;
                  uint16_t port = 0;

                  ++pack_ptr;

                  pack_ptr = PackIPAddressItem(ipv4String, kEtcPalIpTypeV4, pack_ptr,
                                               (static_config_type == E133_STATIC_CONFIG_IPV4));

                  if (static_config_type == E133_STATIC_CONFIG_IPV4 && pack_ptr)
                  {
                    // This way, packIPAddressItem obtained the port value for us.
                    // Save the port value for later - we don't want it packed here.
                    pack_ptr -= 2;
                    port = etcpal_unpack_u16b(pack_ptr);
                  }

                  pack_ptr = PackIPAddressItem(ipv6String, kEtcPalIpTypeV6, pack_ptr,
                                               (static_config_type != E133_STATIC_CONFIG_IPV4));

                  if ((static_config_type == E133_STATIC_CONFIG_IPV4) && pack_ptr)
                  {
                    // Pack the port value saved from earlier.
                    etcpal_pack_u16b(pack_ptr, port);
                    pack_ptr += 2;
                  }
                }
                else
                {
                  updateValue = false;
                }
                break;
            }

            updateValue = updateValue && (pack_ptr != nullptr);

            if (updateValue)
            {
              BrokerItem* broker_item = GetNearestParentItemOfType<BrokerItem>(parentItem);
              SendSetCommand(broker_item, parentItem->uid(), pid, data_buf.get(),
                             static_cast<uint8_t>(pack_ptr - data_buf.get()));
              if (pid == E120_DMX_PERSONALITY)
              {
                SendGetCommand(broker_item, parentItem->uid(), E120_DEVICE_INFO);
              }
            }
          }
        }
      }
    }
  }

  return updateValue ? QStandardItemModel::setData(index, newValue, role) : false;
}

void RDMnetNetworkModel::HandleClientListUpdate(rdmnet::Controller::Handle /* controller_handle */,
                                                rdmnet::ScopeHandle          scope_handle,
                                                client_list_action_t         action,
                                                const rdmnet::RptClientList& list)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  BrokerItem* broker_item = broker_connections_[scope_handle];

  // TODO the four possible actions need to be handled properly
  // kRdmnetClientListAppend means this list should be added to the existing clients
  // kRdmnetClientListReplace means this list should replace the current client list
  // kRdmnetClientListUpdate means this list contains updated information for some existing clients
  // kRdmnetClientListRemove means this list should be removed from the existing clients
  if (action == kRdmnetClientListRemove)
    emit removeRdmnetClients(broker_item, list.GetClientEntries());
  else
    emit addRdmnetClients(broker_item, list.GetClientEntries());
}

void RDMnetNetworkModel::HandleRptStatus(rdmnet::Controller::Handle /* controller_handle */,
                                         rdmnet::ScopeHandle /* scope_handle */,
                                         const rdmnet::RptStatus& status)
{
  log_->Info("Received RPT Status response from component %s: '%s' (code %d)", status.source_uid().ToString().c_str(),
             status.CodeToCString(), status.status_code());
}

bool RDMnetNetworkModel::SendGetCommand(const BrokerItem* broker_item,
                                        const rdm::Uid&   uid,
                                        uint16_t          param_id,
                                        const uint8_t*    get_data,
                                        uint8_t           get_data_len)
{
  if (!broker_item)
    return false;

  auto destination_addr = broker_item->FindResponder(uid);
  if (destination_addr)
  {
    return rdmnet_.SendGetCommand(broker_item->scope_handle(), *destination_addr, param_id, get_data, get_data_len)
        .has_value();
  }
  return false;
}

bool RDMnetNetworkModel::SendSetCommand(const BrokerItem* broker_item,
                                        const rdm::Uid&   uid,
                                        uint16_t          param_id,
                                        const uint8_t*    set_data,
                                        uint8_t           set_data_len)
{
  if (!broker_item)
    return false;

  auto destination_addr = broker_item->FindResponder(uid);
  if (destination_addr)
  {
    return rdmnet_.SendSetCommand(broker_item->scope_handle(), *destination_addr, param_id, set_data, set_data_len)
        .has_value();
  }
  return false;
}

BrokerItem* RDMnetNetworkModel::GetBrokerItem(rdmnet::ScopeHandle scope_handle)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  if (broker_connections_.find(scope_handle) != broker_connections_.end())
  {
    return broker_connections_[scope_handle];
  }
  return nullptr;
}

void RDMnetNetworkModel::HandleRdmResponse(rdmnet::Controller::Handle /* controller_handle */,
                                           rdmnet::ScopeHandle        scope_handle,
                                           const rdmnet::RdmResponse& resp)
{
  // Since we are compiling with RDMNET_DYNAMIC_MEM, we should never get partial responses.
  assert(!resp.more_coming());

  switch (resp.response_type())
  {
    case kRdmResponseTypeAck:
    case kRdmResponseTypeAckOverflow:
      HandleRdmAck(scope_handle, resp);
      break;
    case E120_RESPONSE_TYPE_NACK_REASON:
      HandleRdmNack(scope_handle, resp);
      break;
    case kRdmResponseTypeAckTimer:
    default:
      break;
  }
}

void RDMnetNetworkModel::HandleRdmAck(rdmnet::ScopeHandle scope_handle, const rdmnet::RdmResponse& resp)
{
  if (resp.IsGetResponse())
  {
    log_->Info("Got GET_COMMAND_RESPONSE with PID 0x%04x from responder %s", resp.param_id(),
               resp.rdmnet_source_uid().ToString().c_str());

    switch (resp.param_id())
    {
      case E120_STATUS_MESSAGES: {
        // TODO
        //   for (unsigned int i = 0; i < cmd->getLength(); i += 9)
        //   {
        //     cmd->setSubdevice((uint8_t)etcpal_unpack_u16b(&cmdBuffer[i]));

        //     status(cmdBuffer[i + 2], etcpal_unpack_u16b(&cmdBuffer[i + 3]),
        //            etcpal_unpack_u16b(&cmdBuffer[i + 5]), etcpal_unpack_u16b(&cmdBuffer[i + 7]),
        //            cmd);
        //   }

        //   if (cmd->getLength() == 0)
        //   {
        //     HandleStatusMessagesResponse(E120_STATUS_ADVISORY_CLEARED, 0, 0, 0, cmd);
        //   }
        break;
      }
      case E120_SUPPORTED_PARAMETERS: {
        std::vector<uint16_t> param_list;
        for (const uint8_t* offset = resp.data(); offset < resp.data() + resp.data_len(); offset += 2)
        {
          param_list.push_back(etcpal_unpack_u16b(offset));
        }

        if (!param_list.empty())
          HandleSupportedParametersResponse(scope_handle, param_list, resp.rdmnet_source_uid());
        break;
      }
      case E120_DEVICE_INFO: {
        if (resp.data_len() >= 19)
        {
          const uint8_t* resp_data = resp.data();
          // Current personality is reset if less than 1
          uint8_t cur_pers = (resp_data[12] < 1 ? 1 : resp_data[12]);
          // Total personality is reset if current or total is less than 1
          uint8_t total_pers = ((resp_data[12] < 1 || resp_data[13] < 1) ? 1 : resp_data[13]);

          RdmDeviceInfo dev_info{
              etcpal_unpack_u16b(&resp_data[0]),
              etcpal_unpack_u16b(&resp_data[2]),
              etcpal_unpack_u16b(&resp_data[4]),
              etcpal_unpack_u32b(&resp_data[6]),
              etcpal_unpack_u16b(&resp_data[10]),
              cur_pers,
              total_pers,
              etcpal_unpack_u16b(&resp_data[14]),
              etcpal_unpack_u16b(&resp_data[16]),
              resp_data[18],
          };
          HandleDeviceInfoResponse(scope_handle, dev_info, resp.rdmnet_source_uid());
        }
        break;
      }
      case E120_DEVICE_MODEL_DESCRIPTION:
      case E120_MANUFACTURER_LABEL:
      case E120_DEVICE_LABEL:
      case E120_SOFTWARE_VERSION_LABEL:
      case E120_BOOT_SOFTWARE_VERSION_LABEL: {
        char label[33]{};
        memcpy(label, resp.data(), (resp.data_len() > 32) ? 32 : resp.data_len());

        switch (resp.param_id())
        {
          case E120_DEVICE_MODEL_DESCRIPTION:
            HandleModelDescResponse(scope_handle, QString::fromUtf8(label), resp.rdmnet_source_uid());
            break;
          case E120_SOFTWARE_VERSION_LABEL:
            HandleSoftwareLabelResponse(scope_handle, QString::fromUtf8(label), resp.rdmnet_source_uid());
            break;
          case E120_MANUFACTURER_LABEL:
            HandleManufacturerLabelResponse(scope_handle, QString::fromUtf8(label), resp.rdmnet_source_uid());
            break;
          case E120_DEVICE_LABEL:
            HandleDeviceLabelResponse(scope_handle, QString::fromUtf8(label), resp.rdmnet_source_uid());
            break;
          case E120_BOOT_SOFTWARE_VERSION_LABEL:
            HandleBootSoftwareLabelResponse(scope_handle, QString::fromUtf8(label), resp.rdmnet_source_uid());
            break;
        }
        break;
      }
      case E120_BOOT_SOFTWARE_VERSION_ID:
        if (resp.data_len() >= 4)
        {
          HandleBootSoftwareIdResponse(scope_handle, etcpal_unpack_u32b(resp.data()), resp.rdmnet_source_uid());
        }
        break;
      case E120_DMX_PERSONALITY:
        if (resp.data_len() >= 2)
        {
          HandlePersonalityResponse(scope_handle, resp.data()[0], resp.data()[1], resp.rdmnet_source_uid());
        }
        break;
      case E120_DMX_PERSONALITY_DESCRIPTION:
        if (resp.data_len() >= 3)
        {
          char   description[33]{};
          size_t descriptionLength = resp.data_len() - 3;
          memcpy(description, &resp.data()[3], (descriptionLength > 32) ? 32 : descriptionLength);

          HandlePersonalityDescResponse(scope_handle, resp.data()[0], etcpal_unpack_u16b(&resp.data()[1]),
                                        QString::fromUtf8(description), resp.rdmnet_source_uid());
        }
        break;
      case E137_7_ENDPOINT_LIST:
        if (resp.data_len() >= 4)
        {
          std::vector<std::pair<uint16_t, uint8_t>> list;
          uint32_t                                  change_number = etcpal_unpack_u32b(resp.data());
          for (const uint8_t* offset = &resp.data()[4]; offset + 2 < resp.data() + resp.data_len(); offset += 3)
          {
            uint16_t endpoint_id = etcpal_unpack_u16b(offset);
            uint8_t  endpoint_type = *(offset + 2);
            list.push_back(std::make_pair(endpoint_id, endpoint_type));
          }
          HandleEndpointListResponse(scope_handle, change_number, list, resp.rdmnet_source_uid());
        }
        break;
      case E137_7_ENDPOINT_RESPONDERS:
        if (resp.data_len() >= 6)
        {
          std::vector<rdm::Uid> list;
          uint16_t              endpoint_id = etcpal_unpack_u16b(resp.data());
          uint32_t              change_number = etcpal_unpack_u32b(&resp.data()[2]);

          for (const uint8_t* offset = &resp.data()[6]; offset + 5 < resp.data() + resp.data_len(); offset += 6)
          {
            list.push_back(rdm::Uid(etcpal_unpack_u16b(offset), etcpal_unpack_u32b(offset + 2)));
          }

          HandleEndpointRespondersResponse(scope_handle, endpoint_id, change_number, list, resp.rdmnet_source_uid());
        }
        break;
      case E137_7_ENDPOINT_LIST_CHANGE:
        if (resp.data_len() >= 4)
        {
          uint32_t change_number = etcpal_unpack_u32b(resp.data());
          HandleEndpointListChangeResponse(scope_handle, change_number, resp.rdmnet_source_uid());
        }
        break;
      case E137_7_ENDPOINT_RESPONDER_LIST_CHANGE:
        if (resp.data_len() >= 6)
        {
          uint16_t endpoint_id = etcpal_unpack_u16b(resp.data());
          uint32_t change_num = etcpal_unpack_u32b(&resp.data()[2]);
          HandleResponderListChangeResponse(scope_handle, change_num, endpoint_id, resp.rdmnet_source_uid());
        }
        break;
      case E133_TCP_COMMS_STATUS: {
        for (const uint8_t* offset = resp.data(); offset + 86 < resp.data() + resp.data_len(); offset += 87)
        {
          char scopeString[E133_SCOPE_STRING_PADDED_LENGTH]{};
          memcpy(scopeString, offset, E133_SCOPE_STRING_PADDED_LENGTH - 1);
          QString v4AddrString = UnpackAndParseIPAddress(offset + E133_SCOPE_STRING_PADDED_LENGTH, kEtcPalIpTypeV4);
          QString v6AddrString = UnpackAndParseIPAddress(offset + E133_SCOPE_STRING_PADDED_LENGTH + 4, kEtcPalIpTypeV6);
          uint16_t port = etcpal_unpack_u16b(offset + E133_SCOPE_STRING_PADDED_LENGTH + 4 + ETCPAL_IPV6_BYTES);
          uint16_t unhealthyTCPEvents =
              etcpal_unpack_u16b(offset + E133_SCOPE_STRING_PADDED_LENGTH + 4 + ETCPAL_IPV6_BYTES + 2);

          HandleTcpCommsStatusResponse(scope_handle, QString::fromUtf8(scopeString), v4AddrString, v6AddrString, port,
                                       unhealthyTCPEvents, resp.rdmnet_source_uid());
        }

        break;
      }
      default: {
        // Process data for PIDs that support get and set, where the data has the same form in either case.
        ProcessRdmGetSetData(scope_handle, resp.param_id(), resp.data(), static_cast<uint8_t>(resp.data_len()),
                             resp.rdmnet_source_uid());
        break;
      }
    }
  }
  else if (resp.IsSetResponse())
  {
    log_->Info("Got SET_COMMAND_RESPONSE with PID 0x%04x from responder %s", resp.param_id(),
               resp.rdmnet_source_uid().ToString().c_str());

    if (resp.OriginalCommandIncluded())
    {
      // Make sure this Controller is up-to-date with data that was set on a Device.
      switch (resp.param_id())
      {
        case E120_DMX_PERSONALITY: {
          if (resp.original_cmd_data_len() >= 2)
            HandlePersonalityResponse(scope_handle, resp.original_cmd_data()[0], 0, resp.rdmnet_source_uid());
          break;
        }
        default: {
          // Process PIDs with data that is in the same format for get and set.
          ProcessRdmGetSetData(scope_handle, resp.param_id(), resp.original_cmd_data(), resp.original_cmd_data_len(),
                               resp.rdmnet_source_uid());
          break;
        }
      }
    }
  }
}

void RDMnetNetworkModel::ProcessRdmGetSetData(rdmnet::ScopeHandle scope_handle,
                                              uint16_t            param_id,
                                              const uint8_t*      data,
                                              uint8_t             datalen,
                                              const rdm::Uid&     source_uid)
{
  if (data)
  {
    switch (param_id)
    {
      case E120_DEVICE_LABEL: {
        char label[33]{};
        memcpy(label, data, (datalen > 32) ? 32 : datalen);
        HandleDeviceLabelResponse(scope_handle, QString::fromUtf8(label), source_uid);
        break;
      }
      case E120_DMX_START_ADDRESS: {
        if (datalen >= 2)
          HandleStartAddressResponse(scope_handle, etcpal_unpack_u16b(data), source_uid);
        break;
      }
      case E120_IDENTIFY_DEVICE: {
        if (datalen >= 1)
          HandleIdentifyResponse(scope_handle, data[0], source_uid);
        break;
      }
      case E133_COMPONENT_SCOPE: {
        uint16_t       scope_slot;
        char           scope_string[E133_SCOPE_STRING_PADDED_LENGTH];
        QString        static_config_v4;
        QString        static_config_v6;
        uint16_t       port = 0;
        const uint8_t* cur_ptr = data;

        scope_slot = etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;
        memcpy(scope_string, cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH);
        scope_string[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
        cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

        uint8_t staticConfigType = *cur_ptr++;
        switch (staticConfigType)
        {
          case E133_STATIC_CONFIG_IPV4:
            static_config_v4 = UnpackAndParseIPAddress(cur_ptr, kEtcPalIpTypeV4);
            cur_ptr += 4 + 16;
            port = etcpal_unpack_u16b(cur_ptr);
            break;
          case E133_STATIC_CONFIG_IPV6:
            cur_ptr += 4;
            static_config_v6 = UnpackAndParseIPAddress(cur_ptr, kEtcPalIpTypeV6);
            cur_ptr += 16;
            port = etcpal_unpack_u16b(cur_ptr);
            break;
          case E133_NO_STATIC_CONFIG:
          default:
            break;
        }
        HandleComponentScopeResponse(scope_handle, scope_slot, QString::fromUtf8(scope_string), static_config_v4,
                                     static_config_v6, port, source_uid);
        break;
      }
      case E133_SEARCH_DOMAIN: {
        char domain_string[E133_DOMAIN_STRING_PADDED_LENGTH]{};
        memcpy(domain_string, data, datalen);
        HandleSearchDomainResponse(scope_handle, QString::fromUtf8(domain_string), source_uid);
        break;
      }
      default:
        break;
    }
  }
}

void RDMnetNetworkModel::HandleEndpointListResponse(rdmnet::ScopeHandle scope_handle,
                                                    uint32_t /*change_number*/,
                                                    const std::vector<std::pair<uint16_t, uint8_t>>& list,
                                                    const rdm::Uid&                                  source_uid)
{
  if (broker_connections_.find(scope_handle) == broker_connections_.end())
  {
    log_->Error("Error: HandleEndpointListResponse called with invalid scope handle.");
  }
  else
  {
    BrokerItem* broker_item = broker_connections_[scope_handle];
    if (broker_item)
    {
      for (auto client : broker_item->rdmnet_clients_)
      {
        if (client->uid() == source_uid)
        {
          // Found a matching discovered client
          emit newEndpointList(client, list);
          break;
        }
      }
    }
  }
}

void RDMnetNetworkModel::HandleEndpointRespondersResponse(rdmnet::ScopeHandle scope_handle,
                                                          uint16_t            endpoint,
                                                          uint32_t /*changeNumber*/,
                                                          const std::vector<rdm::Uid>& list,
                                                          const rdm::Uid&              source_uid)
{
  if (broker_connections_.find(scope_handle) == broker_connections_.end())
  {
    log_->Error("Error: HandleEndpointRespondersResponse called with invalid scope handle.");
  }
  else
  {
    BrokerItem* broker_item = broker_connections_[scope_handle];
    if (broker_item)
    {
      for (auto client : broker_item->rdmnet_clients_)
      {
        if (client->uid() == source_uid)
        {
          // Found a matching discovered client

          // Now find the matching endpoint
          for (auto endpt : client->endpoints_)
          {
            if (endpt->id() == endpoint)
            {
              // Found a matching endpoint
              emit newResponderList(endpt, list);
              break;
            }
          }
          break;
        }
      }
    }
  }
}

void RDMnetNetworkModel::HandleEndpointListChangeResponse(rdmnet::ScopeHandle scope_handle,
                                                          uint32_t /*changeNumber*/,
                                                          const rdm::Uid& source_uid)
{
  SendGetCommand(GetBrokerItem(scope_handle), source_uid, E137_7_ENDPOINT_LIST);
}

void RDMnetNetworkModel::HandleResponderListChangeResponse(rdmnet::ScopeHandle scope_handle,
                                                           uint32_t /*changeNumber*/,
                                                           uint16_t        endpoint,
                                                           const rdm::Uid& source_uid)
{
  uint8_t data[2];
  etcpal_pack_u16b(data, endpoint);
  SendGetCommand(GetBrokerItem(scope_handle), source_uid, E137_7_ENDPOINT_RESPONDERS, data, 2);
}

void RDMnetNetworkModel::HandleRdmNack(rdmnet::ScopeHandle scope_handle, const rdmnet::RdmResponse& resp)
{
  if (resp.IsSetResponse() && PropertyValueItem::pidInfoExists(resp.param_id()))
  {
    // Attempt to set a property failed. Get the original property value back.
    if (resp.param_id() == E133_COMPONENT_SCOPE)
    {
      uint8_t data[2];
      etcpal_pack_u16b(data, 0x0001);  // Scope slot, default to 1 for RPT Devices (non-controllers, non-brokers).
      SendGetCommand(GetBrokerItem(scope_handle), resp.rdmnet_source_uid(), resp.param_id(), data, 2);
    }
    else
    {
      SendGetCommand(GetBrokerItem(scope_handle), resp.rdmnet_source_uid(), resp.param_id());
    }
  }
  else if (resp.IsGetResponse() && (resp.param_id() == E133_COMPONENT_SCOPE) &&
           (resp.GetNackReason()->code() == kRdmNRDataOutOfRange))
  {
    RDMnetClientItem*  client = GetClientItem(scope_handle, resp.rdmnet_source_uid());
    RDMnetNetworkItem* rdmNetGroup = dynamic_cast<RDMnetNetworkItem*>(
        client->child(0)->data() == tr("RDMnet") ? client->child(0) : client->child(1));

    RemoveScopeSlotItemsInRange(rdmNetGroup, &client->properties, previous_slot_[resp.rdmnet_source_uid()] + 1, 0xFFFF);

    // We have all of this controller's scope-slot pairs. Now request scope-specific properties.
    previous_slot_[resp.rdmnet_source_uid()] = 0;
    SendGetCommand(GetBrokerItem(scope_handle), resp.rdmnet_source_uid(), E133_TCP_COMMS_STATUS);
  }
}

void RDMnetNetworkModel::HandleStatusMessagesResponse(uint8_t /*type*/,
                                                      uint16_t /*messageId*/,
                                                      uint16_t /*data1*/,
                                                      uint16_t /*data2*/,
                                                      const rdm::Uid& /*source_uid*/)
{
}

void RDMnetNetworkModel::HandleSupportedParametersResponse(rdmnet::ScopeHandle          scope_handle,
                                                           const std::vector<uint16_t>& params_list,
                                                           const rdm::Uid&              source_uid)
{
  if (params_list.size() > 0)
  {
    for (const uint16_t& param : params_list)
    {
      if (PidSupportedByGui(param, true) && param != E120_SUPPORTED_PARAMETERS)
      {
        SendGetCommand(GetBrokerItem(scope_handle), source_uid, param);
      }
      else if (param == E120_RESET_DEVICE)
      {
        RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

        if (device)
        {
          device->enableFeature(kResetDevice);
          emit featureSupportChanged(device, kResetDevice);
        }
      }
    }
  }
}

void RDMnetNetworkModel::HandleDeviceInfoResponse(rdmnet::ScopeHandle  scope_handle,
                                                  const RdmDeviceInfo& device_info,
                                                  const rdm::Uid&      source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 0),
                         device_info.protocol_version);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 1),
                         device_info.model_id);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 2),
                         device_info.category);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 3),
                         device_info.sw_version_id);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 4),
                         device_info.footprint);
    HandlePersonalityResponse(scope_handle, device_info.personality, device_info.num_personalities, source_uid);
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), device_info.dmx_address);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 5),
                         device_info.subdevice_count);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 6),
                         (uint16_t)device_info.sensor_count);
  }
}

void RDMnetNetworkModel::HandleModelDescResponse(rdmnet::ScopeHandle scope_handle,
                                                 const QString&      label,
                                                 const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    emit setPropertyData(device, E120_DEVICE_MODEL_DESCRIPTION,
                         PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_MODEL_DESCRIPTION), label);
  }
}

void RDMnetNetworkModel::HandleManufacturerLabelResponse(rdmnet::ScopeHandle scope_handle,
                                                         const QString&      label,
                                                         const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    emit setPropertyData(device, E120_MANUFACTURER_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_MANUFACTURER_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleDeviceLabelResponse(rdmnet::ScopeHandle scope_handle,
                                                   const QString&      label,
                                                   const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    emit setPropertyData(device, E120_DEVICE_LABEL, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_LABEL),
                         label);
  }
}

void RDMnetNetworkModel::HandleSoftwareLabelResponse(rdmnet::ScopeHandle scope_handle,
                                                     const QString&      label,
                                                     const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    emit setPropertyData(device, E120_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_SOFTWARE_VERSION_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleBootSoftwareIdResponse(rdmnet::ScopeHandle scope_handle,
                                                      uint32_t            id,
                                                      const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_ID,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_ID), id);
  }
}

void RDMnetNetworkModel::HandleBootSoftwareLabelResponse(rdmnet::ScopeHandle scope_handle,
                                                         const QString&      label,
                                                         const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleStartAddressResponse(rdmnet::ScopeHandle scope_handle,
                                                    uint16_t            address,
                                                    const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), address);
  }
}

void RDMnetNetworkModel::HandleIdentifyResponse(rdmnet::ScopeHandle scope_handle,
                                                bool                identifying,
                                                const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    device->setDeviceIdentifying(identifying);
    emit identifyChanged(device, identifying);
  }
}

void RDMnetNetworkModel::HandlePersonalityResponse(rdmnet::ScopeHandle scope_handle,
                                                   uint8_t             current,
                                                   uint8_t             number,
                                                   const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);

  if (device)
  {
    if (device->allPersonalityDescriptionsFound() && (current != 0))
    {
      emit setPropertyData(device, E120_DMX_PERSONALITY,
                           PropertyValueItem::pidPropertyDisplayName(E120_DMX_PERSONALITY),
                           device->personalityDescriptionAt(current - 1));
    }
    else if (!device->allPersonalityDescriptionsFound())
    {
      emit setPropertyData(device, E120_DMX_PERSONALITY,
                           PropertyValueItem::pidPropertyDisplayName(E120_DMX_PERSONALITY), tr(""));
    }

    bool personalityChanged =
        (current !=
         static_cast<uint8_t>(
             getPropertyData(device, E120_DMX_PERSONALITY, RDMnetNetworkItem::PersonalityNumberRole).toInt()));

    if ((current != 0) && personalityChanged)
    {
      emit setPropertyData(device, E120_DMX_PERSONALITY,
                           PropertyValueItem::pidPropertyDisplayName(E120_DMX_PERSONALITY), (uint16_t)current,
                           RDMnetNetworkItem::PersonalityNumberRole);

      SendGetCommand(GetNearestParentItemOfType<BrokerItem>(device), source_uid, E120_DEVICE_INFO);
    }

    CheckPersonalityDescriptions(device, number, source_uid);
  }
}

void RDMnetNetworkModel::HandlePersonalityDescResponse(rdmnet::ScopeHandle scope_handle,
                                                       uint8_t             personality,
                                                       uint16_t            footprint,
                                                       const QString&      description,
                                                       const rdm::Uid&     source_uid)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, source_uid);
  const bool         SHOW_FOOTPRINT = false;

  if (device)
  {
    device->personalityDescriptionFound(
        personality, footprint,
        SHOW_FOOTPRINT ? QString("(FP=%1) %2").arg(QString::number(footprint).rightJustified(2, '0'), description)
                       : description);

    if (device->allPersonalityDescriptionsFound())
    {
      QStringList personalityDescriptions = device->personalityDescriptionList();
      uint8_t     currentPersonality = static_cast<uint8_t>(
          getPropertyData(device, E120_DMX_PERSONALITY, RDMnetNetworkItem::PersonalityNumberRole).toInt());

      if (currentPersonality == 0)
      {
        emit setPropertyData(device, E120_DMX_PERSONALITY,
                             PropertyValueItem::pidPropertyDisplayName(E120_DMX_PERSONALITY), tr(""));
      }
      else
      {
        emit setPropertyData(device, E120_DMX_PERSONALITY,
                             PropertyValueItem::pidPropertyDisplayName(E120_DMX_PERSONALITY),
                             device->personalityDescriptionAt(currentPersonality - 1));
      }

      emit setPropertyData(device, E120_DMX_PERSONALITY,
                           PropertyValueItem::pidPropertyDisplayName(E120_DMX_PERSONALITY), personalityDescriptions,
                           RDMnetNetworkItem::PersonalityDescriptionListRole);
    }
  }
}

void RDMnetNetworkModel::HandleComponentScopeResponse(rdmnet::ScopeHandle scope_handle,
                                                      uint16_t            scopeSlot,
                                                      const QString&      scopeString,
                                                      const QString&      staticConfigV4,
                                                      const QString&      staticConfigV6,
                                                      uint16_t            port,
                                                      const rdm::Uid&     source_uid)
{
  RDMnetClientItem* client = GetClientItem(scope_handle, source_uid);

  if (client)
  {
    RDMnetNetworkItem* rdmNetGroup = dynamic_cast<RDMnetNetworkItem*>(
        client->child(0)->data() == tr("RDMnet") ? client->child(0) : client->child(1));

    if (client->rpt_type() == kRPTClientTypeController)
    {
      RemoveScopeSlotItemsInRange(rdmNetGroup, &client->properties, previous_slot_[client->uid()] + 1, scopeSlot - 1);
    }

    QString displayName;
    if (client->rpt_type() == kRPTClientTypeController)
    {
      displayName = QString("%0 (Slot %1)")
                        .arg(PropertyValueItem::pidPropertyDisplayName(E133_COMPONENT_SCOPE, 0))
                        .arg(scopeSlot);
    }
    else
    {
      displayName = PropertyValueItem::pidPropertyDisplayName(E133_COMPONENT_SCOPE, 0);
    }

    client->setScopeSlot(scopeString, scopeSlot);

    emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, scopeString);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, scopeString, RDMnetNetworkItem::ScopeDataRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, scopeSlot, RDMnetNetworkItem::ScopeSlotRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, 0, RDMnetNetworkItem::DisplayNameIndexRole);

    QString staticV4PropName = getScopeSubPropertyFullName(client, E133_COMPONENT_SCOPE, 1, scopeString);
    QString staticV6PropName = getScopeSubPropertyFullName(client, E133_COMPONENT_SCOPE, 2, scopeString);

    if (!staticConfigV4.isEmpty())
    {
      QString ipv4String = QString("%0:%1").arg(staticConfigV4).arg(port);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, ipv4String);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, QString(""));

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, ipv4String,
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, QString(""),
                           RDMnetNetworkItem::StaticIPv6DataRole);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, ipv4String,
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, QString(""),
                           RDMnetNetworkItem::StaticIPv6DataRole);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, ipv4String,
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, QString(""),
                           RDMnetNetworkItem::StaticIPv6DataRole);
    }
    else if (!staticConfigV6.isEmpty())
    {
      QString ipv6String = QString("[%0]:%1").arg(staticConfigV6).arg(port);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, QString(""));
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, ipv6String);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, QString(""),
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, ipv6String,
                           RDMnetNetworkItem::StaticIPv6DataRole);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, QString(""),
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, ipv6String,
                           RDMnetNetworkItem::StaticIPv6DataRole);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, QString(""),
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, ipv6String,
                           RDMnetNetworkItem::StaticIPv6DataRole);
    }
    else
    {
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, QString(""));
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, QString(""));

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, QString(""),
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, QString(""),
                           RDMnetNetworkItem::StaticIPv6DataRole);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, QString(""),
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, QString(""),
                           RDMnetNetworkItem::StaticIPv6DataRole);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, QString(""),
                           RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, QString(""),
                           RDMnetNetworkItem::StaticIPv6DataRole);
    }

    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, 1, RDMnetNetworkItem::DisplayNameIndexRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, 2, RDMnetNetworkItem::DisplayNameIndexRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, scopeString, RDMnetNetworkItem::ScopeDataRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, scopeString, RDMnetNetworkItem::ScopeDataRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, scopeSlot, RDMnetNetworkItem::ScopeSlotRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, scopeSlot, RDMnetNetworkItem::ScopeSlotRole);

    if (client->rpt_type() == kRPTClientTypeController)
    {
      previous_slot_[client->uid()] = scopeSlot;
      uint8_t data_buf[2];
      etcpal_pack_u16b(data_buf, std::min<uint16_t>(scopeSlot + 1, 0xffff));  // Scope slot, start with #1
      SendGetCommand(GetBrokerItem(scope_handle), source_uid, E133_COMPONENT_SCOPE, data_buf, 2);
    }
  }
}

void RDMnetNetworkModel::HandleSearchDomainResponse(rdmnet::ScopeHandle scope_handle,
                                                    const QString&      domainNameString,
                                                    const rdm::Uid&     source_uid)
{
  RDMnetClientItem* client = GetClientItem(scope_handle, source_uid);
  if (client)
  {
    emit setPropertyData(client, E133_SEARCH_DOMAIN, PropertyValueItem::pidPropertyDisplayName(E133_SEARCH_DOMAIN, 0),
                         domainNameString);
  }
}

void RDMnetNetworkModel::HandleTcpCommsStatusResponse(rdmnet::ScopeHandle scope_handle,
                                                      const QString&      scopeString,
                                                      const QString&      v4AddrString,
                                                      const QString&      v6AddrString,
                                                      uint16_t            port,
                                                      uint16_t            unhealthyTCPEvents,
                                                      const rdm::Uid&     source_uid)
{
  RDMnetClientItem* client = GetClientItem(scope_handle, source_uid);

  if (client)
  {
    if (client->getScopeSlot(scopeString) != 0)
    {
      QVariant    callbackObjectVariant;
      const char* callbackSlotString = SLOT(processPropertyButtonClick(const QPersistentModelIndex&));
      QString     callbackSlotQString(callbackSlotString);

      QString propertyName0 = getScopeSubPropertyFullName(client, E133_TCP_COMMS_STATUS, 0, scopeString);
      QString propertyName1 = getScopeSubPropertyFullName(client, E133_TCP_COMMS_STATUS, 1, scopeString);
      QString propertyName2 = getScopeSubPropertyFullName(client, E133_TCP_COMMS_STATUS, 2, scopeString);

      callbackObjectVariant.setValue(this);

      if (v4AddrString.isEmpty() && v6AddrString.isEmpty())
      {
        emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName0, QString(""));
      }
      else if (v4AddrString.isEmpty())  // use v6
      {
        emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName0,
                             QString("[%0]:%1").arg(v6AddrString).arg(port));
      }
      else  // use v4
      {
        emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName0,
                             QString("%0:%1").arg(v4AddrString).arg(port));
      }

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName1, unhealthyTCPEvents);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, tr("Reset"));

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, scopeString, RDMnetNetworkItem::ScopeDataRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, callbackObjectVariant,
                           RDMnetNetworkItem::CallbackObjectRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, callbackSlotQString,
                           RDMnetNetworkItem::CallbackSlotRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, source_uid.manufacturer_id(),
                           RDMnetNetworkItem::ClientManuRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, source_uid.device_id(),
                           RDMnetNetworkItem::ClientDevRole);

      // This needs to be the last call to setPropertyData so that the button can be enabled if needed.
      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, EditorWidgetType::kButton,
                           RDMnetNetworkItem::EditorWidgetTypeRole);
    }
  }
}

void RDMnetNetworkModel::AddPropertyEntries(RDMnetNetworkItem* item, PIDFlags location)
{
  // Start out by adding all known properties and disabling them. Later on,
  // only the properties that the device supports will be enabled.
  for (PIDInfoIterator i = PropertyValueItem::pidsBegin(); i != PropertyValueItem::pidsEnd(); ++i)
  {
    bool excludeFromModel = i->second.pidFlags & kExcludeFromModel;
    location = location & (kLocResponder | kLocEndpoint | kLocDevice | kLocController | kLocBroker);

    if (!excludeFromModel && ((i->second.pidFlags & location) == location))
    {
      for (const QString& j : i->second.propertyDisplayNames)
      {
        emit addPropertyEntry(item, i->first, j, i->second.role);
      }
    }
  }
}

void RDMnetNetworkModel::InitializeResponderProperties(ResponderItem* item)
{
  BrokerItem* broker_item = GetNearestParentItemOfType<BrokerItem>(item);

  AddPropertyEntries(item, kLocResponder);

  SendGetCommand(broker_item, item->uid(), E120_SUPPORTED_PARAMETERS);
  SendGetCommand(broker_item, item->uid(), E120_DEVICE_INFO);
  SendGetCommand(broker_item, item->uid(), E120_SOFTWARE_VERSION_LABEL);
  SendGetCommand(broker_item, item->uid(), E120_DMX_START_ADDRESS);
  SendGetCommand(broker_item, item->uid(), E120_IDENTIFY_DEVICE);
}

void RDMnetNetworkModel::InitializeRptClientProperties(RDMnetClientItem* item,
                                                       const rdm::Uid&   uid,
                                                       rpt_client_type_t clientType)
{
  BrokerItem* broker_item = GetNearestParentItemOfType<BrokerItem>(item);

  AddPropertyEntries(item, (clientType == kRPTClientTypeDevice) ? kLocDevice : kLocController);

  // Now send requests for core required properties.
  SendGetCommand(broker_item, uid, E120_SUPPORTED_PARAMETERS);
  SendGetCommand(broker_item, uid, E120_DEVICE_INFO);
  SendGetCommand(broker_item, uid, E120_SOFTWARE_VERSION_LABEL);
  SendGetCommand(broker_item, uid, E120_DMX_START_ADDRESS);
  SendGetCommand(broker_item, uid, E120_IDENTIFY_DEVICE);

  SendGetCommand(broker_item, uid, E133_SEARCH_DOMAIN);

  if (clientType == kRPTClientTypeDevice)  // For controllers, we need to wait for all the scopes first.
  {
    SendGetCommand(broker_item, uid, E133_TCP_COMMS_STATUS);
  }

  uint8_t data[2];
  etcpal_pack_u16b(data, 0x0001);  // Scope slot, start with #1
  SendGetCommand(broker_item, uid, E133_COMPONENT_SCOPE, data, 2);
}

uint8_t* RDMnetNetworkModel::PackIPAddressItem(const QVariant& value,
                                               etcpal_iptype_t addrType,
                                               uint8_t*        packPtr,
                                               bool            packPort)
{
  char         ipStrBuffer[64];
  unsigned int portNumber;
  size_t       memSize = ((addrType == kEtcPalIpTypeV4) ? 4 : (ETCPAL_IPV6_BYTES)) + (packPort ? 2 : 0);

  if (!packPtr)
  {
    return nullptr;
  }

  QString     valueQString = value.toString();
  QByteArray  local8Bit = valueQString.toLocal8Bit();
  const char* valueData = local8Bit.constData();

  if (value.toString().length() == 0)
  {
    memset(packPtr, 0, memSize);
  }
  else if (sscanf(valueData,
                  (addrType == kEtcPalIpTypeV4) ? "%63[1234567890.]:%u" : "[%63[1234567890:abcdefABCDEF]]:%u",
                  ipStrBuffer, &portNumber) < 2)
  {
    // Incorrect format entered.
    return nullptr;
  }
  else if (!ParseAndPackIPAddress(addrType, ipStrBuffer, packPtr))
  {
    return nullptr;
  }
  else if (portNumber > 65535)
  {
    return nullptr;
  }
  else if (packPort)
  {
    etcpal_pack_u16b(packPtr + memSize - 2, static_cast<uint16_t>(portNumber));
  }

  return packPtr + memSize;
}

bool RDMnetNetworkModel::PidSupportedByGui(uint16_t pid, bool checkSupportGet)
{
  for (PIDInfoIterator iter = PropertyValueItem::pidsBegin(); iter != PropertyValueItem::pidsEnd(); ++iter)
  {
    if ((iter->first == pid) && (!checkSupportGet || (iter->second.pidFlags & kSupportsGet)))
    {
      return true;
    }
  }

  return false;
}

RDMnetClientItem* RDMnetNetworkModel::GetClientItem(rdmnet::ScopeHandle scope_handle, const rdm::Uid& uid)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  if (broker_connections_.find(scope_handle) == broker_connections_.end())
  {
    log_->Error("Error: getClientItem called with invalid scope handle.");
  }
  else
  {
    BrokerItem* broker_item = broker_connections_[scope_handle];
    if (broker_item)
    {
      for (auto client : broker_item->rdmnet_clients_)
      {
        if (client->uid() == uid)
        {
          return client;
        }
      }
    }
  }

  return nullptr;
}

RDMnetNetworkItem* RDMnetNetworkModel::GetNetworkItem(rdmnet::ScopeHandle scope_handle, const rdm::Uid& uid)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  if (broker_connections_.find(scope_handle) == broker_connections_.end())
  {
    log_->Error("Error: getNetworkItem called with invalid connection cookie.");
  }
  else
  {
    BrokerItem* broker_item = broker_connections_[scope_handle];
    if (broker_item)
    {
      for (auto client : broker_item->rdmnet_clients_)
      {
        if (client->uid() == uid)
          return client;

        for (auto endpoint : client->endpoints_)
        {
          for (auto responder : endpoint->responders_)
          {
            if (responder->uid() == uid)
              return responder;
          }
        }
      }
    }
  }

  return nullptr;
}

void RDMnetNetworkModel::CheckPersonalityDescriptions(RDMnetNetworkItem* device,
                                                      uint8_t            numberOfPersonalities,
                                                      const rdm::Uid&    source_uid)
{
  if (numberOfPersonalities > 0)
  {
    if (device->initiatePersonalityDescriptionSearch(numberOfPersonalities))
    {
      // Get descriptions for all supported personalities of this device
      for (uint8_t personality_num = 1; personality_num <= numberOfPersonalities; ++personality_num)
      {
        SendGetCommand(GetNearestParentItemOfType<BrokerItem>(device), source_uid, E120_DMX_PERSONALITY,
                       &personality_num, 1);
      }
    }
  }
}

QVariant RDMnetNetworkModel::getPropertyData(RDMnetNetworkItem* parent, unsigned short pid, int role)
{
  QVariant result = QVariant();
  bool     foundProperty = false;

  for (std::vector<PropertyItem*>::iterator iter = parent->properties.begin();
       (iter != parent->properties.end()) && !foundProperty; ++iter)
  {
    if ((*iter)->getValueItem())
    {
      if ((*iter)->getValueItem()->getPID() == pid)
      {
        result = (*iter)->getValueItem()->data(role);
        foundProperty = true;
      }
    }
  }

  return result;
}

PropertyItem* RDMnetNetworkModel::createPropertyItem(RDMnetNetworkItem* parent, const QString& fullName)
{
  RDMnetNetworkItem* currentParent = parent;
  QString            currentPathName = fullName;
  QString            shortName = getShortPropertyName(fullName);
  PropertyItem*      propertyItem = new PropertyItem(fullName, shortName);

  while (currentPathName != shortName)
  {
    QString groupName = getHighestGroupName(currentPathName);

    RDMnetNetworkItem* groupingItem = getGroupingItem(currentParent, groupName);

    if (!groupingItem)
    {
      groupingItem = createGroupingItem(currentParent, groupName);
    }

    currentParent = groupingItem;
    groupingItem->properties.push_back(propertyItem);

    currentPathName = getChildPathName(currentPathName);
  }

  AppendRowToItem(currentParent, propertyItem);

  return propertyItem;
}

QString RDMnetNetworkModel::getShortPropertyName(const QString& fullPropertyName)
{
  QRegExp     re("(\\\\)");
  QStringList query = fullPropertyName.split(re);

  if (query.length() > 0)
  {
    return query.at(query.length() - 1);
  }

  return QString();
}

QString RDMnetNetworkModel::getHighestGroupName(const QString& pathName)
{
  QRegExp     re("(\\\\)");
  QStringList query = pathName.split(re);

  if (query.length() > 0)
  {
    return query.at(0);
  }

  return QString();
}

QString RDMnetNetworkModel::getPathSubset(const QString& fullPath, int first, int last)
{
  QRegExp     re("(\\\\)");
  QStringList query = fullPath.split(re);
  QString     result;

  if (last == -1)
  {
    last = (query.length() - 1);
  }

  for (int i = first; i <= std::min(last, (query.length() - 1)); ++i)
  {
    result += query.at(i);

    if (i != (query.length() - 1))
    {
      result += "\\";
    }
  }

  return result;
}

PropertyItem* RDMnetNetworkModel::getGroupingItem(RDMnetNetworkItem* parent, const QString& groupName)
{
  for (int i = 0; i < parent->rowCount(); ++i)
  {
    PropertyItem* item = dynamic_cast<PropertyItem*>(parent->child(i));

    if (item)
    {
      if (item->text() == groupName)
      {
        return item;
      }
    }
  }

  return nullptr;
}

PropertyItem* RDMnetNetworkModel::createGroupingItem(RDMnetNetworkItem* parent, const QString& groupName)
{
  PropertyItem* groupingItem = new PropertyItem(groupName, groupName);

  AppendRowToItem(parent, groupingItem);
  groupingItem->setEnabled(true);

  // Make sure values of group items are blank and inaccessible.
  PropertyValueItem* valueItem = new PropertyValueItem(QVariant(), false);
  groupingItem->setValueItem(valueItem);

  emit expandNewItem(groupingItem->index(), PropertyItem::PropertyItemType);

  return groupingItem;
}

QString RDMnetNetworkModel::getChildPathName(const QString& superPathName)
{
  QString highGroupName = getHighestGroupName(superPathName);
  int     startPosition = highGroupName.length() + 1;  // Name + delimiter character

  return superPathName.mid(startPosition, superPathName.length() - startPosition);
}

QString RDMnetNetworkModel::getScopeSubPropertyFullName(RDMnetClientItem* client,
                                                        uint16_t          pid,
                                                        int32_t           index,
                                                        const QString&    scope)
{
  QString original = PropertyValueItem::pidPropertyDisplayName(pid, index);

  if (client)
  {
    if (client->rpt_type() == kRPTClientTypeController)
    {
      QString     scopePropertyDisplay = PropertyValueItem::pidPropertyDisplayName(E133_COMPONENT_SCOPE, 0);
      QRegExp     re("(\\\\)");
      QStringList query = scopePropertyDisplay.split(re);

      return QString("%0%1 (Slot %2)\\%3")
          .arg(getPathSubset(original, 0, query.length() - 2))
          .arg(query.at(query.length() - 1))
          .arg(client->getScopeSlot(scope))
          .arg(getPathSubset(original, query.length() - 1));
    }
  }

  return original;
}

void RDMnetNetworkModel::RemoveScopeSlotItemsInRange(RDMnetNetworkItem*          parent,
                                                     std::vector<PropertyItem*>* properties,
                                                     uint16_t                    firstSlot,
                                                     uint16_t                    lastSlot)
{
  if (lastSlot >= firstSlot)
  {
    emit removePropertiesInRange(parent, properties, E133_COMPONENT_SCOPE, RDMnetNetworkItem::ScopeSlotRole, firstSlot,
                                 lastSlot);
  }
}
