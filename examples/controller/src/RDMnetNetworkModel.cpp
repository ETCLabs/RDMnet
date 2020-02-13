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

#include "RDMnetNetworkModel.h"

#include <cassert>
#include <algorithm>
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

void appendRowToItem(QStandardItem* parent, QStandardItem* child)
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
T* getNearestParentItemOfType(QStandardItem* child)
{
  T* parent = nullptr;
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
  bool newScopeAdded = false;

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
      rdmnet_client_scope_t new_scope_handle = rdmnet_->AddScope(scope.toStdString());
      if (new_scope_handle != RDMNET_CLIENT_SCOPE_INVALID)
      {
        BrokerItem* broker = new BrokerItem(scope, new_scope_handle);
        appendRowToItem(invisibleRootItem(), broker);
        broker->enableChildrenSearch();

        emit expandNewItem(broker->index(), BrokerItem::BrokerItemType);

        broker_connections_.insert(std::make_pair(new_scope_handle, broker));

        default_responder_.AddScope(scope.toStdString());
        newScopeAdded = true;
      }
    }
  }

  if (newScopeAdded)
  {
    // Broadcast GET_RESPONSE notification because of newly added scope
    std::vector<RdmParamData> resp_data_list;
    uint16_t nack_reason;
    if (default_responder_.GetComponentScope(0x0001, resp_data_list, nack_reason))
    {
      SendRDMGetResponsesBroadcast(E133_COMPONENT_SCOPE, resp_data_list);
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
  bool shouldSendRDMGetResponsesBroadcast = false;
  std::vector<RdmParamData> resp_data_list;

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
      StaticBrokerConfig static_broker;
      static_broker.valid = true;
      static_broker.addr = addr;

      rdmnet_client_scope_t new_scope_handle = rdmnet_->AddScope(scope.toStdString(), static_broker);
      if (new_scope_handle != RDMNET_CLIENT_SCOPE_INVALID)
      {
        BrokerItem* broker = new BrokerItem(scope, new_scope_handle, static_broker);
        appendRowToItem(invisibleRootItem(), broker);
        broker->enableChildrenSearch();

        emit expandNewItem(broker->index(), BrokerItem::BrokerItemType);

        broker_connections_.insert(std::make_pair(new_scope_handle, broker));

        default_responder_.AddScope(scope.toStdString(), static_broker);
        // Broadcast GET_RESPONSE notification because of newly added scope
        uint16_t nack_reason;
        if (default_responder_.GetComponentScope(0x0001, resp_data_list, nack_reason))
        {
          shouldSendRDMGetResponsesBroadcast = true;
        }
      }
    }
  }

  if (shouldSendRDMGetResponsesBroadcast)
  {
    SendRDMGetResponsesBroadcast(E133_COMPONENT_SCOPE, resp_data_list);
  }
}

void RDMnetNetworkModel::addCustomLogOutputStream(LogOutputStream* stream)
{
  log_->addCustomOutputStream(stream);
}

void RDMnetNetworkModel::removeCustomLogOutputStream(LogOutputStream* stream)
{
  log_->removeCustomOutputStream(stream);
}

void RDMnetNetworkModel::Connected(rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo& info)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  auto broker_itemIter = broker_connections_.find(scope_handle);
  if (broker_itemIter != broker_connections_.end())
  {
    // Update relevant data
    broker_itemIter->second->setConnected(true, info.broker_addr);
    std::string utf8_scope = broker_itemIter->second->scope().toStdString();
    default_responder_.UpdateScopeConnectionStatus(utf8_scope, true, info.broker_addr);

    // Broadcast GET_RESPONSE notification because of new connection
    std::vector<RdmParamData> resp_data_list;
    uint16_t nack_reason;
    if (default_responder_.GetTCPCommsStatus(nullptr, 0, resp_data_list, nack_reason))
    {
      SendRDMGetResponsesBroadcast(E133_TCP_COMMS_STATUS, resp_data_list);
    }

    log_->Log(ETCPAL_LOG_INFO, "Connected to broker on scope %s", utf8_scope.c_str());
    rdmnet_->RequestClientList(scope_handle);
  }
}

void RDMnetNetworkModel::ConnectFailed(rdmnet_client_scope_t scope_handle, const RdmnetClientConnectFailedInfo& info)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  BrokerItem* broker_item = broker_connections_[scope_handle];
  if (broker_item)
  {
    log_->Log(ETCPAL_LOG_INFO, "Connection failed to broker on scope %s: %s. %s",
              broker_item->scope().toStdString().c_str(), rdmnet_connect_fail_event_to_string(info.event),
              info.will_retry ? "Retrying..." : "NOT retrying!");
    if (info.event == kRdmnetConnectFailSocketFailure || info.event == kRdmnetConnectFailTcpLevel)
      log_->Log(ETCPAL_LOG_INFO, "Socket error: '%s'", etcpal_strerror(info.socket_err));
    if (info.event == kRdmnetConnectFailRejected)
      log_->Log(ETCPAL_LOG_INFO, "Reject reason: '%s'", rdmnet_connect_status_to_string(info.rdmnet_reason));
    // TODO: display user-facing information if this is a fatal connect failure.
  }
}

void RDMnetNetworkModel::Disconnected(rdmnet_client_scope_t scope_handle, const RdmnetClientDisconnectedInfo& info)
{
  etcpal::WriteGuard conn_write(conn_lock_);

  BrokerItem* broker_item = broker_connections_[scope_handle];
  if (broker_item)
  {
    if (broker_item->connected())
    {
      broker_item->setConnected(false);

      log_->Log(ETCPAL_LOG_INFO, "Disconnected from broker on scope %s: %s. %s",
                broker_item->scope().toStdString().c_str(), rdmnet_disconnect_event_to_string(info.event),
                info.will_retry ? "Retrying..." : "NOT retrying!");
      if (info.event == kRdmnetDisconnectAbruptClose)
        log_->Log(ETCPAL_LOG_INFO, "Socket error: '%s'", etcpal_strerror(info.socket_err));
      if (info.event == kRdmnetDisconnectGracefulRemoteInitiated)
        log_->Log(ETCPAL_LOG_INFO, "Disconnect reason: '%s'", rdmnet_disconnect_reason_to_string(info.rdmnet_reason));
      // TODO: display user-facing information if this is a fatal connect failure.

      emit brokerItemTextUpdated(broker_item);

      broker_item->rdmnet_clients_.clear();
      broker_item->completelyRemoveChildren(0, broker_item->rowCount());
      broker_item->enableChildrenSearch();

      // Broadcast GET_RESPONSE notification because of lost connection
      std::vector<RdmParamData> resp_data_list;
      uint16_t nack_reason;
      if (default_responder_.GetTCPCommsStatus(nullptr, 0, resp_data_list, nack_reason))
      {
        SendRDMGetResponsesBroadcast(E133_TCP_COMMS_STATUS, resp_data_list);
      }
    }
  }
}

void RDMnetNetworkModel::processAddRDMnetClients(BrokerItem* broker_item, const std::vector<RptClientEntry>& list)
{
  // Update the Controller's discovered list to match
  if (list.size() > 0)
  {
    broker_item->disableChildrenSearch();
  }

  for (const auto& rpt_entry : list)
  {
    bool is_me = (rpt_entry.cid == my_cid_);
    RDMnetClientItem* newRDMnetClientItem = new RDMnetClientItem(rpt_entry, is_me);
    bool itemAlreadyAdded = false;

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
      appendRowToItem(broker_item, newRDMnetClientItem);
      broker_item->rdmnet_clients_.push_back(newRDMnetClientItem);

      if (rpt_entry.type != kRPTClientTypeUnknown)
      {
        initializeRPTClientProperties(newRDMnetClientItem, rpt_entry.uid.manu, rpt_entry.uid.id, rpt_entry.type);

        newRDMnetClientItem->enableFeature(kIdentifyDevice);
        emit featureSupportChanged(newRDMnetClientItem, kIdentifyDevice);
      }

      newRDMnetClientItem->enableChildrenSearch();
    }
  }
}

void RDMnetNetworkModel::processRemoveRDMnetClients(BrokerItem* broker_item, const std::vector<RptClientEntry>& list)
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
        if (rpt_entry.type == clientItem->ClientType() && rpt_entry.uid == clientItem->uid())
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

void RDMnetNetworkModel::processNewEndpointList(RDMnetClientItem* treeClientItem,
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
      bool itemAlreadyAdded = false;

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
        appendRowToItem(treeClientItem, newEndpointItem);
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

void RDMnetNetworkModel::processNewResponderList(EndpointItem* treeEndpointItem, const std::vector<RdmUid>& list)
{
  bool somethingWasAdded = false;

  std::vector<ResponderItem*> prev_list = treeEndpointItem->responders_;

  // Save these devices
  for (auto resp_uid : list)
  {
    ResponderItem* newResponderItem = new ResponderItem(resp_uid.manu, resp_uid.id);
    bool itemAlreadyAdded = false;

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
      appendRowToItem(treeEndpointItem, newResponderItem);
      treeEndpointItem->responders_.push_back(newResponderItem);
      somethingWasAdded = true;

      initializeResponderProperties(newResponderItem, resp_uid.manu, resp_uid.id);

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

void RDMnetNetworkModel::processSetPropertyData(RDMnetNetworkItem* parent, unsigned short pid, const QString& name,
                                                const QVariant& value, int role)
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
      PropertyItem* propertyItem = createPropertyItem(parent, name);
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

void RDMnetNetworkModel::processRemovePropertiesInRange(RDMnetNetworkItem* parent,
                                                        std::vector<PropertyItem*>* properties, unsigned short pid,
                                                        int role, const QVariant& min, const QVariant& max)
{
  if (parent)
  {
    if (parent->isEnabled())
    {
      for (int i = parent->rowCount() - 1; i >= 0; --i)
      {
        PropertyItem* child = dynamic_cast<PropertyItem*>(parent->child(i, 0));
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

void RDMnetNetworkModel::processAddPropertyEntry(RDMnetNetworkItem* parent, unsigned short pid, const QString& name,
                                                 int role)
{
  processSetPropertyData(parent, pid, name, QVariant(), role);
}

void RDMnetNetworkModel::processPropertyButtonClick(const QPersistentModelIndex& propertyIndex)
{
  // Assuming this is SET TCP_COMMS_STATUS for now.
  if (propertyIndex.isValid())
  {
    QString scope = propertyIndex.data(RDMnetNetworkItem::ScopeDataRole).toString();

    RdmCommand setCmd;
    uint8_t maxBuffSize = PropertyValueItem::pidMaxBufferSize(E133_TCP_COMMS_STATUS);
    QVariant manuVariant = propertyIndex.data(RDMnetNetworkItem::ClientManuRole);
    QVariant devVariant = propertyIndex.data(RDMnetNetworkItem::ClientDevRole);

    // TODO Christian, I'm curious if it's possible to get the BrokerItem by moving upward through
    // parent items from the model index instead of finding it by scope string.
    rdmnet_client_scope_t scope_handle = RDMNET_CLIENT_SCOPE_INVALID;
    {
      etcpal::ReadGuard conn_read(conn_lock_);
      for (const auto& broker_pair : broker_connections_)
      {
        if (broker_pair.second->scope() == scope)
        {
          scope_handle = broker_pair.second->scope_handle();
          break;
        }
      }
    }

    if (scope_handle == RDMNET_CLIENT_SCOPE_INVALID)
    {
      log_->Log(ETCPAL_LOG_ERR, "Error: Cannot find broker connection for clicked button.");
    }
    else
    {
      setCmd.dest_uid.manu = static_cast<uint16_t>(manuVariant.toUInt());
      setCmd.dest_uid.id = static_cast<uint32_t>(devVariant.toUInt());
      setCmd.subdevice = 0;
      setCmd.command_class = kRdmCCSetCommand;
      setCmd.param_id = E133_TCP_COMMS_STATUS;
      setCmd.datalen = maxBuffSize;
      memset(setCmd.data, 0, maxBuffSize);
      memcpy(setCmd.data, scope.toUtf8().constData(), std::min<size_t>(scope.length(), maxBuffSize));

      SendRDMCommand(setCmd, scope_handle);
    }
  }
  else
  {
    log_->Log(ETCPAL_LOG_ERR, "Error: Button clicked on invalid property.");
  }
}

void RDMnetNetworkModel::removeBroker(BrokerItem* broker_item)
{
  bool removeComplete = false;

  rdmnet_client_scope_t scope_handle = broker_item->scope_handle();
  rdmnet_->RemoveScope(scope_handle, kRdmnetDisconnectUserReconfigure);
  {  // Write lock scope
    etcpal::WriteGuard conn_write(conn_lock_);
    broker_connections_.erase(scope_handle);
  }
  default_responder_.RemoveScope(broker_item->scope().toStdString());

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

  // Broadcast GET_RESPONSE notification because of removed scope
  std::vector<RdmParamData> resp_data_list;
  uint16_t nack_reason;
  if (default_responder_.GetComponentScope(0x0001, resp_data_list, nack_reason))
  {
    SendRDMGetResponsesBroadcast(E133_COMPONENT_SCOPE, resp_data_list);
  }
}

void RDMnetNetworkModel::removeAllBrokers()
{
  {  // Write lock scope
    etcpal::WriteGuard conn_write(conn_lock_);

    auto broker_iter = broker_connections_.begin();
    while (broker_iter != broker_connections_.end())
    {
      rdmnet_->RemoveScope(broker_iter->second->scope_handle(), kRdmnetDisconnectUserReconfigure);
      default_responder_.RemoveScope(broker_iter->second->scope().toStdString());
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

  // Broadcast GET_RESPONSE notification, which will send an empty scope
  // to show that there are no scopes left.

  std::vector<RdmParamData> resp_data_list;
  uint16_t nack_reason;
  if (default_responder_.GetComponentScope(0x0001, resp_data_list, nack_reason))
  {
    SendRDMGetResponsesBroadcast(E133_COMPONENT_SCOPE, resp_data_list);
  }
}

void RDMnetNetworkModel::activateFeature(RDMnetNetworkItem* device, SupportedDeviceFeature feature)
{
  if (device)
  {
    RdmCommand setCmd;

    setCmd.dest_uid = device->uid();
    setCmd.subdevice = 0;
    setCmd.command_class = kRdmCCSetCommand;

    if (feature & kResetDevice)
    {
      if (device->hasValidProperties())  // Means device wasn't reset
      {
        device->disableAllChildItems();
        device->setDeviceWasReset(true);
        device->setEnabled(false);

        emit featureSupportChanged(device, kResetDevice | kIdentifyDevice);

        setCmd.param_id = E120_RESET_DEVICE;
        setCmd.datalen = PropertyValueItem::pidMaxBufferSize(E120_RESET_DEVICE);

        memset(setCmd.data, 0, setCmd.datalen);
        setCmd.data[0] = 0xFF;  // Default to cold reset
      }
    }

    if (feature & kIdentifyDevice)
    {
      setCmd.param_id = E120_IDENTIFY_DEVICE;
      setCmd.datalen = PropertyValueItem::pidMaxBufferSize(E120_IDENTIFY_DEVICE);

      memset(setCmd.data, 0, setCmd.datalen);
      setCmd.data[0] = device->identifying() ? 0x00 : 0x01;
    }

    SendRDMCommand(setCmd, getNearestParentItemOfType<BrokerItem>(device));
  }
}

RDMnetNetworkModel::RDMnetNetworkModel(RDMnetLibInterface* library, ControllerLog* log) : rdmnet_(library), log_(log)
{
}

RDMnetNetworkModel* RDMnetNetworkModel::makeRDMnetNetworkModel(RDMnetLibInterface* library, ControllerLog* log)
{
  RDMnetNetworkModel* model = new RDMnetNetworkModel(library, log);

  model->rdmnet_->Startup(model->my_cid_, model);

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

  qRegisterMetaType<std::vector<RptClientEntry>>("std::vector<RptClientEntry>");
  qRegisterMetaType<std::vector<std::pair<uint16_t, uint8_t>>>("std::vector<std::pair<uint16_t, uint8_t>>");
  qRegisterMetaType<std::vector<RdmUid>>("std::vector<RdmUid>");
  qRegisterMetaType<std::vector<PropertyItem*>*>("std::vector<PropertyItem*>*");
  qRegisterMetaType<QVector<int>>("QVector<int>");
  qRegisterMetaType<uint16_t>("uint16_t");

  connect(model, SIGNAL(addRDMnetClients(BrokerItem*, const std::vector<ClientEntryData>&)), model,
          SLOT(processAddRDMnetClients(BrokerItem*, const std::vector<ClientEntryData>&)), Qt::AutoConnection);
  connect(model, SIGNAL(removeRDMnetClients(BrokerItem*, const std::vector<ClientEntryData>&)), model,
          SLOT(processRemoveRDMnetClients(BrokerItem*, const std::vector<ClientEntryData>&)), Qt::AutoConnection);
  connect(model, SIGNAL(newEndpointList(RDMnetClientItem*, const std::vector<std::pair<uint16_t, uint8_t>>&)), model,
          SLOT(processNewEndpointList(RDMnetClientItem*, const std::vector<std::pair<uint16_t, uint8_t>>&)),
          Qt::AutoConnection);
  connect(model, SIGNAL(newResponderList(EndpointItem*, const std::vector<RdmUid>&)), model,
          SLOT(processNewResponderList(EndpointItem*, const std::vector<RdmUid>&)), Qt::AutoConnection);
  connect(model, SIGNAL(setPropertyData(RDMnetNetworkItem*, unsigned short, const QString&, const QVariant&, int)),
          model, SLOT(processSetPropertyData(RDMnetNetworkItem*, unsigned short, const QString&, const QVariant&, int)),
          Qt::AutoConnection);
  connect(model,
          SIGNAL(removePropertiesInRange(RDMnetNetworkItem*, std::vector<PropertyItem*>*, unsigned short, int,
                                         const QVariant&, const QVariant&)),
          model,
          SLOT(processRemovePropertiesInRange(RDMnetNetworkItem*, std::vector<PropertyItem*>*, unsigned short, int,
                                              const QVariant&, const QVariant&)),
          Qt::AutoConnection);
  connect(model, SIGNAL(addPropertyEntry(RDMnetNetworkItem*, unsigned short, const QString&, int)), model,
          SLOT(processAddPropertyEntry(RDMnetNetworkItem*, unsigned short, const QString&, int)), Qt::AutoConnection);

  return model;
}

RDMnetNetworkModel* RDMnetNetworkModel::makeTestModel()
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

    appendRowToItem(parentItem, item);
    parentItem->setChild(parentItem->rowCount() - 1, 1, item2);

    parentItem = item;
  }

  if (parentItem->type() == RDMnetNetworkItem::RDMnetNetworkItemType)
    dynamic_cast<RDMnetNetworkItem*>(parentItem)->enableChildrenSearch();

  return model;
}

void RDMnetNetworkModel::Shutdown()
{
  {  // Write lock scope
    etcpal::WriteGuard conn_write(conn_lock_);

    for (auto& connection : broker_connections_)
      rdmnet_->RemoveScope(connection.first, kRdmnetDisconnectShutdown);

    broker_connections_.clear();
  }

  rdmnet_->Shutdown();

  rdmnet_ = nullptr;
  log_ = nullptr;
}

void RDMnetNetworkModel::searchingItemRevealed(SearchingStatusItem* searchItem)
{
  if (searchItem)
  {
    if (!searchItem->wasSearchInitiated())
    {
      // A search item was likely just revealed in the tree, starting a search process.
      QStandardItem* searchItemParent = searchItem->parent();

      if (searchItemParent)
      {
        switch (searchItemParent->type())
        {
          case BrokerItem::BrokerItemType:
          {
            searchItem->setSearchInitiated(true);

            break;
          }

          case RDMnetClientItem::RDMnetClientItemType:
          {
            RDMnetClientItem* clientItem = dynamic_cast<RDMnetClientItem*>(searchItemParent);

            if (clientItem)
            {
              RdmCommand cmd;

              cmd.dest_uid = clientItem->uid();
              cmd.subdevice = 0;

              searchItem->setSearchInitiated(true);

              // Send command to get endpoint list
              cmd.command_class = kRdmCCGetCommand;
              cmd.param_id = E137_7_ENDPOINT_LIST;
              cmd.datalen = 0;

              SendRDMCommand(cmd, getNearestParentItemOfType<BrokerItem>(clientItem));
            }

            break;
          }

          case EndpointItem::EndpointItemType:
          {
            EndpointItem* endpointItem = dynamic_cast<EndpointItem*>(searchItemParent);

            if (endpointItem)
            {
              // Ask for the devices on each endpoint
              RdmCommand cmd;

              cmd.dest_uid = endpointItem->parent_uid();
              cmd.subdevice = 0;

              searchItem->setSearchInitiated(true);

              // Send command to get endpoint devices
              cmd.command_class = kRdmCCGetCommand;
              cmd.param_id = E137_7_ENDPOINT_RESPONDERS;
              cmd.datalen = sizeof(uint16_t);
              etcpal_pack_u16b(cmd.data, endpointItem->id());

              SendRDMCommand(cmd, getNearestParentItemOfType<BrokerItem>(endpointItem));
            }

            break;
          }
        }
      }
    }
  }
}

size_t RDMnetNetworkModel::getNumberOfCustomLogOutputStreams()
{
  return log_->getNumberOfCustomLogOutputStreams();
}

bool RDMnetNetworkModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  QStandardItem* item = itemFromIndex(index);
  bool updateValue = true;
  QVariant newValue = value;

  if (item)
  {
    if (item->type() == PropertyValueItem::PropertyValueItemType)
    {
      PropertyValueItem* propertyValueItem = dynamic_cast<PropertyValueItem*>(item);
      RDMnetNetworkItem* parentItem = getNearestParentItemOfType<ResponderItem>(item);

      if (!parentItem)
      {
        parentItem = getNearestParentItemOfType<RDMnetClientItem>(item);
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
            RdmCommand setCmd;
            uint8_t maxBuffSize = PropertyValueItem::pidMaxBufferSize(pid);
            QString qstr;
            std::string stdstr;
            uint8_t* packPtr;

            // IP static config variables
            char ipStrBuffer[64];

            memset(ipStrBuffer, '\0', 64);

            setCmd.dest_uid = parentItem->uid();
            setCmd.subdevice = 0;
            setCmd.command_class = kRdmCCSetCommand;
            setCmd.param_id = pid;
            setCmd.datalen = maxBuffSize;
            memset(setCmd.data, 0, maxBuffSize);
            packPtr = setCmd.data;

            // Special cases for certain PIDs
            if (pid == E133_COMPONENT_SCOPE)
            {
              // Scope slot (default to 1)
              etcpal_pack_u16b(packPtr, static_cast<uint16_t>(index.data(RDMnetNetworkItem::ScopeSlotRole).toInt()));
              packPtr += 2;
            }

            switch (PropertyValueItem::pidDataType(pid))
            {
              case QVariant::Type::Int:
                switch (maxBuffSize - (packPtr - setCmd.data))
                {
                  case 2:
                    etcpal_pack_u16b(packPtr, static_cast<uint16_t>(value.toInt()));
                    break;
                  case 4:
                    etcpal_pack_u32b(packPtr, static_cast<uint32_t>(value.toInt()));
                    break;
                }
                break;
              case QVariant::Type::String:
                qstr = value.toString();
                qstr.truncate(maxBuffSize - static_cast<uint8_t>((packPtr - setCmd.data)));
                newValue = qstr;
                stdstr = qstr.toStdString();
                memcpy(packPtr, stdstr.data(), stdstr.length());
                break;
              case QVariant::Type::Bool:
                if (value.toBool())
                {
                  packPtr[0] = 1;
                }
                else
                {
                  packPtr[0] = 0;
                }
                break;
              case QVariant::Type::Char:
                packPtr[0] = static_cast<uint8_t>(value.toInt());
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

                  qstr = scopeString.toString();
                  qstr.truncate(E133_SCOPE_STRING_PADDED_LENGTH);
                  if (displayNameIndex == 0)
                  {
                    newValue = qstr;
                  }
                  stdstr = qstr.toStdString();
                  memcpy(packPtr, stdstr.data(), stdstr.length());
                  packPtr += 63;

                  if ((ipv4String.toString().length() > 0) &&
                      ((displayNameIndex != 2) || (ipv6String.toString().length() == 0)))
                  {
                    *packPtr = E133_STATIC_CONFIG_IPV4;
                  }
                  else if ((ipv6String.toString().length() > 0) &&
                           ((displayNameIndex != 1) || (ipv4String.toString().length() == 0)))
                  {
                    *packPtr = E133_STATIC_CONFIG_IPV6;
                    updateValue = false;  // IPv6 is still in development, so make this read-only for now.
                  }
                  else
                  {
                    *packPtr = E133_NO_STATIC_CONFIG;
                  }

                  uint8_t staticConfigType = *packPtr;
                  uint16_t port = 0;

                  ++packPtr;

                  packPtr = packIPAddressItem(ipv4String, kEtcPalIpTypeV4, packPtr,
                                              (staticConfigType == E133_STATIC_CONFIG_IPV4));

                  if ((staticConfigType == E133_STATIC_CONFIG_IPV4) && (packPtr != nullptr))
                  {
                    // This way, packIPAddressItem obtained the port value for us.
                    // Save the port value for later - we don't want it packed here.
                    packPtr -= 2;
                    port = etcpal_unpack_u16b(packPtr);
                  }

                  packPtr = packIPAddressItem(ipv6String, kEtcPalIpTypeV6, packPtr,
                                              (staticConfigType != E133_STATIC_CONFIG_IPV4));

                  if ((staticConfigType == E133_STATIC_CONFIG_IPV4) && (packPtr != nullptr))
                  {
                    // Pack the port value saved from earlier.
                    etcpal_pack_u16b(packPtr, port);
                    packPtr += 2;
                  }
                }
                else
                {
                  updateValue = false;
                }
                break;
            }

            updateValue = updateValue && (packPtr != nullptr);

            if (updateValue)
            {
              BrokerItem* broker_item = getNearestParentItemOfType<BrokerItem>(parentItem);
              SendRDMCommand(setCmd, broker_item);

              if (pid == E120_DMX_PERSONALITY)
              {
                sendGetCommand(broker_item, E120_DEVICE_INFO, parentItem->uid());
              }
            }
          }
        }
      }
    }
  }

  return updateValue ? QStandardItemModel::setData(index, newValue, role) : false;
}

void RDMnetNetworkModel::ClientListUpdate(rdmnet_client_scope_t scope_handle, client_list_action_t action,
                                          const RptClientList& list)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  BrokerItem* broker_item = broker_connections_[scope_handle];

  std::vector<RptClientEntry> entries;
  entries.assign(list.client_entries, list.client_entries + list.num_client_entries);

  // TODO the four possible actions need to be handled properly
  // kRdmnetClientListAppend means this list should be added to the existing clients
  // kRdmnetClientListReplace means this list should replace the current client list
  // kRdmnetClientListUpdate means this list contains updated information for some existing clients
  // kRdmnetClientListRemove means this list should be removed from the existing clients
  if (action == kRdmnetClientListRemove)
    emit removeRDMnetClients(broker_item, entries);
  else
    emit addRDMnetClients(broker_item, entries);
}

void RDMnetNetworkModel::StatusReceived(rdmnet_client_scope_t /* scope_handle */, const RemoteRptStatus& status)
{
  // This function has some work TODO. We should at least be logging things
  // here.

  log_->Log(ETCPAL_LOG_INFO, "Got RPT Status with code %d", status.msg.status_code);
  switch (status.msg.status_code)
  {
    case kRptStatusRdmTimeout:  // See Section 8.5.3
      // printf("Endpoint Status 'RDM Timeout' size= %u\n", statusSize);
      break;
    case kRptStatusInvalidRdmResponse:  // An invalid response was received from the E1.20 device.
      // printf("Endpoint Status 'RDM Invalid Response' size= %u\n",statusSize);
      break;
    case kRptStatusUnknownRdmUid:  // The E1.20 UID is not recognized as a UID associated with the endpoint.
      // printf("Endpoint Status 'Unknown RDM UID' size= %u\n", statusSize);
      break;
    case kRptStatusUnknownRptUid:
      // printf("Endpoint Status 'Unknown RDMnet UID' size= %u\n", statusSize);
      break;
    case kRptStatusUnknownEndpoint:  // Endpoint Number is not defined or does not exist on the device.
      // printf("Endpoint Status 'Unknown Endpoint' size= %u\n", statusSize);
      break;
    case kRptStatusBroadcastComplete:  // The gateway completed sending the previous Broadcast message out the RDM
                                       // Endpoint.
      // printf("Endpoint Status 'Broadcast Complete' size= %u\n", statusSize);
      break;
    case kRptStatusUnknownVector:
      // printf("Endpoint Status 'Unknown Vector' size= %u\n", statusSize);
      break;
    case kRptStatusInvalidCommandClass:
      // printf("Endpoint Status 'Invalid Command Class' size= %u\n", statusSize);
      break;
    case kRptStatusInvalidMessage:
      // printf("Endpoint Status 'Invalid Message' size= %u\n", statusSize);
      break;
    default:
      // printf("ERROR Endpoint Status: Bad Code 0x%04x size= %u\n",
      // statusCode, statusSize);
      break;
  }
}

void RDMnetNetworkModel::LlrpRdmCommandReceived(const LlrpRemoteRdmCommand& cmd)
{
  bool should_nack = false;
  uint16_t nack_reason;

  const RdmCommand& rdm = cmd.rdm;
  if (rdm.command_class == kRdmCCGetCommand)
  {
    std::vector<RdmParamData> resp_data_list;

    if (default_responder_.Get(rdm.param_id, rdm.data, rdm.datalen, resp_data_list, nack_reason))
    {
      if (resp_data_list.size() == 1)
      {
        SendLlrpGetResponse(cmd, resp_data_list);

        log_->Log(ETCPAL_LOG_DEBUG, "ACK'ing GET_COMMAND for PID 0x%04x from LLRP Manager %04x:%08x", rdm.param_id,
                  rdm.source_uid.manu, rdm.source_uid.id);
      }
      else
      {
        should_nack = true;
        nack_reason = E137_7_NR_ACTION_NOT_SUPPORTED;
      }
    }
    else
    {
      should_nack = true;
    }
  }
  else
  {
    // This controller is currently read-only.
    should_nack = true;
    nack_reason = E120_NR_UNSUPPORTED_COMMAND_CLASS;
  }

  if (should_nack)
  {
    SendLlrpNack(cmd, nack_reason);
    log_->Log(ETCPAL_LOG_DEBUG, "Sending NACK to LLRP Manager %04x:%08x for PID 0x%04x with reason 0x%04x",
              rdm.source_uid.manu, rdm.source_uid.id, rdm.param_id, nack_reason);
  }
}

bool RDMnetNetworkModel::SendRDMCommand(const RdmCommand& cmd, const BrokerItem* broker_item)
{
  if (!broker_item)
    return false;

  bool found_responder = false;
  RdmnetLocalRdmCommand cmd_to_send;

  for (const RDMnetClientItem* client : broker_item->rdmnet_clients_)
  {
    if (client->uid() == cmd.dest_uid)
    {
      // We are sending a command to a Default Responder.
      cmd_to_send.dest_uid = cmd.dest_uid;
      cmd_to_send.dest_endpoint = E133_NULL_ENDPOINT;
      found_responder = true;
      break;
    }
    else
    {
      for (const EndpointItem* endpoint : client->endpoints_)
      {
        for (const ResponderItem* responder : endpoint->responders_)
        {
          if (responder->uid() == cmd.dest_uid)
          {
            cmd_to_send.dest_uid = client->uid();
            cmd_to_send.dest_endpoint = endpoint->id();
            found_responder = true;
            break;
          }
        }
        if (found_responder)
          break;
      }
      if (found_responder)
        break;
    }
  }

  if (!found_responder)
    return false;

  cmd_to_send.rdm = cmd;
  return rdmnet_->SendRdmCommand(broker_item->scope_handle(), cmd_to_send);
}

bool RDMnetNetworkModel::SendRDMCommand(const RdmCommand& cmd, rdmnet_client_scope_t scope_handle)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  if (broker_connections_.find(scope_handle) != broker_connections_.end())
  {
    const BrokerItem* broker_item = broker_connections_[scope_handle];
    if (broker_item)
    {
      return SendRDMCommand(cmd, broker_item);
    }
  }
  return false;
}

void RDMnetNetworkModel::SendRDMGetResponses(rdmnet_client_scope_t scope_handle, const RdmUid& dest_uid,
                                             uint16_t param_id, const std::vector<RdmParamData>& resp_data_list,
                                             bool have_command, const RdmnetRemoteRdmCommand& cmd)
{
  std::vector<RdmResponse> resp_list;
  RdmResponse resp_data;
  RdmnetLocalRdmResponse resp;

  resp.dest_uid = dest_uid;
  resp.seq_num = have_command ? cmd.seq_num : 0;
  resp.source_endpoint = E133_NULL_ENDPOINT;
  resp.command_included = have_command;
  if (have_command)
    resp.cmd = cmd.rdm;

  // The source UID is added by the library right before sending.
  resp_data.dest_uid = dest_uid;
  resp_data.transaction_num = have_command ? cmd.rdm.transaction_num : 0;
  resp_data.resp_type = resp_data_list.size() > 1 ? kRdmResponseTypeAckOverflow : kRdmResponseTypeAck;
  resp_data.msg_count = 0;
  resp_data.subdevice = 0;
  resp_data.command_class = kRdmCCGetCommandResponse;
  resp_data.param_id = param_id;

  for (size_t i = 0; i < resp_data_list.size(); ++i)
  {
    memcpy(resp_data.data, resp_data_list[i].data, resp_data_list[i].datalen);
    resp_data.datalen = resp_data_list[i].datalen;
    if (i == resp_data_list.size() - 1)
    {
      resp_data.resp_type = kRdmResponseTypeAck;
    }
    resp_list.push_back(resp_data);
  }

  resp.num_responses = resp_list.size();
  resp.responses = resp_list.data();
  rdmnet_->SendRdmResponse(scope_handle, resp);
}

void RDMnetNetworkModel::SendRDMGetResponsesBroadcast(uint16_t param_id,
                                                      const std::vector<RdmParamData>& resp_data_list)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  for (const auto& broker_pair : broker_connections_)
  {
    if (broker_pair.second->connected())
    {
      SendRDMGetResponses(broker_pair.second->scope_handle(), kRdmnetControllerBroadcastUid, param_id, resp_data_list);
    }
  }
}

void RDMnetNetworkModel::SendRDMNack(rdmnet_client_scope_t scope, const RdmnetRemoteRdmCommand& received_cmd,
                                     uint16_t nack_reason)
{
  RdmResponse rdm_resp;
  rdm_resp.dest_uid = received_cmd.rdm.source_uid;
  rdm_resp.transaction_num = received_cmd.rdm.transaction_num;
  rdm_resp.resp_type = kRdmResponseTypeNackReason;
  rdm_resp.msg_count = 0;
  rdm_resp.subdevice = 0;
  rdm_resp.command_class =
      (received_cmd.rdm.command_class == kRdmCCSetCommand) ? kRdmCCSetCommandResponse : kRdmCCGetCommandResponse;
  rdm_resp.param_id = received_cmd.rdm.param_id;
  rdm_resp.datalen = 2;
  etcpal_pack_u16b(rdm_resp.data, nack_reason);

  RdmnetLocalRdmResponse resp;
  resp.dest_uid = received_cmd.source_uid;
  resp.num_responses = 1;
  resp.responses = &rdm_resp;
  resp.seq_num = received_cmd.seq_num;
  resp.source_endpoint = E133_NULL_ENDPOINT;
  resp.command_included = true;
  resp.cmd = received_cmd.rdm;
  rdmnet_->SendRdmResponse(scope, resp);
}

void RDMnetNetworkModel::SendLlrpGetResponse(const LlrpRemoteRdmCommand& received_cmd,
                                             const std::vector<RdmParamData>& resp_data_list)
{
  if (resp_data_list.size() != 1)
    return;

  RdmResponse rdm_resp;
  rdm_resp.source_uid = received_cmd.rdm.dest_uid;
  rdm_resp.dest_uid = received_cmd.rdm.source_uid;
  rdm_resp.transaction_num = received_cmd.rdm.transaction_num;
  rdm_resp.resp_type = kRdmResponseTypeAck;
  rdm_resp.msg_count = 0;
  rdm_resp.subdevice = 0;
  rdm_resp.command_class = kRdmCCGetCommandResponse;
  rdm_resp.param_id = received_cmd.rdm.param_id;
  memcpy(rdm_resp.data, resp_data_list[0].data, resp_data_list[0].datalen);
  rdm_resp.datalen = resp_data_list[0].datalen;

  LlrpLocalRdmResponse resp;
  LLRP_CREATE_RESPONSE_FROM_COMMAND(&resp, &received_cmd, &rdm_resp);
  rdmnet_->SendLlrpResponse(resp);
}

void RDMnetNetworkModel::SendLlrpNack(const LlrpRemoteRdmCommand& received_cmd, uint16_t nack_reason)
{
  RdmResponse rdm_resp;
  rdm_resp.dest_uid = received_cmd.rdm.source_uid;
  rdm_resp.transaction_num = received_cmd.rdm.transaction_num;
  rdm_resp.resp_type = kRdmResponseTypeNackReason;
  rdm_resp.msg_count = 0;
  rdm_resp.subdevice = 0;
  rdm_resp.command_class =
      (received_cmd.rdm.command_class == kRdmCCSetCommand) ? kRdmCCSetCommandResponse : kRdmCCGetCommandResponse;
  rdm_resp.param_id = received_cmd.rdm.param_id;
  rdm_resp.datalen = 2;
  etcpal_pack_u16b(rdm_resp.data, nack_reason);

  LlrpLocalRdmResponse resp;
  LLRP_CREATE_RESPONSE_FROM_COMMAND(&resp, &received_cmd, &rdm_resp);
  rdmnet_->SendLlrpResponse(resp);
}

void RDMnetNetworkModel::RdmCommandReceived(rdmnet_client_scope_t scope_handle, const RdmnetRemoteRdmCommand& cmd)
{
  bool should_nack = false;
  uint16_t nack_reason;

  const RdmCommand& rdm = cmd.rdm;
  if (rdm.command_class == kRdmCCGetCommand)
  {
    std::vector<RdmParamData> resp_data_list;

    if (default_responder_.Get(rdm.param_id, rdm.data, rdm.datalen, resp_data_list, nack_reason))
    {
      SendRDMGetResponses(scope_handle, cmd.source_uid, rdm.param_id, resp_data_list, true, cmd);

      log_->Log(ETCPAL_LOG_DEBUG, "ACK'ing GET_COMMAND for PID 0x%04x from Controller %04x:%08x", rdm.param_id,
                cmd.source_uid.manu, cmd.source_uid.id);
    }
    else
    {
      should_nack = true;
    }
  }
  else
  {
    // This controller is currently read-only.
    should_nack = true;
    nack_reason = E120_NR_UNSUPPORTED_COMMAND_CLASS;
  }

  if (should_nack)
  {
    SendRDMNack(scope_handle, cmd, nack_reason);
    log_->Log(ETCPAL_LOG_DEBUG, "Sending NACK to Controller %04x:%08x for PID 0x%04x with reason 0x%04x",
              cmd.source_uid.manu, cmd.source_uid.id, rdm.param_id, nack_reason);
  }
}

void RDMnetNetworkModel::RdmResponseReceived(rdmnet_client_scope_t scope_handle, const RdmnetRemoteRdmResponse& resp)
{
  // Since we are compiling with RDMNET_DYNAMIC_MEM, we should never get partial responses.
  assert(!resp.more_coming);
  assert(resp.num_responses >= 1);

  const RdmResponse& first_resp = *resp.responses;
  switch (first_resp.resp_type)
  {
    case kRdmResponseTypeAckTimer:
    {
      return;
    }
    case kRdmResponseTypeAck:
    case kRdmResponseTypeAckOverflow:
      HandleRDMAckOrAckOverflow(scope_handle, resp);
      // Continue processing below
      break;
    case E120_RESPONSE_TYPE_NACK_REASON:
    {
      uint16_t nackReason = 0xffff;

      if (first_resp.datalen == 2)
        nackReason = etcpal_unpack_u16b(first_resp.data);
      HandleRDMNack(scope_handle, nackReason, first_resp);
      return;
    }
    default:
      return;  // Unknown response type
  }
}

void RDMnetNetworkModel::HandleRDMAckOrAckOverflow(rdmnet_client_scope_t scope_handle,
                                                   const RdmnetRemoteRdmResponse& resp)
{
  const RdmResponse* first_resp = resp.responses;

  if (first_resp->command_class == kRdmCCGetCommandResponse)
  {
    log_->Log(ETCPAL_LOG_INFO, "Got GET_COMMAND_RESPONSE with PID 0x%04x from Controller %04x:%08x",
              first_resp->param_id, first_resp->source_uid.manu, first_resp->source_uid.id);

    switch (first_resp->param_id)
    {
      case E120_STATUS_MESSAGES:
      {
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
      case E120_SUPPORTED_PARAMETERS:
      {
        std::vector<uint16_t> list;

        for (const RdmResponse* response = resp.responses; response < resp.responses + resp.num_responses; ++response)
        {
          for (size_t pos = 0; pos + 1 < response->datalen; pos += 2)
            list.push_back(etcpal_unpack_u16b(&response->data[pos]));
        }

        if (!list.empty())
          HandleSupportedParametersResponse(scope_handle, list, *first_resp);
        break;
      }
      case E120_DEVICE_INFO:
      {
        if (first_resp->datalen >= 19)
        {
          // Current personality is reset if less than 1
          uint8_t cur_pers = (first_resp->data[12] < 1 ? 1 : first_resp->data[12]);
          // Total personality is reset if current or total is less than 1
          uint8_t total_pers = ((first_resp->data[12] < 1 || first_resp->data[13] < 1) ? 1 : first_resp->data[13]);

          HandleDeviceInfoResponse(scope_handle, etcpal_unpack_u16b(&first_resp->data[0]),
                                   etcpal_unpack_u16b(&first_resp->data[2]), etcpal_unpack_u16b(&first_resp->data[4]),
                                   etcpal_unpack_u32b(&first_resp->data[6]), etcpal_unpack_u16b(&first_resp->data[10]),
                                   cur_pers, total_pers, etcpal_unpack_u16b(&first_resp->data[14]),
                                   etcpal_unpack_u16b(&first_resp->data[16]), first_resp->data[18], *first_resp);
        }
        break;
      }
      case E120_DEVICE_MODEL_DESCRIPTION:
      case E120_MANUFACTURER_LABEL:
      case E120_DEVICE_LABEL:
      case E120_SOFTWARE_VERSION_LABEL:
      case E120_BOOT_SOFTWARE_VERSION_LABEL:
      {
        char label[33];

        // Ensure that the string is null-terminated
        memset(label, 0, 33);
        // Max label length is 32
        memcpy(label, first_resp->data, (first_resp->datalen > 32) ? 32 : first_resp->datalen);

        switch (first_resp->param_id)
        {
          case E120_DEVICE_MODEL_DESCRIPTION:
            HandleModelDescResponse(scope_handle, QString::fromUtf8(label), *first_resp);
            break;
          case E120_SOFTWARE_VERSION_LABEL:
            HandleSoftwareLabelResponse(scope_handle, QString::fromUtf8(label), *first_resp);
            break;
          case E120_MANUFACTURER_LABEL:
            HandleManufacturerLabelResponse(scope_handle, QString::fromUtf8(label), *first_resp);
            break;
          case E120_DEVICE_LABEL:
            HandleDeviceLabelResponse(scope_handle, QString::fromUtf8(label), *first_resp);
            break;
          case E120_BOOT_SOFTWARE_VERSION_LABEL:
            HandleBootSoftwareLabelResponse(scope_handle, QString::fromUtf8(label), *first_resp);
            break;
        }
        break;
      }
      case E120_BOOT_SOFTWARE_VERSION_ID:
      {
        if (first_resp->datalen >= 4)
          HandleBootSoftwareIdResponse(scope_handle, etcpal_unpack_u32b(first_resp->data), *first_resp);
        break;
      }
      case E120_DMX_PERSONALITY:
      {
        if (first_resp->datalen >= 2)
          HandlePersonalityResponse(scope_handle, first_resp->data[0], first_resp->data[1], *first_resp);
        break;
      }
      case E120_DMX_PERSONALITY_DESCRIPTION:
      {
        if (first_resp->datalen >= 3)
        {
          char description[33];
          uint8_t descriptionLength = first_resp->datalen - 3;

          memset(description, 0, 33);  // Ensure that the string is null-terminated
          memcpy(description, &first_resp->data[3],
                 (descriptionLength > 32) ? 32 : descriptionLength);  // Max description length is 32

          HandlePersonalityDescResponse(scope_handle, first_resp->data[0], etcpal_unpack_u16b(&first_resp->data[1]),
                                        QString::fromUtf8(description), *first_resp);
        }
        break;
      }
      case E137_7_ENDPOINT_LIST:
      {
        bool is_first_message = true;
        uint32_t change_number = 0;
        std::vector<std::pair<uint16_t, uint8_t>> list;
        RdmUid source_uid;

        for (const RdmResponse* response = resp.responses; response < resp.responses + resp.num_responses; ++response)
        {
          size_t pos = 0;
          if (is_first_message)
          {
            if (response->datalen < 4)
              break;
            source_uid = response->source_uid;
            change_number = etcpal_unpack_u32b(&response->data[0]);
            pos = 4;
            is_first_message = false;
          }

          for (; pos + 2 < response->datalen; pos += 3)
          {
            uint16_t endpoint_id = etcpal_unpack_u16b(&response->data[pos]);
            uint8_t endpoint_type = response->data[pos + 2];
            list.push_back(std::make_pair(endpoint_id, endpoint_type));
          }
        }

        HandleEndpointListResponse(scope_handle, change_number, list, source_uid);
      }
      break;
      case E137_7_ENDPOINT_RESPONDERS:
      {
        bool is_first_message = true;
        RdmUid source_uid;
        std::vector<RdmUid> list;
        uint16_t endpoint_id = 0;
        uint32_t change_number = 0;

        for (const RdmResponse* response = resp.responses; response < resp.responses + resp.num_responses; ++response)
        {
          size_t pos = 0;
          if (is_first_message)
          {
            if (response->datalen < 6)
              break;
            source_uid = response->source_uid;
            endpoint_id = etcpal_unpack_u16b(&response->data[0]);
            change_number = etcpal_unpack_u32b(&response->data[2]);
            pos = 6;
            is_first_message = false;
          }

          for (; pos + 5 < response->datalen; pos += 6)
          {
            RdmUid device;
            device.manu = etcpal_unpack_u16b(&response->data[pos]);
            device.id = etcpal_unpack_u32b(&response->data[pos + 2]);
            list.push_back(device);
          }
        }

        HandleEndpointRespondersResponse(scope_handle, endpoint_id, change_number, list, source_uid);
      }
      break;
      case E137_7_ENDPOINT_LIST_CHANGE:
      {
        if (first_resp->datalen >= 4)
        {
          uint32_t change_number = etcpal_unpack_u32b(first_resp->data);
          HandleEndpointListChangeResponse(scope_handle, change_number, first_resp->source_uid);
        }
        break;
      }
      case E137_7_ENDPOINT_RESPONDER_LIST_CHANGE:
      {
        if (first_resp->datalen >= 6)
        {
          uint16_t endpoint_id = etcpal_unpack_u16b(first_resp->data);
          uint32_t change_num = etcpal_unpack_u32b(&first_resp->data[2]);
          HandleResponderListChangeResponse(scope_handle, change_num, endpoint_id, first_resp->source_uid);
        }
        break;
      }
      case E133_TCP_COMMS_STATUS:
      {
        for (const RdmResponse* response = resp.responses; response < resp.responses + resp.num_responses; ++response)
        {
          char scopeString[E133_SCOPE_STRING_PADDED_LENGTH];
          memset(scopeString, 0, E133_SCOPE_STRING_PADDED_LENGTH);
          memcpy(scopeString, response->data, E133_SCOPE_STRING_PADDED_LENGTH - 1);

          QString v4AddrString =
              UnpackAndParseIPAddress(response->data + E133_SCOPE_STRING_PADDED_LENGTH, kEtcPalIpTypeV4);
          QString v6AddrString =
              UnpackAndParseIPAddress(response->data + E133_SCOPE_STRING_PADDED_LENGTH + 4, kEtcPalIpTypeV6);
          uint16_t port = etcpal_unpack_u16b(response->data + E133_SCOPE_STRING_PADDED_LENGTH + 4 + ETCPAL_IPV6_BYTES);
          uint16_t unhealthyTCPEvents =
              etcpal_unpack_u16b(response->data + E133_SCOPE_STRING_PADDED_LENGTH + 4 + ETCPAL_IPV6_BYTES + 2);

          HandleTcpCommsStatusResponse(scope_handle, QString::fromUtf8(scopeString), v4AddrString, v6AddrString, port,
                                       unhealthyTCPEvents, *first_resp);
        }

        break;
      }
      default:
      {
        // Process data for PIDs that support get and set, where the data has the same form in either case.
        ProcessRDMGetSetData(scope_handle, first_resp->param_id, first_resp->data, first_resp->datalen, *first_resp);
        break;
      }
    }
  }
  else if (first_resp->command_class == E120_SET_COMMAND_RESPONSE)
  {
    log_->Log(ETCPAL_LOG_INFO, "Got SET_COMMAND_RESPONSE with PID %d", first_resp->param_id);

    if (resp.command_included)
    {
      // Make sure this Controller is up-to-date with data that was set on a Device.
      switch (first_resp->param_id)
      {
        case E120_DMX_PERSONALITY:
        {
          if (resp.cmd.datalen >= 2)
            HandlePersonalityResponse(scope_handle, resp.cmd.data[0], 0, *first_resp);
          break;
        }
        default:
        {
          // Process PIDs with data that is in the same format for get and set.
          ProcessRDMGetSetData(scope_handle, first_resp->param_id, resp.cmd.data, resp.cmd.datalen, *first_resp);
          break;
        }
      }
    }
  }
}

void RDMnetNetworkModel::ProcessRDMGetSetData(rdmnet_client_scope_t scope_handle, uint16_t param_id,
                                              const uint8_t* data, uint8_t datalen, const RdmResponse& firstResp)
{
  if (data)
  {
    switch (param_id)
    {
      case E120_DEVICE_LABEL:
      {
        char label[33];

        // Ensure that the string is null-terminated
        memset(label, 0, 33);
        // Max label length is 32
        memcpy(label, data, (datalen > 32) ? 32 : datalen);

        HandleDeviceLabelResponse(scope_handle, QString::fromUtf8(label), firstResp);
        break;
      }
      case E120_DMX_START_ADDRESS:
      {
        if (datalen >= 2)
        {
          HandleStartAddressResponse(scope_handle, etcpal_unpack_u16b(data), firstResp);
        }
        break;
      }
      case E120_IDENTIFY_DEVICE:
      {
        if (datalen >= 1)
          HandleIdentifyResponse(scope_handle, data[0], firstResp);
        break;
      }
      case E133_COMPONENT_SCOPE:
      {
        uint16_t scopeSlot;
        char scopeString[E133_SCOPE_STRING_PADDED_LENGTH];
        QString staticConfigV4;
        QString staticConfigV6;
        uint16_t port = 0;
        const uint8_t* cur_ptr = data;

        scopeSlot = etcpal_unpack_u16b(cur_ptr);
        cur_ptr += 2;
        memcpy(scopeString, cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH);
        scopeString[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
        cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

        uint8_t staticConfigType = *cur_ptr++;
        switch (staticConfigType)
        {
          case E133_STATIC_CONFIG_IPV4:
            staticConfigV4 = UnpackAndParseIPAddress(cur_ptr, kEtcPalIpTypeV4);
            cur_ptr += 4 + 16;
            port = etcpal_unpack_u16b(cur_ptr);
            break;
          case E133_STATIC_CONFIG_IPV6:
            cur_ptr += 4;
            staticConfigV6 = UnpackAndParseIPAddress(cur_ptr, kEtcPalIpTypeV6);
            cur_ptr += 16;
            port = etcpal_unpack_u16b(cur_ptr);
            break;
          case E133_NO_STATIC_CONFIG:
          default:
            break;
        }
        HandleComponentScopeResponse(scope_handle, scopeSlot, QString::fromUtf8(scopeString), staticConfigV4,
                                     staticConfigV6, port, firstResp);
        break;
      }
      case E133_SEARCH_DOMAIN:
      {
        char domainString[E133_DOMAIN_STRING_PADDED_LENGTH];

        memset(domainString, 0, E133_DOMAIN_STRING_PADDED_LENGTH);
        memcpy(domainString, data, datalen);

        HandleSearchDomainResponse(scope_handle, QString::fromUtf8(domainString), firstResp);
        break;
      }
      default:
        break;
    }
  }
}

void RDMnetNetworkModel::HandleEndpointListResponse(rdmnet_client_scope_t scope_handle, uint32_t /*changeNumber*/,
                                                    const std::vector<std::pair<uint16_t, uint8_t>>& list,
                                                    const RdmUid& source_uid)
{
  if (broker_connections_.find(scope_handle) == broker_connections_.end())
  {
    log_->Log(ETCPAL_LOG_ERR, "Error: HandleEndpointListResponse called with invalid scope handle.");
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

void RDMnetNetworkModel::HandleEndpointRespondersResponse(rdmnet_client_scope_t scope_handle, uint16_t endpoint,
                                                          uint32_t /*changeNumber*/, const std::vector<RdmUid>& list,
                                                          const RdmUid& source_uid)
{
  if (broker_connections_.find(scope_handle) == broker_connections_.end())
  {
    log_->Log(ETCPAL_LOG_ERR, "Error: HandleEndpointRespondersResponse called with invalid scope handle.");
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

void RDMnetNetworkModel::HandleEndpointListChangeResponse(rdmnet_client_scope_t scope_handle, uint32_t /*changeNumber*/,
                                                          const RdmUid& source_uid)
{
  RdmCommand rdm;
  rdm.dest_uid = source_uid;
  rdm.subdevice = 0;
  rdm.command_class = kRdmCCGetCommand;
  rdm.param_id = E137_7_ENDPOINT_LIST;
  rdm.datalen = 0;

  RdmnetLocalRdmCommand cmd;
  cmd.dest_uid = source_uid;
  cmd.dest_endpoint = E133_NULL_ENDPOINT;
  cmd.rdm = rdm;

  rdmnet_->SendRdmCommand(scope_handle, cmd);
}

void RDMnetNetworkModel::HandleResponderListChangeResponse(rdmnet_client_scope_t scope_handle,
                                                           uint32_t /*changeNumber*/, uint16_t endpoint,
                                                           const RdmUid& source_uid)
{
  // Ask for the devices on each endpoint
  RdmCommand rdm;
  rdm.dest_uid = source_uid;
  rdm.subdevice = 0;
  rdm.command_class = kRdmCCGetCommand;
  rdm.param_id = E137_7_ENDPOINT_RESPONDERS;
  rdm.datalen = sizeof(uint16_t);
  etcpal_pack_u16b(rdm.data, endpoint);

  RdmnetLocalRdmCommand cmd;
  cmd.dest_uid = source_uid;
  cmd.dest_endpoint = E133_NULL_ENDPOINT;
  cmd.rdm = rdm;

  rdmnet_->SendRdmCommand(scope_handle, cmd);
}

void RDMnetNetworkModel::HandleRDMNack(rdmnet_client_scope_t scope_handle, uint16_t reason, const RdmResponse& resp)
{
  if ((resp.command_class == E120_SET_COMMAND_RESPONSE) && PropertyValueItem::pidInfoExists(resp.param_id))
  {
    // Attempt to set a property failed. Get the original property value back.
    RdmCommand cmd;

    memset(cmd.data, 0, RDM_MAX_PDL);
    cmd.dest_uid.manu = resp.source_uid.manu;
    cmd.dest_uid.id = resp.source_uid.id;
    cmd.subdevice = 0;

    cmd.command_class = kRdmCCGetCommand;
    cmd.param_id = resp.param_id;

    if (cmd.param_id == E133_COMPONENT_SCOPE)
    {
      cmd.datalen = 2;
      etcpal_pack_u16b(cmd.data, 0x0001);  // Scope slot, default to 1 for RPT Devices (non-controllers, non-brokers).
    }
    else
    {
      cmd.datalen = 0;
    }

    SendRDMCommand(cmd, scope_handle);
  }
  else if ((resp.command_class == kRdmCCGetCommandResponse) && (resp.param_id == E133_COMPONENT_SCOPE) &&
           (reason == E120_NR_DATA_OUT_OF_RANGE))
  {
    RDMnetClientItem* client = GetClientItem(scope_handle, resp);
    RDMnetNetworkItem* rdmNetGroup = dynamic_cast<RDMnetNetworkItem*>(
        client->child(0)->data() == tr("RDMnet") ? client->child(0) : client->child(1));

    removeScopeSlotItemsInRange(rdmNetGroup, &client->properties, previous_slot_[resp.source_uid] + 1, 0xFFFF);

    // We have all of this controller's scope-slot pairs. Now request scope-specific properties.
    previous_slot_[resp.source_uid] = 0;
    sendGetControllerScopeProperties(scope_handle, resp.source_uid.manu, resp.source_uid.id);
  }
}

void RDMnetNetworkModel::HandleStatusMessagesResponse(uint8_t /*type*/, uint16_t /*messageId*/, uint16_t /*data1*/,
                                                      uint16_t /*data2*/, const RdmResponse& /*resp*/)
{
}

void RDMnetNetworkModel::HandleSupportedParametersResponse(rdmnet_client_scope_t scope_handle,
                                                           const std::vector<uint16_t>& params_list,
                                                           const RdmResponse& resp)
{
  if (params_list.size() > 0)
  {
    // Get any properties that are supported
    RdmCommand getCmd;

    getCmd.dest_uid = resp.source_uid;
    getCmd.subdevice = 0;

    getCmd.command_class = kRdmCCGetCommand;
    getCmd.datalen = 0;

    for (const uint16_t& param : params_list)
    {
      if (pidSupportedByGUI(param, true) && param != E120_SUPPORTED_PARAMETERS)
      {
        getCmd.param_id = param;
        SendRDMCommand(getCmd, scope_handle);
      }
      else if (param == E120_RESET_DEVICE)
      {
        RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

        if (device)
        {
          device->enableFeature(kResetDevice);
          emit featureSupportChanged(device, kResetDevice);
        }
      }
    }
  }
}

void RDMnetNetworkModel::HandleDeviceInfoResponse(rdmnet_client_scope_t scope_handle, uint16_t protocolVersion,
                                                  uint16_t modelId, uint16_t category, uint32_t swVersionId,
                                                  uint16_t footprint, uint8_t personality, uint8_t totalPersonality,
                                                  uint16_t address, uint16_t subdeviceCount, uint8_t sensorCount,
                                                  const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 0),
                         protocolVersion);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 1),
                         modelId);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 2),
                         category);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 3),
                         swVersionId);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 4),
                         footprint);
    HandlePersonalityResponse(scope_handle, personality, totalPersonality, resp);
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), address);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 5),
                         subdeviceCount);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 6),
                         (uint16_t)sensorCount);
  }
}

void RDMnetNetworkModel::HandleModelDescResponse(rdmnet_client_scope_t scope_handle, const QString& label,
                                                 const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_DEVICE_MODEL_DESCRIPTION,
                         PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_MODEL_DESCRIPTION), label);
  }
}

void RDMnetNetworkModel::HandleManufacturerLabelResponse(rdmnet_client_scope_t scope_handle, const QString& label,
                                                         const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_MANUFACTURER_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_MANUFACTURER_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleDeviceLabelResponse(rdmnet_client_scope_t scope_handle, const QString& label,
                                                   const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_DEVICE_LABEL, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_LABEL),
                         label);
  }
}

void RDMnetNetworkModel::HandleSoftwareLabelResponse(rdmnet_client_scope_t scope_handle, const QString& label,
                                                     const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_SOFTWARE_VERSION_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleBootSoftwareIdResponse(rdmnet_client_scope_t scope_handle, uint32_t id,
                                                      const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_ID,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_ID), id);
  }
}

void RDMnetNetworkModel::HandleBootSoftwareLabelResponse(rdmnet_client_scope_t scope_handle, const QString& label,
                                                         const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleStartAddressResponse(rdmnet_client_scope_t scope_handle, uint16_t address,
                                                    const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), address);
  }
}

void RDMnetNetworkModel::HandleIdentifyResponse(rdmnet_client_scope_t scope_handle, bool identifying,
                                                const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    device->setDeviceIdentifying(identifying);
    emit identifyChanged(device, identifying);
  }
}

void RDMnetNetworkModel::HandlePersonalityResponse(rdmnet_client_scope_t scope_handle, uint8_t current, uint8_t number,
                                                   const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);

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

      sendGetCommand(getNearestParentItemOfType<BrokerItem>(device), E120_DEVICE_INFO, resp.source_uid);
    }

    checkPersonalityDescriptions(device, number, resp);
  }
}

void RDMnetNetworkModel::HandlePersonalityDescResponse(rdmnet_client_scope_t scope_handle, uint8_t personality,
                                                       uint16_t footprint, const QString& description,
                                                       const RdmResponse& resp)
{
  RDMnetNetworkItem* device = GetNetworkItem(scope_handle, resp);
  const bool SHOW_FOOTPRINT = false;

  if (device)
  {
    device->personalityDescriptionFound(
        personality, footprint,
        SHOW_FOOTPRINT ? QString("(FP=%1) %2").arg(QString::number(footprint).rightJustified(2, '0'), description)
                       : description);

    if (device->allPersonalityDescriptionsFound())
    {
      QStringList personalityDescriptions = device->personalityDescriptionList();
      uint8_t currentPersonality = static_cast<uint8_t>(
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

void RDMnetNetworkModel::HandleComponentScopeResponse(rdmnet_client_scope_t scope_handle, uint16_t scopeSlot,
                                                      const QString& scopeString, const QString& staticConfigV4,
                                                      const QString& staticConfigV6, uint16_t port,
                                                      const RdmResponse& resp)
{
  RDMnetClientItem* client = GetClientItem(scope_handle, resp);

  if (client)
  {
    RDMnetNetworkItem* rdmNetGroup = dynamic_cast<RDMnetNetworkItem*>(
        client->child(0)->data() == tr("RDMnet") ? client->child(0) : client->child(1));

    if (client->ClientType() == kRPTClientTypeController)
    {
      removeScopeSlotItemsInRange(rdmNetGroup, &client->properties, previous_slot_[client->uid()] + 1, scopeSlot - 1);
    }

    QString displayName;
    if (client->ClientType() == kRPTClientTypeController)
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

    if (client->ClientType() == kRPTClientTypeController)
    {
      previous_slot_[client->uid()] = scopeSlot;
      sendGetNextControllerScope(scope_handle, resp.source_uid.manu, resp.source_uid.id, scopeSlot);
    }
  }
}

void RDMnetNetworkModel::HandleSearchDomainResponse(rdmnet_client_scope_t scope_handle, const QString& domainNameString,
                                                    const RdmResponse& resp)
{
  RDMnetClientItem* client = GetClientItem(scope_handle, resp);
  if (client)
  {
    emit setPropertyData(client, E133_SEARCH_DOMAIN, PropertyValueItem::pidPropertyDisplayName(E133_SEARCH_DOMAIN, 0),
                         domainNameString);
  }
}

void RDMnetNetworkModel::HandleTcpCommsStatusResponse(rdmnet_client_scope_t scope_handle, const QString& scopeString,
                                                      const QString& v4AddrString, const QString& v6AddrString,
                                                      uint16_t port, uint16_t unhealthyTCPEvents,
                                                      const RdmResponse& resp)
{
  RDMnetClientItem* client = GetClientItem(scope_handle, resp);

  if (client)
  {
    if (client->getScopeSlot(scopeString) != 0)
    {
      QVariant callbackObjectVariant;
      const char* callbackSlotString = SLOT(processPropertyButtonClick(const QPersistentModelIndex&));
      QString callbackSlotQString(callbackSlotString);

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

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, resp.source_uid.manu,
                           RDMnetNetworkItem::ClientManuRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, resp.source_uid.id,
                           RDMnetNetworkItem::ClientDevRole);

      // This needs to be the last call to setPropertyData so that the button can be enabled if needed.
      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, EditorWidgetType::kButton,
                           RDMnetNetworkItem::EditorWidgetTypeRole);
    }
  }
}

void RDMnetNetworkModel::addPropertyEntries(RDMnetNetworkItem* parent, PIDFlags location)
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
        emit addPropertyEntry(parent, i->first, j, i->second.role);
      }
    }
  }
}

void RDMnetNetworkModel::initializeResponderProperties(ResponderItem* parent, uint16_t manuID, uint32_t deviceID)
{
  RdmCommand cmd;
  BrokerItem* broker_item = getNearestParentItemOfType<BrokerItem>(parent);

  addPropertyEntries(parent, kLocResponder);

  // Now send requests for core required properties.
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = kRdmCCGetCommand;
  cmd.datalen = 0;

  cmd.param_id = E120_SUPPORTED_PARAMETERS;
  SendRDMCommand(cmd, broker_item);
  cmd.param_id = E120_DEVICE_INFO;
  SendRDMCommand(cmd, broker_item);
  cmd.param_id = E120_SOFTWARE_VERSION_LABEL;
  SendRDMCommand(cmd, broker_item);
  cmd.param_id = E120_DMX_START_ADDRESS;
  SendRDMCommand(cmd, broker_item);
  cmd.param_id = E120_IDENTIFY_DEVICE;
  SendRDMCommand(cmd, broker_item);
}

void RDMnetNetworkModel::initializeRPTClientProperties(RDMnetClientItem* parent, uint16_t manuID, uint32_t deviceID,
                                                       rpt_client_type_t clientType)
{
  RdmCommand cmd;
  BrokerItem* broker_item = getNearestParentItemOfType<BrokerItem>(parent);

  addPropertyEntries(parent, (clientType == kRPTClientTypeDevice) ? kLocDevice : kLocController);

  // Now send requests for core required properties.
  memset(cmd.data, 0, RDM_MAX_PDL);
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = kRdmCCGetCommand;
  cmd.datalen = 0;

  cmd.param_id = E120_SUPPORTED_PARAMETERS;
  SendRDMCommand(cmd, broker_item);
  cmd.param_id = E120_DEVICE_INFO;
  SendRDMCommand(cmd, broker_item);
  cmd.param_id = E120_SOFTWARE_VERSION_LABEL;
  SendRDMCommand(cmd, broker_item);
  cmd.param_id = E120_DMX_START_ADDRESS;
  SendRDMCommand(cmd, broker_item);
  cmd.param_id = E120_IDENTIFY_DEVICE;
  SendRDMCommand(cmd, broker_item);

  cmd.param_id = E133_SEARCH_DOMAIN;
  SendRDMCommand(cmd, broker_item);

  if (clientType == kRPTClientTypeDevice)  // For controllers, we need to wait for all the scopes first.
  {
    cmd.param_id = E133_TCP_COMMS_STATUS;
    SendRDMCommand(cmd, broker_item);
  }

  cmd.datalen = 2;
  etcpal_pack_u16b(cmd.data, 0x0001);  // Scope slot, start with #1
  cmd.param_id = E133_COMPONENT_SCOPE;
  SendRDMCommand(cmd, broker_item);
}

void RDMnetNetworkModel::sendGetControllerScopeProperties(rdmnet_client_scope_t scope_handle, uint16_t manuID,
                                                          uint32_t deviceID)
{
  RdmCommand cmd;

  memset(cmd.data, 0, RDM_MAX_PDL);
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = kRdmCCGetCommand;
  cmd.datalen = 0;

  cmd.param_id = E133_TCP_COMMS_STATUS;
  SendRDMCommand(cmd, scope_handle);
}

void RDMnetNetworkModel::sendGetNextControllerScope(rdmnet_client_scope_t scope_handle, uint16_t manuID,
                                                    uint32_t deviceID, uint16_t currentSlot)
{
  RdmCommand cmd;

  memset(cmd.data, 0, RDM_MAX_PDL);
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = kRdmCCGetCommand;
  cmd.datalen = 2;

  etcpal_pack_u16b(cmd.data, std::min<uint16_t>(currentSlot + 1, 0xffff));  // Scope slot, start with #1
  cmd.param_id = E133_COMPONENT_SCOPE;
  SendRDMCommand(cmd, scope_handle);
}

void RDMnetNetworkModel::sendGetCommand(BrokerItem* broker_item, uint16_t pid, const RdmUid& dest_uid)
{
  RdmCommand getCmd;

  getCmd.dest_uid = dest_uid;
  getCmd.subdevice = 0;

  getCmd.command_class = kRdmCCGetCommand;
  getCmd.param_id = pid;
  getCmd.datalen = 0;
  SendRDMCommand(getCmd, broker_item);
}

uint8_t* RDMnetNetworkModel::packIPAddressItem(const QVariant& value, etcpal_iptype_t addrType, uint8_t* packPtr,
                                               bool packPort)
{
  char ipStrBuffer[64];
  unsigned int portNumber;
  size_t memSize = ((addrType == kEtcPalIpTypeV4) ? 4 : (ETCPAL_IPV6_BYTES)) + (packPort ? 2 : 0);

  if (!packPtr)
  {
    return nullptr;
  }

  QString valueQString = value.toString();
  QByteArray local8Bit = valueQString.toLocal8Bit();
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

bool RDMnetNetworkModel::pidSupportedByGUI(uint16_t pid, bool checkSupportGet)
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

RDMnetClientItem* RDMnetNetworkModel::GetClientItem(rdmnet_client_scope_t scope_handle, const RdmResponse& resp)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  if (broker_connections_.find(scope_handle) == broker_connections_.end())
  {
    log_->Log(ETCPAL_LOG_ERR, "Error: getClientItem called with invalid scope handle.");
  }
  else
  {
    BrokerItem* broker_item = broker_connections_[scope_handle];
    if (broker_item)
    {
      for (auto client : broker_item->rdmnet_clients_)
      {
        if (client->uid() == resp.source_uid)
        {
          return client;
        }
      }
    }
  }

  return nullptr;
}

RDMnetNetworkItem* RDMnetNetworkModel::GetNetworkItem(rdmnet_client_scope_t scope_handle, const RdmResponse& resp)
{
  etcpal::ReadGuard conn_read(conn_lock_);

  if (broker_connections_.find(scope_handle) == broker_connections_.end())
  {
    log_->Log(ETCPAL_LOG_ERR, "Error: getNetworkItem called with invalid connection cookie.");
  }
  else
  {
    BrokerItem* broker_item = broker_connections_[scope_handle];
    if (broker_item)
    {
      for (auto client : broker_item->rdmnet_clients_)
      {
        if (client->uid() == resp.source_uid)
        {
          return client;
        }

        for (auto endpoint : client->endpoints_)
        {
          for (auto responder : endpoint->responders_)
          {
            if (responder->uid() == resp.source_uid)
            {
              return responder;
            }
          }
        }
      }
    }
  }

  return nullptr;
}

void RDMnetNetworkModel::checkPersonalityDescriptions(RDMnetNetworkItem* device, uint8_t numberOfPersonalities,
                                                      const RdmResponse& resp)
{
  if (numberOfPersonalities > 0)
  {
    if (device->initiatePersonalityDescriptionSearch(numberOfPersonalities))
    {
      // Get descriptions for all supported personalities of this device
      RdmCommand getCmd;

      getCmd.dest_uid.manu = resp.source_uid.manu;
      getCmd.dest_uid.id = resp.source_uid.id;
      getCmd.subdevice = 0;
      getCmd.command_class = kRdmCCGetCommand;
      getCmd.param_id = E120_DMX_PERSONALITY_DESCRIPTION;
      getCmd.datalen = 1;
      for (uint8_t i = 1; i <= numberOfPersonalities; ++i)
      {
        getCmd.data[0] = i;
        SendRDMCommand(getCmd, getNearestParentItemOfType<BrokerItem>(device));
      }
    }
  }
}

QVariant RDMnetNetworkModel::getPropertyData(RDMnetNetworkItem* parent, unsigned short pid, int role)
{
  QVariant result = QVariant();
  bool foundProperty = false;

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
  QString currentPathName = fullName;
  QString shortName = getShortPropertyName(fullName);
  PropertyItem* propertyItem = new PropertyItem(fullName, shortName);

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

  appendRowToItem(currentParent, propertyItem);

  return propertyItem;
}

QString RDMnetNetworkModel::getShortPropertyName(const QString& fullPropertyName)
{
  QRegExp re("(\\\\)");
  QStringList query = fullPropertyName.split(re);

  if (query.length() > 0)
  {
    return query.at(query.length() - 1);
  }

  return QString();
}

QString RDMnetNetworkModel::getHighestGroupName(const QString& pathName)
{
  QRegExp re("(\\\\)");
  QStringList query = pathName.split(re);

  if (query.length() > 0)
  {
    return query.at(0);
  }

  return QString();
}

QString RDMnetNetworkModel::getPathSubset(const QString& fullPath, int first, int last)
{
  QRegExp re("(\\\\)");
  QStringList query = fullPath.split(re);
  QString result;

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

  appendRowToItem(parent, groupingItem);
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
  int startPosition = highGroupName.length() + 1;  // Name + delimiter character

  return superPathName.mid(startPosition, superPathName.length() - startPosition);
}

QString RDMnetNetworkModel::getScopeSubPropertyFullName(RDMnetClientItem* client, uint16_t pid, int32_t index,
                                                        const QString& scope)
{
  QString original = PropertyValueItem::pidPropertyDisplayName(pid, index);

  if (client)
  {
    if (client->ClientType() == kRPTClientTypeController)
    {
      QString scopePropertyDisplay = PropertyValueItem::pidPropertyDisplayName(E133_COMPONENT_SCOPE, 0);
      QRegExp re("(\\\\)");
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

void RDMnetNetworkModel::removeScopeSlotItemsInRange(RDMnetNetworkItem* parent, std::vector<PropertyItem*>* properties,
                                                     uint16_t firstSlot, uint16_t lastSlot)
{
  if (lastSlot >= firstSlot)
  {
    emit removePropertiesInRange(parent, properties, E133_COMPONENT_SCOPE, RDMnetNetworkItem::ScopeSlotRole, firstSlot,
                                 lastSlot);
  }
}
