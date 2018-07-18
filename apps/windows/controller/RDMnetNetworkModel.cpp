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
#include <QDateTime>
#include <QTimeZone>
#include <QMessageBox>
#include "lwpa_pack.h"
#include "rdmnet/rdmresponder.h"
#include "PropertyItem.h"
#include "PersonalityPropertyValueItem.h"

LwpaCid BrokerConnection::local_cid_;
LwpaUid BrokerConnection::local_uid_;
// IRDMnetSocketProxy_Notify *BrokerConnection::socketProxyNotify = NULL;
MyLog *BrokerConnection::log_ = NULL;
bool BrokerConnection::static_info_initialized_ = false;

bool RDMnetNetworkModel::rdmnet_initialized_ = false;

bool g_TestActive = false;
bool g_IgnoreEmptyStatus = true;
bool g_ShuttingDown = false;

lwpa_thread_t tick_thread_;

static void LogCallback(void *context, const char * /*syslog_str*/, const char *human_str)
{
  MyLog *log = static_cast<MyLog *>(context);
  if (log)
    log->LogFromCallback(human_str);
}

static void TimeCallback(void * /*context*/, LwpaLogTimeParams *time)
{
  QDateTime now = QDateTime::currentDateTime();
  QDate qdate = now.date();
  QTime qtime = now.time();
  time->cur_time.tm_sec = qtime.second();
  time->cur_time.tm_min = qtime.minute();
  time->cur_time.tm_hour = qtime.hour();
  time->cur_time.tm_mday = qdate.day();
  time->cur_time.tm_mon = qdate.month() - 1;
  time->cur_time.tm_year = qdate.year() - 1900;
  time->cur_time.tm_wday = (qdate.dayOfWeek() == 7 ? 0 : qdate.dayOfWeek());
  time->cur_time.tm_isdst = now.isDaylightTime();
  time->msec = qtime.msec();
  time->utc_offset = (QTimeZone::systemTimeZone().offsetFromUtc(now) / 60);
}

void broker_found(const char *scope, const BrokerDiscInfo *broker_info, void *context)
{
  RDMnetNetworkModel *model = static_cast<RDMnetNetworkModel *>(context);
  if (model)
  {
    for (auto iter = model->broker_connections_.begin(); iter != model->broker_connections_.end(); ++iter)
    {
      if (iter->second->scope() == scope)
      {
        iter->second->connect(broker_info);
      }
    }
  }
}

static void broker_lost(const char *service_name, void *context)
{
}

static void scope_monitor_error(const ScopeMonitorInfo *scope_info, int platform_error, void *context)
{
}

static void broker_registered(const BrokerDiscInfo *broker_info, const char *assigned_service_name, void *context)
{
}

static void broker_register_error(const BrokerDiscInfo *broker_info, int platform_error, void *context)
{
}

static void unpackAndParseIPAddress(const uint8_t *addrData, lwpa_iptype_t addrType, char *strBufOut, size_t strBufLen)
{
  LwpaIpAddr ip;

  ip.type = addrType;

  if (addrType == LWPA_IPV4)
  {
    ip.addr.v4 = upack_32b(addrData);
  }
  else if (addrType == LWPA_IPV6)
  {
    memcpy(ip.addr.v6, addrData, IPV6_BYTES);
  }

  lwpa_inet_ntop(&ip, strBufOut, strBufLen);
}

static void parseAndPackIPAddress(lwpa_iptype_t addrType, const char *ipString, size_t ipStringLen, uint8_t *outBuf)
{
  LwpaIpAddr ip;

  lwpa_inet_pton(addrType, ipString, &ip);

  if (addrType == LWPA_IPV4)
  {
    pack_32l(outBuf, ip.addr.v4);
  }
  else if (addrType == LWPA_IPV6)
  {
    memcpy(outBuf, ip.addr.v6, IPV6_BYTES);
  }
}

MyLog::MyLog(const std::string &file_name)
{
  file_.open(file_name.c_str(), std::fstream::out);

  params_.action = kLwpaLogCreateHumanReadableLog;
  params_.log_fn = LogCallback;
  params_.syslog_params.facility = LWPA_LOG_LOCAL1;
  params_.syslog_params.app_name[0] = '\0';
  params_.syslog_params.procid[0] = '\0';
  params_.syslog_params.hostname[0] = '\0';
  params_.log_mask = LWPA_LOG_UPTO(LWPA_LOG_DEBUG);
  params_.time_method = kLwpaLogUseTimeFn;
  params_.time_fn = TimeCallback;
  params_.context = this;
  lwpa_validate_log_params(&params_);

  Log(LWPA_LOG_INFO, "Starting RDMnet Controller...");
}

MyLog::~MyLog()
{
  file_.close();
}

void MyLog::Log(int pri, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  lwpa_vlog(&params_, pri, format, args);
  va_end(args);
}

void MyLog::LogFromCallback(const std::string &str)
{
  if (file_.is_open())
    file_ << str << std::endl;
}

void appendRowToItem(QStandardItem *parent, QStandardItem *child)
{
  if ((parent != NULL) && (child != NULL))
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
  T *parent = NULL;
  QStandardItem *current = child;

  while ((parent == NULL) && (current != NULL))
  {
    current = current->parent();

    if (current != NULL)
    {
      parent = dynamic_cast<T *>(current);
    }
  }

  return parent;
}

static void broker_connect_thread_func(void *arg)
{
  BrokerConnection *bc = static_cast<BrokerConnection *>(arg);
  if (bc)
    bc->runConnectStateMachine();
}

static void rdmnetdisc_tick_thread_func(void *arg)
{
  while (!g_ShuttingDown)
  {
    rdmnetdisc_tick(arg);
  }
}

bool BrokerConnection::initializeStaticConnectionInfo(const LwpaCid &cid, const LwpaUid &uid, MyLog *log)
{
  if (!static_info_initialized_)
  {
    local_cid_ = cid;
    local_uid_ = uid;

    static_info_initialized_ = true;

    log_ = log;

    return true;
  }

  return false;
}

BrokerConnection::BrokerConnection(std::string scope)
    : connected_(false)
    , using_mdns_(true)
    , scope_(QString::fromStdString(scope))
    , broker_item_(NULL)
    , sequence_(0)
    , connect_in_progress_(false)
{
  LwpaCid my_cid = getLocalCID();
  conn_ = rdmnet_new_connection(&my_cid);
  assert(conn_ >= 0);
}

BrokerConnection::BrokerConnection(std::string scope, const LwpaSockaddr &addr)
    : connected_(false)
    , using_mdns_(false)
    , scope_(QString::fromStdString(scope))
    , broker_addr_(addr)
    , broker_item_(NULL)
    , sequence_(0)
    , connect_in_progress_(false)
{
  LwpaCid my_cid = getLocalCID();
  conn_ = rdmnet_new_connection(&my_cid);
  assert(conn_ >= 0);
}

BrokerConnection::~BrokerConnection()
{
  if (connect_in_progress_)
  {
    connect_in_progress_ = false;
    rdmnet_destroy_connection(conn_);
    lwpa_thread_stop(&connect_thread_, 10000);
  }
  else
    rdmnet_destroy_connection(conn_);
}

const QString BrokerConnection::generateBrokerItemText()
{
  if (connected_ || !using_mdns_)
  {
    char addrString[LWPA_INET6_ADDRSTRLEN];
    lwpa_inet_ntop(&broker_addr_.ip, addrString, LWPA_INET6_ADDRSTRLEN);

    return QString("Broker for scope \"%1\" at %2:%3").arg(scope_, addrString, QString::number(broker_addr_.port));
  }

  return QString("Broker for scope \"%1\"").arg(scope_);
}

void BrokerConnection::connect(const BrokerDiscInfo *broker_info)
{
  if (broker_info->listen_addrs_count > 0)
  {
    broker_addr_ = broker_info->listen_addrs[0];
    connect();
  }
}

void BrokerConnection::connect()
{
  if (static_info_initialized_)
  {
    LwpaThreadParams tparams;
    tparams.platform_data = NULL;
    tparams.stack_size = LWPA_THREAD_DEFAULT_STACK;
    tparams.thread_name = "Broker Connect Thread";
    tparams.thread_priority = LWPA_THREAD_DEFAULT_PRIORITY;

    connect_in_progress_ = true;
    lwpa_thread_create(&connect_thread_, &tparams, &broker_connect_thread_func, this);
  }
}

void BrokerConnection::disconnect()
{
  if (connected_)
    rdmnet_disconnect(conn_, true, kRDMnetDisconnectUserReconfigure);
  wasDisconnected();
}

void BrokerConnection::wasDisconnected()
{
  connected_ = false;
  broker_item_->setText(generateBrokerItemText());
}

void BrokerConnection::runConnectStateMachine()
{
  ClientConnectMsg connect_data;
  connect_data.connect_flags = CONNECTFLAG_INCREMENTAL_UPDATES;
  connect_data.e133_version = E133_VERSION;
  QByteArray utf8_scope = scope_.toUtf8();
  connect_data.scope = utf8_scope.data();
  connect_data.search_domain = "local";
  create_rpt_client_entry(&local_cid_, &local_uid_, kRPTClientTypeController, nullptr, &connect_data.client_entry);

  RdmnetData result_data;
  lwpa_error_t connect_result = rdmnet_connect(conn_, &broker_addr_, &connect_data, &result_data);
  while (connect_result != LWPA_OK && connect_in_progress_)
  {
    if (log_->CanLog(LWPA_LOG_WARNING))
    {
      char addr_str[LWPA_INET6_ADDRSTRLEN];
      lwpa_inet_ntop(&broker_addr_.ip, addr_str, LWPA_INET6_ADDRSTRLEN);
      if (rdmnet_data_is_code(&result_data))
      {
        log_->Log(LWPA_LOG_WARNING,
                  "Connection to Broker at address %s:%d failed with error: "
                  "'%s' and additional RDMnet error code %d",
                  addr_str, broker_addr_.port, lwpa_strerror(connect_result), rdmnet_data_code(&result_data));
      }
      else
      {
        log_->Log(LWPA_LOG_WARNING, "Connection to Broker at address %s:%d failed with error: '%s'", addr_str,
                  broker_addr_.port, lwpa_strerror(connect_result));
      }
    }
    // rdmnet_connect() automatically handles the backoff timer for us.
    connect_result = rdmnet_connect(conn_, &broker_addr_, &connect_data, &result_data);
  }

  if (connect_result != LWPA_OK)
    return;

  send_fetch_client_list(conn_, &local_cid_);
  connected_ = true;
  connect_in_progress_ = false;
  broker_item_->setText(generateBrokerItemText());
}

void BrokerConnection::appendBrokerItemToTree(QStandardItem *invisibleRootItem, uint32_t connectionCookie)
{
  if ((broker_item_ == NULL) && static_info_initialized_)
  {
    broker_item_ = new BrokerItem(generateBrokerItemText(), connectionCookie);

    appendRowToItem(invisibleRootItem, broker_item_);

    broker_item_->enableChildrenSearch();
  }
}

bool BrokerConnection::isUsingMDNS()
{
  return using_mdns_;
}

void RDMnetNetworkModel::addScopeToMonitor(std::string scope)
{
  int platform_error;
  bool scopeAlreadyAdded = false;

  if (scope.length() > 0)
  {
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

      errorMessageBox.setText(tr("The broker for the scope \"%1\" has already been added to this tree. \
             Duplicates with the same scope cannot be added.")
                                  .arg(scope.c_str()));
      errorMessageBox.setIcon(QMessageBox::Icon::Critical);
      errorMessageBox.exec();
    }
    else
    {
      auto connection = std::make_unique<BrokerConnection>(scope);

      broker_connections_[broker_count_] = std::move(connection);
      broker_connections_[broker_count_]->appendBrokerItemToTree(invisibleRootItem(), broker_count_);

      ++broker_count_;

      memset(scope_info_.scope, '\0', E133_SCOPE_STRING_PADDED_LENGTH);
      memcpy(scope_info_.scope, scope.c_str(), min(scope.length(), E133_SCOPE_STRING_PADDED_LENGTH));

      rdmnetdisc_startmonitoring(&scope_info_, &platform_error, this);
    }
  }
}

void RDMnetNetworkModel::directChildrenRevealed(const QModelIndex &parentIndex)
{
  QStandardItem *item = itemFromIndex(parentIndex);

  if (item != NULL)
  {
    for (int i = 0; i < item->rowCount(); ++i)
    {
      QStandardItem *child = item->child(i);

      if (child != NULL)
      {
        if (child->type() == SearchingStatusItem::SearchingStatusItemType)
        {
          searchingItemRevealed(dynamic_cast<SearchingStatusItem *>(child));
        }
      }
    }
  }
}

void RDMnetNetworkModel::addBrokerByIP(std::string scope, const LwpaSockaddr &addr)
{
  bool brokerAlreadyAdded = false;

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
                                .arg(scope.c_str()));
    errorMessageBox.setIcon(QMessageBox::Icon::Critical);
    errorMessageBox.exec();
  }
  else
  {
    auto connection = std::make_unique<BrokerConnection>(scope, addr);
    int new_conn = connection->handle();
    broker_connections_[new_conn] = std::move(connection);
    broker_connections_[new_conn]->appendBrokerItemToTree(invisibleRootItem(), broker_count_);
    broker_connections_[new_conn]->connect();

    ++broker_count_;
  }
}

void RDMnetNetworkModel::processBrokerDisconnection(int conn)
{
  BrokerConnection *connection = broker_connections_[conn].get();

  if (connection->connected())
  {
    connection->disconnect();

    if (connection->treeBrokerItem() != NULL)
    {
      emit brokerItemTextUpdated(connection->treeBrokerItem());
    }

    connection->treeBrokerItem()->rdmnet_devices_.clear();
    connection->treeBrokerItem()->completelyRemoveChildren(0, connection->treeBrokerItem()->rowCount());
    connection->treeBrokerItem()->enableChildrenSearch();
  }

  if (!connection->isUsingMDNS())
  {
    connection->connect();
  }
}

void RDMnetNetworkModel::processAddRDMnetClients(BrokerItem *treeBrokerItem, const std::vector<ClientEntryData> &list)
{
  // Update the Controller's discovered list to match
  if (list.size() > 0)
  {
    treeBrokerItem->disableChildrenSearch();
  }

  for (const auto entry : list)
  {
    if (!is_rpt_client_entry(&entry))
      continue;

    bool is_me = (get_rpt_client_entry_data(&entry)->client_uid == BrokerConnection::getLocalUID());
    RDMnetClientItem *newRDMnetClientItem = new RDMnetClientItem(entry, is_me);
    bool itemAlreadyAdded = false;

    for (auto j = treeBrokerItem->rdmnet_devices_.begin();
         (j != treeBrokerItem->rdmnet_devices_.end()) && !itemAlreadyAdded; ++j)
    {
      if ((*j) != NULL)
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
      treeBrokerItem->rdmnet_devices_.push_back(newRDMnetClientItem);

      if (get_rpt_client_entry_data(&entry)->client_type == kRPTClientTypeDevice)
      {
        initializeRPTDeviceProperties(newRDMnetClientItem, get_rpt_client_entry_data(&entry)->client_uid.manu,
                                      get_rpt_client_entry_data(&entry)->client_uid.id);
      }

      if (!is_me)
        newRDMnetClientItem->enableChildrenSearch();
    }
  }
}

void RDMnetNetworkModel::processRemoveRDMnetClients(BrokerItem *treeBrokerItem,
                                                    const std::vector<ClientEntryData> &list)
{
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
          treeBrokerItem->rdmnet_devices_.erase(
              std::remove(treeBrokerItem->rdmnet_devices_.begin(), treeBrokerItem->rdmnet_devices_.end(), clientItem),
              treeBrokerItem->rdmnet_devices_.end());
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
  if (treeClientItem->childrenSearchRunning())
  {
    treeClientItem->disableChildrenSearch();

  //  // Add the Default Responder
  //  EndpointItem *def_resp_item = new EndpointItem(treeClientItem->Uid().manu, treeClientItem->Uid().id);
  //  appendRowToItem(treeClientItem, def_resp_item);
  //  treeClientItem->endpoints_.push_back(def_resp_item);
  //  std::vector<LwpaUid> responder_list;
  //  responder_list.push_back(treeClientItem->Uid());
  //  processNewResponderList(def_resp_item, responder_list);
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

void RDMnetNetworkModel::processNewResponderList(EndpointItem *treeEndpointItem, const std::vector<LwpaUid> &list)
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
  if (parent != NULL)
  {
    if (parent->isEnabled())
    {
      // Check if this property already exists before adding it. If it exists
      // already, then update the existing property.
      for (auto item : parent->properties)
      {
        if (item->getValueItem() != NULL)
        {
          if ((item->getFullName() == name) && (item->getValueItem()->getPID() == pid))
          {
            item->getValueItem()->setData(value, role);

            item->setEnabled(value.isValid());
            item->getValueItem()->setEnabled(value.isValid() ? PropertyValueItem::pidSupportsSet(pid) : false);

            return;
          }
        }
      }

      // Property doesn't exist, so make a new one.
      PropertyItem *propertyItem = createPropertyItem(parent, name);// new PropertyItem(name, name);
      PropertyValueItem *propertyValueItem;

      if (pid == E120_DMX_PERSONALITY)
      {
        propertyValueItem = new PersonalityPropertyValueItem(value, role, PropertyValueItem::pidSupportsSet(pid));
      }
      else
      {
        propertyValueItem = new PropertyValueItem(value, role, PropertyValueItem::pidSupportsSet(pid));
      }

      propertyValueItem->setPID(pid);

      //appendRowToItem(parent, propertyItem);
      propertyItem->setValueItem(propertyValueItem);

      parent->properties.push_back(propertyItem);

      propertyItem->setEnabled(value.isValid());
      propertyValueItem->setEnabled(value.isValid() ? PropertyValueItem::pidSupportsSet(pid) : false);
    }
  }
}

void RDMnetNetworkModel::processAddPropertyEntry(RDMnetNetworkItem *parent, unsigned short pid, const QString &name,
                                                 int role)
{
  processSetPropertyData(parent, pid, name, QVariant(), role);
}

void RDMnetNetworkModel::removeBroker(BrokerItem *brokerItem)
{
  uint32_t connectionCookie = brokerItem->getConnectionCookie();
  BrokerConnection *brokerConnection = broker_connections_[connectionCookie].get();
  bool removeComplete = false;

  brokerConnection->disconnect();
  broker_connections_.erase(connectionCookie);

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
}

void RDMnetNetworkModel::removeAllBrokers()
{
  for (auto &&broker_conn : broker_connections_)
    broker_conn.second->disconnect();

  broker_connections_.clear();

  for (int i = invisibleRootItem()->rowCount() - 1; i >= 0; --i)
  {
    BrokerItem *currentItem = dynamic_cast<BrokerItem *>(invisibleRootItem()->child(i));

    if (currentItem)
    {
      currentItem->completelyRemoveChildren(0, currentItem->rowCount());
    }
  }

  invisibleRootItem()->removeRows(0, invisibleRootItem()->rowCount());
}

void RDMnetNetworkModel::resetDevice(ResponderItem *device)
{
  if (device != NULL)
  {
    if (device->hasValidProperties()) // Means device wasn't reset
    {
      device->disableAllChildItems();
      device->setDeviceWasReset(true);
      device->setEnabled(false);

      emit resetDeviceSupportChanged(device);

      RdmCommand setCmd;
      int32_t maxBuffSize = PropertyValueItem::pidMaxBufferSize(E120_RESET_DEVICE);

      setCmd.dest_uid.manu = device->getMan();
      setCmd.dest_uid.id = device->getDev();
      setCmd.subdevice = 0;
      setCmd.command_class = E120_SET_COMMAND;
      setCmd.param_id = E120_RESET_DEVICE;
      setCmd.datalen = maxBuffSize;
      memset(setCmd.data, 0, maxBuffSize);

      setCmd.data[0] = 0xFF;  // Default to cold reset

      SendRDMCommand(setCmd);
    }
  }
}

void RDMnetNetworkModel::InitRDMnet()
{
  if (!rdmnet_initialized_)
  {
    rdmnet_initialized_ = true;

    rdmnet_init(log_.GetLogParams());
  }
}

void RDMnetNetworkModel::ShutdownRDMnet()
{
  if (rdmnet_initialized_)
  {
    rdmnet_deinit();

    rdmnet_initialized_ = false;
  }
}

RDMnetNetworkModel *RDMnetNetworkModel::makeRDMnetNetworkModel()
{
  RDMnetNetworkModel *model = new RDMnetNetworkModel;
  UUID uuid;

  LwpaCid my_cid;
  LwpaUid my_uid;

  model->InitRDMnet();
  model->StartRecvThread();

  UuidCreate(&uuid);
  memcpy(my_cid.data, &uuid, CID_BYTES);

  srand(timeGetTime());
  my_uid.manu = 0xe574;
  my_uid.id = rand() & 0xFFFFFFFF;

  BrokerConnection::initializeStaticConnectionInfo(my_cid, my_uid, &model->log_);

  // Use mDNS to discover the broker, an mDNS callback will do this connect
  RdmnetDiscCallbacks callbacks;
  callbacks.broker_found = &broker_found;
  callbacks.broker_lost = &broker_lost;
  callbacks.broker_registered = &broker_registered;
  callbacks.broker_register_error = &broker_register_error;
  callbacks.scope_monitor_error = &scope_monitor_error;

  rdmnetdisc_init(&callbacks);
  fill_default_scope_info(&model->scope_info_);

  LwpaThreadParams tparams;
  tparams.platform_data = NULL;
  tparams.stack_size = LWPA_THREAD_DEFAULT_STACK;
  tparams.thread_name = "RDMnet Discovery Tick Thread";
  tparams.thread_priority = LWPA_THREAD_DEFAULT_PRIORITY;

  lwpa_thread_create(&tick_thread_, &tparams, &rdmnetdisc_tick_thread_func, model);

  // Initialize GUI-supported PID information
  QString rdmGroupName("RDM");
  QString rdmNetGroupName("RDMnet");

  // E1.20
  PropertyValueItem::setPIDInfo(E120_SUPPORTED_PARAMETERS, true, false, QVariant::Type::Invalid, false);

  PropertyValueItem::setPIDInfo(E120_DEVICE_INFO, true, false, QVariant::Type::Invalid, Qt::EditRole);
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

  PropertyValueItem::setPIDInfo(E120_DEVICE_MODEL_DESCRIPTION, true, false, QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_MODEL_DESCRIPTION, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("Device Model Description")));

  PropertyValueItem::setPIDInfo(E120_MANUFACTURER_LABEL, true, false, QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_MANUFACTURER_LABEL, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("Manufacturer Label")));

  PropertyValueItem::setPIDInfo(E120_DEVICE_LABEL, true, true, QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DEVICE_LABEL, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("Device Label")));
  PropertyValueItem::setPIDMaxBufferSize(E120_DEVICE_LABEL, 32);

  PropertyValueItem::setPIDInfo(E120_SOFTWARE_VERSION_LABEL, true, false, QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_SOFTWARE_VERSION_LABEL, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("Software Label")));

  PropertyValueItem::setPIDInfo(E120_BOOT_SOFTWARE_VERSION_ID, true, false, QVariant::Type::Int);
  PropertyValueItem::addPIDPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_ID, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("Boot Software ID")));

  PropertyValueItem::setPIDInfo(E120_BOOT_SOFTWARE_VERSION_LABEL, true, false, QVariant::Type::String);
  PropertyValueItem::addPIDPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_LABEL, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("Boot Software Label")));

  PropertyValueItem::setPIDInfo(E120_DMX_START_ADDRESS, true, true, QVariant::Type::Int);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DMX_START_ADDRESS, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("DMX512 Start Address")));
  PropertyValueItem::setPIDNumericDomain(E120_DMX_START_ADDRESS, 1, 512);
  PropertyValueItem::setPIDMaxBufferSize(E120_DMX_START_ADDRESS, 2);

  PropertyValueItem::setPIDInfo(E120_IDENTIFY_DEVICE, true, true, QVariant::Type::Bool, Qt::CheckStateRole);
  PropertyValueItem::addPIDPropertyDisplayName(E120_IDENTIFY_DEVICE, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("Identify")));
  PropertyValueItem::setPIDMaxBufferSize(E120_IDENTIFY_DEVICE, 1);

  PropertyValueItem::setPIDInfo(E120_DMX_PERSONALITY, true, true, QVariant::Type::Char,
                                PersonalityPropertyValueItem::PersonalityNumberRole);
  PropertyValueItem::addPIDPropertyDisplayName(E120_DMX_PERSONALITY, 
    QString("%0\\%1").arg(rdmGroupName).arg(tr("DMX512 Personality")));
  PropertyValueItem::setPIDNumericDomain(E120_DMX_PERSONALITY, 1, 255);
  PropertyValueItem::setPIDMaxBufferSize(E120_DMX_PERSONALITY, 1);

  PropertyValueItem::setPIDInfo(E120_RESET_DEVICE, false, true, QVariant::Type::Char, false);
  PropertyValueItem::setPIDMaxBufferSize(E120_RESET_DEVICE, 1);

  // E1.33
  PropertyValueItem::setPIDInfo(E133_COMPONENT_SCOPE, true, true, QVariant::Type::String, Qt::EditRole,
                                kDevice);
  PropertyValueItem::addPIDPropertyDisplayName(E133_COMPONENT_SCOPE, 
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Component Scope")));
  PropertyValueItem::setPIDMaxBufferSize(E133_COMPONENT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH + 2);

  PropertyValueItem::setPIDInfo(E133_BROKER_STATIC_CONFIG_IPV4, true, true, QVariant::Type::Invalid, Qt::EditRole,
                                kDevice);
  PropertyValueItem::addPIDPropertyDisplayName(E133_BROKER_STATIC_CONFIG_IPV4,
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Broker IPv4 Address (Static Configuration)")));
  PropertyValueItem::addPIDPropertyDisplayName(E133_BROKER_STATIC_CONFIG_IPV4,
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Port Number (Static Configuration)")));
  PropertyValueItem::setPIDMaxBufferSize(E133_BROKER_STATIC_CONFIG_IPV4, 6);

  PropertyValueItem::setPIDInfo(E133_BROKER_STATIC_CONFIG_IPV6, true, false, QVariant::Type::Invalid, Qt::EditRole,
                                kDevice);
  PropertyValueItem::addPIDPropertyDisplayName(E133_BROKER_STATIC_CONFIG_IPV6,
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Broker IPv6 Address (Static Configuration)")));

  PropertyValueItem::setPIDInfo(E133_SEARCH_DOMAIN, true, true, QVariant::Type::String, Qt::EditRole, kDevice);
  PropertyValueItem::addPIDPropertyDisplayName(E133_SEARCH_DOMAIN, 
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Search Domain")));
  PropertyValueItem::setPIDMaxBufferSize(E133_SEARCH_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);

  PropertyValueItem::setPIDInfo(E133_TCP_COMMS_STATUS, true, false, QVariant::Type::Invalid, Qt::EditRole, kDevice);
  PropertyValueItem::addPIDPropertyDisplayName(E133_TCP_COMMS_STATUS, 
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Broker IPv4 Address (Current)")));
  PropertyValueItem::addPIDPropertyDisplayName(E133_TCP_COMMS_STATUS, 
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Broker IPv6 Address (Current)")));
  PropertyValueItem::addPIDPropertyDisplayName(E133_TCP_COMMS_STATUS, 
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Port Number (Current)")));
  PropertyValueItem::addPIDPropertyDisplayName(E133_TCP_COMMS_STATUS, 
    QString("%0\\%1").arg(rdmNetGroupName).arg(tr("Unhealthy TCP Events")));

  model->setColumnCount(2);
  model->setHeaderData(0, Qt::Orientation::Horizontal, tr("Property"));
  model->setHeaderData(1, Qt::Orientation::Horizontal, tr("Value"));

  model->addScopeToMonitor(E133_DEFAULT_SCOPE);

  qRegisterMetaType<std::vector<ClientEntryData>>("std::vector<ClientEntryData>");
  qRegisterMetaType<std::vector<std::pair<uint16_t, uint8_t>>>("std::vector<std::pair<uint16_t, uint8_t>>");
  qRegisterMetaType<std::vector<LwpaUid>>("std::vector<LwpaUid>");

  connect(model, SIGNAL(brokerDisconnection(int)), model, SLOT(processBrokerDisconnection(int)), Qt::AutoConnection);
  connect(model, SIGNAL(addRDMnetClients(BrokerItem *, const std::vector<ClientEntryData> &)), model,
          SLOT(processAddRDMnetClients(BrokerItem *, const std::vector<ClientEntryData> &)), Qt::AutoConnection);
  connect(model, SIGNAL(removeRDMnetClients(BrokerItem *, const std::vector<ClientEntryData> &)), model,
          SLOT(processRemoveRDMnetClients(BrokerItem *, const std::vector<ClientEntryData> &)), Qt::AutoConnection);
  connect(model, SIGNAL(newEndpointList(RDMnetClientItem *, const std::vector<std::pair<uint16_t, uint8_t>> &)), model,
          SLOT(processNewEndpointList(RDMnetClientItem *, const std::vector<std::pair<uint16_t, uint8_t>> &)),
          Qt::AutoConnection);
  connect(model, SIGNAL(newResponderList(EndpointItem *, const std::vector<LwpaUid> &)), model,
          SLOT(processNewResponderList(EndpointItem *, const std::vector<LwpaUid> &)), Qt::AutoConnection);
  connect(model, SIGNAL(setPropertyData(RDMnetNetworkItem *, unsigned short, const QString &, const QVariant &, int)),
          model,
          SLOT(processSetPropertyData(RDMnetNetworkItem *, unsigned short, const QString &, const QVariant &, int)),
          Qt::AutoConnection);
  connect(model, SIGNAL(addPropertyEntry(RDMnetNetworkItem *, unsigned short, const QString &, int)), model,
          SLOT(processAddPropertyEntry(RDMnetNetworkItem *, unsigned short, const QString &, int)), Qt::AutoConnection);

  return model;
}

RDMnetNetworkModel *RDMnetNetworkModel::makeTestModel()
{
  RDMnetNetworkModel *model = new RDMnetNetworkModel;

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

RDMnetNetworkModel::~RDMnetNetworkModel()
{
  g_ShuttingDown = true;

  for (auto &&connection : broker_connections_)
    connection.second->disconnect();

  lwpa_thread_stop(&tick_thread_, 10000);
  rdmnetdisc_deinit();

  StopRecvThread();
  broker_connections_.clear();
  ShutdownRDMnet();
}

void RDMnetNetworkModel::searchingItemRevealed(SearchingStatusItem *searchItem)
{
  if (searchItem != NULL)
  {
    if (!searchItem->wasSearchInitiated())
    {
      // A search item was likely just revealed in the tree, starting a search
      // process.
      QStandardItem *searchItemParent = searchItem->parent();

      if (searchItemParent != NULL)
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

            if (clientItem != NULL)
            {
              RdmCommand cmd;

              cmd.dest_uid.manu = clientItem->Uid().manu;
              cmd.dest_uid.id = clientItem->Uid().id;
              cmd.subdevice = 0;

              searchItem->setSearchInitiated(true);

              // Send command to get endpoint list
              cmd.command_class = E120_GET_COMMAND;
              cmd.param_id = E137_7_ENDPOINT_LIST;
              cmd.datalen = 0;

              SendRDMCommand(cmd);
            }

            break;
          }

          case EndpointItem::EndpointItemType:
          {
            EndpointItem *endpointItem = dynamic_cast<EndpointItem *>(searchItemParent);

            if (endpointItem != NULL)
            {
              // Ask for the devices on each endpoint
              RdmCommand cmd;

              cmd.dest_uid.manu = endpointItem->parent_uid_.manu;
              cmd.dest_uid.id = endpointItem->parent_uid_.id;
              cmd.subdevice = 0;

              searchItem->setSearchInitiated(true);

              // Send command to get endpoint devices
              cmd.command_class = E120_GET_COMMAND;
              cmd.param_id = E137_7_ENDPOINT_RESPONDERS;
              cmd.datalen = sizeof(uint16_t);
              pack_16b(cmd.data, endpointItem->endpoint_);

              SendRDMCommand(cmd);
            }

            break;
          }
        }
      }
    }
  }
}

bool RDMnetNetworkModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  QStandardItem *item = itemFromIndex(index);
  bool updateValue = true;
  QVariant newValue = value;

  if (item != NULL)
  {
    if (item->type() == PropertyValueItem::PropertyValueItemType)
    {
      PropertyValueItem *propertyValueItem = dynamic_cast<PropertyValueItem *>(item);
      RDMnetNetworkItem *parentItem = getNearestParentItemOfType<ResponderItem>(item);

      if (parentItem == NULL)
      {
        parentItem = getNearestParentItemOfType<RDMnetClientItem>(item);
      }

      if ((propertyValueItem != NULL) && (parentItem != NULL))
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
            int32_t maxBuffSize = PropertyValueItem::pidMaxBufferSize(pid);
            QString qstr;
            std::string stdstr;
            uint8_t *packPtr;
            PropertyValueItem *ipValueItem;
            PropertyValueItem *portValueItem;

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
              pack_16b(packPtr, 1); // Scope slot (default to 1)
              packPtr += 2;
            }

            switch (PropertyValueItem::pidDataType(pid))
            {
              case QVariant::Type::Int:
                switch (maxBuffSize - (packPtr - setCmd.data))
                {
                  case 2:
                    pack_16b(packPtr, value.toInt());
                    break;
                  case 4:
                    pack_32b(packPtr, value.toInt());
                    break;
                }
                break;
              case QVariant::Type::String:
                qstr = value.toString();
                qstr.truncate(maxBuffSize - (packPtr - setCmd.data));
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
                switch (pid)
                {
                case E133_BROKER_STATIC_CONFIG_IPV4:
                  ipValueItem = getSiblingValueItem(propertyValueItem, E133_BROKER_STATIC_CONFIG_IPV4, 0);
                  portValueItem = getSiblingValueItem(propertyValueItem, E133_BROKER_STATIC_CONFIG_IPV4, 1);

                  if (propertyValueItem == ipValueItem) // IP was changed - value contains IP
                  {
                    parseAndPackIPAddress(LWPA_IPV4, value.toString().toLatin1().constData(),
                      value.toString().length(), packPtr);
                    pack_16l(packPtr + 4, static_cast<uint16_t>(portValueItem->data().toInt()));
                  }
                  else // Port was changed - value contains port
                  {
                    parseAndPackIPAddress(LWPA_IPV4, ipValueItem->data().toString().toLatin1().constData(),
                      ipValueItem->data().toString().length(), packPtr);
                    pack_16l(packPtr + 4, static_cast<uint16_t>(value.toInt()));
                  }

                  break;
                default:
                  return false;
                }
            }

            SendRDMCommand(setCmd);

            if (pid == E120_DMX_PERSONALITY)
            {
              sendGetCommand(E120_DEVICE_INFO, parentItem->getMan(), parentItem->getDev());
            }
          }
        }
      }
    }
  }

  return updateValue ? QStandardItemModel::setData(index, newValue, role) : false;
}

static void broker_recv_thread_func(void *arg)
{
  RDMnetNetworkModel *nm = static_cast<RDMnetNetworkModel *>(arg);
  if (nm)
    nm->RecvThreadRun();
}

void RDMnetNetworkModel::StartRecvThread()
{
  LwpaThreadParams tparams;
  tparams.platform_data = NULL;
  tparams.stack_size = LWPA_THREAD_DEFAULT_STACK;
  tparams.thread_name = "RDMnet Receive Thread";
  tparams.thread_priority = LWPA_THREAD_DEFAULT_PRIORITY;

  recv_thread_run_ = true;
  lwpa_thread_create(&recv_thread_, &tparams, &broker_recv_thread_func, this);
}

void RDMnetNetworkModel::RecvThreadRun()
{
  while (recv_thread_run_)
  {
    RdmnetPoll *poll_arr = new RdmnetPoll[broker_connections_.size()];
    size_t poll_arr_size = 0;

    for (const auto &broker_conn : broker_connections_)
    {
      if (broker_conn.second->connected())
      {
        poll_arr[poll_arr_size].handle = broker_conn.second->handle();
        ++poll_arr_size;
      }
    }

    int poll_res = 0;
    if (poll_arr && poll_arr_size)
    {
      poll_res = rdmnet_poll(poll_arr, poll_arr_size, 200);

      if (poll_res > 0)
      {
        for (size_t i = 0; i < poll_arr_size && poll_res; ++i)
        {
          if (poll_arr[i].err == LWPA_OK)
          {
            RdmnetData data;
            lwpa_error_t res = rdmnet_recv(poll_arr[i].handle, &data);
            switch (res)
            {
              case LWPA_OK:
                ProcessMessage(poll_arr[i].handle, rdmnet_data_msg(&data));
                break;
              case LWPA_NODATA:
                break;
              default:
              {
                // TODO handle errors
                emit brokerDisconnection(poll_arr[i].handle);
              }
              break;
            }
            --poll_res;
          }
          else if (poll_arr[i].err != LWPA_NODATA)
          {
            // TODO handle error
            --poll_res;
          }
        }
      }
      else if (poll_res < 0 && poll_res != LWPA_TIMEDOUT)
      {
        log_.Log(LWPA_LOG_ERR, "Error calling rdmnet_poll(): '%s'", lwpa_strerror(poll_res));
      }
    }
    else
      lwpa_thread_sleep(200);

    delete[] poll_arr;
  }
}

void RDMnetNetworkModel::StopRecvThread()
{
  if (recv_thread_run_)
  {
    recv_thread_run_ = false;
    lwpa_thread_stop(&recv_thread_, 10000);
  }
}

void RDMnetNetworkModel::ProcessMessage(int conn, const RdmnetMessage *msg)
{
  switch (msg->vector)
  {
    case VECTOR_ROOT_RPT:
      ProcessRPTMessage(conn, msg);
      break;
    case VECTOR_ROOT_BROKER:
      ProcessBrokerMessage(conn, msg);
      break;
    default:
      // printf("Received ROOT vector= 0x%08x len= %u\n", vector,
      // vector_data_len);
      break;
  }
}

void RDMnetNetworkModel::ProcessRPTMessage(int conn, const RdmnetMessage *msg)
{
  const RptMessage *rptmsg = get_rpt_msg(msg);
  switch (rptmsg->vector)
  {
    case VECTOR_RPT_STATUS:
      ProcessRPTStatus(conn, &rptmsg->header, get_status_msg(rptmsg));
    case VECTOR_RPT_NOTIFICATION:
      ProcessRPTNotification(conn, &rptmsg->header, get_rdm_cmd_list(rptmsg));
    default:
      // printf("\nERROR Received Endpoint vector= 0x%04x                 len=
      // %u\n", vector, len);
      break;
  }
}

void RDMnetNetworkModel::ProcessBrokerMessage(int conn, const RdmnetMessage *msg)
{
  const BrokerMessage *broker_msg = get_broker_msg(msg);

  BrokerItem *treeBrokerItem = broker_connections_[conn]->treeBrokerItem();

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
        emit removeRDMnetClients(treeBrokerItem, list);
      else
        emit addRDMnetClients(treeBrokerItem, list);
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

void RDMnetNetworkModel::ProcessRPTStatus(int /*conn*/, const RptHeader * /*header*/, const RptStatusMsg *status)
{
  // This function has some work TODO. We should at least be logging things
  // here.

  log_.Log(LWPA_LOG_INFO, "Got RPT Status with code %d", status->status_code);
  switch (status->status_code)
  {
    case VECTOR_RPT_STATUS_RDM_TIMEOUT: /* See Section 8.5.3 */
      // printf("Endpoint Status 'RDM Timeout' size= %u\n", statusSize);
      break;
    case VECTOR_RPT_STATUS_RDM_INVALID_RESPONSE: /* An invalid response was
                                                    received from the E1.20
                                                    device. */
      // printf("Endpoint Status 'RDM Invalid Response' size= %u\n",
      // statusSize);
      break;
    case VECTOR_RPT_STATUS_UNKNOWN_RDM_UID: /* The E1.20 UID is not recognized
                                               as a UID associated with the
                                               endpoint. */
      // printf("Endpoint Status 'Unknown RDM UID' size= %u\n", statusSize);
      break;
    case VECTOR_RPT_STATUS_UNKNOWN_RPT_UID:
      // printf("Endpoint Status 'Unknown RDMnet UID' size= %u\n", statusSize);
      break;
    case VECTOR_RPT_STATUS_UNKNOWN_ENDPOINT: /* Endpoint Number is not defined
                                                or does not exist on the
                                                device. */
      // printf("Endpoint Status 'Unknown Endpoint' size= %u\n", statusSize);
      break;
    case VECTOR_RPT_STATUS_BROADCAST_COMPLETE: /* The gateway completed sending
                                                  the previous Broadcast
                                                  message out the RDM Endpoint.
                                                */
      // printf("Endpoint Status 'Broadcast Complete' size= %u\n", statusSize);
      break;
    case VECTOR_RPT_STATUS_UNKNOWN_VECTOR:
      // printf("Endpoint Status 'Unknown Vector' size= %u\n", statusSize);
      break;
    case VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS:
      // printf("Endpoint Status 'Invalid Command Class' size= %u\n",
      // statusSize);
      break;
    case VECTOR_RPT_STATUS_INVALID_MESSAGE:
      // printf("Endpoint Status 'Invalid Message' size= %u\n", statusSize);
      break;
    default:
      // printf("ERROR Endpoint Status: Bad Code 0x%04x size= %u\n",
      // statusCode, statusSize);
      break;
  }
}

void RDMnetNetworkModel::ProcessRPTNotification(int conn, const RptHeader * /*header*/, const RdmCmdList *cmd_list)
{
  // TODO handle partial list
  // TODO error checking

  bool is_first_message = true;
  bool have_command = false;
  RdmCommand command;
  std::vector<RdmResponse> response;

  for (RdmCmdListEntry *cmd_msg = cmd_list->list; cmd_msg; cmd_msg = cmd_msg->next)
  {
    uint8_t cmd_class = get_command_class(&cmd_msg->msg);
    if (is_first_message && (cmd_class == E120_GET_COMMAND || cmd_class == E120_SET_COMMAND))
    {
      rdmresp_unpack_command(&cmd_msg->msg, &command);
      have_command = true;
    }
    else
    {
      RdmResponse resp;
      if (LWPA_OK != rdmctl_unpack_response(&cmd_msg->msg, &resp))
        return;
      response.push_back(resp);
    }
    is_first_message = false;
  }

  ProcessRDMResponse(conn, have_command, command, response);
}

bool RDMnetNetworkModel::SendRDMCommand(const RdmCommand &cmd)
{
  RptHeader header;
  LwpaUid rpt_dest_uid = cmd.dest_uid;
  LwpaUid rdm_dest_uid = cmd.dest_uid;
  uint16_t dest_endpoint = 0;

  BrokerConnection *connectionToUse = NULL;

  // Find "dest_endpoint" for this cmd (if NOT found, then this is an E133
  // command for the management endpoint, 0)
  for (auto &brokerConnectionIter : broker_connections_)
  {
    if (brokerConnectionIter.second != NULL && brokerConnectionIter.second->connected())
    {
      BrokerItem *brokerItem = brokerConnectionIter.second->treeBrokerItem();

      if (brokerItem != NULL)
      {
        for (auto i : brokerItem->rdmnet_devices_)
        {
          if (i->Uid() == cmd.dest_uid)
          {
            connectionToUse = brokerConnectionIter.second.get();
            break;
          }

          for (auto j : i->endpoints_)
          {
            for (auto k : j->devices_)
            {
              if ((k->getMan() == cmd.dest_uid.manu) && (k->getDev() == cmd.dest_uid.id))
              {
                // This command is for an E120 device on this endpoint
                rpt_dest_uid = i->Uid();
                dest_endpoint = j->endpoint_;
                connectionToUse = brokerConnectionIter.second.get();
                break;
              }
            }
          }
        }
      }
    }
  }

  if (connectionToUse != NULL)
  {
    header.source_uid = BrokerConnection::getLocalUID();
    header.source_endpoint_id = 0;
    header.dest_uid = rpt_dest_uid;
    header.dest_endpoint_id = dest_endpoint;
    header.seqnum = connectionToUse->sequencePreIncrement();

    RdmCommand to_send = cmd;
    to_send.src_uid = header.source_uid;
    to_send.port_id = 1;
    to_send.transaction_num = static_cast<uint8_t>(header.seqnum & 0xffu);
    RdmBuffer rdmbuf;
    if (LWPA_OK != rdmctl_create_command(&to_send, &rdmbuf))
      return false;

    LwpaCid my_cid = BrokerConnection::getLocalCID();
    if (LWPA_OK != send_rpt_request(connectionToUse->handle(), &my_cid, &header, &rdmbuf))
      return false;
    return true;
  }

  return false;
}

void RDMnetNetworkModel::ProcessRDMResponse(int /*conn*/, bool have_command, const RdmCommand &cmd,
                                            const std::vector<RdmResponse> &response)
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
        nackReason = upack_16b(first_resp.data);
      nack(nackReason, &first_resp);
      return;
    }
    default:
      return;  // Unknown response type
  }

  if (first_resp.command_class == E120_GET_COMMAND_RESPONSE)
  {
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
            list.push_back(upack_16b(&resp_part.data[pos]));
        }

        if (!list.empty())
          commands(list, &first_resp);
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

          deviceInfo(upack_16b(&first_resp.data[0]), upack_16b(&first_resp.data[2]), upack_16b(&first_resp.data[4]),
                     upack_32b(&first_resp.data[6]), upack_16b(&first_resp.data[10]), cur_pers, total_pers,
                     upack_16b(&first_resp.data[14]), upack_16b(&first_resp.data[16]), first_resp.data[18],
                     &first_resp);
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

        // Ensure that the string is NULL terminated
        memset(label, 0, 33);
        // Max label length is 32
        memcpy(label, first_resp.data, (first_resp.datalen > 32) ? 32 : first_resp.datalen);

        switch (first_resp.param_id)
        {
          case E120_DEVICE_MODEL_DESCRIPTION:
            modelDesc(label, &first_resp);
            break;
          case E120_SOFTWARE_VERSION_LABEL:
            softwareLabel(label, &first_resp);
            break;
          case E120_MANUFACTURER_LABEL:
            manufacturerLabel(label, &first_resp);
            break;
          case E120_DEVICE_LABEL:
            deviceLabel(label, &first_resp);
            break;
          case E120_BOOT_SOFTWARE_VERSION_LABEL:
            bootSoftwareLabel(label, &first_resp);
            break;
        }
        break;
      }
      case E120_BOOT_SOFTWARE_VERSION_ID:
      {
        if (first_resp.datalen >= 4)
          bootSoftwareID(upack_32b(first_resp.data), &first_resp);
        break;
      }
      case E120_DMX_PERSONALITY:
      {
        if (first_resp.datalen >= 2)
          personality(first_resp.data[0], first_resp.data[1], &first_resp);
        break;
      }
      case E120_DMX_PERSONALITY_DESCRIPTION:
      {
        if (first_resp.datalen >= 3)
        {
          char description[33];
          uint8_t descriptionLength = first_resp.datalen - 3;

          memset(description, 0,
                 33);  // Ensure that the string is NULL terminated
          memcpy(description, &first_resp.data[3],
                 (descriptionLength > 32) ? 32 : descriptionLength);  // Max description length is 32

          personalityDescription(first_resp.data[0], upack_16b(&first_resp.data[1]), description, &first_resp);
        }
        break;
      }
      case E137_7_ENDPOINT_LIST:
      {
        bool is_first_message = true;
        uint32_t change_number = 0;
        std::vector<std::pair<uint16_t, uint8_t>> list;
        LwpaUid src_uid;

        for (auto resp_part : response)
        {
          size_t pos = 0;
          if (is_first_message)
          {
            if (resp_part.datalen < 4)
              break;
            src_uid = resp_part.src_uid;
            change_number = upack_32b(&resp_part.data[0]);
            pos = 4;
          }

          for (; pos + 2 < resp_part.datalen; pos += 3)
          {
            uint16_t endpoint_id = upack_16b(&resp_part.data[pos]);
            uint8_t endpoint_type = resp_part.data[pos + 2];
            list.push_back(std::make_pair(endpoint_id, endpoint_type));
          }
        }

        endpointList(change_number, list, src_uid);
      }
      break;
      case E137_7_ENDPOINT_RESPONDERS:
      {
        bool is_first_message = true;
        LwpaUid src_uid;
        std::vector<LwpaUid> list;
        uint16_t endpoint_id = 0;
        uint32_t change_number = 0;

        for (auto resp_part : response)
        {
          size_t pos = 0;
          if (is_first_message)
          {
            if (resp_part.datalen < 6)
              break;
            src_uid = resp_part.src_uid;
            endpoint_id = upack_16b(&resp_part.data[0]);
            change_number = upack_32b(&resp_part.data[2]);
            pos = 6;
          }

          for (; pos + 5 < resp_part.datalen; pos += 6)
          {
            LwpaUid device;
            device.manu = upack_16b(&resp_part.data[pos]);
            device.id = upack_32b(&resp_part.data[pos + 2]);
            list.push_back(device);
          }
        }

        endpointResponders(endpoint_id, change_number, list, src_uid);
      }
      break;
      case E137_7_ENDPOINT_LIST_CHANGE:
      {
        if (first_resp.datalen >= 4)
        {
          uint32_t change_number = upack_32b(first_resp.data);
          endpointListChange(change_number, first_resp.src_uid);
        }
        break;
      }
      case E137_7_ENDPOINT_RESPONDER_LIST_CHANGE:
      {
        if (first_resp.datalen >= 6)
        {
          uint16_t endpoint_id = upack_16b(first_resp.data);
          uint32_t change_num = upack_32b(&first_resp.data[2]);
          responderListChange(change_num, endpoint_id, first_resp.src_uid);
        }
        break;
      }
      case E133_TCP_COMMS_STATUS:
      {
        char scopeString[E133_SCOPE_STRING_PADDED_LENGTH];
        char v4AddrString[64];
        char v6AddrString[64];
        uint16_t port;
        uint16_t unhealthyTCPEvents;

        memset(scopeString, 0, E133_SCOPE_STRING_PADDED_LENGTH);
        memset(v4AddrString, 0, 64);
        memset(v6AddrString, 0, 64);

        memcpy(scopeString, first_resp.data, E133_SCOPE_STRING_PADDED_LENGTH);
        unpackAndParseIPAddress(first_resp.data + E133_SCOPE_STRING_PADDED_LENGTH, LWPA_IPV4, v4AddrString, 64);
        unpackAndParseIPAddress(first_resp.data + E133_SCOPE_STRING_PADDED_LENGTH + 4, LWPA_IPV6, v6AddrString, 64);
        port = upack_16b(first_resp.data + E133_SCOPE_STRING_PADDED_LENGTH + 4 + IPV6_BYTES);
        unhealthyTCPEvents = upack_16b(first_resp.data + E133_SCOPE_STRING_PADDED_LENGTH + 4 + IPV6_BYTES + 2);

        tcpCommsStatus(scopeString, v4AddrString, v6AddrString, port, unhealthyTCPEvents, &first_resp);

        break;
      }
      default:
      {
        // Process data for PIDs that support get and set, where the data has the same form in either case.
        ProcessRDMGetSetData(first_resp.param_id, first_resp.data, first_resp.datalen, &first_resp);
        break;
      }
    }
  }
  else if (first_resp.command_class == E120_SET_COMMAND_RESPONSE)
  {
    if (have_command)
    {
      // Make sure this Controller is up-to-date with data that was set on a Device.
      switch (first_resp.param_id)
      {
        case E120_DMX_PERSONALITY:
        {
          if (cmd.datalen >= 2)
            personality(cmd.data[0], 0, &first_resp);
          break;
        }
        default:
        {
          // Process PIDs with data that is in the same format for get and set.
          ProcessRDMGetSetData(first_resp.param_id, cmd.data, cmd.datalen, &first_resp);
          break;
        }
      }
    }
  }
}

void RDMnetNetworkModel::ProcessRDMGetSetData(uint16_t param_id, const uint8_t *data, uint8_t datalen,
                                              RdmResponse *resp)
{
  if ((data != NULL) && (resp != NULL))
  {
    switch (param_id)
    {
      case E120_DEVICE_LABEL:
      {
        char label[33];

        // Ensure that the string is NULL terminated
        memset(label, 0, 33);
        // Max label length is 32
        memcpy(label, data, (datalen > 32) ? 32 : datalen);

        deviceLabel(label, resp);
        break;
      }
      case E120_DMX_START_ADDRESS:
      {
        if (datalen >= 2)
        {
          address(upack_16b(data), resp);
        }
        break;
      }
      case E120_IDENTIFY_DEVICE:
      {
        if (datalen >= 1)
          identify(data[0], resp);
        break;
      }
      case E133_COMPONENT_SCOPE:
      {
        uint16_t scopeSlot;
        char scopeString[E133_SCOPE_STRING_PADDED_LENGTH];

        memset(scopeString, 0, E133_SCOPE_STRING_PADDED_LENGTH);

        scopeSlot = upack_16b(data);
        memcpy(scopeString, data + 2, E133_SCOPE_STRING_PADDED_LENGTH);

        componentScope(scopeSlot, scopeString, resp);

        break;
      }
      case E133_BROKER_STATIC_CONFIG_IPV4:
      {
        char addrString[64];
        uint16_t port;
        char scopeString[E133_SCOPE_STRING_PADDED_LENGTH];

        memset(addrString, 0, 64);
        memset(scopeString, 0, E133_SCOPE_STRING_PADDED_LENGTH);

        unpackAndParseIPAddress(data, LWPA_IPV4, addrString, 64);
        port = upack_16b(data + 4);
        memcpy(scopeString, data + 6, E133_SCOPE_STRING_PADDED_LENGTH);

        brokerStaticConfigIPv4(addrString, port, scopeString, resp);

        break;
      }
      case E133_BROKER_STATIC_CONFIG_IPV6:
      {
        char addrString[64];
        uint16_t port;
        char scopeString[E133_SCOPE_STRING_PADDED_LENGTH];

        memset(addrString, 0, 64);
        memset(scopeString, 0, E133_SCOPE_STRING_PADDED_LENGTH);

        unpackAndParseIPAddress(data, LWPA_IPV6, addrString, 64);
        port = upack_16b(data + IPV6_BYTES);
        memcpy(scopeString, data + IPV6_BYTES + 2, E133_SCOPE_STRING_PADDED_LENGTH);

        brokerStaticConfigIPv6(addrString, port, scopeString, resp);

        break;
      }
      case E133_SEARCH_DOMAIN:
      {
        char domainString[E133_DOMAIN_STRING_PADDED_LENGTH];

        memset(domainString, 0, E133_DOMAIN_STRING_PADDED_LENGTH);

        memcpy(domainString, data, datalen);

        searchDomain(domainString, resp);

        break;
      }
      default:
        break;
    }
  }
}

void RDMnetNetworkModel::endpointList(uint32_t /*changeNumber*/, const std::vector<std::pair<uint16_t, uint8_t>> &list,
                                      const LwpaUid &src_uid)
{
  for (auto &brokerConnectionIter : broker_connections_)
  {
    if (brokerConnectionIter.second != NULL && brokerConnectionIter.second->connected())
    {
      BrokerItem *brokerItem = brokerConnectionIter.second->treeBrokerItem();
      if (brokerItem != NULL)
      {
        for (auto i : brokerItem->rdmnet_devices_)
        {
          if (i->Uid() == src_uid)
          {
            // Found a matching discovered client
            emit newEndpointList(i, list);

            break;
          }
        }
      }
    }
  }

  // printf("*** Endpoint List: ***\n");
  // printf("\tChange Number= %u\n", changeNumber);
  // printf("\tContains=     ");

  // for (std::vector<std::pair<uint16_t, uint8_t>>::const_iterator i =
  // list.begin(); i != list.end(); i++) 	printf(" %u (%s)", i->first,
  // i->second
  //== VIRTUAL_ENDPOINT ? "virtual" : "physical"); if (sizeof(list) < 1)
  //	printf(" EMPTY");
  // printf("\n");
}

void RDMnetNetworkModel::endpointResponders(uint16_t endpoint, uint32_t /*changeNumber*/,
                                            const std::vector<LwpaUid> &list, const LwpaUid &src_uid)
{
  for (auto &brokerConnectionIter : broker_connections_)
  {
    if (brokerConnectionIter.second != NULL && brokerConnectionIter.second->connected())
    {
      BrokerItem *brokerItem = brokerConnectionIter.second->treeBrokerItem();
      if (brokerItem != NULL)
      {
        for (auto i : brokerItem->rdmnet_devices_)
        {
          if (i->Uid() == src_uid)
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

  // printf("*** Endpoint Devices: ***\n");
  // printf("\tEndpoint=      %u\n", endpoint);
  // printf("\tChange Number= %u\n", changeNumber);
  // printf("\tAge =          %u\n", age);
  // printf("\tContains=      (size= %u)\n", list.size());

  // for (std::vector<LwpaUid>::const_iterator i = list.begin(); i != list.end();
  // i++) 	printf("\t\tMan=0x%04x Dev= 0x%08x\n", i->manu, i->id);
  // printf("\n");
}

void RDMnetNetworkModel::endpointListChange(uint32_t /*changeNumber*/, const LwpaUid &src_uid)
{
  RdmCommand cmd;

  cmd.dest_uid = src_uid;
  cmd.subdevice = 0;
  cmd.command_class = E120_GET_COMMAND;
  cmd.param_id = E137_7_ENDPOINT_LIST;
  cmd.datalen = 0;

  SendRDMCommand(cmd);
}

void RDMnetNetworkModel::responderListChange(uint32_t /*changeNumber*/, uint16_t endpoint, const LwpaUid &src_uid)
{
  // Ask for the devices on each endpoint
  RdmCommand cmd;

  cmd.dest_uid = src_uid;
  cmd.subdevice = 0;
  cmd.command_class = E120_GET_COMMAND;
  cmd.param_id = E137_7_ENDPOINT_RESPONDERS;
  cmd.datalen = sizeof(uint16_t);
  pack_16b(cmd.data, endpoint);

  SendRDMCommand(cmd);
}

void RDMnetNetworkModel::nack(uint16_t reason, const RdmResponse * resp)
{
  if ((resp->command_class == E120_SET_COMMAND_RESPONSE) && (PropertyValueItem::pidInfoExists(resp->param_id)))
  {
    // Attempt to set a property failed. Get the original property value back.
    RdmCommand cmd;

    memset(cmd.data, 0, RDM_MAX_PDL);
    cmd.dest_uid.manu = resp->src_uid.manu;
    cmd.dest_uid.id = resp->src_uid.id;
    cmd.subdevice = 0;

    cmd.command_class = E120_GET_COMMAND;
    cmd.param_id = resp->param_id;

    if (cmd.param_id == E133_COMPONENT_SCOPE)
    {
      cmd.datalen = 2;
      pack_16b(cmd.data, 0x0001);  // Scope slot, default to 1 for RPT Devices (non-controllers, non-brokers).
    }
    else
    {
      cmd.datalen = 0;
    }

    SendRDMCommand(cmd);
  }
}

void RDMnetNetworkModel::status(uint8_t /*type*/, uint16_t /*messageId*/, uint16_t /*data1*/, uint16_t /*data2*/,
                                RdmResponse * /*resp*/)
{
}

void RDMnetNetworkModel::commands(std::vector<uint16_t> list, RdmResponse *resp)
{
  if (list.size() > 0)
  {
    // Get any properties that are supported
    RdmCommand getCmd;

    getCmd.dest_uid = resp->src_uid;
    getCmd.subdevice = 0;

    getCmd.command_class = E120_GET_COMMAND;
    getCmd.datalen = 0;

    for (uint32_t i = 0; i < list.size(); ++i)
    {
      if (pidSupportedByGUI(list[i], true) && list[i] != E120_SUPPORTED_PARAMETERS)
      {
        getCmd.param_id = list[i];
        SendRDMCommand(getCmd);
      }
      else if (list[i] == E120_RESET_DEVICE)
      {
        RDMnetNetworkItem *device = getNetworkItem(resp);

        if (device != NULL)
        {
          device->enableResetDevice();
          emit resetDeviceSupportChanged(device);
        }
      }
    }
  }
}

void RDMnetNetworkModel::deviceInfo(uint16_t protocolVersion, uint16_t modelId, uint16_t category, uint32_t swVersionId,
                                    uint16_t footprint, uint8_t personality, uint8_t totalPersonality, uint16_t address,
                                    uint16_t subdeviceCount, uint8_t sensorCount, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
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
    this->personality(personality, totalPersonality, resp);
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), address);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 5),
                         subdeviceCount);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 6),
                         (uint16_t)sensorCount);
  }
}

void RDMnetNetworkModel::modelDesc(const char *label, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_DEVICE_MODEL_DESCRIPTION,
                         PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_MODEL_DESCRIPTION), tr(label));
  }
}

void RDMnetNetworkModel::manufacturerLabel(const char *label, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_MANUFACTURER_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_MANUFACTURER_LABEL), tr(label));
  }
}

void RDMnetNetworkModel::deviceLabel(const char *label, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_DEVICE_LABEL, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_LABEL),
                         tr(label));
  }
}

void RDMnetNetworkModel::softwareLabel(const char *label, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_SOFTWARE_VERSION_LABEL), tr(label));
  }
}

void RDMnetNetworkModel::bootSoftwareID(uint32_t id, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_ID,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_ID), id);
  }
}

void RDMnetNetworkModel::bootSoftwareLabel(const char *label, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_LABEL), tr(label));
  }
}

void RDMnetNetworkModel::address(uint16_t address, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), address);
  }
}

void RDMnetNetworkModel::identify(bool enable, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_IDENTIFY_DEVICE, PropertyValueItem::pidPropertyDisplayName(E120_IDENTIFY_DEVICE),
                         enable ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
  }
}

void RDMnetNetworkModel::personality(uint8_t current, uint8_t number, RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);
  bool personalityChanged = false;

  if (device != NULL)
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

    personalityChanged =
        (current != static_cast<uint8_t>(getPropertyData(device, E120_DMX_PERSONALITY,
                                                         PersonalityPropertyValueItem::PersonalityNumberRole)
                                             .toInt()));

    if ((current != 0) && personalityChanged)
    {
      emit setPropertyData(device, E120_DMX_PERSONALITY,
                           PropertyValueItem::pidPropertyDisplayName(E120_DMX_PERSONALITY), (uint16_t)current,
                           PersonalityPropertyValueItem::PersonalityNumberRole);

      sendGetCommand(E120_DEVICE_INFO, resp->src_uid.manu, resp->src_uid.id);
    }

    checkPersonalityDescriptions(device, number, resp);
  }
}

void RDMnetNetworkModel::personalityDescription(uint8_t personality, uint16_t footprint, const char *description,
                                                RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(resp);
  const bool SHOW_FOOTPRINT = false;

  if (device != NULL)
  {
    device->personalityDescriptionFound(
        personality, footprint,
        SHOW_FOOTPRINT ? QString("(FP=%1) %2").arg(QString::number(footprint).rightJustified(2, '0'), description)
                       : description);

    if (device->allPersonalityDescriptionsFound())
    {
      QStringList personalityDescriptions = device->personalityDescriptionList();
      uint8_t currentPersonality =
          getPropertyData(device, E120_DMX_PERSONALITY, PersonalityPropertyValueItem::PersonalityNumberRole).toInt();

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
                           PersonalityPropertyValueItem::PersonalityDescriptionListRole);
    }
  }
}

void RDMnetNetworkModel::componentScope(uint16_t scopeSlot, const char *scopeString, RdmResponse *resp)
{
  RDMnetClientItem *client = getClientItem(resp);

  if (client != NULL)
  {
    emit setPropertyData(client, E133_COMPONENT_SCOPE,
                         PropertyValueItem::pidPropertyDisplayName(E133_COMPONENT_SCOPE, 0), scopeString);
  }
}

void RDMnetNetworkModel::brokerStaticConfigIPv4(const char *addrString, uint16_t port, const char *scopeString,
                                                RdmResponse *resp)
{
  RDMnetClientItem *client = getClientItem(resp);

  if (client != NULL)
  {
    emit setPropertyData(client, E133_BROKER_STATIC_CONFIG_IPV4,
                         PropertyValueItem::pidPropertyDisplayName(E133_BROKER_STATIC_CONFIG_IPV4, 0), addrString);
    emit setPropertyData(client, E133_BROKER_STATIC_CONFIG_IPV4,
                         PropertyValueItem::pidPropertyDisplayName(E133_BROKER_STATIC_CONFIG_IPV4, 1), port);
  }
}

void RDMnetNetworkModel::brokerStaticConfigIPv6(const char *addrString, uint16_t port, const char *scopeString,
                                                RdmResponse *resp)
{
  RDMnetClientItem *client = getClientItem(resp);

  if (client != NULL)
  {
    emit setPropertyData(client, E133_BROKER_STATIC_CONFIG_IPV6,
                         PropertyValueItem::pidPropertyDisplayName(E133_BROKER_STATIC_CONFIG_IPV6, 0), addrString);

    // Use v4 version here to assume port should be handled the same and use the same property
    emit setPropertyData(client, E133_BROKER_STATIC_CONFIG_IPV4,
                         PropertyValueItem::pidPropertyDisplayName(E133_BROKER_STATIC_CONFIG_IPV4, 1), port);
  }
}

void RDMnetNetworkModel::searchDomain(const char *domainNameString, RdmResponse *resp)
{
  RDMnetClientItem *client = getClientItem(resp);

  if (client != NULL)
  {
    emit setPropertyData(client, E133_SEARCH_DOMAIN, PropertyValueItem::pidPropertyDisplayName(E133_SEARCH_DOMAIN, 0),
                         domainNameString);
  }
}

void RDMnetNetworkModel::tcpCommsStatus(const char *scopeString, const char *v4AddrString, const char *v6AddrString,
                                        uint16_t port, uint16_t unhealthyTCPEvents, RdmResponse *resp)
{
  RDMnetClientItem *client = getClientItem(resp);

  if (client != NULL)
  {
    emit setPropertyData(client, E133_TCP_COMMS_STATUS,
                         PropertyValueItem::pidPropertyDisplayName(E133_TCP_COMMS_STATUS, 0), v4AddrString);
    emit setPropertyData(client, E133_TCP_COMMS_STATUS,
                         PropertyValueItem::pidPropertyDisplayName(E133_TCP_COMMS_STATUS, 1), v6AddrString);
    emit setPropertyData(client, E133_TCP_COMMS_STATUS,
                         PropertyValueItem::pidPropertyDisplayName(E133_TCP_COMMS_STATUS, 2), port);
    emit setPropertyData(client, E133_TCP_COMMS_STATUS,
                         PropertyValueItem::pidPropertyDisplayName(E133_TCP_COMMS_STATUS, 3), unhealthyTCPEvents);
  }
}

void RDMnetNetworkModel::addPropertyEntries(RDMnetNetworkItem *parent, PropertyLocation location)
{
  // Start out by adding all known properties and disabling them. Later on,
  // only the properties that the device supports will be enabled.
  for (PIDInfoIterator i = PropertyValueItem::pidsBegin(); i != PropertyValueItem::pidsEnd(); ++i)
  {
    if (i->second.includedInDataModel && ((i->second.locationOfProperties & location) == location))
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

  addPropertyEntries(parent, kResponder);

  // Now send requests for core required properties.
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = E120_GET_COMMAND;
  cmd.datalen = 0;

  cmd.param_id = E120_SUPPORTED_PARAMETERS;
  SendRDMCommand(cmd);
  cmd.param_id = E120_DEVICE_INFO;
  SendRDMCommand(cmd);
  cmd.param_id = E120_SOFTWARE_VERSION_LABEL;
  SendRDMCommand(cmd);
  cmd.param_id = E120_DMX_START_ADDRESS;
  SendRDMCommand(cmd);
  cmd.param_id = E120_IDENTIFY_DEVICE;
  SendRDMCommand(cmd);
}

void RDMnetNetworkModel::initializeRPTDeviceProperties(RDMnetClientItem *parent, uint16_t manuID, uint32_t deviceID)
{
  RdmCommand cmd;

  addPropertyEntries(parent, kDevice);

  // Now send requests for core required properties.
  memset(cmd.data, 0, RDM_MAX_PDL);
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = E120_GET_COMMAND;
  cmd.datalen = 0;

  cmd.param_id = E120_SUPPORTED_PARAMETERS;
  SendRDMCommand(cmd);
  cmd.param_id = E120_DEVICE_INFO;
  SendRDMCommand(cmd);
  cmd.param_id = E120_SOFTWARE_VERSION_LABEL;
  SendRDMCommand(cmd);
  cmd.param_id = E120_DMX_START_ADDRESS;
  SendRDMCommand(cmd);
  cmd.param_id = E120_IDENTIFY_DEVICE;
  SendRDMCommand(cmd);
  cmd.param_id = E133_BROKER_STATIC_CONFIG_IPV4;
  SendRDMCommand(cmd);
  cmd.param_id = E133_BROKER_STATIC_CONFIG_IPV6;
  SendRDMCommand(cmd);
  cmd.param_id = E133_SEARCH_DOMAIN;
  SendRDMCommand(cmd);
  cmd.param_id = E133_TCP_COMMS_STATUS;
  SendRDMCommand(cmd);

  cmd.datalen = 2;
  pack_16b(cmd.data, 0x0001);  // Scope slot, default to 1 for RPT Devices (non-controllers, non-brokers).
  cmd.param_id = E133_COMPONENT_SCOPE;
  SendRDMCommand(cmd);
}

void RDMnetNetworkModel::sendGetCommand(uint16_t pid, uint16_t manu, uint32_t dev)
{
  RdmCommand getCmd;

  getCmd.dest_uid.manu = manu;
  getCmd.dest_uid.id = dev;
  getCmd.subdevice = 0;

  getCmd.command_class = E120_GET_COMMAND;
  getCmd.param_id = pid;
  getCmd.datalen = 0;
  SendRDMCommand(getCmd);
}

bool RDMnetNetworkModel::pidSupportedByGUI(uint16_t pid, bool checkSupportGet)
{
  for (PIDInfoIterator iter = PropertyValueItem::pidsBegin(); iter != PropertyValueItem::pidsEnd(); ++iter)
  {
    if ((iter->first == pid) && (!checkSupportGet || iter->second.supportsGet))
    {
      return true;
    }
  }

  return false;
}

RDMnetClientItem *RDMnetNetworkModel::getClientItem(RdmResponse *resp)
{
  for (auto &brokerConnectionIter : broker_connections_)
  {
    if (brokerConnectionIter.second != NULL)
    {
      BrokerItem *brokerItem = brokerConnectionIter.second->treeBrokerItem();

      if (brokerItem != NULL)
      {
        for (auto i : brokerItem->rdmnet_devices_)
        {
          if ((i->getMan() == resp->src_uid.manu) &&
              (i->getDev() == resp->src_uid.id))
          {
            return i;
          }
        }
      }
    }
  }

  return NULL;
}

RDMnetNetworkItem *RDMnetNetworkModel::getNetworkItem(RdmResponse *resp)
{
  for (auto &brokerConnectionIter : broker_connections_)
  {
    if (brokerConnectionIter.second != NULL)
    {
      BrokerItem *brokerItem = brokerConnectionIter.second->treeBrokerItem();

      if (brokerItem != NULL)
      {
        for (auto i : brokerItem->rdmnet_devices_)
        {
          if ((i->getMan() == resp->src_uid.manu) && (i->getDev() == resp->src_uid.id))
          {
            return i;
          }

          for (auto j : i->endpoints_)
          {
            for (auto k : j->devices_)
            {
              if ((k->getMan() == resp->src_uid.manu) && (k->getDev() == resp->src_uid.id))
              {
                return k;
              }
            }
          }
        }
      }
    }
  }

  return NULL;
}

void RDMnetNetworkModel::checkPersonalityDescriptions(RDMnetNetworkItem *device, uint8_t numberOfPersonalities,
                                                      RdmResponse *resp)
{
  if (numberOfPersonalities > 0)
  {
    if (device->initiatePersonalityDescriptionSearch(numberOfPersonalities))
    {
      // Get descriptions for all supported personalities of this device
      RdmCommand getCmd;

      getCmd.dest_uid.manu = resp->src_uid.manu;
      getCmd.dest_uid.id = resp->src_uid.id;
      getCmd.subdevice = 0;
      getCmd.command_class = E120_GET_COMMAND;
      getCmd.param_id = E120_DMX_PERSONALITY_DESCRIPTION;
      getCmd.datalen = 1;
      for (uint8_t i = 1; i <= numberOfPersonalities; ++i)
      {
        getCmd.data[0] = i;
        SendRDMCommand(getCmd);
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
    if ((*iter)->getValueItem() != NULL)
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

PropertyItem * RDMnetNetworkModel::createPropertyItem(RDMnetNetworkItem * parent, const QString & fullName)
{
  RDMnetNetworkItem *currentParent = parent;
  QString currentPathName = fullName;
  QString shortName = getShortPropertyName(fullName);
  PropertyItem *propertyItem = new PropertyItem(fullName, shortName);

  while (currentPathName != shortName)
  {
    QString groupName = getHighestGroupName(currentPathName);

    RDMnetNetworkItem *groupingItem = getGroupingItem(currentParent, groupName);

    if (groupingItem == NULL)
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

QString RDMnetNetworkModel::getShortPropertyName(const QString & fullPropertyName)
{
  QRegExp re("(\\\\)");
  QStringList query = fullPropertyName.split(re);

  if (query.length() > 0)
  {
    return query.at(query.length() - 1);
  }

  return QString();
}

QString RDMnetNetworkModel::getHighestGroupName(const QString & pathName)
{
  QRegExp re("(\\\\)");
  QStringList query = pathName.split(re);

  if (query.length() > 0)
  {
    return query.at(0);
  }

  return QString();
}

PropertyItem * RDMnetNetworkModel::getGroupingItem(RDMnetNetworkItem * parent, const QString & groupName)
{
  for (int i = 0; i < parent->rowCount(); ++i)
  {
    PropertyItem *item = dynamic_cast<PropertyItem *>(parent->child(i));

    if (item != NULL)
    {
      if (item->text() == groupName)
      {
        return item;
      }
    }
  }

  return NULL;
}

PropertyItem * RDMnetNetworkModel::createGroupingItem(RDMnetNetworkItem * parent, const QString & groupName)
{
  PropertyItem *groupingItem = new PropertyItem(groupName, groupName);

  appendRowToItem(parent, groupingItem);
  groupingItem->setEnabled(true);

  return groupingItem;
}

QString RDMnetNetworkModel::getChildPathName(const QString & superPathName)
{
  QString highGroupName = getHighestGroupName(superPathName);
  int startPosition = highGroupName.length() + 1; // Name + delimiter character

  return superPathName.mid(startPosition, superPathName.length() - startPosition);;
}

PropertyValueItem * RDMnetNetworkModel::getSiblingValueItem(PropertyValueItem * item, uint16_t pid, int32_t index)
{
  PropertyItem *parent = dynamic_cast<PropertyItem *>(item->parent());
  QString siblingShortName = getShortPropertyName(PropertyValueItem::pidPropertyDisplayName(pid, index));

  if (parent)
  {
    for (auto item : parent->properties)
    {
      if ((item->text() == siblingShortName) && (item->getValueItem()->getPID() == pid))
      {
        return item->getValueItem();
      }
    }
  }

  return NULL;
}

RDMnetNetworkModel::RDMnetNetworkModel() : log_("RDMnetController.log")
{
}
