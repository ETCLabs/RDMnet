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

/// \file broker_core.h
#ifndef _BROKER_CORE_H_
#define _BROKER_CORE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "etcpal/socket.h"
#include "rdmnet/broker.h"
#include "broker_client.h"
#include "broker_responder.h"
#include "broker_threads.h"
#include "broker_discovery.h"
#include "broker_uid_manager.h"
#include "rdmnet_conn_wrapper.h"

class BrokerCore : public RdmnetConnNotify,
                   public RDMnet::BrokerSocketManagerNotify,
                   public ListenThreadNotify,
                   public ClientServiceThreadNotify,
                   public BrokerDiscoveryManagerNotify
{
public:
  BrokerCore(RDMnet::BrokerLog* log, RDMnet::BrokerSocketManager* socket_manager, RDMnet::BrokerNotify* notify,
             std::unique_ptr<RdmnetConnInterface> conn);
  virtual ~BrokerCore();

  // Some utility functions
  static constexpr bool IsControllerBroadcastUID(const RdmUid& uid);
  static constexpr bool IsDeviceBroadcastUID(const RdmUid& uid);
  static bool IsDeviceManuBroadcastUID(const RdmUid& uid, uint16_t& manu);
  bool IsValidControllerDestinationUID(const RdmUid& uid) const;
  bool IsValidDeviceDestinationUID(const RdmUid& uid) const;

  RDMnet::BrokerLog* GetLog() { return log_; }

  bool Startup(const RDMnet::BrokerSettings& settings, uint16_t listen_port,
               const std::vector<EtcPalIpAddr>& listen_addrs);
  void Shutdown();
  void Tick();

  void GetSettings(RDMnet::BrokerSettings& settings) const;

  // Notification messages from the RDMnet core library

private:
  // These are never modified between startup and shutdown, so they don't need to be locked.
  bool started_{false};
  bool service_registered_{false};

  RDMnet::BrokerLog* log_{nullptr};
  RDMnet::BrokerSocketManager* socket_manager_{nullptr};
  RDMnet::BrokerNotify* notify_{nullptr};
  std::unique_ptr<RdmnetConnInterface> conn_interface_;

  RDMnet::BrokerSettings settings_;
  RdmUid my_uid_{};

  std::vector<std::unique_ptr<ListenThread>> listeners_;
  std::vector<EtcPalIpAddr> listen_addrs_;
  uint16_t listen_port_;

  // The Broker's RDM responder
  BrokerResponder responder_;

  ClientServiceThread service_thread_;
  BrokerDiscoveryManager disc_;

  etcpal_socket_t StartListening(const EtcPalIpAddr& ip, uint16_t& port);
  bool StartBrokerServices();
  void StopBrokerServices();

  // RdmnetCoreLibraryNotify messages
  virtual void RdmnetConnDisconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo& disconn_info) override;
  virtual void RdmnetConnMsgReceived(rdmnet_conn_t handle, const RdmnetMessage& msg) override;

  // RDMnet::BrokerSocketManagerNotify messages
  virtual void SocketDataReceived(rdmnet_conn_t conn_handle, const uint8_t* data, size_t data_size) override;
  virtual void SocketClosed(rdmnet_conn_t conn_handle, bool graceful) override;

  // ListenThreadNotify messages
  virtual bool NewConnection(etcpal_socket_t new_sock, const EtcPalSockaddr& addr) override;

  // ClientServiceThreadNotify messages
  virtual bool ServiceClients() override;

  // BrokerDiscoveryManagerNotify messages
  virtual void BrokerRegistered(const std::string& assigned_service_name) override;
  virtual void OtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info) override;
  virtual void OtherBrokerLost(const std::string& service_name) override;
  virtual void BrokerRegisterError(int platform_error) override;

  // The list of connected clients, indexed by the connection handle
  std::map<rdmnet_conn_t, std::shared_ptr<BrokerClient>> clients_;
  // Manages the UIDs of connected clients and generates new ones upon request
  BrokerUidManager uid_manager_;
  // Protects the list of clients and uid lookup, but not the data in the clients themselves.
  mutable etcpal_rwlock_t client_lock_;

  void GetConnSnapshot(std::vector<rdmnet_conn_t>& conns, bool include_devices, bool include_controllers,
                       bool include_unknown, uint16_t manufacturer_filter = 0xffff);

  // The state data for each controller, indexed by its connection handle.
  std::map<rdmnet_conn_t, std::shared_ptr<RPTController>> controllers_;
  // The set of devices, indexed by the connection handle.
  std::map<rdmnet_conn_t, std::shared_ptr<RPTDevice>> devices_;

  std::set<rdmnet_conn_t> clients_to_destroy_;

  void MarkConnForDestruction(rdmnet_conn_t conn, SendDisconnect send_disconnect = SendDisconnect());
  void DestroyMarkedClientSockets();

  // Message processing and sending functions
  void ProcessRPTMessage(rdmnet_conn_t conn, const RdmnetMessage* msg);
  void ProcessConnectRequest(rdmnet_conn_t conn, const ClientConnectMsg* cmsg);
  bool ProcessRPTConnectRequest(rdmnet_conn_t conn, const ClientEntryData& data,
                                rdmnet_connect_status_t& connect_status);

  void SendRDMBrokerResponse(rdmnet_conn_t conn, const RPTMessageRef& msg, uint8_t response_type, uint8_t command_class,
                             uint16_t param_id, uint8_t packedlen, uint8_t* pdata);

  void SendClientList(rdmnet_conn_t conn);
  void SendClientsAdded(client_protocol_t client_prot, rdmnet_conn_t conn_to_ignore,
                        std::vector<ClientEntryData>& entries);
  void SendClientsRemoved(client_protocol_t client_prot, std::vector<ClientEntryData>& entries);
  void SendStatus(RPTController* controller, const RptHeader& header, rpt_status_code_t status_code,
                  const std::string& status_str = std::string());
};

#endif  // _BROKER_CORE_H_
