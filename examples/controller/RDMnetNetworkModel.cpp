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

#include "RDMnetNetworkModel.h"

#include <cassert>
#include <cstdarg>
#include "lwpa/pack.h"
#include "lwpa/socket.h"
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
static QString UnpackAndParseIPAddress(const uint8_t *addrData, lwpa_iptype_t addrType)
{
  char ip_str_buf[LWPA_INET6_ADDRSTRLEN];
  LwpaIpAddr ip;
  bool zeroedOut = false;

  if (addrType == kLwpaIpTypeV4)
  {
    lwpaip_set_v4_address(&ip, lwpa_upack_32b(addrData));
    zeroedOut = (ip.addr.v4 == 0);
  }
  else if (addrType == kLwpaIpTypeV6)
  {
    lwpaip_set_v6_address(&ip, addrData);

    zeroedOut = true;
    for (int i = 0; (i < LWPA_IPV6_BYTES) && zeroedOut; ++i)
    {
      zeroedOut = zeroedOut && (ip.addr.v6[i] == 0);
    }
  }

  if (!zeroedOut)
  {
    lwpa_inet_ntop(&ip, ip_str_buf, LWPA_INET6_ADDRSTRLEN);
    return QString::fromUtf8(ip_str_buf);
  }
  else
  {
    return QString();
  }
}

static lwpa_error_t ParseAndPackIPAddress(lwpa_iptype_t addrType, const std::string &ipString, uint8_t *outBuf)
{
  LwpaIpAddr ip;

  lwpa_error_t result = lwpa_inet_pton(addrType, ipString.c_str(), &ip);
  if (result == kLwpaErrOk)
  {
    if (addrType == kLwpaIpTypeV4)
    {
      lwpa_pack_32b(outBuf, ip.addr.v4);
    }
    else if (addrType == kLwpaIpTypeV6)
    {
      memcpy(outBuf, ip.addr.v6, LWPA_IPV6_BYTES);
    }
  }

  return result;
}

void appendRowToItem(QStandardItem *parent, QStandardItem *child)
{
  if ((parent != nullptr) && (child != nullptr))
  {
    parent->appendRow(child);

    if (child->columnCount() != 2)
    {
      child->setColumnCount(2);
    }
  }
}

template <typename T>
T *getNearestParentItemOfType(QStandardItem *child)
{
  T *parent = nullptr;
  QStandardItem *current = child;

  while ((parent == nullptr) && (current != nullptr))
  {
    current = current->parent();

    if (current != nullptr)
    {
      parent = dynamic_cast<T *>(current);
    }
  }

  return parent;
}

void RDMnetNetworkModel::addScopeToMonitor(QString scope)
{
  static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
  size_t num_responses;
  int platform_error;
  bool scopeAlreadyAdded = false;

  if (scope.length() > 0)
  {
    ControllerWriteGuard conn_write(conn_lock_);

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
        BrokerItem *broker = new BrokerItem(scope, new_scope_handle);
        appendRowToItem(invisibleRootItem(), broker);
        broker->enableChildrenSearch();

        emit expandNewItem(broker->index(), BrokerItem::BrokerItemType);

        broker_connections_.insert(std::make_pair(new_scope_handle, broker));
      }
    }

    if (!scopeAlreadyAdded)  // Scope must have been added
    {
      // Broadcast GET_RESPONSE notification because of newly added scope
      if (getComponentScope(0x0001, resp_data_list, &num_responses))
      {
        SendRDMGetResponses(kRdmnetControllerBroadcastUid, E133_BROADCAST_ENDPOINT, E133_COMPONENT_SCOPE,
                            resp_data_list, num_responses, 0, 0);
      }
    }
  }
}

void RDMnetNetworkModel::directChildrenRevealed(const QModelIndex &parentIndex)
{
  QStandardItem *item = itemFromIndex(parentIndex);

  if (item != nullptr)
  {
    for (int i = 0; i < item->rowCount(); ++i)
    {
      QStandardItem *child = item->child(i);

      if (child != nullptr)
      {
        if (child->type() == SearchingStatusItem::SearchingStatusItemType)
        {
          searchingItemRevealed(dynamic_cast<SearchingStatusItem *>(child));
        }
      }
    }
  }
}

void RDMnetNetworkModel::addBrokerByIP(QString scope, const LwpaSockaddr &addr)
{
  static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
  size_t num_responses;
  bool brokerAlreadyAdded = false;

  ControllerWriteGuard conn_write(conn_lock_);
  for (auto iter = broker_connections_.cbegin(); (iter != broker_connections_.cend()) && !brokerAlreadyAdded; ++iter)
  {
    if (iter->second->scope() == scope)
    {
      brokerAlreadyAdded = true;
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
    auto connection = std::make_unique<BrokerConnection>(scope, addr);

    broker_connections_[broker_create_count_] = std::move(connection);
    broker_connections_[broker_create_count_]->appendBrokerItemToTree(invisibleRootItem(), broker_create_count_);
    broker_connections_[broker_create_count_]->connect();

    emit expandNewItem(broker_connections_[broker_create_count_]->treeBrokerItem()->index(),
                       BrokerItem::BrokerItemType);

    ++broker_create_count_;
  }

  if (!brokerAlreadyAdded)  // Broker must have been added
  {
    // Broadcast GET_RESPONSE notification because of newly added scope
    if (getComponentScope(0x0001, resp_data_list, &num_responses))
    {
      SendRDMGetResponses(kRdmnetControllerBroadcastUid, E133_BROADCAST_ENDPOINT, E133_COMPONENT_SCOPE, resp_data_list,
                          num_responses, 0, 0);
    }
  }
}

void RDMnetNetworkModel::addCustomLogOutputStream(LogOutputStream *stream)
{
  log_->addCustomOutputStream(stream);
}

void RDMnetNetworkModel::removeCustomLogOutputStream(LogOutputStream *stream)
{
  log_->removeCustomOutputStream(stream);
}

void RDMnetNetworkModel::Connected(rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo &info)
{
  if (broker_connections_.find(scope_handle) != broker_connections_.end())
  {
    static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
    size_t num_responses;
    uint16_t nack_reason;

    // Broadcast GET_RESPONSE notification because of new connection
    if (getTCPCommsStatus(nullptr, 0, resp_data_list, &num_responses, &nack_reason))
    {
      SendRDMGetResponses(kRdmnetControllerBroadcastUid, E133_BROADCAST_ENDPOINT, E133_TCP_COMMS_STATUS, resp_data_list,
                          num_responses, 0, 0);
    }
  }
}

void RDMnetNetworkModel::NotConnected(rdmnet_client_scope_t scope_handle, const RdmnetClientNotConnectedInfo &info)
{
  if (lwpa_rwlock_writelock(&conn_lock, LWPA_WAIT_FOREVER))
  {
    BrokerConnection *connection = broker_connections_[scope_handle].get();
    if (connection)
    {
      if (connection->connected())
      {
        connection->();

        if (connection->treeBrokerItem() != nullptr)
        {
          emit brokerItemTextUpdated(connection->treeBrokerItem());
        }

        connection->treeBrokerItem()->rdmnet_clients_.clear();
        connection->treeBrokerItem()->completelyRemoveChildren(0, connection->treeBrokerItem()->rowCount());
        connection->treeBrokerItem()->enableChildrenSearch();

        static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
        size_t num_responses;
        uint16_t nack_reason;

        // Broadcast GET_RESPONSE notification because of lost connection
        if (getTCPCommsStatus(nullptr, 0, resp_data_list, &num_responses, &nack_reason))
        {
          SendRDMGetResponses(kRdmnetControllerBroadcastUid, E133_BROADCAST_ENDPOINT, E133_TCP_COMMS_STATUS,
                              resp_data_list, num_responses, 0, 0);
        }
      }
    }

    lwpa_rwlock_writeunlock(&conn_lock);
  }
}

void RDMnetNetworkModel::processAddRDMnetClients(BrokerConnection *brokerConn, const std::vector<ClientEntryData> &list)
{
  BrokerItem *treeBrokerItem = brokerConn->treeBrokerItem();

  // Update the Controller's discovered list to match
  if (list.size() > 0)
  {
    treeBrokerItem->disableChildrenSearch();
  }

  for (const auto entry : list)
  {
    if (!is_rpt_client_entry(&entry))
      continue;

    bool is_me = (get_rpt_client_entry_data(&entry)->client_uid == brokerConn->getLocalUID());
    RDMnetClientItem *newRDMnetClientItem = new RDMnetClientItem(entry, is_me);
    bool itemAlreadyAdded = false;

    for (auto j = treeBrokerItem->rdmnet_clients_.begin();
         (j != treeBrokerItem->rdmnet_clients_.end()) && !itemAlreadyAdded; ++j)
    {
      if ((*j) != nullptr)
      {
        if ((*newRDMnetClientItem) == (*(*j)))
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
      appendRowToItem(treeBrokerItem, newRDMnetClientItem);
      treeBrokerItem->rdmnet_clients_.push_back(newRDMnetClientItem);

      if (get_rpt_client_entry_data(&entry)->client_type != kRPTClientTypeUnknown)
      {
        initializeRPTClientProperties(newRDMnetClientItem, get_rpt_client_entry_data(&entry)->client_uid.manu,
                                      get_rpt_client_entry_data(&entry)->client_uid.id,
                                      get_rpt_client_entry_data(&entry)->client_type);

        newRDMnetClientItem->enableFeature(kIdentifyDevice);
        emit featureSupportChanged(newRDMnetClientItem, kIdentifyDevice);
      }

      newRDMnetClientItem->enableChildrenSearch();
    }
  }
}

void RDMnetNetworkModel::processRemoveRDMnetClients(BrokerConnection *brokerConn,
                                                    const std::vector<ClientEntryData> &list)
{
  BrokerItem *treeBrokerItem = brokerConn->treeBrokerItem();
  // Update the Controller's discovered list by removing these newly lost
  // clients
  for (int i = treeBrokerItem->rowCount() - 1; i >= 0; --i)
  {
    RDMnetClientItem *clientItem = dynamic_cast<RDMnetClientItem *>(treeBrokerItem->child(i));

    if (clientItem)
    {
      for (auto j = list.begin(); j != list.end(); ++j)
      {
        const ClientEntryDataRpt *rpt_entry = get_rpt_client_entry_data(&(*j));
        if (rpt_entry->client_type == clientItem->ClientType() && rpt_entry->client_uid == clientItem->Uid())
        {
          // Found the match
          treeBrokerItem->rdmnet_clients_.erase(
              std::remove(treeBrokerItem->rdmnet_clients_.begin(), treeBrokerItem->rdmnet_clients_.end(), clientItem),
              treeBrokerItem->rdmnet_clients_.end());
          treeBrokerItem->completelyRemoveChildren(i);
          break;
        }
      }
    }
  }

  if (treeBrokerItem->rowCount() == 0)
  {
    treeBrokerItem->enableChildrenSearch();
  }
}

void RDMnetNetworkModel::processNewEndpointList(RDMnetClientItem *treeClientItem,
                                                const std::vector<std::pair<uint16_t, uint8_t>> &list)
{
  if (treeClientItem->childrenSearchRunning() && (list.size() > 1))
  {
    treeClientItem->disableChildrenSearch();
  }

  std::vector<EndpointItem *> prev_list = treeClientItem->endpoints_;
  // Slight hack to avoid removing the NULL_ENDPOINT.
  if (!prev_list.empty())
  {
    prev_list.erase(prev_list.begin());
  }

  // Save these endpoints here
  for (auto endpoint_id : list)
  {
    if (endpoint_id.first != 0)
    {
      EndpointItem *newEndpointItem =
          new EndpointItem(treeClientItem->Uid().manu, treeClientItem->Uid().id, endpoint_id.first, endpoint_id.second);
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
    EndpointItem *endpointItem = dynamic_cast<EndpointItem *>(treeClientItem->child(i));

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

void RDMnetNetworkModel::processNewResponderList(EndpointItem *treeEndpointItem, const std::vector<RdmUid> &list)
{
  bool somethingWasAdded = false;

  std::vector<ResponderItem *> prev_list = treeEndpointItem->devices_;

  // Save these devices
  for (auto resp_uid : list)
  {
    ResponderItem *newResponderItem = new ResponderItem(resp_uid.manu, resp_uid.id);
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
      treeEndpointItem->devices_.push_back(newResponderItem);
      somethingWasAdded = true;

      initializeResponderProperties(newResponderItem, resp_uid.manu, resp_uid.id);

      newResponderItem->enableFeature(kIdentifyDevice);
      emit featureSupportChanged(newResponderItem, kIdentifyDevice);
    }
  }

  // Now remove the ones that aren't there anymore
  for (int i = treeEndpointItem->rowCount() - 1; i >= 0; --i)
  {
    ResponderItem *responderItem = dynamic_cast<ResponderItem *>(treeEndpointItem->child(i));

    if (responderItem)
    {
      for (auto removed_responder : prev_list)
      {
        if (*removed_responder == *responderItem)
        {
          // Found the match
          // responderItem->properties.clear();
          // responderItem->removeRows(0, responderItem->rowCount());

          treeEndpointItem->devices_.erase(
              std::remove(treeEndpointItem->devices_.begin(), treeEndpointItem->devices_.end(), responderItem),
              treeEndpointItem->devices_.end());
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

void RDMnetNetworkModel::processSetPropertyData(RDMnetNetworkItem *parent, unsigned short pid, const QString &name,
                                                const QVariant &value, int role)
{
  bool enable = value.isValid() || PropertyValueItem::pidStartEnabled(pid);
  bool overrideEnableSet = (role == RDMnetNetworkItem::EditorWidgetTypeRole) &&
                           (static_cast<EditorWidgetType>(value.toInt()) == kButton) &&
                           (PropertyValueItem::pidFlags(pid) & kEnableButtons);

  if (parent != nullptr)
  {
    if (parent->isEnabled())
    {
      // Check if this property already exists before adding it. If it exists
      // already, then update the existing property.
      for (auto item : parent->properties)
      {
        if (item->getValueItem() != nullptr)
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
      PropertyItem *propertyItem = createPropertyItem(parent, name);
      PropertyValueItem *propertyValueItem = new PropertyValueItem(value, role);

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

void RDMnetNetworkModel::processRemovePropertiesInRange(RDMnetNetworkItem *parent,
                                                        std::vector<PropertyItem *> *properties, unsigned short pid,
                                                        int role, const QVariant &min, const QVariant &max)
{
  if (parent != nullptr)
  {
    if (parent->isEnabled())
    {
      for (int i = parent->rowCount() - 1; i >= 0; --i)
      {
        PropertyItem *child = dynamic_cast<PropertyItem *>(parent->child(i, 0));
        PropertyValueItem *sibling = dynamic_cast<PropertyValueItem *>(parent->child(i, 1));

        if ((child != nullptr) && (sibling != nullptr))
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

void RDMnetNetworkModel::processAddPropertyEntry(RDMnetNetworkItem *parent, unsigned short pid, const QString &name,
                                                 int role)
{
  processSetPropertyData(parent, pid, name, QVariant(), role);
}

void RDMnetNetworkModel::processPropertyButtonClick(const QPersistentModelIndex &propertyIndex)
{
  // Assuming this is SET TCP_COMMS_STATUS for now.
  if (propertyIndex.isValid())
  {
    QString scope = propertyIndex.data(RDMnetNetworkItem::ScopeDataRole).toString();
    QByteArray local8Bit = scope.toLocal8Bit();
    const char *scopeData = local8Bit.constData();

    RdmCommand setCmd;
    uint8_t maxBuffSize = PropertyValueItem::pidMaxBufferSize(E133_TCP_COMMS_STATUS);
    QVariant manuVariant = propertyIndex.data(RDMnetNetworkItem::ClientManuRole);
    QVariant devVariant = propertyIndex.data(RDMnetNetworkItem::ClientDevRole);

    BrokerConnection *conn = getBrokerConnection(scopeData);

    if (conn == nullptr)
    {
      log_->Log(LWPA_LOG_ERR, "Error: Cannot find broker connection for clicked button.");
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
      memcpy(setCmd.data, scopeData, min(scope.length(), maxBuffSize));

      SendRDMCommand(setCmd, conn->handle());
    }
  }
  else
  {
    log_->Log(LWPA_LOG_ERR, "Error: Button clicked on invalid property.");
  }
}

void RDMnetNetworkModel::removeBroker(BrokerItem *brokerItem)
{
  static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
  size_t num_responses;

  uint32_t connectionCookie = brokerItem->getConnectionCookie();
  bool removeComplete = false;

  if (lwpa_rwlock_writelock(&conn_lock, LWPA_WAIT_FOREVER))
  {
    BrokerConnection *brokerConnection = broker_connections_[connectionCookie].get();
    brokerConnection->disconnect();
    broker_connections_.erase(connectionCookie);

    lwpa_rwlock_writeunlock(&conn_lock);
  }

  for (int i = invisibleRootItem()->rowCount() - 1; (i >= 0) && !removeComplete; --i)
  {
    BrokerItem *currentItem = dynamic_cast<BrokerItem *>(invisibleRootItem()->child(i));

    if (currentItem)
    {
      if (currentItem->getConnectionCookie() == connectionCookie)
      {
        currentItem->completelyRemoveChildren(0, currentItem->rowCount());
        invisibleRootItem()->removeRow(i);
        removeComplete = true;
      }
    }
  }

  // Broadcast GET_RESPONSE notification because of removed scope
  if (getComponentScope(0x0001, resp_data_list, &num_responses))
  {
    SendRDMGetResponses(kRdmnetControllerBroadcastUid, E133_BROADCAST_ENDPOINT, E133_COMPONENT_SCOPE, resp_data_list,
                        num_responses, 0, 0);
  }
}

void RDMnetNetworkModel::removeAllBrokers()
{
  static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
  size_t num_responses;

  if (lwpa_rwlock_writelock(&conn_lock, LWPA_WAIT_FOREVER))
  {
    for (auto &&broker_conn : broker_connections_)
      broker_conn.second->disconnect();

    broker_connections_.clear();

    lwpa_rwlock_writeunlock(&conn_lock);
  }

  for (int i = invisibleRootItem()->rowCount() - 1; i >= 0; --i)
  {
    BrokerItem *currentItem = dynamic_cast<BrokerItem *>(invisibleRootItem()->child(i));

    if (currentItem)
    {
      currentItem->completelyRemoveChildren(0, currentItem->rowCount());
    }
  }

  invisibleRootItem()->removeRows(0, invisibleRootItem()->rowCount());

  // Broadcast GET_RESPONSE notification, which will send an empty scope
  // to show that there are no scopes left.
  if (getComponentScope(0x0001, resp_data_list, &num_responses))
  {
    SendRDMGetResponses(kRdmnetControllerBroadcastUid, E133_BROADCAST_ENDPOINT, E133_COMPONENT_SCOPE, resp_data_list,
                        num_responses, 0, 0);
  }
}

void RDMnetNetworkModel::activateFeature(RDMnetNetworkItem *device, SupportedDeviceFeature feature)
{
  if (device != nullptr)
  {
    RdmCommand setCmd;

    setCmd.dest_uid.manu = device->getMan();
    setCmd.dest_uid.id = device->getDev();
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

RDMnetNetworkModel *RDMnetNetworkModel::makeRDMnetNetworkModel(RDMnetLibWrapper *library, ControllerLog *log)
{
  RDMnetNetworkModel *model = new RDMnetNetworkModel(library, log);

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
  PropertyValueItem::setPIDInfo(E120_SUPPORTED_PARAMETERS,
                                rdmPIDFlags | kSupportsGet | kExcludeFromModel,
                                QVariant::Type::Invalid);

  // DEVICE_INFO
  PropertyValueItem::setPIDInfo(E120_DEVICE_INFO,
                                rdmPIDFlags | kSupportsGet,
                                QVariant::Type::Invalid);
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
  PropertyValueItem::setPIDMaxBufferSize(E120_DEVICE_LABEL, DEVICE_LABEL_MAX_LEN);

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

  qRegisterMetaType<std::vector<ClientEntryData>>("std::vector<ClientEntryData>");
  qRegisterMetaType<std::vector<std::pair<uint16_t, uint8_t>>>("std::vector<std::pair<uint16_t, uint8_t>>");
  qRegisterMetaType<std::vector<RdmUid>>("std::vector<RdmUid>");
  qRegisterMetaType<std::vector<PropertyItem *> *>("std::vector<PropertyItem*>*");
  qRegisterMetaType<QVector<int>>("QVector<int>");
  qRegisterMetaType<uint16_t>("uint16_t");

  connect(model, SIGNAL(addRDMnetClients(BrokerConnection *, const std::vector<ClientEntryData> &)), model,
          SLOT(processAddRDMnetClients(BrokerConnection *, const std::vector<ClientEntryData> &)), Qt::AutoConnection);
  connect(model, SIGNAL(removeRDMnetClients(BrokerConnection *, const std::vector<ClientEntryData> &)), model,
          SLOT(processRemoveRDMnetClients(BrokerConnection *, const std::vector<ClientEntryData> &)),
          Qt::AutoConnection);
  connect(model, SIGNAL(newEndpointList(RDMnetClientItem *, const std::vector<std::pair<uint16_t, uint8_t>> &)), model,
          SLOT(processNewEndpointList(RDMnetClientItem *, const std::vector<std::pair<uint16_t, uint8_t>> &)),
          Qt::AutoConnection);
  connect(model, SIGNAL(newResponderList(EndpointItem *, const std::vector<RdmUid> &)), model,
          SLOT(processNewResponderList(EndpointItem *, const std::vector<RdmUid> &)), Qt::AutoConnection);
  connect(model, SIGNAL(setPropertyData(RDMnetNetworkItem *, unsigned short, const QString &, const QVariant &, int)),
          model,
          SLOT(processSetPropertyData(RDMnetNetworkItem *, unsigned short, const QString &, const QVariant &, int)),
          Qt::AutoConnection);
  connect(model,
          SIGNAL(removePropertiesInRange(RDMnetNetworkItem *, std::vector<PropertyItem *> *, unsigned short, int,
                                         const QVariant &, const QVariant &)),
          model,
          SLOT(processRemovePropertiesInRange(RDMnetNetworkItem *, std::vector<PropertyItem *> *, unsigned short, int,
                                              const QVariant &, const QVariant &)),
          Qt::AutoConnection);
  connect(model, SIGNAL(addPropertyEntry(RDMnetNetworkItem *, unsigned short, const QString &, int)), model,
          SLOT(processAddPropertyEntry(RDMnetNetworkItem *, unsigned short, const QString &, int)), Qt::AutoConnection);

  return model;
}

RDMnetNetworkModel *RDMnetNetworkModel::makeTestModel()
{
  RDMnetNetworkModel *model = new RDMnetNetworkModel(nullptr, nullptr);

  QStandardItem *parentItem = model->invisibleRootItem();

  model->setColumnCount(2);
  model->setHeaderData(0, Qt::Orientation::Horizontal, tr("Name"));
  model->setHeaderData(1, Qt::Orientation::Horizontal, tr("Value"));

  for (int i = 0; i < 4; ++i)
  {
    QStandardItem *item = new RDMnetNetworkItem(QString("item %0").arg(i));
    QStandardItem *item2 = new RDMnetNetworkItem(QString("item2 %0").arg(i));

    appendRowToItem(parentItem, item);
    parentItem->setChild(parentItem->rowCount() - 1, 1, item2);

    parentItem = item;
  }

  if (parentItem->type() == RDMnetNetworkItem::RDMnetNetworkItemType)
    dynamic_cast<RDMnetNetworkItem *>(parentItem)->enableChildrenSearch();

  return model;
}

void RDMnetNetworkModel::searchingItemRevealed(SearchingStatusItem *searchItem)
{
  if (searchItem != nullptr)
  {
    if (!searchItem->wasSearchInitiated())
    {
      // A search item was likely just revealed in the tree, starting a search process.
      QStandardItem *searchItemParent = searchItem->parent();

      if (searchItemParent != nullptr)
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
            RDMnetClientItem *clientItem = dynamic_cast<RDMnetClientItem *>(searchItemParent);

            if (clientItem != nullptr)
            {
              RdmCommand cmd;

              cmd.dest_uid.manu = clientItem->Uid().manu;
              cmd.dest_uid.id = clientItem->Uid().id;
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
            EndpointItem *endpointItem = dynamic_cast<EndpointItem *>(searchItemParent);

            if (endpointItem != nullptr)
            {
              // Ask for the devices on each endpoint
              RdmCommand cmd;

              cmd.dest_uid.manu = endpointItem->parent_uid_.manu;
              cmd.dest_uid.id = endpointItem->parent_uid_.id;
              cmd.subdevice = 0;

              searchItem->setSearchInitiated(true);

              // Send command to get endpoint devices
              cmd.command_class = kRdmCCGetCommand;
              cmd.param_id = E137_7_ENDPOINT_RESPONDERS;
              cmd.datalen = sizeof(uint16_t);
              lwpa_pack_16b(cmd.data, endpointItem->endpoint_);

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

bool RDMnetNetworkModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  QStandardItem *item = itemFromIndex(index);
  bool updateValue = true;
  QVariant newValue = value;

  if (item != nullptr)
  {
    if (item->type() == PropertyValueItem::PropertyValueItemType)
    {
      PropertyValueItem *propertyValueItem = dynamic_cast<PropertyValueItem *>(item);
      RDMnetNetworkItem *parentItem = getNearestParentItemOfType<ResponderItem>(item);

      if (parentItem == nullptr)
      {
        parentItem = getNearestParentItemOfType<RDMnetClientItem>(item);
      }

      if ((propertyValueItem != nullptr) && (parentItem != nullptr))
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
            uint8_t *packPtr;

            // IP static config variables
            char ipStrBuffer[64];

            memset(ipStrBuffer, '\0', 64);

            setCmd.dest_uid.manu = parentItem->getMan();
            setCmd.dest_uid.id = parentItem->getDev();
            setCmd.subdevice = 0;
            setCmd.command_class = E120_SET_COMMAND;
            setCmd.param_id = pid;
            setCmd.datalen = maxBuffSize;
            memset(setCmd.data, 0, maxBuffSize);
            packPtr = setCmd.data;

            // Special cases for certain PIDs
            if (pid == E133_COMPONENT_SCOPE)
            {
              // Scope slot (default to 1)
              int slot = index.data(RDMnetNetworkItem::ScopeSlotRole).toInt();
              lwpa_pack_16b(packPtr, index.data(RDMnetNetworkItem::ScopeSlotRole).toInt());
              packPtr += 2;
            }

            switch (PropertyValueItem::pidDataType(pid))
            {
              case QVariant::Type::Int:
                switch (maxBuffSize - (packPtr - setCmd.data))
                {
                  case 2:
                    lwpa_pack_16b(packPtr, value.toInt());
                    break;
                  case 4:
                    lwpa_pack_32b(packPtr, value.toInt());
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

                  QVariant &scopeString = index.data(RDMnetNetworkItem::ScopeDataRole);
                  QVariant &ipv4String = index.data(RDMnetNetworkItem::StaticIPv4DataRole);
                  QVariant &ipv6String = index.data(RDMnetNetworkItem::StaticIPv6DataRole);

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

                  packPtr = packIPAddressItem(ipv4String, kLwpaIpTypeV4, packPtr,
                                              (staticConfigType == E133_STATIC_CONFIG_IPV4));

                  if (staticConfigType == E133_STATIC_CONFIG_IPV4)
                  {
                    // This way, packIPAddressItem obtained the port value for us.
                    // Save the port value for later - we don't want it packed here.
                    packPtr -= 2;
                    port = lwpa_upack_16b(packPtr);
                  }

                  packPtr = packIPAddressItem(ipv6String, kLwpaIpTypeV6, packPtr,
                                              (staticConfigType != E133_STATIC_CONFIG_IPV4));

                  if (staticConfigType == E133_STATIC_CONFIG_IPV4)
                  {
                    // Pack the port value saved from earlier.
                    lwpa_pack_16b(packPtr, port);
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
              BrokerItem *brokerItem = getNearestParentItemOfType<BrokerItem>(parentItem);
              SendRDMCommand(setCmd, brokerItem);

              if (pid == E120_DMX_PERSONALITY)
              {
                sendGetCommand(brokerItem, E120_DEVICE_INFO, parentItem->getMan(), parentItem->getDev());
              }
            }
          }
        }
      }
    }
  }

  return updateValue ? QStandardItemModel::setData(index, newValue, role) : false;
}

void RDMnetNetworkModel::ClientListUpdate(rdmnet_client_scope_t sopce_handle, client_list_action_t action,
                                          const ClientList &list)
{
  ControllerReadGuard conn_read(conn_lock_);

  brokerConn = broker_connections_[conn].get();

  switch (broker_msg->vector)
  {
    case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
    case VECTOR_BROKER_CLIENT_ADD:
    case VECTOR_BROKER_CLIENT_REMOVE:
    {
      // printf("\nReceived vector= MGMT_CLIENT_LIST       len= %u\n",
      // mgmt_vector_data_len);

      std::vector<ClientEntryData> list;
      const ClientList *client_list = get_client_list(broker_msg);

      for (ClientEntryData *entry = client_list->client_entry_list; entry; entry = entry->next)
      {
        list.push_back(*entry);
      }

      if (broker_msg->vector == VECTOR_BROKER_CLIENT_REMOVE)
        emit removeRDMnetClients(brokerConn, list);
      else
        emit addRDMnetClients(brokerConn, list);
    }
    break;
    // case VECTOR_BROKER_DISCONNECT:
    //{
    //  emit brokerDisconnection(conn);
    //}
    // break;
    default:
      // printf("\nERROR Received MGMT vector= 0x%04x      len= %u\n",
      // mgmt_vector, mgmt_vector_data_len);
      break;
  }
}

void RDMnetNetworkModel::StatusReceived(const std::string &scope, const RemoteRptStatus &status)
{
  // This function has some work TODO. We should at least be logging things
  // here.

  log_->Log(LWPA_LOG_INFO, "Got RPT Status with code %d", status.msg.status_code);
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

bool RDMnetNetworkModel::SendRDMCommand(const RdmCommand &cmd, BrokerItem *brokerItem)
{
  if (!brokerItem)
  {
    log_->Log(LWPA_LOG_ERR, "Error: SendRDMCommand called with invalid Broker item.");
    return false;
  }
  else
  {
    return SendRDMCommand(cmd, brokerItem->scope_handle());
  }
}

bool RDMnetNetworkModel::SendRDMCommand(const RdmCommand &cmd, rdmnet_client_scope_t scope_handle)
{
  RptHeader header;
  RdmUid rpt_dest_uid = cmd.dest_uid;
  RdmUid rdm_dest_uid = cmd.dest_uid;
  uint16_t dest_endpoint = 0;

  LocalRdmCommand cmd_to_send;
  cmd_to_send.dest_endpoint = dest_endpoint;
  cmd_to_send.dest_uid = cmd.dest_uid;
  cmd_to_send.rdm = cmd;

  header.source_uid = connectionToUse->getLocalUID();
  header.source_endpoint_id = 0;
  header.dest_uid = rpt_dest_uid;
  header.dest_endpoint_id = dest_endpoint;
  header.seqnum = connectionToUse->sequencePreIncrement();

  RdmCommand to_send = cmd;
  to_send.source_uid = header.source_uid;
  to_send.port_id = 1;
  to_send.transaction_num = static_cast<uint8_t>(header.seqnum & 0xffu);
  RdmBuffer rdmbuf;
  if (kLwpaErrOk != rdmctl_create_command(&to_send, &rdmbuf))
  {
    return false;
  }

  LwpaUuid my_cid = BrokerConnection::getLocalCID();
  if (kLwpaErrOk != send_rpt_request(connectionToUse->handle(), &my_cid, &header, &rdmbuf))
  {
    return false;
  }

  return true;
}

void RDMnetNetworkModel::SendRDMGetResponses(const RdmUid &dest_uid, uint16_t dest_endpoint_id, uint16_t param_id,
                                             const RdmParamData *resp_data_list, size_t num_responses, uint32_t seqnum,
                                             uint8_t transaction_num, rdmnet_client_scope_t scope_handle)
{
  std::vector<RdmResponse> resp_list;
  RdmResponse resp_data;

  // The source UID will be added later, right before sending.
  resp_data.dest_uid = dest_uid;
  resp_data.transaction_num = transaction_num;
  resp_data.resp_type = num_responses > 1 ? kRdmResponseTypeAckOverflow : kRdmResponseTypeAck;
  resp_data.msg_count = 0;
  resp_data.subdevice = 0;
  resp_data.command_class = kRdmCCGetCommandResponse;
  resp_data.param_id = param_id;

  for (size_t i = 0; i < num_responses; ++i)
  {
    memcpy(resp_data.data, resp_data_list[i].data, resp_data_list[i].datalen);
    resp_data.datalen = resp_data_list[i].datalen;
    if (i == num_responses - 1)
    {
      resp_data.resp_type = kRdmResponseTypeAck;
    }
    resp_list.push_back(resp_data);
  }
  SendNotification(conn, dest_uid, dest_endpoint_id, seqnum, resp_list);
}

void RDMnetNetworkModel::SendRDMNack(rdmnet_client_scope_t scope, const RptHeader *received_header,
                                     const RdmCommand *cmd_data, uint16_t nack_reason)
{
  std::vector<RdmResponse> resp_list;
  RdmResponse resp_data;

  resp_data.dest_uid = received_header->source_uid;
  resp_data.transaction_num = cmd_data->transaction_num;
  resp_data.resp_type = kRdmResponseTypeNackReason;
  resp_data.msg_count = 0;
  resp_data.subdevice = 0;
  resp_data.command_class =
      (cmd_data->command_class == kRdmCCSetCommand) ? kRdmCCSetCommandResponse : kRdmCCGetCommandResponse;
  resp_data.param_id = cmd_data->param_id;
  resp_data.datalen = 2;
  lwpa_pack_16b(resp_data.data, nack_reason);

  resp_list.push_back(resp_data);

  LocalRdmResponse resp;
  resp.dest_uid = received_header->source_uid;
  resp.num_responses = 1;
  resp.rdm_arr = &resp_data;
  resp.seq_num = received_header->seqnum;
  resp.source_endpoint = E133_NULL_ENDPOINT;
  rdmnet_->SendRdmResponse(scope, resp);
}

void RDMnetNetworkModel::RdmCommandReceived(rdmnet_client_scope_t scope_handle, const RemoteRdmCommand &cmd)
{
  bool should_nack = false;
  uint16_t nack_reason;

  const RdmCommand &rdm = cmd.rdm;
  if (rdm.command_class == kRdmCCGetCommand)
  {
    std::vector<RdmParamData> resp_data_list;

    if (default_responder_.Get(rdm.param_id, rdm.data, rdm.datalen, resp_data_list, nack_reason))
    {
      SendRDMGetResponses(header->source_uid, header->source_endpoint_id, cmd.param_id, resp_data_list, num_responses,
                          header->seqnum, cmd.transaction_num, conn);

      log_->Log(LWPA_LOG_DEBUG, "ACK'ing GET_COMMAND for PID 0x%04x from Controller %04x:%08x", cmd.param_id,
                header->source_uid.manu, header->source_uid.id);
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
    log_->Log(LWPA_LOG_DEBUG, "Sending NACK to Controller %04x:%08x for PID 0x%04x with reason 0x%04x",
              cmd.source_uid.manu, cmd.source_uid.id, rdm.param_id, nack_reason);
  }
}

void RDMnetNetworkModel::RdmResponseReceived(rdmnet_client_scope_t scope_handle, const RemoteRdmResponse &resp)
{
  if (response.size() == 0)
  {
    return;
  }

  auto first_resp = response.front();
  switch (first_resp.resp_type)
  {
    case E120_RESPONSE_TYPE_ACK_TIMER:
    {
      return;
    }
    case E120_RESPONSE_TYPE_ACK:
    case E120_RESPONSE_TYPE_ACK_OVERFLOW:
      break;
    case E120_RESPONSE_TYPE_NACK_REASON:
    {
      uint16_t nackReason = 0xffff;

      if (first_resp.datalen == 2)
        nackReason = lwpa_upack_16b(first_resp.data);
      nack(conn, nackReason, &first_resp);
      return;
    }
    default:
      return;  // Unknown response type
  }

  if (first_resp.command_class == kRdmCCGetCommandResponse)
  {
    log_->Log(LWPA_LOG_INFO, "Got GET_COMMAND_RESPONSE with PID 0x%04x from Controller %04x:%08x", first_resp.param_id,
              first_resp.source_uid.manu, first_resp.source_uid.id);

    switch (first_resp.param_id)
    {
      case E120_STATUS_MESSAGES:
      {
        // TODO
        //   for (unsigned int i = 0; i < cmd->getLength(); i += 9)
        //   {
        //     cmd->setSubdevice((uint8_t)Upack16B(&cmdBuffer[i]));

        //     status(cmdBuffer[i + 2], Upack16B(&cmdBuffer[i + 3]),
        //            Upack16B(&cmdBuffer[i + 5]), Upack16B(&cmdBuffer[i + 7]),
        //            cmd);
        //   }

        //   if (cmd->getLength() == 0)
        //   {
        //     status(E120_STATUS_ADVISORY_CLEARED, 0, 0, 0, cmd);
        //   }
        break;
      }
      case E120_SUPPORTED_PARAMETERS:
      {
        std::vector<uint16_t> list;

        for (auto resp_part : response)
        {
          for (size_t pos = 0; pos + 1 < resp_part.datalen; pos += 2)
            list.push_back(lwpa_upack_16b(&resp_part.data[pos]));
        }

        if (!list.empty())
          commands(conn, list, &first_resp);
        break;
      }
      case E120_DEVICE_INFO:
      {
        if (first_resp.datalen >= 19)
        {
          // Current personality is reset if less than 1
          uint8_t cur_pers = (first_resp.data[12] < 1 ? 1 : first_resp.data[12]);
          // Total personality is reset if current or total is less than 1
          uint8_t total_pers = ((first_resp.data[12] < 1 || first_resp.data[13] < 1) ? 1 : first_resp.data[13]);

          deviceInfo(conn, lwpa_upack_16b(&first_resp.data[0]), lwpa_upack_16b(&first_resp.data[2]),
                     lwpa_upack_16b(&first_resp.data[4]), lwpa_upack_32b(&first_resp.data[6]),
                     lwpa_upack_16b(&first_resp.data[10]), cur_pers, total_pers, lwpa_upack_16b(&first_resp.data[14]),
                     lwpa_upack_16b(&first_resp.data[16]), first_resp.data[18], &first_resp);
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

        // Ensure that the string is nullptr terminated
        memset(label, 0, 33);
        // Max label length is 32
        memcpy(label, first_resp.data, (first_resp.datalen > 32) ? 32 : first_resp.datalen);

        switch (first_resp.param_id)
        {
          case E120_DEVICE_MODEL_DESCRIPTION:
            modelDesc(conn, label, &first_resp);
            break;
          case E120_SOFTWARE_VERSION_LABEL:
            softwareLabel(conn, label, &first_resp);
            break;
          case E120_MANUFACTURER_LABEL:
            manufacturerLabel(conn, label, &first_resp);
            break;
          case E120_DEVICE_LABEL:
            deviceLabel(conn, label, &first_resp);
            break;
          case E120_BOOT_SOFTWARE_VERSION_LABEL:
            bootSoftwareLabel(conn, label, &first_resp);
            break;
        }
        break;
      }
      case E120_BOOT_SOFTWARE_VERSION_ID:
      {
        if (first_resp.datalen >= 4)
          bootSoftwareID(conn, lwpa_upack_32b(first_resp.data), &first_resp);
        break;
      }
      case E120_DMX_PERSONALITY:
      {
        if (first_resp.datalen >= 2)
          personality(conn, first_resp.data[0], first_resp.data[1], &first_resp);
        break;
      }
      case E120_DMX_PERSONALITY_DESCRIPTION:
      {
        if (first_resp.datalen >= 3)
        {
          char description[33];
          uint8_t descriptionLength = first_resp.datalen - 3;

          memset(description, 0,
                 33);  // Ensure that the string is nullptr terminated
          memcpy(description, &first_resp.data[3],
                 (descriptionLength > 32) ? 32 : descriptionLength);  // Max description length is 32

          personalityDescription(conn, first_resp.data[0], lwpa_upack_16b(&first_resp.data[1]), description,
                                 &first_resp);
        }
        break;
      }
      case E137_7_ENDPOINT_LIST:
      {
        bool is_first_message = true;
        uint32_t change_number = 0;
        std::vector<std::pair<uint16_t, uint8_t>> list;
        RdmUid source_uid;

        for (auto resp_part : response)
        {
          size_t pos = 0;
          if (is_first_message)
          {
            if (resp_part.datalen < 4)
              break;
            source_uid = resp_part.source_uid;
            change_number = lwpa_upack_32b(&resp_part.data[0]);
            pos = 4;
          }

          for (; pos + 2 < resp_part.datalen; pos += 3)
          {
            uint16_t endpoint_id = lwpa_upack_16b(&resp_part.data[pos]);
            uint8_t endpoint_type = resp_part.data[pos + 2];
            list.push_back(std::make_pair(endpoint_id, endpoint_type));
          }
        }

        endpointList(conn, change_number, list, source_uid);
      }
      break;
      case E137_7_ENDPOINT_RESPONDERS:
      {
        bool is_first_message = true;
        RdmUid source_uid;
        std::vector<RdmUid> list;
        uint16_t endpoint_id = 0;
        uint32_t change_number = 0;

        for (auto resp_part : response)
        {
          size_t pos = 0;
          if (is_first_message)
          {
            if (resp_part.datalen < 6)
              break;
            source_uid = resp_part.source_uid;
            endpoint_id = lwpa_upack_16b(&resp_part.data[0]);
            change_number = lwpa_upack_32b(&resp_part.data[2]);
            pos = 6;
          }

          for (; pos + 5 < resp_part.datalen; pos += 6)
          {
            RdmUid device;
            device.manu = lwpa_upack_16b(&resp_part.data[pos]);
            device.id = lwpa_upack_32b(&resp_part.data[pos + 2]);
            list.push_back(device);
          }
        }

        endpointResponders(conn, endpoint_id, change_number, list, source_uid);
      }
      break;
      case E137_7_ENDPOINT_LIST_CHANGE:
      {
        if (first_resp.datalen >= 4)
        {
          uint32_t change_number = lwpa_upack_32b(first_resp.data);
          endpointListChange(conn, change_number, first_resp.source_uid);
        }
        break;
      }
      case E137_7_ENDPOINT_RESPONDER_LIST_CHANGE:
      {
        if (first_resp.datalen >= 6)
        {
          uint16_t endpoint_id = lwpa_upack_16b(first_resp.data);
          uint32_t change_num = lwpa_upack_32b(&first_resp.data[2]);
          responderListChange(conn, change_num, endpoint_id, first_resp.source_uid);
        }
        break;
      }
      case E133_TCP_COMMS_STATUS:
      {
        for (auto resp_part : response)
        {
          char scopeString[E133_SCOPE_STRING_PADDED_LENGTH];
          memset(scopeString, 0, E133_SCOPE_STRING_PADDED_LENGTH);
          memcpy(scopeString, resp_part.data, E133_SCOPE_STRING_PADDED_LENGTH - 1);

          QString v4AddrString =
              UnpackAndParseIPAddress(resp_part.data + E133_SCOPE_STRING_PADDED_LENGTH, kLwpaIpTypeV4);
          QString v6AddrString =
              UnpackAndParseIPAddress(resp_part.data + E133_SCOPE_STRING_PADDED_LENGTH + 4, kLwpaIpTypeV6);
          uint16_t port = lwpa_upack_16b(resp_part.data + E133_SCOPE_STRING_PADDED_LENGTH + 4 + LWPA_IPV6_BYTES);
          uint16_t unhealthyTCPEvents =
              lwpa_upack_16b(resp_part.data + E133_SCOPE_STRING_PADDED_LENGTH + 4 + LWPA_IPV6_BYTES + 2);

          HandleTcpCommsStatusResponse(scope_handle, QString::fromUtf8(scopeString), v4AddrString, v6AddrString, port,
                                       unhealthyTCPEvents, &first_resp);
        }

        break;
      }
      default:
      {
        // Process data for PIDs that support get and set, where the data has the same form in either case.
        ProcessRDMGetSetData(conn, first_resp.param_id, first_resp.data, first_resp.datalen, &first_resp);
        break;
      }
    }
  }
  else if (first_resp.command_class == E120_SET_COMMAND_RESPONSE)
  {
    log_->Log(LWPA_LOG_INFO, "Got SET_COMMAND_RESPONSE with PID %d", first_resp.param_id);

    if (have_command)
    {
      // Make sure this Controller is up-to-date with data that was set on a Device.
      switch (first_resp.param_id)
      {
        case E120_DMX_PERSONALITY:
        {
          if (cmd.datalen >= 2)
            personality(conn, cmd.data[0], 0, &first_resp);
          break;
        }
        default:
        {
          // Process PIDs with data that is in the same format for get and set.
          ProcessRDMGetSetData(conn, first_resp.param_id, cmd.data, cmd.datalen, &first_resp);
          break;
        }
      }
    }
  }
}

void RDMnetNetworkModel::ProcessRDMGetSetData(rdmnet_client_scope_t scope_handle, uint16_t param_id,
                                              const uint8_t *data, uint8_t datalen, const RdmResponse &firstResp)
{
  if (data)
  {
    switch (param_id)
    {
      case E120_DEVICE_LABEL:
      {
        char label[33];

        // Ensure that the string is nullptr terminated
        memset(label, 0, 33);
        // Max label length is 32
        memcpy(label, data, (datalen > 32) ? 32 : datalen);

        deviceLabel(conn, label, firstResp);
        break;
      }
      case E120_DMX_START_ADDRESS:
      {
        if (datalen >= 2)
        {
          address(conn, lwpa_upack_16b(data), firstResp);
        }
        break;
      }
      case E120_IDENTIFY_DEVICE:
      {
        if (datalen >= 1)
          identify(conn, data[0], firstResp);
        break;
      }
      case E133_COMPONENT_SCOPE:
      {
        uint16_t scopeSlot;
        char scopeString[E133_SCOPE_STRING_PADDED_LENGTH];
        char addrBuf[LWPA_INET6_ADDRSTRLEN] = {};
        char *staticConfigV4 = nullptr;
        char *staticConfigV6 = nullptr;
        uint16_t port = 0;
        const uint8_t *cur_ptr = data;

        scopeSlot = lwpa_upack_16b(cur_ptr);
        cur_ptr += 2;
        memcpy(scopeString, cur_ptr, E133_SCOPE_STRING_PADDED_LENGTH);
        scopeString[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
        cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

        uint8_t staticConfigType = *cur_ptr++;
        switch (staticConfigType)
        {
          case E133_STATIC_CONFIG_IPV4:
            UnpackAndParseIPAddress(cur_ptr, kLwpaIpTypeV4, addrBuf, LWPA_INET6_ADDRSTRLEN);
            cur_ptr += 4 + 16;
            port = lwpa_upack_16b(cur_ptr);
            staticConfigV4 = addrBuf;
            break;
          case E133_STATIC_CONFIG_IPV6:
            cur_ptr += 4;
            UnpackAndParseIPAddress(cur_ptr, kLwpaIpTypeV6, addrBuf, LWPA_INET6_ADDRSTRLEN);
            cur_ptr += 16;
            port = lwpa_upack_16b(cur_ptr);
            staticConfigV6 = addrBuf;
            break;
          case E133_NO_STATIC_CONFIG:
          default:
            break;
        }
        componentScope(conn, scopeSlot, scopeString, staticConfigV4, staticConfigV6, port, firstResp);
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

void RDMnetNetworkModel::endpointList(rdmnet_client_scope_t scope_handle, uint32_t /*changeNumber*/,
                                      const std::vector<std::pair<uint16_t, uint8_t>> &list, const RdmUid &source_uid)
{
  if (broker_connections_.find(conn) == broker_connections_.end())
  {
    log_->Log(LWPA_LOG_ERR, "Error: endpointList called with invalid connection cookie.");
  }
  else
  {
    BrokerConnection *connection = broker_connections_[conn].get();

    if (connection != nullptr && connection->connected())
    {
      BrokerItem *brokerItem = connection->treeBrokerItem();
      if (brokerItem != nullptr)
      {
        for (auto i : brokerItem->rdmnet_clients_)
        {
          if (i->Uid() == source_uid)
          {
            // Found a matching discovered client
            emit newEndpointList(i, list);

            break;
          }
        }
      }
    }
  }
}

void RDMnetNetworkModel::endpointResponders(rdmnet_client_scope_t scope_handle, uint16_t endpoint,
                                            uint32_t /*changeNumber*/, const std::vector<RdmUid> &list,
                                            const RdmUid &source_uid)
{
  if (broker_connections_.find(conn) == broker_connections_.end())
  {
    log_->Log(LWPA_LOG_ERR, "Error: endpointResponders called with invalid connection cookie.");
  }
  else
  {
    BrokerConnection *connection = broker_connections_[conn].get();

    if (connection != nullptr && connection->connected())
    {
      BrokerItem *brokerItem = connection->treeBrokerItem();
      if (brokerItem != nullptr)
      {
        for (auto i : brokerItem->rdmnet_clients_)
        {
          if (i->Uid() == source_uid)
          {
            // Found a matching discovered client

            // Now find the matching endpoint
            for (auto j : i->endpoints_)
            {
              if (j->endpoint_ == endpoint)
              {
                // Found a matching endpoint
                emit newResponderList(j, list);
                break;
              }
            }
            break;
          }
        }
      }
    }
  }
}

void RDMnetNetworkModel::endpointListChange(rdmnet_client_scope_t scope_handle, uint32_t /*changeNumber*/,
                                            const RdmUid &source_uid)
{
  RdmCommand cmd;

  cmd.dest_uid = source_uid;
  cmd.subdevice = 0;
  cmd.command_class = kRdmCCGetCommand;
  cmd.param_id = E137_7_ENDPOINT_LIST;
  cmd.datalen = 0;

  rdmnet_->SendRdmCommand(cmd, conn);
}

void RDMnetNetworkModel::responderListChange(rdmnet_client_scope_t scope_handle, uint32_t /*changeNumber*/,
                                             uint16_t endpoint, const RdmUid &source_uid)
{
  // Ask for the devices on each endpoint
  RdmCommand cmd;

  cmd.dest_uid = source_uid;
  cmd.subdevice = 0;
  cmd.command_class = kRdmCCGetCommand;
  cmd.param_id = E137_7_ENDPOINT_RESPONDERS;
  cmd.datalen = sizeof(uint16_t);
  lwpa_pack_16b(cmd.data, endpoint);

  rdmnet_->SendRdmCommand(cmd, conn);
}

void RDMnetNetworkModel::HandleRdmNack(rdmnet_client_scope_t scope_handle, uint16_t reason, const RdmResponse &resp)
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
      lwpa_pack_16b(cmd.data, 0x0001);  // Scope slot, default to 1 for RPT Devices (non-controllers, non-brokers).
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
    RDMnetClientItem *client = GetClientItem(scope_handle, resp);
    RDMnetNetworkItem *rdmNetGroup = dynamic_cast<RDMnetNetworkItem *>(
        client->child(0)->data() == tr("RDMnet") ? client->child(0) : client->child(1));

    removeScopeSlotItemsInRange(rdmNetGroup, &client->properties, previous_slot_[resp.source_uid] + 1, 0xFFFF);

    // We have all of this controller's scope-slot pairs. Now request scope-specific properties.
    previous_slot_[resp.source_uid] = 0;
    sendGetControllerScopeProperties(scope_handle, resp.source_uid.manu, resp.source_uid.id);
  }
}

void RDMnetNetworkModel::HandleStatusMessagesResponse(uint8_t /*type*/, uint16_t /*messageId*/, uint16_t /*data1*/,
                                                      uint16_t /*data2*/, const RdmResponse & /*resp*/)
{
}

void RDMnetNetworkModel::HandleSupportedParametersResponse(rdmnet_client_scope_t scope_handle,
                                                           const std::vector<uint16_t> &params_list,
                                                           const RdmResponse &resp)
{
  if (params_list.size() > 0)
  {
    // Get any properties that are supported
    RdmCommand getCmd;

    getCmd.dest_uid = resp.source_uid;
    getCmd.subdevice = 0;

    getCmd.command_class = kRdmCCGetCommand;
    getCmd.datalen = 0;

    for (const uint16_t &param : params_list)
    {
      if (pidSupportedByGUI(param, true) && param != E120_SUPPORTED_PARAMETERS)
      {
        getCmd.param_id = param;
        SendRDMCommand(getCmd, scope_handle);
      }
      else if (param == E120_RESET_DEVICE)
      {
        RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

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
                                                  const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

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

void RDMnetNetworkModel::HandleModelDescResponse(rdmnet_client_scope_t scope_handle, const QString &label,
                                                 const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_DEVICE_MODEL_DESCRIPTION,
                         PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_MODEL_DESCRIPTION), label);
  }
}

void RDMnetNetworkModel::HandleManufacturerLabelResponse(rdmnet_client_scope_t scope_handle, const QString &label,
                                                         const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

  if (device != nullptr)
  {
    emit setPropertyData(device, E120_MANUFACTURER_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_MANUFACTURER_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleDeviceLabelResponse(rdmnet_client_scope_t scope_handle, const QString &label,
                                                   const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_DEVICE_LABEL, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_LABEL),
                         label);
  }
}

void RDMnetNetworkModel::HandleSoftwareLabelResponse(rdmnet_client_scope_t scope_handle, const QString &label,
                                                     const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_SOFTWARE_VERSION_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleBootSoftwareIdResponse(rdmnet_client_scope_t scope_handle, uint32_t id,
                                                      const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

  if (device != nullptr)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_ID,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_ID), id);
  }
}

void RDMnetNetworkModel::HandleBootSoftwareLabelResponse(rdmnet_client_scope_t scope_handle, const QString &label,
                                                         const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_LABEL), label);
  }
}

void RDMnetNetworkModel::HandleStartAddressResponse(rdmnet_client_scope_t scope_handle, uint16_t address,
                                                    const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), address);
  }
}

void RDMnetNetworkModel::HandleIdentifyResponse(rdmnet_client_scope_t scope_handle, bool identifying,
                                                const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

  if (device)
  {
    device->setDeviceIdentifying(identifying);
    emit identifyChanged(device, identifying);
  }
}

void RDMnetNetworkModel::HandlePersonalityResponse(rdmnet_client_scope_t scope_handle, uint8_t current, uint8_t number,
                                                   const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);

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

      sendGetCommand(getNearestParentItemOfType<BrokerItem>(device), E120_DEVICE_INFO, resp.source_uid.manu,
                     resp.source_uid.id);
    }

    checkPersonalityDescriptions(device, number, resp);
  }
}

void RDMnetNetworkModel::HandlePersonalityDescResponse(rdmnet_client_scope_t scope_handle, uint8_t personality,
                                                       uint16_t footprint, const QString &description,
                                                       const RdmResponse &resp)
{
  RDMnetNetworkItem *device = GetNetworkItem(scope_handle, resp);
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
                                                      const QString &scopeString, const QString &staticConfigV4,
                                                      const QString &staticConfigV6, uint16_t port,
                                                      const RdmResponse &resp)
{
  RDMnetClientItem *client = GetClientItem(scope_handle, resp);

  if (client)
  {
    RDMnetNetworkItem *rdmNetGroup = dynamic_cast<RDMnetNetworkItem *>(
        client->child(0)->data() == tr("RDMnet") ? client->child(0) : client->child(1));

    if (client->ClientType() == kRPTClientTypeController)
    {
      removeScopeSlotItemsInRange(rdmNetGroup, &client->properties, previous_slot_[client->Uid()] + 1, scopeSlot - 1);
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
      previous_slot_[client->Uid()] = scopeSlot;
      sendGetNextControllerScope(scope_handle, resp.source_uid.manu, resp.source_uid.id, scopeSlot);
    }
  }
}

void RDMnetNetworkModel::HandleSearchDomainResponse(rdmnet_client_scope_t scope_handle, const QString &domainNameString,
                                                    const RdmResponse &resp)
{
  RDMnetClientItem *client = GetClientItem(scope_handle, resp);
  if (client)
  {
    emit setPropertyData(client, E133_SEARCH_DOMAIN, PropertyValueItem::pidPropertyDisplayName(E133_SEARCH_DOMAIN, 0),
                         domainNameString);
  }
}

void RDMnetNetworkModel::HandleTcpCommsStatusResponse(rdmnet_client_scope_t scope_handle, const QString &scopeString,
                                                      const QString &v4AddrString, const QString &v6AddrString,
                                                      uint16_t port, uint16_t unhealthyTCPEvents,
                                                      const RdmResponse &resp)
{
  RDMnetClientItem *client = GetClientItem(scope_handle, resp);

  if (client)
  {
    if (client->getScopeSlot(scopeString) != 0)
    {
      QVariant callbackObjectVariant;
      const char *callbackSlotString = SLOT(processPropertyButtonClick(const QPersistentModelIndex &));
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

void RDMnetNetworkModel::addPropertyEntries(RDMnetNetworkItem *parent, PIDFlags location)
{
  // Start out by adding all known properties and disabling them. Later on,
  // only the properties that the device supports will be enabled.
  for (PIDInfoIterator i = PropertyValueItem::pidsBegin(); i != PropertyValueItem::pidsEnd(); ++i)
  {
    bool excludeFromModel = i->second.pidFlags & kExcludeFromModel;
    location = location & (kLocResponder | kLocEndpoint | kLocDevice | kLocController | kLocBroker);

    if (!excludeFromModel && ((i->second.pidFlags & location) == location))
    {
      for (QStringList::iterator j = i->second.propertyDisplayNames.begin(); j != i->second.propertyDisplayNames.end();
           ++j)
      {
        emit addPropertyEntry(parent, i->first, *j, i->second.role);
      }
    }
  }
}

void RDMnetNetworkModel::initializeResponderProperties(ResponderItem *parent, uint16_t manuID, uint32_t deviceID)
{
  RdmCommand cmd;
  BrokerItem *brokerItem = getNearestParentItemOfType<BrokerItem>(parent);

  addPropertyEntries(parent, kLocResponder);

  // Now send requests for core required properties.
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = kRdmCCGetCommand;
  cmd.datalen = 0;

  cmd.param_id = E120_SUPPORTED_PARAMETERS;
  SendRDMCommand(cmd, brokerItem);
  cmd.param_id = E120_DEVICE_INFO;
  SendRDMCommand(cmd, brokerItem);
  cmd.param_id = E120_SOFTWARE_VERSION_LABEL;
  SendRDMCommand(cmd, brokerItem);
  cmd.param_id = E120_DMX_START_ADDRESS;
  SendRDMCommand(cmd, brokerItem);
  cmd.param_id = E120_IDENTIFY_DEVICE;
  SendRDMCommand(cmd, brokerItem);
}

void RDMnetNetworkModel::initializeRPTClientProperties(RDMnetClientItem *parent, uint16_t manuID, uint32_t deviceID,
                                                       rpt_client_type_t clientType)
{
  RdmCommand cmd;
  BrokerItem *brokerItem = getNearestParentItemOfType<BrokerItem>(parent);

  addPropertyEntries(parent, (clientType == kRPTClientTypeDevice) ? kLocDevice : kLocController);

  // Now send requests for core required properties.
  memset(cmd.data, 0, RDM_MAX_PDL);
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = kRdmCCGetCommand;
  cmd.datalen = 0;

  cmd.param_id = E120_SUPPORTED_PARAMETERS;
  SendRDMCommand(cmd, brokerItem);
  cmd.param_id = E120_DEVICE_INFO;
  SendRDMCommand(cmd, brokerItem);
  cmd.param_id = E120_SOFTWARE_VERSION_LABEL;
  SendRDMCommand(cmd, brokerItem);
  cmd.param_id = E120_DMX_START_ADDRESS;
  SendRDMCommand(cmd, brokerItem);
  cmd.param_id = E120_IDENTIFY_DEVICE;
  SendRDMCommand(cmd, brokerItem);

  cmd.param_id = E133_SEARCH_DOMAIN;
  SendRDMCommand(cmd, brokerItem);

  if (clientType == kRPTClientTypeDevice)  // For controllers, we need to wait for all the scopes first.
  {
    cmd.param_id = E133_TCP_COMMS_STATUS;
    SendRDMCommand(cmd, brokerItem);
  }

  cmd.datalen = 2;
  lwpa_pack_16b(cmd.data, 0x0001);  // Scope slot, start with #1
  cmd.param_id = E133_COMPONENT_SCOPE;
  SendRDMCommand(cmd, brokerItem);
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

  lwpa_pack_16b(cmd.data, min(currentSlot + 1, 0xffff));  // Scope slot, start with #1
  cmd.param_id = E133_COMPONENT_SCOPE;
  SendRDMCommand(cmd, scope_handle);
}

void RDMnetNetworkModel::sendGetCommand(BrokerItem *brokerItem, uint16_t pid, uint16_t manu, uint32_t dev)
{
  RdmCommand getCmd;

  getCmd.dest_uid.manu = manu;
  getCmd.dest_uid.id = dev;
  getCmd.subdevice = 0;

  getCmd.command_class = kRdmCCGetCommand;
  getCmd.param_id = pid;
  getCmd.datalen = 0;
  SendRDMCommand(getCmd, brokerItem);
}

uint8_t *RDMnetNetworkModel::packIPAddressItem(const QVariant &value, lwpa_iptype_t addrType, uint8_t *packPtr,
                                               bool packPort)
{
  char ipStrBuffer[64];
  unsigned int portNumber;
  size_t memSize = ((addrType == kLwpaIpTypeV4) ? 4 : (LWPA_IPV6_BYTES)) + (packPort ? 2 : 0);

  if (!packPtr)
  {
    return nullptr;
  }

  QString valueQString = value.toString();
  QByteArray local8Bit = valueQString.toLocal8Bit();
  const char *valueData = local8Bit.constData();

  if (value.toString().length() == 0)
  {
    memset(packPtr, 0, memSize);
  }
  else if (sscanf(valueData, (addrType == kLwpaIpTypeV4) ? "%63[1234567890.]:%u" : "[%63[1234567890:abcdefABCDEF]]:%u",
                  ipStrBuffer, &portNumber) < 2)
  {
    // Incorrect format entered.
    return nullptr;
  }
  else if (ParseAndPackIPAddress(addrType, ipStrBuffer, packPtr) != kLwpaErrOk)
  {
    return nullptr;
  }
  else if (portNumber > 65535)
  {
    return nullptr;
  }
  else if (packPort)
  {
    lwpa_pack_16b(packPtr + memSize - 2, static_cast<uint16_t>(portNumber));
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

RDMnetClientItem *RDMnetNetworkModel::GetClientItem(rdmnet_client_scope_t conn, const RdmResponse &resp)
{
  ControllerReadGuard conn_read(conn_lock_);

  if (broker_connections_.find(conn) == broker_connections_.end())
  {
    log_->Log(LWPA_LOG_ERR, "Error: getClientItem called with invalid connection cookie.");
  }
  else
  {
    BrokerItem *brokerItem = broker_connections_[conn];
    if (brokerItem)
    {
      for (auto i : brokerItem->rdmnet_clients_)
      {
        if ((i->getMan() == resp.source_uid.manu) && (i->getDev() == resp.source_uid.id))
        {
          return i;
        }
      }
    }
  }

  return nullptr;
}

RDMnetNetworkItem *RDMnetNetworkModel::GetNetworkItem(rdmnet_client_scope_t conn, const RdmResponse &resp)
{
  ControllerReadGuard conn_read(conn_lock_);

  if (broker_connections_.find(conn) == broker_connections_.end())
  {
    log_->Log(LWPA_LOG_ERR, "Error: getNetworkItem called with invalid connection cookie.");
  }
  else
  {
    BrokerItem *brokerItem = broker_connections_[conn];
    if (brokerItem)
    {
      for (auto client : brokerItem->rdmnet_clients_)
      {
        if ((client->getMan() == resp.source_uid.manu) && (client->getDev() == resp.source_uid.id))
        {
          return client;
        }

        for (auto endpoint : client->endpoints_)
        {
          for (auto device : endpoint->devices_)
          {
            if ((device->getMan() == resp.source_uid.manu) && (device->getDev() == resp.source_uid.id))
            {
              return device;
            }
          }
        }
      }
    }
  }

  return nullptr;
}

void RDMnetNetworkModel::checkPersonalityDescriptions(RDMnetNetworkItem *device, uint8_t numberOfPersonalities,
                                                      const RdmResponse &resp)
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

QVariant RDMnetNetworkModel::getPropertyData(RDMnetNetworkItem *parent, unsigned short pid, int role)
{
  QVariant result = QVariant();
  bool foundProperty = false;

  for (std::vector<PropertyItem *>::iterator iter = parent->properties.begin();
       (iter != parent->properties.end()) && !foundProperty; ++iter)
  {
    if ((*iter)->getValueItem() != nullptr)
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

PropertyItem *RDMnetNetworkModel::createPropertyItem(RDMnetNetworkItem *parent, const QString &fullName)
{
  RDMnetNetworkItem *currentParent = parent;
  QString currentPathName = fullName;
  QString shortName = getShortPropertyName(fullName);
  PropertyItem *propertyItem = new PropertyItem(fullName, shortName);

  while (currentPathName != shortName)
  {
    QString groupName = getHighestGroupName(currentPathName);

    RDMnetNetworkItem *groupingItem = getGroupingItem(currentParent, groupName);

    if (groupingItem == nullptr)
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

QString RDMnetNetworkModel::getShortPropertyName(const QString &fullPropertyName)
{
  QRegExp re("(\\\\)");
  QStringList query = fullPropertyName.split(re);

  if (query.length() > 0)
  {
    return query.at(query.length() - 1);
  }

  return QString();
}

QString RDMnetNetworkModel::getHighestGroupName(const QString &pathName)
{
  QRegExp re("(\\\\)");
  QStringList query = pathName.split(re);

  if (query.length() > 0)
  {
    return query.at(0);
  }

  return QString();
}

QString RDMnetNetworkModel::getPathSubset(const QString &fullPath, int first, int last)
{
  QRegExp re("(\\\\)");
  QStringList query = fullPath.split(re);
  QString result;

  if (last == -1)
  {
    last = (query.length() - 1);
  }

  for (int i = first; i <= min(last, (query.length() - 1)); ++i)
  {
    result += query.at(i);

    if (i != (query.length() - 1))
    {
      result += "\\";
    }
  }

  return result;
}

PropertyItem *RDMnetNetworkModel::getGroupingItem(RDMnetNetworkItem *parent, const QString &groupName)
{
  for (int i = 0; i < parent->rowCount(); ++i)
  {
    PropertyItem *item = dynamic_cast<PropertyItem *>(parent->child(i));

    if (item != nullptr)
    {
      if (item->text() == groupName)
      {
        return item;
      }
    }
  }

  return nullptr;
}

PropertyItem *RDMnetNetworkModel::createGroupingItem(RDMnetNetworkItem *parent, const QString &groupName)
{
  PropertyItem *groupingItem = new PropertyItem(groupName, groupName);

  appendRowToItem(parent, groupingItem);
  groupingItem->setEnabled(true);

  // Make sure values of group items are blank and inaccessible.
  PropertyValueItem *valueItem = new PropertyValueItem(QVariant(), false);
  groupingItem->setValueItem(valueItem);

  emit expandNewItem(groupingItem->index(), PropertyItem::PropertyItemType);

  return groupingItem;
}

QString RDMnetNetworkModel::getChildPathName(const QString &superPathName)
{
  QString highGroupName = getHighestGroupName(superPathName);
  int startPosition = highGroupName.length() + 1;  // Name + delimiter character

  return superPathName.mid(startPosition, superPathName.length() - startPosition);
}

QString RDMnetNetworkModel::getScopeSubPropertyFullName(RDMnetClientItem *client, uint16_t pid, int32_t index,
                                                        const QString &scope)
{
  QString original = PropertyValueItem::pidPropertyDisplayName(pid, index);

  if (client != nullptr)
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

void RDMnetNetworkModel::removeScopeSlotItemsInRange(RDMnetNetworkItem *parent, std::vector<PropertyItem *> *properties,
                                                     uint16_t firstSlot, uint16_t lastSlot)
{
  if (lastSlot >= firstSlot)
  {
    emit removePropertiesInRange(parent, properties, E133_COMPONENT_SCOPE, RDMnetNetworkItem::ScopeSlotRole, firstSlot,
                                 lastSlot);
  }
}

RDMnetNetworkModel::RDMnetNetworkModel(RDMnetLibInterface *library, ControllerLog *log) : rdmnet_(library), log_(log)
{
  lwpa_rwlock_create(&conn_lock_);
}

RDMnetNetworkModel::~RDMnetNetworkModel()
{
  {  // Write lock scope
    ControllerWriteGuard conn_write(conn_lock_);

    for (auto &connection : broker_connections_)
      rdmnet_->RemoveScope(connection.first);

    broker_connections_.clear();
  }

  lwpa_rwlock_destroy(&conn_lock_);
}
