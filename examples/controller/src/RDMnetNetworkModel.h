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

#pragma once

#include <map>
#include <vector>
#include <cstddef>
#include "lwpa/lock.h"
#include "BrokerItem.h"
#include "SearchingStatusItem.h"
#include "PropertyValueItem.h"
#include "ControllerUtils.h"
#include "RDMnetLibInterface.h"
#include "ControllerDefaultResponder.h"
#include "ControllerLog.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QStandardItemModel>
END_INCLUDE_QT_HEADERS()

void appendRowToItem(QStandardItem* parent, QStandardItem* child);

class RDMnetNetworkModel : public QStandardItemModel, public RDMnetLibNotify
{
  Q_OBJECT

signals:
  void addRDMnetClients(BrokerItem* brokerItem, const std::vector<ClientEntryData>& list);
  void removeRDMnetClients(BrokerItem* brokerItem, const std::vector<ClientEntryData>& list);
  void newEndpointList(RDMnetClientItem* treeClientItem, const std::vector<std::pair<uint16_t, uint8_t>>& list);
  void newResponderList(EndpointItem* treeEndpointItem, const std::vector<RdmUid>& list);
  void setPropertyData(RDMnetNetworkItem* parent, unsigned short pid, const QString& name, const QVariant& value,
                       int role = Qt::DisplayRole);
  void removePropertiesInRange(RDMnetNetworkItem* parent, std::vector<class PropertyItem*>* properties,
                               unsigned short pid, int role, const QVariant& min, const QVariant& max);
  void brokerItemTextUpdated(const BrokerItem* item);
  void addPropertyEntry(RDMnetNetworkItem* parent, unsigned short pid, const QString& name, int role);
  void featureSupportChanged(const class RDMnetNetworkItem* item, SupportedDeviceFeature feature);
  void expandNewItem(const QModelIndex& index, int type);
  void identifyChanged(const RDMnetNetworkItem* item, bool identify);

private:
  ControllerLog* log_{nullptr};
  RDMnetLibInterface* rdmnet_{nullptr};
  LwpaUuid my_cid_;

  ControllerDefaultResponder default_responder_;

  std::map<rdmnet_client_scope_t, BrokerItem*> broker_connections_;
  lwpa_rwlock_t conn_lock_;

  // Keeps track of scope updates of other controllers
  std::map<RdmUid, uint16_t> previous_slot_;

public slots:
  void addScopeToMonitor(QString scope);
  void directChildrenRevealed(const QModelIndex& parentIndex);
  void addBrokerByIP(QString scope, const LwpaSockaddr& addr);
  void addCustomLogOutputStream(LogOutputStream* stream);
  void removeCustomLogOutputStream(LogOutputStream* stream);

protected slots:
  void processAddRDMnetClients(BrokerItem* brokerItem, const std::vector<ClientEntryData>& list);
  void processRemoveRDMnetClients(BrokerItem* brokerItem, const std::vector<ClientEntryData>& list);
  void processNewEndpointList(RDMnetClientItem* treeClientItem, const std::vector<std::pair<uint16_t, uint8_t>>& list);
  void processNewResponderList(EndpointItem* treeEndpointItem, const std::vector<RdmUid>& list);
  void processSetPropertyData(RDMnetNetworkItem* parent, unsigned short pid, const QString& name, const QVariant& value,
                              int role);
  void processRemovePropertiesInRange(RDMnetNetworkItem* parent, std::vector<class PropertyItem*>* properties,
                                      unsigned short pid, int role, const QVariant& min, const QVariant& max);
  void processAddPropertyEntry(RDMnetNetworkItem* parent, unsigned short pid, const QString& name, int role);
  void processPropertyButtonClick(const QPersistentModelIndex& propertyIndex);
  void removeBroker(BrokerItem* brokerItem);
  void removeAllBrokers();
  void activateFeature(RDMnetNetworkItem* device, SupportedDeviceFeature feature);

protected:
  RDMnetNetworkModel(RDMnetLibInterface* library, ControllerLog* log);

public:
  static RDMnetNetworkModel* makeRDMnetNetworkModel(RDMnetLibInterface* library, ControllerLog* log);
  static RDMnetNetworkModel* makeTestModel();

  RDMnetNetworkModel() = delete;
  void Shutdown();

  void searchingItemRevealed(SearchingStatusItem* searchItem);

  size_t getNumberOfCustomLogOutputStreams();

  /******* QStandardItemModel overrides *******/

  virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);

protected:
  // RDMnetLibNotify overrides
  virtual void Connected(rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo& info) override;
  virtual void ConnectFailed(rdmnet_client_scope_t scope_handle, const RdmnetClientConnectFailedInfo& info) override;
  virtual void Disconnected(rdmnet_client_scope_t scope_handle, const RdmnetClientDisconnectedInfo& info) override;
  virtual void ClientListUpdate(rdmnet_client_scope_t scope_handle, client_list_action_t action,
                                const ClientList& list) override;
  virtual void RdmCommandReceived(rdmnet_client_scope_t scope_handle, const RemoteRdmCommand& cmd) override;
  virtual void RdmResponseReceived(rdmnet_client_scope_t scope_handle, const RemoteRdmResponse& resp) override;
  virtual void StatusReceived(rdmnet_client_scope_t scope_handle, const RemoteRptStatus& status) override;
  virtual void LlrpRdmCommandReceived(const LlrpRemoteRdmCommand& cmd) override;

  /******* RDM message handling functions *******/
  void HandleRDMAckOrAckOverflow(rdmnet_client_scope_t scope_handle, const RemoteRdmResponse& resp);
  // Use this with data that has identical GET_COMMAND_RESPONSE and SET_COMMAND forms.
  void ProcessRDMGetSetData(rdmnet_client_scope_t scope_handle, uint16_t param_id, const uint8_t* data, uint8_t datalen,
                            const RdmResponse& firstResp);

  bool SendRDMCommand(const RdmCommand& cmd, const BrokerItem* brokerItem);
  bool SendRDMCommand(const RdmCommand& cmd, rdmnet_client_scope_t scope_handle);
  void SendRDMGetResponses(rdmnet_client_scope_t scope_handle, const RdmUid& dest_uid, uint16_t param_id,
                           const std::vector<RdmParamData>& resp_data_list, bool have_command = false,
                           const RemoteRdmCommand& cmd = RemoteRdmCommand());
  void SendRDMGetResponsesBroadcast(uint16_t param_id, const std::vector<RdmParamData>& resp_data_list);
  void SendRDMNack(rdmnet_client_scope_t scope, const RemoteRdmCommand& received_cmd, uint16_t nack_reason);
  void SendLlrpGetResponse(const LlrpRemoteRdmCommand& received_cmd, const std::vector<RdmParamData>& resp_data_list);
  void SendLlrpNack(const LlrpRemoteRdmCommand& received_cmd, uint16_t nack_reason);

  /* GET/SET RESPONSE PROCESSING */

  // E1.33
  // COMPONENT_SCOPE
  void HandleComponentScopeResponse(rdmnet_client_scope_t conn, uint16_t scopeSlot, const QString& scopeString,
                                    const QString& staticConfigV4, const QString& staticConfigV6, uint16_t port,
                                    const RdmResponse& resp);
  // SEARCH_DOMAIN
  void HandleSearchDomainResponse(rdmnet_client_scope_t scope_handle, const QString& domainNameString,
                                  const RdmResponse& resp);
  // TCP_COMMS_STATUS
  void HandleTcpCommsStatusResponse(rdmnet_client_scope_t scope_handle, const QString& scopeString,
                                    const QString& v4AddrString, const QString& v6AddrString, uint16_t port,
                                    uint16_t unhealthyTCPEvents, const RdmResponse& resp);

  // E1.37-7
  // ENDPOINT_LIST
  void HandleEndpointListResponse(rdmnet_client_scope_t scope_handle, uint32_t changeNumber,
                                  const std::vector<std::pair<uint16_t, uint8_t>>& list, const RdmUid& src_uid);
  // ENDPOINT_RESPONDERS
  void HandleEndpointRespondersResponse(rdmnet_client_scope_t scope_handle, uint16_t endpoint, uint32_t changeNumber,
                                        const std::vector<RdmUid>& list, const RdmUid& src_uid);
  // ENDPOINT_LIST_CHANGE
  void HandleEndpointListChangeResponse(rdmnet_client_scope_t scope_handle, uint32_t changeNumber,
                                        const RdmUid& src_uid);
  // ENDPOINT_RESPONDER_LIST_CHANGE
  void HandleResponderListChangeResponse(rdmnet_client_scope_t scope_handle, uint32_t changeNumber, uint16_t endpoint,
                                         const RdmUid& src_uid);

  // E1.20
  // SUPPORTED_PARAMETERS
  void HandleSupportedParametersResponse(rdmnet_client_scope_t scope_handle, const std::vector<uint16_t>& params_list,
                                         const RdmResponse& resp);
  // DEVICE_INFO
  void HandleDeviceInfoResponse(rdmnet_client_scope_t scope_handle, uint16_t protocolVersion, uint16_t modelId,
                                uint16_t category, uint32_t swVersionId, uint16_t footprint, uint8_t personality,
                                uint8_t totalPersonality, uint16_t address, uint16_t subdeviceCount,
                                uint8_t sensorCount, const RdmResponse& resp);
  // DEVICE_MODEL_DESCRIPTION
  void HandleModelDescResponse(rdmnet_client_scope_t scope_handle, const QString& label, const RdmResponse& resp);
  // MANUFACTURER_LABEL
  void HandleManufacturerLabelResponse(rdmnet_client_scope_t scope_handle, const QString& label,
                                       const RdmResponse& resp);
  // DEVICE_LABEL
  void HandleDeviceLabelResponse(rdmnet_client_scope_t scope_handle, const QString& label, const RdmResponse& resp);
  // SOFTWARE_VERSION_LABEL
  void HandleSoftwareLabelResponse(rdmnet_client_scope_t scope_handle, const QString& label, const RdmResponse& resp);
  // BOOT_SOFTWARE_VERSION_ID
  void HandleBootSoftwareIdResponse(rdmnet_client_scope_t scope_handle, uint32_t id, const RdmResponse& resp);
  // BOOT_SOFTWARE_VERSION_LABEL
  void HandleBootSoftwareLabelResponse(rdmnet_client_scope_t scope_handle, const QString& label,
                                       const RdmResponse& resp);
  // DMX_START_ADDRESS
  void HandleStartAddressResponse(rdmnet_client_scope_t scope_handle, uint16_t address, const RdmResponse& resp);
  // IDENTIFY_DEVICE
  void HandleIdentifyResponse(rdmnet_client_scope_t scope_handle, bool identifying, const RdmResponse& resp);
  // DMX_PERSONALITY (number may equal zero if data is not provided)
  void HandlePersonalityResponse(rdmnet_client_scope_t scope_handle, uint8_t current, uint8_t number,
                                 const RdmResponse& resp);
  // DMX_PERSONALITY_DESCRIPTION
  void HandlePersonalityDescResponse(rdmnet_client_scope_t scope_handle, uint8_t personality, uint16_t footprint,
                                     const QString& description, const RdmResponse& resp);

  // RDM PID GET responses/updates
  void HandleRDMNack(rdmnet_client_scope_t scope_handle, uint16_t reason, const RdmResponse& resp);
  void HandleStatusMessagesResponse(uint8_t type, uint16_t messageId, uint16_t data1, uint16_t data2,
                                    const RdmResponse& resp);

  // Message sending
  void addPropertyEntries(RDMnetNetworkItem* parent, PIDFlags location);
  void initializeResponderProperties(ResponderItem* parent, uint16_t manuID, uint32_t deviceID);
  void initializeRPTClientProperties(RDMnetClientItem* parent, uint16_t manuID, uint32_t deviceID,
                                     rpt_client_type_t clientType);
  void sendGetControllerScopeProperties(rdmnet_client_scope_t scope_handle, uint16_t manuID, uint32_t deviceID);
  void sendGetNextControllerScope(rdmnet_client_scope_t scope_handle, uint16_t manuID, uint32_t deviceID,
                                  uint16_t currentSlot);
  void sendGetCommand(BrokerItem* brokerItem, uint16_t pid, const RdmUid& dest_uid);
  uint8_t* packIPAddressItem(const QVariant& value, lwpa_iptype_t addrType, uint8_t* packPtr, bool packPort = true);

  // PID handling
  bool pidSupportedByGUI(uint16_t pid, bool checkSupportGet);

  // Item handling
  RDMnetClientItem* GetClientItem(rdmnet_client_scope_t conn, const RdmResponse& resp);
  RDMnetNetworkItem* GetNetworkItem(rdmnet_client_scope_t conn, const RdmResponse& resp);
  void checkPersonalityDescriptions(RDMnetNetworkItem* device, uint8_t numberOfPersonalities, const RdmResponse& resp);
  QVariant getPropertyData(RDMnetNetworkItem* parent, unsigned short pid, int role);

  class PropertyItem* createPropertyItem(RDMnetNetworkItem* parent, const QString& fullName);
  QString getShortPropertyName(const QString& fullPropertyName);
  QString getHighestGroupName(const QString& pathName);
  QString getPathSubset(const QString& fullPath, int first, int last = -1);
  class PropertyItem* getGroupingItem(RDMnetNetworkItem* parent, const QString& groupName);
  class PropertyItem* createGroupingItem(RDMnetNetworkItem* parent, const QString& groupName);
  QString getChildPathName(const QString& superPathName);
  QString getScopeSubPropertyFullName(RDMnetClientItem* client, uint16_t pid, int32_t index, const QString& scope);

  void removeScopeSlotItemsInRange(RDMnetNetworkItem* parent, std::vector<class PropertyItem*>* properties,
                                   uint16_t firstSlot, uint16_t lastSlot);
};
