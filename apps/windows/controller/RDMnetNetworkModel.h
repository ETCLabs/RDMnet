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

#include <fstream>
#include <map>
#include <memory>
#include <QStandardItemModel>
#include "lwpa_log.h"
#include "lwpa_thread.h"
#include "lwpa_cid.h"
#include "lwpa_uid.h"
#include "rdmnet/connection.h"
#include "rdmnet/rdmcontroller.h"
#include "BrokerItem.h"
#include "SearchingStatusItem.h"
#include "rdmnet/discovery.h"
#include "PropertyValueItem.h"

class BrokerConnection;

void appendRowToItem(QStandardItem *parent, QStandardItem *child);

class MyLog
{
public:
  MyLog(const std::string &file_name);
  virtual ~MyLog();

  void Log(int pri, const char *format, ...);
  bool CanLog(int pri) const { return lwpa_canlog(&params_, pri); }
  const LwpaLogParams *GetLogParams() const { return &params_; }

  void LogFromCallback(const std::string &str);

protected:
  std::fstream file_;
  LwpaLogParams params_;
};

class BrokerConnection
{
private:
  static LwpaCid local_cid_;
  static LwpaUid local_uid_;

  static MyLog *log_;

  static bool static_info_initialized_;

  bool connected_;
  bool using_mdns_;

  int conn_;
  QString scope_;
  LwpaSockaddr broker_addr_;

  BrokerItem *broker_item_;

  uint32_t sequence_;  // For RDM_AppCallbacks_Specific_C functions

  lwpa_thread_t connect_thread_;
  bool connect_in_progress_;

public:
  static bool initializeStaticConnectionInfo(const LwpaCid &cid, const LwpaUid &uid, MyLog *log);
  static LwpaCid getLocalCID() { return local_cid_; }
  static LwpaUid getLocalUID() { return local_uid_; }

  BrokerConnection(std::string scope);
  BrokerConnection(std::string scope, const LwpaSockaddr &addr);
  ~BrokerConnection();

  // Accessors
  bool connected() const { return connected_; }
  const std::string scope() const { return scope_.toStdString(); }
  int handle() const { return conn_; }

  BrokerItem *treeBrokerItem() const { return broker_item_; }

  uint32_t sequence() const { return sequence_; }

  const QString generateBrokerItemText();

  bool operator==(const BrokerConnection &other) { return scope_ == other.scope_; }

  BrokerConnection &operator=(const BrokerConnection &copy)
  {
    scope_ = copy.scope_;
    broker_addr_ = copy.broker_addr_;
    broker_item_ = copy.broker_item_;

    if (copy.connected_)
      rdmnet_disconnect(copy.conn_, false, kRDMnetDisconnectShutdown);
  }

  uint32_t sequencePreIncrement() { return ++sequence_; }

  void connect(const BrokerDiscInfo *broker_info);
  void connect();
  void disconnect();
  void wasDisconnected();

  void runConnectStateMachine();

  void appendBrokerItemToTree(QStandardItem *invisibleRootItem, uint32_t connectionCookie);

  bool isUsingMDNS();
};

class RDMnetNetworkModel : public QStandardItemModel  //, public IRDMnet_MDNS_Notify
{
  Q_OBJECT

public:
  std::map<int, std::unique_ptr<BrokerConnection>> broker_connections_;

private:
  static bool rdmnet_initialized_;

  MyLog log_;
  ScopeMonitorInfo scope_info_;
  uint32_t broker_count_ = 0;
  bool recv_thread_run_;
  lwpa_thread_t recv_thread_;

signals:
  void brokerDisconnection(int conn);
  void addRDMnetClients(BrokerItem *treeBrokerItem, const std::vector<ClientEntryData> &list);
  void removeRDMnetClients(BrokerItem *treeBrokerItem, const std::vector<ClientEntryData> &list);
  void newEndpointList(RDMnetClientItem *treeClientItem, const std::vector<std::pair<uint16_t, uint8_t>> &list);
  void newResponderList(EndpointItem *treeEndpointItem, const std::vector<LwpaUid> &list);
  void setPropertyData(RDMnetNetworkItem *parent, unsigned short pid, const QString &name, const QVariant &value,
                       int role = Qt::DisplayRole);
  void brokerItemTextUpdated(const BrokerItem *item);
  void addPropertyEntry(RDMnetNetworkItem *parent, unsigned short pid, const QString &name, int role);
  void resetDeviceSupportChanged(const class RDMnetNetworkItem *item);

public slots:
  void addScopeToMonitor(std::string scope);
  void directChildrenRevealed(const QModelIndex &parentIndex);
  void addBrokerByIP(std::string scope, const LwpaSockaddr &addr);

protected slots:
  void processBrokerDisconnection(int conn);
  void processAddRDMnetClients(BrokerItem *treeBrokerItem, const std::vector<ClientEntryData> &list);
  void processRemoveRDMnetClients(BrokerItem *treeBrokerItem, const std::vector<ClientEntryData> &list);
  void processNewEndpointList(RDMnetClientItem *treeClientItem, const std::vector<std::pair<uint16_t, uint8_t>> &list);
  void processNewResponderList(EndpointItem *treeEndpointItem, const std::vector<LwpaUid> &list);
  void processSetPropertyData(RDMnetNetworkItem *parent, unsigned short pid, const QString &name, const QVariant &value,
                              int role);
  void processAddPropertyEntry(RDMnetNetworkItem *parent, unsigned short pid, const QString &name, int role);
  void removeBroker(BrokerItem *brokerItem);
  void removeAllBrokers();
  void resetDevice(ResponderItem *device);

public:
  void InitRDMnet();
  void ShutdownRDMnet();

  static RDMnetNetworkModel *makeRDMnetNetworkModel();
  static RDMnetNetworkModel *makeTestModel();

  ~RDMnetNetworkModel();

  void searchingItemRevealed(SearchingStatusItem *searchItem);

  /******* QStandardItemModel overrides *******/

  virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

  void StartRecvThread();
  void RecvThreadRun();
  void StopRecvThread();

  /******* IRDMnet_MDNS_Notify overrides *******/

  // Called whenever a broker was found. As this could be called multiple times
  // for the same service (e.g. the ip address changes or more are discovered),
  // you should always check the service_name field.  If you find more one
  // broker for the scope, the user should be notified of an error!
  // virtual void BrokerFound(const std::string &     scope,
  //                          const mdns_broker_info &broker_found);

  // Called whenever a broker went away.  The name corresponds to the
  // service_name field given in BrokerFound.
  // virtual void BrokerRemoved(const std::string &broker_service_name);

  // Called when the query had a platform-specific error. Monitoring will
  // attempt to continue.
  // virtual void ScopeMonitorError(const mdns_monitor_info &info,
  //                                int platform_specific_error);

protected:
  /******* RDM message handling functions *******/
  void ProcessMessage(int conn, const RdmnetMessage *msg);
  void ProcessRPTMessage(int conn, const RdmnetMessage *msg);
  void ProcessBrokerMessage(int conn, const RdmnetMessage *msg);

  void ProcessRPTStatus(int conn, const RptHeader *header, const RptStatusMsg *status);
  void ProcessRPTNotification(int conn, const RptHeader *header, const RdmCmdList *cmd_list);
  void ProcessRDMResponse(int conn, bool have_command, const RdmCommand &cmd, const std::vector<RdmResponse> &response);

  // Use this with data that has identical GET_COMMAND_RESPONSE and SET_COMMAND forms.
  void ProcessRDMGetSetData(uint16_t param_id, const uint8_t *data, uint8_t datalen, RdmResponse *resp);

  bool SendRDMCommand(const RdmCommand &cmd);

  // RDMnet messages
  void endpointList(uint32_t changeNumber, const std::vector<std::pair<uint16_t, uint8_t>> &list,
                    const LwpaUid &src_uid);
  void endpointResponders(uint16_t endpoint, uint32_t changeNumber, const std::vector<LwpaUid> &list,
                          const LwpaUid &src_uid);
  void endpointListChange(uint32_t changeNumber, const LwpaUid &src_uid);
  void responderListChange(uint32_t changeNumber, uint16_t endpoint, const LwpaUid &src_uid);

  // RDM PID GET responses/updates
  void nack(uint16_t reason, const RdmResponse *resp);
  void status(uint8_t type, uint16_t messageId, uint16_t data1, uint16_t data2, RdmResponse *resp);

  // E1.20
  // SUPPORTED_PARAMETERS
  void commands(std::vector<uint16_t> list, RdmResponse *resp);
  // DEVICE_INFO
  void deviceInfo(uint16_t protocolVersion, uint16_t modelId, uint16_t category, uint32_t swVersionId,
                  uint16_t footprint, uint8_t personality, uint8_t totalPersonality, uint16_t address,
                  uint16_t subdeviceCount, uint8_t sensorCount, RdmResponse *resp);
  // DEVICE_MODEL_DESCRIPTION
  void modelDesc(const char *label, RdmResponse *resp);
  // MANUFACTURER_LABEL
  void manufacturerLabel(const char *label, RdmResponse *resp);
  // DEVICE_LABEL
  void deviceLabel(const char *label, RdmResponse *resp);
  // SOFTWARE_VERSION_LABEL
  void softwareLabel(const char *label, RdmResponse *resp);
  // BOOT_SOFTWARE_VERSION_ID
  void bootSoftwareID(uint32_t id, RdmResponse *resp);
  // BOOT_SOFTWARE_VERSION_LABEL
  void bootSoftwareLabel(const char *label, RdmResponse *resp);
  // DMX_START_ADDRESS
  void address(uint16_t address, RdmResponse *resp);
  // IDENTIFY_DEVICE
  void identify(bool enable, RdmResponse *resp);
  // DMX_PERSONALITY (number may equal zero if data is not provided)
  void personality(uint8_t current, uint8_t number, RdmResponse *resp);
  // DMX_PERSONALITY_DESCRIPTION
  void personalityDescription(uint8_t personality, uint16_t footprint, const char *description, RdmResponse *resp);

  // E1.33
  // COMPONENT_SCOPE
  void componentScope(uint16_t scopeSlot, const char *scopeString, RdmResponse *resp);
  // BROKER_STATIC_CONFIG_IPV4
  void brokerStaticConfigIPv4(const char *addrString, uint16_t port, const char *scopeString,
                              RdmResponse *resp);
  // BROKER_STATIC_CONFIG_IPV6
  void brokerStaticConfigIPv6(const char *addrString, uint16_t port, const char *scopeString,
                              RdmResponse *resp);
  // SEARCH_DOMAIN
  void searchDomain(const char *domainNameString, RdmResponse *resp);
  // TCP_COMMS_STATUS
  void tcpCommsStatus(const char *scopeString, const char *v4AddrString, const char *v6AddrString, uint16_t port,
                      uint16_t unhealthyTCPEvents, RdmResponse *resp);

  // Message sending
  void addPropertyEntries(RDMnetNetworkItem *parent, PropertyLocation location);
  void initializeResponderProperties(ResponderItem *parent, uint16_t manuID, uint32_t deviceID);
  void initializeRPTDeviceProperties(RDMnetClientItem *parent, uint16_t manuID, uint32_t deviceID);
  void sendGetCommand(uint16_t pid, uint16_t manu, uint32_t dev);

  // PID handling
  bool pidSupportedByGUI(uint16_t pid, bool checkSupportGet);

  // Item handling
  RDMnetClientItem *getClientItem(RdmResponse *resp);
  ResponderItem *getResponderItem(RdmResponse *resp);
  void checkPersonalityDescriptions(ResponderItem *device, uint8_t numberOfPersonalities, RdmResponse *resp);
  QVariant getPropertyData(RDMnetNetworkItem *parent, unsigned short pid, int role);

private:
  RDMnetNetworkModel();
};
