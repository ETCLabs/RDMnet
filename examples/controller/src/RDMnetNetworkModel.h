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

#pragma once

#include <map>
#include <vector>
#include <cstddef>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/log.h"
#include "etcpal/cpp/rwlock.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/message.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/version.h"
#include "rdmnet/cpp/controller.h"
#include "BrokerItem.h"
#include "SearchingStatusItem.h"
#include "PropertyValueItem.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QStandardItemModel>
END_INCLUDE_QT_HEADERS()

constexpr uint16_t kExampleControllerModelId = 0xfe00;
constexpr uint32_t kExampleControllerSwVersionId = ((RDMNET_VERSION_MAJOR << 24) | (RDMNET_VERSION_MINOR << 16) |
                                                    (RDMNET_VERSION_PATCH << 8) | (RDMNET_VERSION_BUILD));

struct RdmDeviceInfo
{
  uint16_t protocol_version;
  uint16_t model_id;
  uint16_t category;
  uint32_t sw_version_id;
  uint16_t footprint;
  uint8_t  personality;
  uint8_t  num_personalities;
  uint16_t dmx_address;
  uint16_t subdevice_count;
  uint8_t  sensor_count;
};

void AppendRowToItem(QStandardItem* parent, QStandardItem* child);

class RDMnetNetworkModel : public QStandardItemModel, public rdmnet::Controller::NotifyHandler
{
  Q_OBJECT

signals:
  void addRdmnetClients(BrokerItem* brokerItem, const std::vector<rdmnet::RptClientEntry>& list);
  void removeRdmnetClients(BrokerItem* brokerItem, const std::vector<rdmnet::RptClientEntry>& list);
  void newEndpointList(RDMnetClientItem* treeClientItem, const std::vector<std::pair<uint16_t, uint8_t>>& list);
  void newResponderList(EndpointItem* treeEndpointItem, const std::vector<rdm::Uid>& list);
  void setPropertyData(RDMnetNetworkItem* parent,
                       unsigned short     pid,
                       const QString&     name,
                       const QVariant&    value,
                       int                role = Qt::DisplayRole);
  void removePropertiesInRange(RDMnetNetworkItem*                parent,
                               std::vector<class PropertyItem*>* properties,
                               unsigned short                    pid,
                               int                               role,
                               const QVariant&                   min,
                               const QVariant&                   max);
  void brokerItemTextUpdated(const BrokerItem* item);
  void addPropertyEntry(RDMnetNetworkItem* parent, unsigned short pid, const QString& name, int role);
  void featureSupportChanged(const class RDMnetNetworkItem* item, SupportedDeviceFeature feature);
  void expandNewItem(const QModelIndex& index, int type);
  void identifyChanged(const RDMnetNetworkItem* item, bool identify);

private:
  etcpal::Logger*     log_{nullptr};
  rdmnet::Controller& rdmnet_;
  etcpal::Uuid        my_cid_{etcpal::Uuid::V4()};

  std::map<rdmnet::ScopeHandle, BrokerItem*> broker_connections_;
  etcpal::RwLock                             conn_lock_;

  // Keeps track of scope updates of other controllers
  std::map<rdm::Uid, uint16_t> previous_slot_;

public slots:
  void addScopeToMonitor(QString scope);
  void directChildrenRevealed(const QModelIndex& parentIndex);
  void addBrokerByIP(QString scope, const etcpal::SockAddr& addr);
  void removeBroker(BrokerItem* brokerItem);
  void removeAllBrokers();
  void activateFeature(RDMnetNetworkItem* device, SupportedDeviceFeature feature);

protected slots:
  void processAddRdmnetClients(BrokerItem* brokerItem, const std::vector<rdmnet::RptClientEntry>& list);
  void processRemoveRdmnetClients(BrokerItem* brokerItem, const std::vector<rdmnet::RptClientEntry>& list);
  void processNewEndpointList(RDMnetClientItem* treeClientItem, const std::vector<std::pair<uint16_t, uint8_t>>& list);
  void processNewResponderList(EndpointItem* treeEndpointItem, const std::vector<rdm::Uid>& list);
  void processSetPropertyData(RDMnetNetworkItem* parent,
                              unsigned short     pid,
                              const QString&     name,
                              const QVariant&    value,
                              int                role);
  void processRemovePropertiesInRange(RDMnetNetworkItem*                parent,
                                      std::vector<class PropertyItem*>* properties,
                                      unsigned short                    pid,
                                      int                               role,
                                      const QVariant&                   min,
                                      const QVariant&                   max);
  void processAddPropertyEntry(RDMnetNetworkItem* parent, unsigned short pid, const QString& name, int role);
  void processPropertyButtonClick(const QPersistentModelIndex& propertyIndex);

protected:
  RDMnetNetworkModel(rdmnet::Controller& library, etcpal::Logger& log);

public:
  static RDMnetNetworkModel* MakeRDMnetNetworkModel(rdmnet::Controller& library, etcpal::Logger& log);
  // static RDMnetNetworkModel* MakeTestModel();

  RDMnetNetworkModel() = delete;
  void Shutdown();

  void searchingItemRevealed(SearchingStatusItem* searchItem);

  // QStandardItemModel overrides

  virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

protected:
  // rdmnet::Controller::NotifyHandler overrides
  virtual void HandleConnectedToBroker(rdmnet::Controller::Handle         controller_handle,
                                       rdmnet::ScopeHandle                scope_handle,
                                       const rdmnet::ClientConnectedInfo& info) override;
  virtual void HandleBrokerConnectFailed(rdmnet::Controller::Handle             controller_handle,
                                         rdmnet::ScopeHandle                    scope_handle,
                                         const rdmnet::ClientConnectFailedInfo& info) override;
  virtual void HandleDisconnectedFromBroker(rdmnet::Controller::Handle            controller_handle,
                                            rdmnet::ScopeHandle                   scope_handle,
                                            const rdmnet::ClientDisconnectedInfo& info) override;
  virtual void HandleClientListUpdate(rdmnet::Controller::Handle   controller_handle,
                                      rdmnet::ScopeHandle          scope_handle,
                                      client_list_action_t         action,
                                      const rdmnet::RptClientList& list) override;
  virtual void HandleRdmResponse(rdmnet::Controller::Handle controller_handle,
                                 rdmnet::ScopeHandle        scope_handle,
                                 const rdmnet::RdmResponse& resp) override;
  virtual void HandleRptStatus(rdmnet::Controller::Handle controller,
                               rdmnet::ScopeHandle        scope_handle,
                               const rdmnet::RptStatus&   status) override;

  /******* RDM message handling functions *******/
  void HandleRdmAck(rdmnet::ScopeHandle scope_handle, const rdmnet::RdmResponse& resp);
  void HandleRdmNack(rdmnet::ScopeHandle scope_handle, const rdmnet::RdmResponse& resp);
  // Use this with data that has identical GET_COMMAND_RESPONSE and SET_COMMAND forms.
  void ProcessRdmGetSetData(rdmnet::ScopeHandle scope_handle,
                            uint16_t            param_id,
                            const uint8_t*      data,
                            uint8_t             datalen,
                            const rdm::Uid&     source_uid);

  bool        SendGetCommand(const BrokerItem* broker_item,
                             const rdm::Uid&   dest_uid,
                             uint16_t          param_id,
                             const uint8_t*    get_data = nullptr,
                             uint8_t           get_data_len = 0);
  bool        SendSetCommand(const BrokerItem* broker_item,
                             const rdm::Uid&   dest_uid,
                             uint16_t          param_id,
                             const uint8_t*    set_data = nullptr,
                             uint8_t           set_data_len = 0);
  BrokerItem* GetBrokerItem(rdmnet::ScopeHandle scope_handle);

  /* GET/SET RESPONSE PROCESSING */

  // E1.33
  // COMPONENT_SCOPE
  void HandleComponentScopeResponse(rdmnet::ScopeHandle conn,
                                    uint16_t            scopeSlot,
                                    const QString&      scopeString,
                                    const QString&      staticConfigV4,
                                    const QString&      staticConfigV6,
                                    uint16_t            port,
                                    const rdm::Uid&     source_uid);
  // SEARCH_DOMAIN
  void HandleSearchDomainResponse(rdmnet::ScopeHandle scope_handle,
                                  const QString&      domainNameString,
                                  const rdm::Uid&     source_uid);
  // TCP_COMMS_STATUS
  void HandleTcpCommsStatusResponse(rdmnet::ScopeHandle scope_handle,
                                    const QString&      scopeString,
                                    const QString&      v4AddrString,
                                    const QString&      v6AddrString,
                                    uint16_t            port,
                                    uint16_t            unhealthyTCPEvents,
                                    const rdm::Uid&     source_uid);

  // E1.37-7
  // ENDPOINT_LIST
  void HandleEndpointListResponse(rdmnet::ScopeHandle                              scope_handle,
                                  uint32_t                                         change_number,
                                  const std::vector<std::pair<uint16_t, uint8_t>>& list,
                                  const rdm::Uid&                                  source_uid);
  // ENDPOINT_RESPONDERS
  void HandleEndpointRespondersResponse(rdmnet::ScopeHandle          scope_handle,
                                        uint16_t                     endpoint,
                                        uint32_t                     changeNumber,
                                        const std::vector<rdm::Uid>& list,
                                        const rdm::Uid&              source_uid);
  // ENDPOINT_LIST_CHANGE
  void HandleEndpointListChangeResponse(rdmnet::ScopeHandle scope_handle,
                                        uint32_t            changeNumber,
                                        const rdm::Uid&     source_uid);
  // ENDPOINT_RESPONDER_LIST_CHANGE
  void HandleResponderListChangeResponse(rdmnet::ScopeHandle scope_handle,
                                         uint32_t            changeNumber,
                                         uint16_t            endpoint,
                                         const rdm::Uid&     source_uid);

  // E1.20
  // SUPPORTED_PARAMETERS
  void HandleSupportedParametersResponse(rdmnet::ScopeHandle          scope_handle,
                                         const std::vector<uint16_t>& params_list,
                                         const rdm::Uid&              source_uid);
  // DEVICE_INFO
  void HandleDeviceInfoResponse(rdmnet::ScopeHandle  scope_handle,
                                const RdmDeviceInfo& device_info,
                                const rdm::Uid&      source_uid);
  // DEVICE_MODEL_DESCRIPTION
  void HandleModelDescResponse(rdmnet::ScopeHandle scope_handle, const QString& label, const rdm::Uid& source_uid);
  // MANUFACTURER_LABEL
  void HandleManufacturerLabelResponse(rdmnet::ScopeHandle scope_handle,
                                       const QString&      label,
                                       const rdm::Uid&     source_uid);
  // DEVICE_LABEL
  void HandleDeviceLabelResponse(rdmnet::ScopeHandle scope_handle, const QString& label, const rdm::Uid& source_uid);
  // SOFTWARE_VERSION_LABEL
  void HandleSoftwareLabelResponse(rdmnet::ScopeHandle scope_handle, const QString& label, const rdm::Uid& source_uid);
  // BOOT_SOFTWARE_VERSION_ID
  void HandleBootSoftwareIdResponse(rdmnet::ScopeHandle scope_handle, uint32_t id, const rdm::Uid& source_uid);
  // BOOT_SOFTWARE_VERSION_LABEL
  void HandleBootSoftwareLabelResponse(rdmnet::ScopeHandle scope_handle,
                                       const QString&      label,
                                       const rdm::Uid&     source_uid);
  // DMX_START_ADDRESS
  void HandleStartAddressResponse(rdmnet::ScopeHandle scope_handle, uint16_t address, const rdm::Uid& source_uid);
  // IDENTIFY_DEVICE
  void HandleIdentifyResponse(rdmnet::ScopeHandle scope_handle, bool identifying, const rdm::Uid& source_uid);
  // DMX_PERSONALITY (number may equal zero if data is not provided)
  void HandlePersonalityResponse(rdmnet::ScopeHandle scope_handle,
                                 uint8_t             current,
                                 uint8_t             number,
                                 const rdm::Uid&     source_uid);
  // DMX_PERSONALITY_DESCRIPTION
  void HandlePersonalityDescResponse(rdmnet::ScopeHandle scope_handle,
                                     uint8_t             personality,
                                     uint16_t            footprint,
                                     const QString&      description,
                                     const rdm::Uid&     source_uid);

  // RDM PID GET responses/updates
  void HandleStatusMessagesResponse(uint8_t         type,
                                    uint16_t        messageId,
                                    uint16_t        data1,
                                    uint16_t        data2,
                                    const rdm::Uid& source_uid);

  // Message sending
  void     AddPropertyEntries(RDMnetNetworkItem* item, PIDFlags location);
  void     InitializeResponderProperties(ResponderItem* item);
  void     InitializeRptClientProperties(RDMnetClientItem* parent, const rdm::Uid& uid, rpt_client_type_t clientType);
  uint8_t* PackIPAddressItem(const QVariant& value, etcpal_iptype_t addrType, uint8_t* packPtr, bool packPort = true);

  // PID handling
  bool PidSupportedByGui(uint16_t pid, bool checkSupportGet);

  // Item handling
  RDMnetClientItem*  GetClientItem(rdmnet::ScopeHandle conn, const rdm::Uid& uid);
  RDMnetNetworkItem* GetNetworkItem(rdmnet::ScopeHandle conn, const rdm::Uid& uid);
  void               CheckPersonalityDescriptions(RDMnetNetworkItem* device,
                                                  uint8_t            numberOfPersonalities,
                                                  const rdm::Uid&    source_uid);
  QVariant           getPropertyData(RDMnetNetworkItem* parent, unsigned short pid, int role);

  class PropertyItem* createPropertyItem(RDMnetNetworkItem* parent, const QString& fullName);
  QString             getShortPropertyName(const QString& fullPropertyName);
  QString             getHighestGroupName(const QString& pathName);
  QString             getPathSubset(const QString& fullPath, int first, int last = -1);
  class PropertyItem* getGroupingItem(RDMnetNetworkItem* parent, const QString& groupName);
  class PropertyItem* createGroupingItem(RDMnetNetworkItem* parent, const QString& groupName);
  QString             getChildPathName(const QString& superPathName);
  QString getScopeSubPropertyFullName(RDMnetClientItem* client, uint16_t pid, int32_t index, const QString& scope);

  void RemoveScopeSlotItemsInRange(RDMnetNetworkItem*          parent,
                                   std::vector<PropertyItem*>* properties,
                                   uint16_t                    firstSlot,
                                   uint16_t                    lastSlot);
};
