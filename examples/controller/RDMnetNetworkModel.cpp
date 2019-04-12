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
#include "rdm/responder.h"
#include "rdmnet/version.h"
#include "PropertyItem.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QDateTime>
#include <QTimeZone>
#include <QMessageBox>
END_INCLUDE_QT_HEADERS()

LwpaUuid BrokerConnection::local_cid_;
MyLog *BrokerConnection::log_ = NULL;
bool BrokerConnection::static_info_initialized_ = false;

bool RDMnetNetworkModel::rdmnet_initialized_ = false;

bool g_TestActive = false;
bool g_IgnoreEmptyStatus = true;
bool g_ShuttingDown = false;

lwpa_thread_t tick_thread_;

#define NUM_SUPPORTED_PIDS 14
static const uint16_t kSupportedPIDList[NUM_SUPPORTED_PIDS] = {
    E120_IDENTIFY_DEVICE,    E120_SUPPORTED_PARAMETERS,     E120_DEVICE_INFO,
    E120_MANUFACTURER_LABEL, E120_DEVICE_MODEL_DESCRIPTION, E120_SOFTWARE_VERSION_LABEL,
    E120_DEVICE_LABEL,       E133_COMPONENT_SCOPE,          E133_SEARCH_DOMAIN,
    E133_TCP_COMMS_STATUS,   E137_7_ENDPOINT_RESPONDERS,    E137_7_ENDPOINT_LIST,
};

/* clang-format off */
static const uint8_t kDeviceInfo[] = {
  0x01, 0x00, /* RDM Protocol version */
  0xe1, 0x33, /* Device Model ID */
  0xe1, 0x33, /* Product Category */

  /* Software Version ID */
  RDMNET_VERSION_MAJOR, RDMNET_VERSION_MINOR,
  RDMNET_VERSION_PATCH, RDMNET_VERSION_BUILD,

  0x00, 0x00, /* DMX512 Footprint */
  0x00, 0x00, /* DMX512 Personality */
  0xff, 0xff, /* DMX512 Start Address */
  0x00, 0x00, /* Sub-device count */
  0x00 /* Sensor count */
};
/* clang-format on */

#define MAX_SCOPE_SLOT_NUMBER 0xFFFF
#define MAX_RESPONSES_IN_ACK_OVERFLOW (MAX_SCOPE_SLOT_NUMBER + 1)
#define DEVICE_LABEL_MAX_LEN 32
#define DEFAULT_DEVICE_LABEL "My ETC RDMnet Controller"
#define SOFTWARE_VERSION_LABEL RDMNET_VERSION_STRING
#define MANUFACTURER_LABEL "ETC"
#define DEVICE_MODEL_DESCRIPTION "Prototype RDMnet Controller"

static void LogCallback(void *context, const char * /*syslog_str*/, const char *human_str, const char * /*raw_str*/)
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
  time->second = qtime.second();
  time->minute = qtime.minute();
  time->hour = qtime.hour();
  time->day = qdate.day();
  time->month = qdate.month();
  time->year = qdate.year();
  time->msec = qtime.msec();
  time->utc_offset = (QTimeZone::systemTimeZone().offsetFromUtc(now) / 60);
}

void broker_found(const char *scope, const BrokerDiscInfo *broker_info, void *context)
{
  RDMnetNetworkModel *model = static_cast<RDMnetNetworkModel *>(context);
  if (model)
  {
    model->connectToBroker(scope, broker_info);
  }
}

static void broker_lost(const char * /*service_name*/, void * /*context*/)
{
}

static void scope_monitor_error(const ScopeMonitorInfo * /*scope_info*/, int /*platform_error*/, void * /*context*/)
{
}

static void broker_registered(const BrokerDiscInfo * /*broker_info*/, const char * /*assigned_service_name*/,
                              void * /*context*/)
{
}

static void broker_register_error(const BrokerDiscInfo * /*broker_info*/, int /*platform_error*/, void * /*context*/)
{
}

static void unpackAndParseIPAddress(const uint8_t *addrData, lwpa_iptype_t addrType, char *strBufOut, size_t strBufLen)
{
  LwpaIpAddr ip;
  bool zeroedOut = false;

  ip.type = addrType;

  if (addrType == kLwpaIpTypeV4)
  {
    ip.addr.v4 = lwpa_upack_32b(addrData);
    zeroedOut = (ip.addr.v4 == 0);
  }
  else if (addrType == kLwpaIpTypeV6)
  {
    memcpy(ip.addr.v6, addrData, LWPA_IPV6_BYTES);

    zeroedOut = true;
    for (int i = 0; (i < LWPA_IPV6_BYTES) && zeroedOut; ++i)
    {
      zeroedOut = zeroedOut && (ip.addr.v6[i] == 0);
    }
  }

  if (!zeroedOut)
  {
    lwpa_inet_ntop(&ip, strBufOut, strBufLen);
  }
}

static lwpa_error_t parseAndPackIPAddress(lwpa_iptype_t addrType, const char *ipString, size_t /*ipStringLen*/,
                                          uint8_t *outBuf)
{
  LwpaIpAddr ip;

  lwpa_error_t result = lwpa_inet_pton(addrType, ipString, &ip);

  if (result == LWPA_OK)
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

MyLog::MyLog(const std::string &file_name) : file_name_(file_name)
{
  file_.open(file_name.c_str(), std::fstream::out);

  params_.action = kLwpaLogCreateHumanReadableLog;
  params_.log_fn = LogCallback;
  params_.syslog_params.facility = LWPA_LOG_LOCAL1;
  params_.syslog_params.app_name[0] = '\0';
  params_.syslog_params.procid[0] = '\0';
  params_.syslog_params.hostname[0] = '\0';
  params_.log_mask = LWPA_LOG_UPTO(LWPA_LOG_DEBUG);
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

  for (LogOutputStream *stream : customOutputStreams)
  {
    if (stream != NULL)
    {
      (*stream) << str << "\n";
    }
  }
}

void MyLog::addCustomOutputStream(LogOutputStream *stream)
{
  if (stream != NULL)
  {
    if (std::find(customOutputStreams.begin(), customOutputStreams.end(), stream) == customOutputStreams.end())
    {
      // Reinitialize the stream's contents to the log file's contents.
      stream->clear();

      std::ifstream ifs(file_name_, std::ifstream::in);

      std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

      (*stream) << str;

      ifs.close();

      customOutputStreams.push_back(stream);
    }
  }
}

void MyLog::removeCustomOutputStream(LogOutputStream *stream)
{
  for (int i = 0; i < customOutputStreams.size(); ++i)
  {
    if (customOutputStreams.at(i) == stream)
    {
      customOutputStreams.erase(customOutputStreams.begin() + i);
    }
  }
}

size_t MyLog::getNumberOfCustomLogOutputStreams()
{
  return customOutputStreams.size();
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
  (void)arg;
  while (!g_ShuttingDown)
  {
    rdmnetdisc_tick();
  }
}

bool BrokerConnection::initializeStaticConnectionInfo(const LwpaUuid &cid, MyLog *log)
{
  if (!static_info_initialized_)
  {
    local_cid_ = cid;

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
  LwpaUuid my_cid = getLocalCID();
  memset(&broker_addr_, 0, sizeof(LwpaSockaddr));
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
  LwpaUuid my_cid = getLocalCID();
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
    // Temporary - until we add IPv6 support
    lwpaip_set_invalid(&broker_addr_.ip);
    for (size_t i = 0; i < broker_info->listen_addrs_count; ++i)
    {
      if (lwpaip_is_v4(&broker_info->listen_addrs[i].ip))
      {
        broker_addr_ = broker_info->listen_addrs[i];
      }
    }
    if (!lwpaip_is_invalid(&broker_addr_.ip))
      connect();
  }
}

void BrokerConnection::connect()
{
  if (static_info_initialized_ && !connected_)
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
    rdmnet_disconnect(conn_, true, kRdmnetDisconnectUserReconfigure);
  wasDisconnected();
}

void BrokerConnection::wasDisconnected()
{
  connected_ = false;
  broker_item_->setText(generateBrokerItemText());
  broker_item_->setScope(scope_);
}

void BrokerConnection::runConnectStateMachine()
{
  ClientConnectMsg connect_data;
  connect_data.connect_flags = CONNECTFLAG_INCREMENTAL_UPDATES;
  connect_data.e133_version = E133_VERSION;
  client_connect_msg_set_scope(&connect_data, scope_.toUtf8().constData());
  client_connect_msg_set_default_search_domain(&connect_data);

  rdmnet_init_dynamic_uid_request(&local_uid_, 0x6574);
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
                  "Connection to Broker at address %s:%d failed with error: '%s' and additional RDMnet error code %d",
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

  local_uid_ = get_connect_reply_msg(get_broker_msg(rdmnet_data_msg(&result_data)))->client_uid;
  send_fetch_client_list(conn_, &local_cid_);
  connected_ = true;
  connect_in_progress_ = false;
  broker_item_->setText(generateBrokerItemText());
  broker_item_->setScope(scope_);

  RDMnetNetworkModel *model = dynamic_cast<RDMnetNetworkModel *>(broker_item_->model());

  if (model != NULL)
  {
    model->emitBrokerConnection(conn_);
  }
}

void BrokerConnection::appendBrokerItemToTree(QStandardItem *invisibleRootItem, uint32_t connectionCookie)
{
  if ((broker_item_ == NULL) && static_info_initialized_)
  {
    broker_item_ = new BrokerItem(generateBrokerItemText(), connectionCookie);
    broker_item_->setScope(scope_);

    appendRowToItem(invisibleRootItem, broker_item_);

    broker_item_->enableChildrenSearch();
  }
}

bool BrokerConnection::isUsingMDNS()
{
  return using_mdns_;
}

LwpaSockaddr BrokerConnection::currentSockAddr()
{
  return broker_addr_;
}

LwpaSockaddr BrokerConnection::staticSockAddr()
{
  if (using_mdns_)
  {
    LwpaSockaddr result;

    memset(&result, 0, sizeof(LwpaSockaddr));

    return result;
  }

  return broker_addr_;
}

void RDMnetNetworkModel::addScopeToMonitor(std::string scope)
{
  static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
  size_t num_responses;
  int platform_error;
  bool scopeAlreadyAdded = false;

  if (scope.length() > 0)
  {
    if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
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

        broker_connections_[broker_create_count_] = std::move(connection);
        broker_connections_[broker_create_count_]->appendBrokerItemToTree(invisibleRootItem(), broker_create_count_);

        emit expandNewItem(broker_connections_[broker_create_count_]->treeBrokerItem()->index(),
                           BrokerItem::BrokerItemType);

        ++broker_create_count_;

        memset(scope_info_.scope, '\0', E133_SCOPE_STRING_PADDED_LENGTH);
        memcpy(scope_info_.scope, scope.c_str(), min(scope.length(), E133_SCOPE_STRING_PADDED_LENGTH));

        rdmnetdisc_startmonitoring(&scope_info_, &platform_error, this);
      }

      lwpa_rwlock_writeunlock(&prop_lock);
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
  static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
  size_t num_responses;
  bool brokerAlreadyAdded = false;

  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
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

      broker_connections_[broker_create_count_] = std::move(connection);
      broker_connections_[broker_create_count_]->appendBrokerItemToTree(invisibleRootItem(), broker_create_count_);
      broker_connections_[broker_create_count_]->connect();

      emit expandNewItem(broker_connections_[broker_create_count_]->treeBrokerItem()->index(),
                         BrokerItem::BrokerItemType);

      ++broker_create_count_;
    }

    lwpa_rwlock_writeunlock(&prop_lock);
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
  log_.addCustomOutputStream(stream);
}

void RDMnetNetworkModel::removeCustomLogOutputStream(LogOutputStream *stream)
{
  log_.removeCustomOutputStream(stream);
}

void RDMnetNetworkModel::processBrokerConnection(uint16_t conn)
{
  if (broker_connections_.find(conn) != broker_connections_.end())
  {
    static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
    size_t num_responses;
    uint16_t nack_reason;

    // Broadcast GET_RESPONSE notification because of new connection
    if (getTCPCommsStatus(NULL, 0, resp_data_list, &num_responses, &nack_reason))
    {
      SendRDMGetResponses(kRdmnetControllerBroadcastUid, E133_BROADCAST_ENDPOINT, E133_TCP_COMMS_STATUS, resp_data_list,
                          num_responses, 0, 0);
    }
  }
}

void RDMnetNetworkModel::processBrokerDisconnection(uint16_t conn)
{
  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    BrokerConnection *connection = broker_connections_[conn].get();

    if (connection)
    {
      if (connection->connected())
      {
        connection->disconnect();

        if (connection->treeBrokerItem() != NULL)
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
        if (getTCPCommsStatus(NULL, 0, resp_data_list, &num_responses, &nack_reason))
        {
          SendRDMGetResponses(kRdmnetControllerBroadcastUid, E133_BROADCAST_ENDPOINT, E133_TCP_COMMS_STATUS, 
            resp_data_list, num_responses, 0, 0);
        }
      }

      if (!connection->isUsingMDNS())
      {
        connection->connect();
      }
    }

    lwpa_rwlock_writeunlock(&prop_lock);
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
  if (parent != NULL)
  {
    if (parent->isEnabled())
    {
      for (int i = parent->rowCount() - 1; i >= 0; --i)
      {
        PropertyItem *child = dynamic_cast<PropertyItem *>(parent->child(i, 0));
        PropertyValueItem *sibling = dynamic_cast<PropertyValueItem *>(parent->child(i, 1));

        if ((child != NULL) && (sibling != NULL))
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

    if (conn == NULL)
    {
      log_.Log(LWPA_LOG_ERR, "Error: Cannot find broker connection for clicked button.");
    }
    else
    {
      setCmd.dest_uid.manu = static_cast<uint16_t>(manuVariant.toUInt());
      setCmd.dest_uid.id = static_cast<uint32_t>(devVariant.toUInt());
      setCmd.subdevice = 0;
      setCmd.command_class = E120_SET_COMMAND;
      setCmd.param_id = E133_TCP_COMMS_STATUS;
      setCmd.datalen = maxBuffSize;
      memset(setCmd.data, 0, maxBuffSize);
      memcpy(setCmd.data, scopeData, min(scope.length(), maxBuffSize));

      SendRDMCommand(setCmd, conn->handle());
    }
  }
  else
  {
    log_.Log(LWPA_LOG_ERR, "Error: Button clicked on invalid property.");
  }
}

void RDMnetNetworkModel::removeBroker(BrokerItem *brokerItem)
{
  static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
  size_t num_responses;

  uint32_t connectionCookie = brokerItem->getConnectionCookie();
  bool removeComplete = false;

  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    BrokerConnection *brokerConnection = broker_connections_[connectionCookie].get();
    brokerConnection->disconnect();
    broker_connections_.erase(connectionCookie);

    lwpa_rwlock_writeunlock(&prop_lock);
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

  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    for (auto &&broker_conn : broker_connections_)
      broker_conn.second->disconnect();

    broker_connections_.clear();

    lwpa_rwlock_writeunlock(&prop_lock);
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
  if (device != NULL)
  {
    RdmCommand setCmd;

    setCmd.dest_uid.manu = device->getMan();
    setCmd.dest_uid.id = device->getDev();
    setCmd.subdevice = 0;
    setCmd.command_class = E120_SET_COMMAND;

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

  LwpaUuid my_cid;

  model->InitRDMnet();
  model->StartRecvThread();

  UuidCreate(&uuid);
  memcpy(my_cid.data, &uuid, LWPA_UUID_BYTES);

  BrokerConnection::initializeStaticConnectionInfo(my_cid, &model->log_);

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

  lwpa_thread_create(&tick_thread_, &tparams, &rdmnetdisc_tick_thread_func, nullptr);

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
  
  connect(model, SIGNAL(brokerConnection(uint16_t)), model, SLOT(processBrokerConnection(uint16_t)),
          Qt::AutoConnection);
  connect(model, SIGNAL(brokerDisconnection(uint16_t)), model, SLOT(processBrokerDisconnection(uint16_t)),
          Qt::AutoConnection);
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

              SendRDMCommand(cmd, getNearestParentItemOfType<BrokerItem>(clientItem));
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
  return log_.getNumberOfCustomLogOutputStreams();
}

void RDMnetNetworkModel::connectToBroker(const char * scope, const BrokerDiscInfo * broker_info)
{
  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    for (auto iter = broker_connections_.begin(); iter != broker_connections_.end(); ++iter)
    {
      if (iter->second->scope() == scope)
      {
        iter->second->connect(broker_info);
      }
    }

    lwpa_rwlock_writeunlock(&prop_lock);
  }
}

void RDMnetNetworkModel::emitBrokerConnection(uint16_t conn)
{
  emit brokerConnection(conn);
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
                  case 0: // scope
                    scopeString = value;
                    break;
                  case 1: // ipv4
                    newValue = ipv4String = value;
                    break;
                  case 2: // ipv6
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

                  if ((ipv4String.toString().length() > 0)
                      && ((displayNameIndex != 2) || (ipv6String.toString().length() == 0)))
                  {
                    *packPtr = E133_STATIC_CONFIG_IPV4;
                  }
                  else if ((ipv6String.toString().length() > 0)
                           && ((displayNameIndex != 1) || (ipv4String.toString().length() == 0)))
                  {
                    *packPtr = E133_STATIC_CONFIG_IPV6;
                    updateValue = false; // IPv6 is still in development, so make this read-only for now.
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

            updateValue = updateValue && (packPtr != NULL);

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
    RdmnetPoll *poll_arr = NULL;
    size_t poll_arr_size = 0;

    if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
    {
      poll_arr = new RdmnetPoll[broker_connections_.size()];

      for (const auto &broker_conn : broker_connections_)
      {
        if (broker_conn.second->connected())
        {
          poll_arr[poll_arr_size].handle = broker_conn.second->handle();
          ++poll_arr_size;
        }
      }

      lwpa_rwlock_readunlock(&prop_lock);
    }

    if (poll_arr && poll_arr_size)
    {
      int poll_res = rdmnet_poll(poll_arr, poll_arr_size, 200);

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

void RDMnetNetworkModel::ProcessMessage(uint16_t conn, const RdmnetMessage *msg)
{
  switch (msg->vector)
  {
    case ACN_VECTOR_ROOT_RPT:
      ProcessRPTMessage(conn, msg);
      break;
    case ACN_VECTOR_ROOT_BROKER:
      ProcessBrokerMessage(conn, msg);
      break;
    default:
      // printf("Received ROOT vector= 0x%08x len= %u\n", vector,
      // vector_data_len);
      break;
  }
}

void RDMnetNetworkModel::ProcessRPTMessage(uint16_t conn, const RdmnetMessage *msg)
{
  const RptMessage *rptmsg = get_rpt_msg(msg);
  switch (rptmsg->vector)
  {
    case VECTOR_RPT_REQUEST:
      ProcessRPTNotificationOrRequest(conn, &rptmsg->header, get_rdm_cmd_list(rptmsg));
      break;
    case VECTOR_RPT_STATUS:
      ProcessRPTStatus(conn, &rptmsg->header, get_rpt_status_msg(rptmsg));
    case VECTOR_RPT_NOTIFICATION:
      ProcessRPTNotificationOrRequest(conn, &rptmsg->header, get_rdm_cmd_list(rptmsg));
    default:
      // printf("\nERROR Received Endpoint vector= 0x%04x                 len=
      // %u\n", vector, len);
      break;
  }
}

void RDMnetNetworkModel::ProcessBrokerMessage(uint16_t conn, const RdmnetMessage *msg)
{
  const BrokerMessage *broker_msg = get_broker_msg(msg);

  BrokerConnection *brokerConn = NULL;

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    brokerConn = broker_connections_[conn].get();
    lwpa_rwlock_readunlock(&prop_lock);
  }

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

void RDMnetNetworkModel::ProcessRPTStatus(uint16_t /*conn*/, const RptHeader * /*header*/, const RptStatusMsg *status)
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

void RDMnetNetworkModel::ProcessRPTNotificationOrRequest(uint16_t conn, const RptHeader *header,
                                                         const RdmCmdList *cmd_list)
{
  // TODO handle partial list
  // TODO error checking

  bool is_first_message = true;
  bool have_command = false;
  RdmCommand command;
  std::vector<RdmResponse> response;

  for (RdmCmdListEntry *cmd_msg = cmd_list->list; cmd_msg; cmd_msg = cmd_msg->next)
  {
    uint8_t cmd_class = rdm_get_command_class(&cmd_msg->msg);
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

  if (response.empty() && have_command)
  {
    ProcessRDMCommand(conn, header, command);
  }
  else
  {
    ProcessRDMResponse(conn, have_command, command, response);
  }
}

bool RDMnetNetworkModel::SendRDMCommand(const RdmCommand &cmd, BrokerItem *brokerItem)
{
  if (brokerItem == NULL)
  {
    log_.Log(LWPA_LOG_ERR, "Error: SendRDMCommand called with invalid Broker item.");
    return false;
  }
  else
  {
    return SendRDMCommand(cmd, brokerItem->getConnectionCookie());
  }
}

bool RDMnetNetworkModel::SendRDMCommand(const RdmCommand &cmd, uint16_t conn)
{
  RptHeader header;
  RdmUid rpt_dest_uid = cmd.dest_uid;
  RdmUid rdm_dest_uid = cmd.dest_uid;
  uint16_t dest_endpoint = 0;

  if (broker_connections_.find(conn) == broker_connections_.end())
  {
    log_.Log(LWPA_LOG_ERR, "Error: SendRDMCommand called with invalid connection cookie.");
    return false;
  }

  BrokerConnection *connectionToUse = broker_connections_[conn].get();

  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    if (connectionToUse != NULL)
    {
      header.source_uid = connectionToUse->getLocalUID();
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
      {
        lwpa_rwlock_writeunlock(&prop_lock);
        return false;
      }

      LwpaUuid my_cid = BrokerConnection::getLocalCID();
      if (LWPA_OK != send_rpt_request(connectionToUse->handle(), &my_cid, &header, &rdmbuf))
      {
        lwpa_rwlock_writeunlock(&prop_lock);
        return false;
      }

      lwpa_rwlock_writeunlock(&prop_lock);
      return true;
    }

    lwpa_rwlock_writeunlock(&prop_lock);
  }

  return false;
}

void RDMnetNetworkModel::SendRDMGetResponses(const RdmUid &dest_uid, uint16_t dest_endpoint_id, uint16_t param_id,
                                             const RdmParamData *resp_data_list, size_t num_responses, uint32_t seqnum,
                                             uint8_t transaction_num, uint16_t conn)
{
  std::vector<RdmResponse> resp_list;
  RdmResponse resp_data;

  // The source UID will be added later, right before sending.
  resp_data.dest_uid = dest_uid;
  resp_data.transaction_num = transaction_num;
  resp_data.resp_type = num_responses > 1 ? E120_RESPONSE_TYPE_ACK_OVERFLOW : E120_RESPONSE_TYPE_ACK;
  resp_data.msg_count = 0;
  resp_data.subdevice = 0;
  resp_data.command_class = E120_GET_COMMAND_RESPONSE;
  resp_data.param_id = param_id;

  for (size_t i = 0; i < num_responses; ++i)
  {
    memcpy(resp_data.data, resp_data_list[i].data, resp_data_list[i].datalen);
    resp_data.datalen = resp_data_list[i].datalen;
    if (i == num_responses - 1)
    {
      resp_data.resp_type = E120_RESPONSE_TYPE_ACK;
    }
    resp_list.push_back(resp_data);
  }
  SendNotification(conn, dest_uid, dest_endpoint_id, seqnum, resp_list);
}

void RDMnetNetworkModel::SendNACK(uint16_t conn, const RptHeader *received_header, const RdmCommand *cmd_data, uint16_t nack_reason)
{
  std::vector<RdmResponse> resp_list;
  RdmResponse resp_data;

  resp_data.dest_uid = received_header->source_uid;
  resp_data.transaction_num = cmd_data->transaction_num;
  resp_data.resp_type = E120_RESPONSE_TYPE_NACK_REASON;
  resp_data.msg_count = 0;
  resp_data.subdevice = 0;
  resp_data.command_class = cmd_data->command_class + 1;
  resp_data.param_id = cmd_data->param_id;
  resp_data.datalen = 2;
  lwpa_pack_16b(resp_data.data, nack_reason);

  resp_list.push_back(resp_data);
  SendNotification(conn, received_header, resp_list);
}

void RDMnetNetworkModel::SendNotification(uint16_t conn, const RptHeader *received_header, 
                                          const std::vector<RdmResponse> &resp_list)
{
  SendNotification(conn, received_header->source_uid, received_header->source_endpoint_id, 
                   received_header->seqnum, resp_list);
}

void RDMnetNetworkModel::SendNotification(uint16_t conn, const RdmUid &dest_uid, uint16_t dest_endpoint_id, 
                                          uint32_t seqnum, const std::vector<RdmResponse> &resp_list)
{
  RptHeader header_to_send;
  int send_res = static_cast<int>(LWPA_OK);
  RdmUid rpt_dest_uid;
  uint16_t dest_endpoint;

  BrokerConnection *connectionToUse = NULL;

  header_to_send.dest_uid = dest_uid;
  header_to_send.dest_endpoint_id = dest_endpoint_id;
  header_to_send.source_endpoint_id = E133_NULL_ENDPOINT;
  header_to_send.seqnum = seqnum;

  LwpaUuid my_cid = BrokerConnection::getLocalCID();

  if ((dest_uid.manu != kRdmnetControllerBroadcastUid.manu) || (dest_uid.id != kRdmnetControllerBroadcastUid.id))
  {
    if (broker_connections_.find(conn) == broker_connections_.end())
    {
      log_.Log(LWPA_LOG_ERR, "Error: SendNotification called with invalid connection cookie.");
    }
    else
    {
      connectionToUse = broker_connections_[conn].get();
    }
  }

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    if ((dest_uid.manu == kRdmnetControllerBroadcastUid.manu) && (dest_uid.id == kRdmnetControllerBroadcastUid.id))
    {
      for (auto &brokerConnectionIter : broker_connections_)
      {
        connectionToUse = brokerConnectionIter.second.get();

        if (connectionToUse != NULL && connectionToUse->connected())
        {
          header_to_send.source_uid = connectionToUse->getLocalUID();
          send_res |= static_cast<int>(SendNotificationOnConnection(connectionToUse, header_to_send, resp_list));
        }
      }
    }
    else if (connectionToUse != NULL)
    {
      header_to_send.source_uid = connectionToUse->getLocalUID();
      send_res = static_cast<int>(SendNotificationOnConnection(connectionToUse, header_to_send, resp_list));
    }
    else
    {
      send_res = static_cast<int>(LWPA_NETERR);
    }

    lwpa_rwlock_readunlock(&prop_lock);
  }

  if (send_res != static_cast<int>(LWPA_OK))
  {
    log_.Log(LWPA_LOG_ERR, "Error sending RPT Notification message to Broker.");
  }
}

lwpa_error_t RDMnetNetworkModel::SendNotificationOnConnection(const BrokerConnection *connection,
                                                              const RptHeader &header,
                                                              const std::vector<RdmResponse> &resp_list)
{
  std::vector<RdmCmdListEntry> packed_resp_list;
  if (resp_list.size() > 1 && resp_list[0].resp_type == E120_RESPONSE_TYPE_ACK_OVERFLOW)
  {
    for (size_t i = 0; i < resp_list.size(); ++i)
    {
      RdmResponse resp = resp_list[i];
      RdmCmdListEntry packed_resp;

      resp.src_uid = connection->getLocalUID();
      rdmresp_create_response(&resp, &packed_resp.msg);
      if (i == resp_list.size() - 1)
        packed_resp.next = nullptr;
      packed_resp_list.push_back(packed_resp);
      if (i > 0)
        packed_resp_list[i - 1].next = &packed_resp_list[i];
    }
  }
  else
  {
    RdmResponse resp = resp_list[0];
    RdmCmdListEntry packed_resp;

    resp.src_uid = connection->getLocalUID();
    rdmresp_create_response(&resp, &packed_resp.msg);
    packed_resp.next = nullptr;
    packed_resp_list.push_back(packed_resp);
  }

  LwpaUuid my_cid = BrokerConnection::getLocalCID();
  return send_rpt_notification(connection->handle(), &my_cid, &header, &packed_resp_list[0]);
}

void RDMnetNetworkModel::ProcessRDMCommand(uint16_t conn, const RptHeader *header, const RdmCommand &cmd)
{
  bool should_nack = false;
  uint16_t nack_reason;

  if (cmd.command_class == E120_GET_COMMAND)
  {
    static RdmParamData resp_data_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
    static RdmCmdListEntry resp_list[MAX_RESPONSES_IN_ACK_OVERFLOW];
    size_t num_responses;

    if (DefaultResponderGet(cmd.param_id, cmd.data, cmd.datalen, resp_data_list, &num_responses, &nack_reason))
    {
      SendRDMGetResponses(header->source_uid, header->source_endpoint_id, cmd.param_id, resp_data_list, num_responses,
                          header->seqnum, cmd.transaction_num, conn);

      log_.Log(LWPA_LOG_DEBUG, "ACK'ing GET_COMMAND for PID 0x%04x from Controller %04x:%08x", cmd.param_id,
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
    SendNACK(conn, header, &cmd, nack_reason);
    log_.Log(LWPA_LOG_DEBUG, "Sending NACK to Controller %04x:%08x for supported PID 0x%04x with reason 0x%04x",
             header->source_uid.manu, header->source_uid.id, cmd.param_id, nack_reason);
  }
}

bool RDMnetNetworkModel::DefaultResponderGet(uint16_t pid, const uint8_t *param_data, uint8_t param_data_len,
                                             RdmParamData *resp_data_list, size_t *num_responses, uint16_t *nack_reason)
{
  bool res = false;

  switch (pid)
  {
    case E120_IDENTIFY_DEVICE:
      res = getIdentifyDevice(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E120_DEVICE_LABEL:
      res = getDeviceLabel(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E133_COMPONENT_SCOPE:
      res = getComponentScope(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E133_SEARCH_DOMAIN:
      res = getSearchDomain(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E133_TCP_COMMS_STATUS:
      res = getTCPCommsStatus(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E120_SUPPORTED_PARAMETERS:
      res = getSupportedParameters(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E120_DEVICE_INFO:
      res = getDeviceInfo(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E120_MANUFACTURER_LABEL:
      res = getManufacturerLabel(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E120_DEVICE_MODEL_DESCRIPTION:
      res = getDeviceModelDescription(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E120_SOFTWARE_VERSION_LABEL:
      res = getSoftwareVersionLabel(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E137_7_ENDPOINT_LIST:
      res = getEndpointList(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    case E137_7_ENDPOINT_RESPONDERS:
      res = getEndpointResponders(param_data, param_data_len, resp_data_list, num_responses, nack_reason);
      break;
    default:
      *nack_reason = E120_NR_UNKNOWN_PID;
      break;
  }

  return res;
}

void RDMnetNetworkModel::ProcessRDMResponse(uint16_t conn, bool have_command, const RdmCommand &cmd,
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
        nackReason = lwpa_upack_16b(first_resp.data);
      nack(conn, nackReason, &first_resp);
      return;
    }
    default:
      return;  // Unknown response type
  }

  if (first_resp.command_class == E120_GET_COMMAND_RESPONSE)
  {
    log_.Log(LWPA_LOG_INFO, "Got GET_COMMAND_RESPONSE with PID 0x%04x from Controller %04x:%08x", 
             first_resp.param_id, first_resp.src_uid.manu, first_resp.src_uid.id);

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

        // Ensure that the string is NULL terminated
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
                 33);  // Ensure that the string is NULL terminated
          memcpy(description, &first_resp.data[3],
                 (descriptionLength > 32) ? 32 : descriptionLength);  // Max description length is 32

          personalityDescription(conn, first_resp.data[0], lwpa_upack_16b(&first_resp.data[1]), description, &first_resp);
        }
        break;
      }
      case E137_7_ENDPOINT_LIST:
      {
        bool is_first_message = true;
        uint32_t change_number = 0;
        std::vector<std::pair<uint16_t, uint8_t>> list;
        RdmUid src_uid;

        for (auto resp_part : response)
        {
          size_t pos = 0;
          if (is_first_message)
          {
            if (resp_part.datalen < 4)
              break;
            src_uid = resp_part.src_uid;
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

        endpointList(conn, change_number, list, src_uid);
      }
      break;
      case E137_7_ENDPOINT_RESPONDERS:
      {
        bool is_first_message = true;
        RdmUid src_uid;
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
            src_uid = resp_part.src_uid;
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

        endpointResponders(conn, endpoint_id, change_number, list, src_uid);
      }
      break;
      case E137_7_ENDPOINT_LIST_CHANGE:
      {
        if (first_resp.datalen >= 4)
        {
          uint32_t change_number = lwpa_upack_32b(first_resp.data);
          endpointListChange(conn, change_number, first_resp.src_uid);
        }
        break;
      }
      case E137_7_ENDPOINT_RESPONDER_LIST_CHANGE:
      {
        if (first_resp.datalen >= 6)
        {
          uint16_t endpoint_id = lwpa_upack_16b(first_resp.data);
          uint32_t change_num = lwpa_upack_32b(&first_resp.data[2]);
          responderListChange(conn, change_num, endpoint_id, first_resp.src_uid);
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

        for (auto resp_part : response)
        {
          memset(scopeString, 0, E133_SCOPE_STRING_PADDED_LENGTH);
          memset(v4AddrString, 0, 64);
          memset(v6AddrString, 0, 64);

          memcpy(scopeString, resp_part.data, E133_SCOPE_STRING_PADDED_LENGTH);
          unpackAndParseIPAddress(resp_part.data + E133_SCOPE_STRING_PADDED_LENGTH, kLwpaIpTypeV4, v4AddrString, 64);
          unpackAndParseIPAddress(resp_part.data + E133_SCOPE_STRING_PADDED_LENGTH + 4, kLwpaIpTypeV6, v6AddrString,
                                  64);
          port = lwpa_upack_16b(resp_part.data + E133_SCOPE_STRING_PADDED_LENGTH + 4 + LWPA_IPV6_BYTES);
          unhealthyTCPEvents =
              lwpa_upack_16b(resp_part.data + E133_SCOPE_STRING_PADDED_LENGTH + 4 + LWPA_IPV6_BYTES + 2);

          tcpCommsStatus(conn, scopeString, v4AddrString, v6AddrString, port, unhealthyTCPEvents, &first_resp);
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
    log_.Log(LWPA_LOG_INFO, "Got SET_COMMAND_RESPONSE with PID %d", first_resp.param_id);

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

void RDMnetNetworkModel::ProcessRDMGetSetData(uint16_t conn, uint16_t param_id, const uint8_t *data, uint8_t datalen,
                                              const RdmResponse *firstResp)
{
  if ((data != NULL) && (firstResp != NULL))
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
            unpackAndParseIPAddress(cur_ptr, kLwpaIpTypeV4, addrBuf, LWPA_INET6_ADDRSTRLEN);
            cur_ptr += 4 + 16;
            port = lwpa_upack_16b(cur_ptr);
            staticConfigV4 = addrBuf;
            break;
          case E133_STATIC_CONFIG_IPV6:
            cur_ptr += 4;
            unpackAndParseIPAddress(cur_ptr, kLwpaIpTypeV6, addrBuf, LWPA_INET6_ADDRSTRLEN);
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

        searchDomain(conn, domainString, firstResp);

        break;
      }
      default:
        break;
    }
  }
}

BrokerConnection * RDMnetNetworkModel::getBrokerConnection(const std::string & scope)
{
  for (auto &brokerConnectionIter : broker_connections_)
  {
    if (brokerConnectionIter.second.get()->scope() == scope)
    {
      return brokerConnectionIter.second.get();
    }
  }

  return NULL;
}

bool RDMnetNetworkModel::getIdentifyDevice(const uint8_t *param_data, uint8_t param_data_len,
                                           RdmParamData *resp_data_list, size_t *num_responses, uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    resp_data_list[0].data[0] = prop_data_.identifying ? 1 : 0;
    lwpa_rwlock_readunlock(&prop_lock);
  }
  resp_data_list[0].datalen = 1;
  *num_responses = 1;
  return true;
}

bool RDMnetNetworkModel::getDeviceLabel(const uint8_t *param_data, uint8_t param_data_len, RdmParamData *resp_data_list,
                                        size_t *num_responses, uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    strncpy((char *)resp_data_list[0].data, prop_data_.device_label, DEVICE_LABEL_MAX_LEN);
    resp_data_list[0].datalen = (uint8_t)strnlen(prop_data_.device_label, DEVICE_LABEL_MAX_LEN);
    lwpa_rwlock_readunlock(&prop_lock);
  }
  *num_responses = 1;
  return true;
}

bool RDMnetNetworkModel::getComponentScope(const uint8_t *param_data, uint8_t param_data_len,
                                           RdmParamData *resp_data_list, size_t *num_responses, uint16_t *nack_reason)
{
  if (param_data_len >= 2)
  {
    return getComponentScope(lwpa_upack_16b(param_data), resp_data_list, num_responses, nack_reason);
  }
  else
  {
    *nack_reason = E120_NR_FORMAT_ERROR;
  }
  return false;
}

bool RDMnetNetworkModel::getComponentScope(uint16_t slot, RdmParamData *resp_data_list, size_t *num_responses,
                                           uint16_t *nack_reason)
{
  if (slot == 0)
  {
    if (nack_reason != NULL)
    {
      *nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }
  }
  else if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    auto connectionIter = broker_connections_.find(slot - 1);

    if (connectionIter == broker_connections_.end())
    {
      // Return the next highest Scope Slot that is not empty.
      connectionIter = broker_connections_.upper_bound(slot - 1);

      if (connectionIter != broker_connections_.end())
      {
        slot = connectionIter->first + 1;
      }
      else
      {
        slot = 0;
      }
    }

    if (slot != 0)
    {
      uint8_t *cur_ptr = resp_data_list[0].data;
      lwpa_pack_16b(cur_ptr, slot);
      cur_ptr += 2;

      std::string scope = connectionIter->second->scope();

      strncpy((char *)cur_ptr, scope.data(), E133_SCOPE_STRING_PADDED_LENGTH);
      cur_ptr[E133_SCOPE_STRING_PADDED_LENGTH - 1] = '\0';
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      LwpaSockaddr saddr = connectionIter->second->staticSockAddr();
      if (lwpaip_is_v4(&saddr.ip))
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV4;
        lwpa_pack_32b(cur_ptr, lwpaip_v4_address(&saddr.ip));
        cur_ptr += 4;
        /* Skip the IPv6 field */
        cur_ptr += 16;
        lwpa_pack_16b(cur_ptr, saddr.port);
        cur_ptr += 2;
      }
      else if (lwpaip_is_v6(&saddr.ip))
      {
        *cur_ptr++ = E133_STATIC_CONFIG_IPV6;
        /* Skip the IPv4 field */
        cur_ptr += 4;
        memcpy(cur_ptr, lwpaip_v6_address(&saddr.ip), LWPA_IPV6_BYTES);
        cur_ptr += LWPA_IPV6_BYTES;
        lwpa_pack_16b(cur_ptr, saddr.port);
        cur_ptr += 2;
      }
      else
      {
        *cur_ptr++ = E133_NO_STATIC_CONFIG;
        /* Skip the IPv4, IPv6 and port fields */
        cur_ptr += 4 + 16 + 2;
      }
      resp_data_list[0].datalen = static_cast<uint8_t>(cur_ptr - resp_data_list[0].data);
      *num_responses = 1;

      lwpa_rwlock_readunlock(&prop_lock);
      return true;
    }
    else if (nack_reason != NULL)
    {
      *nack_reason = E120_NR_DATA_OUT_OF_RANGE;
    }

    lwpa_rwlock_readunlock(&prop_lock);
  }

  return false;
}

bool RDMnetNetworkModel::getSearchDomain(const uint8_t *param_data, uint8_t param_data_len,
                                         RdmParamData *resp_data_list, size_t *num_responses, uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    strncpy((char *)resp_data_list[0].data, prop_data_.search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);
    resp_data_list[0].datalen = (uint8_t)strlen(prop_data_.search_domain);

    lwpa_rwlock_readunlock(&prop_lock);
  }
  *num_responses = 1;
  return true;
}

bool RDMnetNetworkModel::getTCPCommsStatus(const uint8_t *param_data, uint8_t param_data_len,
                                           RdmParamData *resp_data_list, size_t *num_responses, uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  int currentListIndex = 0;

  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    for (const auto &connection : broker_connections_)
    {
      uint8_t *cur_ptr = resp_data_list[currentListIndex].data;
      LwpaSockaddr saddr = connection.second->currentSockAddr();
      std::string scope = connection.second->scope();

      memset(cur_ptr, 0, E133_SCOPE_STRING_PADDED_LENGTH);
      memcpy(cur_ptr, scope.data(), min(scope.length(), E133_SCOPE_STRING_PADDED_LENGTH));
      cur_ptr += E133_SCOPE_STRING_PADDED_LENGTH;

      if (lwpaip_is_invalid(&saddr.ip) || !connection.second->connected())
      {
        lwpa_pack_32b(cur_ptr, 0);
        cur_ptr += 4;
        memset(cur_ptr, 0, LWPA_IPV6_BYTES);
        cur_ptr += LWPA_IPV6_BYTES;
      }
      else if (lwpaip_is_v4(&saddr.ip))
      {
        lwpa_pack_32b(cur_ptr, lwpaip_v4_address(&saddr.ip));
        cur_ptr += 4;
        memset(cur_ptr, 0, LWPA_IPV6_BYTES);
        cur_ptr += LWPA_IPV6_BYTES;
      }
      else  // IPv6
      {
        lwpa_pack_32b(cur_ptr, 0);
        cur_ptr += 4;
        memcpy(cur_ptr, lwpaip_v6_address(&saddr.ip), LWPA_IPV6_BYTES);
        cur_ptr += LWPA_IPV6_BYTES;
      }
      lwpa_pack_16b(cur_ptr, saddr.port);
      cur_ptr += 2;
      lwpa_pack_16b(cur_ptr, prop_data_.tcp_unhealthy_counter);
      cur_ptr += 2;
      resp_data_list[currentListIndex].datalen = (uint8_t)(cur_ptr - resp_data_list[currentListIndex].data);

      ++currentListIndex;

      if (currentListIndex == MAX_RESPONSES_IN_ACK_OVERFLOW)
      {
        break;
      }
    }

    lwpa_rwlock_readunlock(&prop_lock);
  }

  *num_responses = currentListIndex;
  return true;
}

bool RDMnetNetworkModel::getSupportedParameters(const uint8_t *param_data, uint8_t param_data_len,
                                                RdmParamData *resp_data_list, size_t *num_responses,
                                                uint16_t *nack_reason)
{
  size_t list_index = 0;
  uint8_t *cur_ptr = resp_data_list[0].data;
  size_t i;

  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  for (i = 0; i < NUM_SUPPORTED_PIDS; ++i)
  {
    lwpa_pack_16b(cur_ptr, kSupportedPIDList[i]);
    cur_ptr += 2;
    if ((cur_ptr - resp_data_list[list_index].data) >= RDM_MAX_PDL - 1)
    {
      resp_data_list[list_index].datalen = (uint8_t)(cur_ptr - resp_data_list[list_index].data);
      cur_ptr = resp_data_list[++list_index].data;
    }
  }
  resp_data_list[list_index].datalen = (uint8_t)(cur_ptr - resp_data_list[list_index].data);
  *num_responses = list_index + 1;
  return true;
}

bool RDMnetNetworkModel::getDeviceInfo(const uint8_t *param_data, uint8_t param_data_len, RdmParamData *resp_data_list,
                                       size_t *num_responses, uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;
  memcpy(resp_data_list[0].data, kDeviceInfo, sizeof kDeviceInfo);
  resp_data_list[0].datalen = sizeof kDeviceInfo;
  *num_responses = 1;
  return true;
}

bool RDMnetNetworkModel::getManufacturerLabel(const uint8_t *param_data, uint8_t param_data_len,
                                              RdmParamData *resp_data_list, size_t *num_responses,
                                              uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char *)resp_data_list[0].data, MANUFACTURER_LABEL);
  resp_data_list[0].datalen = sizeof(MANUFACTURER_LABEL) - 1;
  *num_responses = 1;
  return true;
}

bool RDMnetNetworkModel::getDeviceModelDescription(const uint8_t *param_data, uint8_t param_data_len,
                                                   RdmParamData *resp_data_list, size_t *num_responses,
                                                   uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char *)resp_data_list[0].data, DEVICE_MODEL_DESCRIPTION);
  resp_data_list[0].datalen = sizeof(DEVICE_MODEL_DESCRIPTION) - 1;
  *num_responses = 1;
  return true;
}

bool RDMnetNetworkModel::getSoftwareVersionLabel(const uint8_t *param_data, uint8_t param_data_len,
                                                 RdmParamData *resp_data_list, size_t *num_responses,
                                                 uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  strcpy((char *)resp_data_list[0].data, SOFTWARE_VERSION_LABEL);
  resp_data_list[0].datalen = sizeof(SOFTWARE_VERSION_LABEL) - 1;
  *num_responses = 1;
  return true;
}

bool RDMnetNetworkModel::getEndpointList(const uint8_t *param_data, uint8_t param_data_len,
                                         RdmParamData *resp_data_list, size_t *num_responses, uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)nack_reason;

  uint8_t *cur_ptr = resp_data_list[0].data;

  /* Hardcoded: no endpoints other than NULL_ENDPOINT. NULL_ENDPOINT is not
   * reported in this response. */
  resp_data_list[0].datalen = 4;
  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    lwpa_pack_32b(cur_ptr, prop_data_.endpoint_list_change_number);
    lwpa_rwlock_readunlock(&prop_lock);
  }
  *num_responses = 1;
  return true;
}

bool RDMnetNetworkModel::getEndpointResponders(const uint8_t *param_data, uint8_t param_data_len,
                                               RdmParamData *resp_data_list, size_t *num_responses,
                                               uint16_t *nack_reason)
{
  (void)param_data;
  (void)param_data_len;
  (void)resp_data_list;
  (void)num_responses;

  if (param_data_len >= 2)
  {
    /* We have no valid endpoints for this message */
    *nack_reason = E137_7_NR_ENDPOINT_NUMBER_INVALID;
  }
  else
    *nack_reason = E120_NR_FORMAT_ERROR;
  return false;
}

void RDMnetNetworkModel::endpointList(uint16_t conn, uint32_t /*changeNumber*/, 
                                      const std::vector<std::pair<uint16_t, uint8_t>> &list,
                                      const RdmUid &src_uid)
{
  if (broker_connections_.find(conn) == broker_connections_.end())
  {
    log_.Log(LWPA_LOG_ERR, "Error: endpointList called with invalid connection cookie.");
  }
  else
  {
    BrokerConnection *connection = broker_connections_[conn].get();

    if (connection != NULL && connection->connected())
    {
      BrokerItem *brokerItem = connection->treeBrokerItem();
      if (brokerItem != NULL)
      {
        for (auto i : brokerItem->rdmnet_clients_)
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
}

void RDMnetNetworkModel::endpointResponders(uint16_t conn, uint16_t endpoint, uint32_t /*changeNumber*/,
                                            const std::vector<RdmUid> &list, const RdmUid &src_uid)
{
  if (broker_connections_.find(conn) == broker_connections_.end())
  {
    log_.Log(LWPA_LOG_ERR, "Error: endpointResponders called with invalid connection cookie.");
  }
  else
  {
    BrokerConnection *connection = broker_connections_[conn].get();

    if (connection != NULL && connection->connected())
    {
      BrokerItem *brokerItem = connection->treeBrokerItem();
      if (brokerItem != NULL)
      {
        for (auto i : brokerItem->rdmnet_clients_)
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
}

void RDMnetNetworkModel::endpointListChange(uint16_t conn, uint32_t /*changeNumber*/, const RdmUid &src_uid)
{
  RdmCommand cmd;

  cmd.dest_uid = src_uid;
  cmd.subdevice = 0;
  cmd.command_class = E120_GET_COMMAND;
  cmd.param_id = E137_7_ENDPOINT_LIST;
  cmd.datalen = 0;

  SendRDMCommand(cmd, conn);
}

void RDMnetNetworkModel::responderListChange(uint16_t conn, uint32_t /*changeNumber*/, uint16_t endpoint, const RdmUid &src_uid)
{
  // Ask for the devices on each endpoint
  RdmCommand cmd;

  cmd.dest_uid = src_uid;
  cmd.subdevice = 0;
  cmd.command_class = E120_GET_COMMAND;
  cmd.param_id = E137_7_ENDPOINT_RESPONDERS;
  cmd.datalen = sizeof(uint16_t);
  lwpa_pack_16b(cmd.data, endpoint);

  SendRDMCommand(cmd, conn);
}

void RDMnetNetworkModel::nack(uint16_t conn, uint16_t reason, const RdmResponse *resp)
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
      lwpa_pack_16b(cmd.data, 0x0001);  // Scope slot, default to 1 for RPT Devices (non-controllers, non-brokers).
    }
    else
    {
      cmd.datalen = 0;
    }

    SendRDMCommand(cmd, conn);
  }
  else if ((resp->command_class == E120_GET_COMMAND_RESPONSE) && (resp->param_id == E133_COMPONENT_SCOPE) &&
           (reason == E120_NR_DATA_OUT_OF_RANGE))
  {
    RDMnetClientItem *client = getClientItem(conn, resp);
    RDMnetNetworkItem *rdmNetGroup = dynamic_cast<RDMnetNetworkItem *>(
        client->child(0)->data() == tr("RDMnet") ? client->child(0) : client->child(1));

    removeScopeSlotItemsInRange(rdmNetGroup, &client->properties, previous_slot_[resp->src_uid] + 1, 0xFFFF);

    // We have all of this controller's scope-slot pairs. Now request scope-specific properties.
    previous_slot_[resp->src_uid] = 0;
    sendGetControllerScopeProperties(conn, resp->src_uid.manu, resp->src_uid.id);
  }
}

void RDMnetNetworkModel::status(uint8_t /*type*/, uint16_t /*messageId*/, uint16_t /*data1*/, uint16_t /*data2*/,
                                const RdmResponse * /*resp*/)
{
}

void RDMnetNetworkModel::commands(uint16_t conn, std::vector<uint16_t> list, const RdmResponse *resp)
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
        SendRDMCommand(getCmd, conn);
      }
      else if (list[i] == E120_RESET_DEVICE)
      {
        RDMnetNetworkItem *device = getNetworkItem(conn, resp);

        if (device != NULL)
        {
          device->enableFeature(kResetDevice);
          emit featureSupportChanged(device, kResetDevice);
        }
      }
    }
  }
}

void RDMnetNetworkModel::deviceInfo(uint16_t conn, uint16_t protocolVersion, uint16_t modelId, uint16_t category, uint32_t swVersionId,
                                    uint16_t footprint, uint8_t personality, uint8_t totalPersonality, uint16_t address,
                                    uint16_t subdeviceCount, uint8_t sensorCount, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

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
    this->personality(conn, personality, totalPersonality, resp);
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), address);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 5),
                         subdeviceCount);
    emit setPropertyData(device, E120_DEVICE_INFO, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_INFO, 6),
                         (uint16_t)sensorCount);
  }
}

void RDMnetNetworkModel::modelDesc(uint16_t conn, const char *label, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_DEVICE_MODEL_DESCRIPTION,
                         PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_MODEL_DESCRIPTION), tr(label));
  }
}

void RDMnetNetworkModel::manufacturerLabel(uint16_t conn, const char *label, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_MANUFACTURER_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_MANUFACTURER_LABEL), tr(label));
  }
}

void RDMnetNetworkModel::deviceLabel(uint16_t conn, const char *label, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_DEVICE_LABEL, PropertyValueItem::pidPropertyDisplayName(E120_DEVICE_LABEL),
                         tr(label));
  }
}

void RDMnetNetworkModel::softwareLabel(uint16_t conn, const char *label, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_SOFTWARE_VERSION_LABEL), tr(label));
  }
}

void RDMnetNetworkModel::bootSoftwareID(uint16_t conn, uint32_t id, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_ID,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_ID), id);
  }
}

void RDMnetNetworkModel::bootSoftwareLabel(uint16_t conn, const char *label, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_BOOT_SOFTWARE_VERSION_LABEL,
                         PropertyValueItem::pidPropertyDisplayName(E120_BOOT_SOFTWARE_VERSION_LABEL), tr(label));
  }
}

void RDMnetNetworkModel::address(uint16_t conn, uint16_t address, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

  if (device != NULL)
  {
    emit setPropertyData(device, E120_DMX_START_ADDRESS,
                         PropertyValueItem::pidPropertyDisplayName(E120_DMX_START_ADDRESS), address);
  }
}

void RDMnetNetworkModel::identify(uint16_t conn, bool enable, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

  if (device != NULL)
  {
    device->setDeviceIdentifying(enable);
    emit identifyChanged(device, enable);
  }
}

void RDMnetNetworkModel::personality(uint16_t conn, uint8_t current, uint8_t number, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);

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

    bool personalityChanged =
        (current !=
         static_cast<uint8_t>(
             getPropertyData(device, E120_DMX_PERSONALITY, RDMnetNetworkItem::PersonalityNumberRole).toInt()));

    if ((current != 0) && personalityChanged)
    {
      emit setPropertyData(device, E120_DMX_PERSONALITY,
                           PropertyValueItem::pidPropertyDisplayName(E120_DMX_PERSONALITY), (uint16_t)current,
                           RDMnetNetworkItem::PersonalityNumberRole);

      sendGetCommand(getNearestParentItemOfType<BrokerItem>(device), 
                     E120_DEVICE_INFO, resp->src_uid.manu, resp->src_uid.id);
    }

    checkPersonalityDescriptions(device, number, resp);
  }
}

void RDMnetNetworkModel::personalityDescription(uint16_t conn, uint8_t personality, uint16_t footprint, 
                                                const char *description, const RdmResponse *resp)
{
  RDMnetNetworkItem *device = getNetworkItem(conn, resp);
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

void RDMnetNetworkModel::componentScope(uint16_t conn, uint16_t scopeSlot, const char *scopeString, const char *staticConfigV4,
                                        const char *staticConfigV6, uint16_t port, const RdmResponse *resp)
{
  RDMnetClientItem *client = getClientItem(conn, resp);

  if (client != NULL)
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

    if (staticConfigV4)
    {
      QString ipv4String = QString("%0:%1").arg(staticConfigV4).arg(port);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, ipv4String);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, QString(""));

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName,
                            ipv4String, RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, QString(""),
                            RDMnetNetworkItem::StaticIPv6DataRole);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName,
                            ipv4String, RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, QString(""),
                            RDMnetNetworkItem::StaticIPv6DataRole);

      emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName,
                            ipv4String, RDMnetNetworkItem::StaticIPv4DataRole);
      emit setPropertyData(client, E133_COMPONENT_SCOPE, displayName, QString(""),
                            RDMnetNetworkItem::StaticIPv6DataRole);
    }
    else if (staticConfigV6)
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
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, scopeString, 
                          RDMnetNetworkItem::ScopeDataRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, scopeString, 
                          RDMnetNetworkItem::ScopeDataRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV4PropName, scopeSlot, RDMnetNetworkItem::ScopeSlotRole);
    emit setPropertyData(client, E133_COMPONENT_SCOPE, staticV6PropName, scopeSlot, RDMnetNetworkItem::ScopeSlotRole);

    if (client->ClientType() == kRPTClientTypeController)
    {
      previous_slot_[client->Uid()] = scopeSlot;
      sendGetNextControllerScope(conn, resp->src_uid.manu, resp->src_uid.id, scopeSlot);
    }
  }
}

void RDMnetNetworkModel::searchDomain(uint16_t conn, const char *domainNameString, const RdmResponse *resp)
{
  RDMnetClientItem *client = getClientItem(conn, resp);

  if (client != NULL)
  {
    emit setPropertyData(client, E133_SEARCH_DOMAIN, PropertyValueItem::pidPropertyDisplayName(E133_SEARCH_DOMAIN, 0),
                         domainNameString);
  }
}

void RDMnetNetworkModel::tcpCommsStatus(uint16_t conn, const char *scopeString, const char *v4AddrString, 
                                        const char *v6AddrString,uint16_t port, uint16_t unhealthyTCPEvents, 
                                        const RdmResponse *resp)
{
  RDMnetClientItem *client = getClientItem(conn, resp);

  if (client != NULL)
  {
    if (client->getScopeSlot(scopeString) != 0)
    {
      QVariant callbackObjectVariant;
      const char *callbackSlotString = SLOT(processPropertyButtonClick(const QPersistentModelIndex &));
      QString callbackSlotQString(callbackSlotString);

      QString propertyName0 = getScopeSubPropertyFullName(client, E133_TCP_COMMS_STATUS, 0, scopeString);
      QString propertyName1 = getScopeSubPropertyFullName(client, E133_TCP_COMMS_STATUS, 1, scopeString);
      QString propertyName2 = getScopeSubPropertyFullName(client, E133_TCP_COMMS_STATUS, 2, scopeString);

      size_t v4AddrStrLen = strlen(v4AddrString);
      size_t v6AddrStrLen = strlen(v6AddrString);

      callbackObjectVariant.setValue(this);

      if ((v4AddrStrLen == 0) && (v6AddrStrLen == 0))
      {
        emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName0, QString(""));
      }
      else if (v4AddrStrLen == 0)  // use v6
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

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, tr(scopeString),
                           RDMnetNetworkItem::ScopeDataRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, callbackObjectVariant,
                           RDMnetNetworkItem::CallbackObjectRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, callbackSlotQString,
                           RDMnetNetworkItem::CallbackSlotRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, resp->src_uid.manu,
                           RDMnetNetworkItem::ClientManuRole);

      emit setPropertyData(client, E133_TCP_COMMS_STATUS, propertyName2, resp->src_uid.id,
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

  cmd.command_class = E120_GET_COMMAND;
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

  cmd.command_class = E120_GET_COMMAND;
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

void RDMnetNetworkModel::sendGetControllerScopeProperties(uint16_t conn, uint16_t manuID, uint32_t deviceID)
{
  RdmCommand cmd;

  memset(cmd.data, 0, RDM_MAX_PDL);
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = E120_GET_COMMAND;
  cmd.datalen = 0;

  cmd.param_id = E133_TCP_COMMS_STATUS;
  SendRDMCommand(cmd, conn);
}

void RDMnetNetworkModel::sendGetNextControllerScope(uint16_t conn, uint16_t manuID, uint32_t deviceID, uint16_t currentSlot)
{
  RdmCommand cmd;

  memset(cmd.data, 0, RDM_MAX_PDL);
  cmd.dest_uid.manu = manuID;
  cmd.dest_uid.id = deviceID;
  cmd.subdevice = 0;

  cmd.command_class = E120_GET_COMMAND;
  cmd.datalen = 2;

  lwpa_pack_16b(cmd.data, min(currentSlot + 1, MAX_SCOPE_SLOT_NUMBER));  // Scope slot, start with #1
  cmd.param_id = E133_COMPONENT_SCOPE;
  SendRDMCommand(cmd, conn);
}

void RDMnetNetworkModel::sendGetCommand(BrokerItem *brokerItem, uint16_t pid, uint16_t manu, uint32_t dev)
{
  RdmCommand getCmd;

  getCmd.dest_uid.manu = manu;
  getCmd.dest_uid.id = dev;
  getCmd.subdevice = 0;

  getCmd.command_class = E120_GET_COMMAND;
  getCmd.param_id = pid;
  getCmd.datalen = 0;
  SendRDMCommand(getCmd, brokerItem);
}

uint8_t *RDMnetNetworkModel::packIPAddressItem(const QVariant &value, lwpa_iptype_t addrType, uint8_t *packPtr, bool packPort)
{
  char ipStrBuffer[64];
  unsigned int portNumber;
  size_t memSize = ((addrType == kLwpaIpTypeV4) ? 4 : (LWPA_IPV6_BYTES)) + (packPort ? 2 : 0);

  if (packPtr == NULL)
  {
    return NULL;
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
    return NULL;
  }
  else if (parseAndPackIPAddress(addrType, ipStrBuffer, strlen(ipStrBuffer), packPtr) != LWPA_OK)
  {
    return NULL;
  }
  else if (portNumber > 65535)
  {
    return NULL;
  }
  else if(packPort)
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

RDMnetClientItem *RDMnetNetworkModel::getClientItem(uint16_t conn, const RdmResponse *resp)
{
  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    if (broker_connections_.find(conn) == broker_connections_.end())
    {
      log_.Log(LWPA_LOG_ERR, "Error: getClientItem called with invalid connection cookie.");
    }
    else
    {
      BrokerConnection *connection = broker_connections_[conn].get();

      if (connection != NULL)
      {
        BrokerItem *brokerItem = connection->treeBrokerItem();

        if (brokerItem != NULL)
        {
          for (auto i : brokerItem->rdmnet_clients_)
          {
            if ((i->getMan() == resp->src_uid.manu) && (i->getDev() == resp->src_uid.id))
            {
              lwpa_rwlock_readunlock(&prop_lock);
              return i;
            }
          }
        }
      }
    }

    lwpa_rwlock_readunlock(&prop_lock);
  }

  return NULL;
}

RDMnetNetworkItem *RDMnetNetworkModel::getNetworkItem(uint16_t conn, const RdmResponse *resp)
{
  if (lwpa_rwlock_readlock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    if (broker_connections_.find(conn) == broker_connections_.end())
    {
      log_.Log(LWPA_LOG_ERR, "Error: getNetworkItem called with invalid connection cookie.");
    }
    else
    {
      BrokerConnection *connection = broker_connections_[conn].get();

      if (connection != NULL)
      {
        BrokerItem *brokerItem = connection->treeBrokerItem();

        if (brokerItem != NULL)
        {
          for (auto i : brokerItem->rdmnet_clients_)
          {
            if ((i->getMan() == resp->src_uid.manu) && (i->getDev() == resp->src_uid.id))
            {
              lwpa_rwlock_readunlock(&prop_lock);
              return i;
            }

            for (auto j : i->endpoints_)
            {
              for (auto k : j->devices_)
              {
                if ((k->getMan() == resp->src_uid.manu) && (k->getDev() == resp->src_uid.id))
                {
                  lwpa_rwlock_readunlock(&prop_lock);
                  return k;
                }
              }
            }
          }
        }
      }
    }

    lwpa_rwlock_readunlock(&prop_lock);
  }

  return NULL;
}

void RDMnetNetworkModel::checkPersonalityDescriptions(RDMnetNetworkItem *device, uint8_t numberOfPersonalities,
                                                      const RdmResponse *resp)
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
                                                        const char *scope)
{
  QString original = PropertyValueItem::pidPropertyDisplayName(pid, index);

  if (client != NULL)
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

RDMnetNetworkModel::RDMnetNetworkModel() : log_("RDMnetController.log")
{
  const char *labelStr = DEFAULT_DEVICE_LABEL;
  const char *domainStr = ".local";

  lwpa_rwlock_create(&prop_lock);

  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    memset(&prop_data_, 0, sizeof(DefaultResponderPropertyData));
    memcpy(prop_data_.device_label, labelStr, strlen(labelStr));
    memcpy(prop_data_.search_domain, domainStr, strlen(domainStr));

    lwpa_rwlock_writeunlock(&prop_lock);
  }
}

RDMnetNetworkModel::~RDMnetNetworkModel()
{
  g_ShuttingDown = true;

  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    for (auto &&connection : broker_connections_)
      connection.second->disconnect();

    lwpa_rwlock_writeunlock(&prop_lock);
  }

  lwpa_thread_stop(&tick_thread_, 10000);
  rdmnetdisc_deinit();

  StopRecvThread();

  if (lwpa_rwlock_writelock(&prop_lock, LWPA_WAIT_FOREVER))
  {
    broker_connections_.clear();
    lwpa_rwlock_writeunlock(&prop_lock);
  }

  ShutdownRDMnet();

  lwpa_rwlock_destroy(&prop_lock);
}