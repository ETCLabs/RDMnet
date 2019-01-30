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

/// \file broker_core.h
#ifndef _BROKER_CORE_H_
#define _BROKER_CORE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "lwpa/socket.h"
#include "rdmnet/broker.h"
#include "rdmnet/client.h"
#include "rdmnet/core/broker_prot.h"
#include "rdmnet/core/rpt_prot.h"
#include "broker_client.h"
#include "broker_responder.h"
#include "broker_threads.h"
#include "broker_discovery.h"
#include "broker_uid_manager.h"

class BrokerCore : public ListenThreadNotify,
                   public ConnPollThreadNotify,
                   public ClientServiceThreadNotify,
                   public BrokerDiscoveryManagerNotify
{
public:
  BrokerCore(RDMnet::BrokerLog *log, RDMnet::BrokerNotify *notify);
  virtual ~BrokerCore();

  // Some utility functions
  static constexpr bool IsBroadcastUID(const RdmUid &uid);
  static constexpr bool IsControllerBroadcastUID(const RdmUid &uid);
  static constexpr bool IsDeviceBroadcastUID(const RdmUid &uid);
  static bool IsDeviceManuBroadcastUID(const RdmUid &uid, uint16_t &manu);
  bool IsValidControllerDestinationUID(const RdmUid &uid) const;
  bool IsValidDeviceDestinationUID(const RdmUid &uid) const;

  RDMnet::BrokerLog *GetLog() { return log_; }

  bool Startup(const RDMnet::BrokerSettings &settings, uint16_t listen_port, std::vector<LwpaIpAddr> &listen_addrs);
  void Shutdown();
  void Tick();

  void GetSettings(RDMnet::BrokerSettings &settings) const;

private:
  // These are never modified between startup and shutdown, so they don't need to be locked.
  bool started_;
  bool service_registered_;
  int other_brokers_found_;
  RDMnet::BrokerLog *log_;
  RDMnet::BrokerNotify *notify_;
  RDMnet::BrokerSettings settings_;
  RdmUid my_uid_;
  std::vector<std::unique_ptr<ListenThread>> listeners_;

  // The Broker's RDM responder
  BrokerResponder responder_;

  ClientServiceThread service_thread_;
  BrokerDiscoveryManager disc_;

  // If you have a maximum number of connections, we may be stopping and starting the listen
  // threads.
  void StartBrokerServices();
  void StopBrokerServices();

  // IListenThread_Notify messages
  virtual bool NewConnection(lwpa_socket_t new_sock, const LwpaSockaddr &addr) override;
  virtual void LogError(const std::string &err) override;

  // IConnPollThread_Notify messages
  virtual void PollConnections(const std::vector<int> &conn_handles, RdmnetPoll *poll_arr) override;

  // IClientServiceThread_Notify messages
  virtual bool ServiceClients() override;

  // IBrokerDiscoveryManager_Notify messages
  virtual void BrokerRegistered(const BrokerDiscInfo &broker_info, const std::string &assigned_service_name) override;
  virtual void OtherBrokerFound(const BrokerDiscInfo &broker_info) override;
  virtual void OtherBrokerLost(const std::string &service_name) override;
  virtual void BrokerRegisterError(const BrokerDiscInfo &broker_info, int platform_error) override;

  // The poll operation has a maximum size, so we need a pool of threads to do the poll operation.
  lwpa_mutex_t poll_thread_lock_;
  std::set<std::shared_ptr<ConnPollThread>> poll_threads_;

  void AddConnToPollThread(int conn, std::shared_ptr<ConnPollThread> &thread);

  // The list of connected clients, indexed by the connection handle
  std::map<int, std::shared_ptr<BrokerClient>> clients_;
  // Manages the UIDs of connected clients and generates new ones upon request
  BrokerUidManager uid_manager_;
  // Protects the list of clients and uid lookup, but not the data in the clients themselves.
  mutable lwpa_rwlock_t client_lock_;

  void GetConnSnapshot(std::vector<int> &conns, bool include_devices, bool include_controllers, bool include_unknown,
                       uint16_t manufacturer_filter = 0xffff);

  // The state data for each controller, indexed by its connection handle.
  std::map<int, std::shared_ptr<RPTController>> controllers_;
  // The set of devices, indexed by the connection handle.
  std::map<int, std::shared_ptr<RPTDevice>> devices_;

  lwpa_mutex_t client_destroy_lock_;
  std::set<int> clients_to_destroy_;

  void MarkConnForDestruction(int conn, bool send_disconnect,
                              rdmnet_disconnect_reason_t reason = kRdmnetDisconnectShutdown);
  void DestroyMarkedClientSockets();
  void RemoveConnections(const std::vector<int> &connections);

  // Message processing and sending functions
  void ProcessTCPMessage(int conn, const RdmnetMessage *msg);
  void ProcessRPTMessage(int conn, const RdmnetMessage *msg);
  void ProcessConnectRequest(int conn, const ClientConnectMsg *cmsg);
  bool ProcessRPTConnectRequest(int conn, const ClientEntryData &data, rdmnet_connect_status_t &connect_status);

  void SendRDMBrokerResponse(int conn, const RPTMessageRef &msg, uint8_t response_type, uint8_t command_class,
                             uint16_t param_id, uint8_t packedlen, uint8_t *pdata);

  void SendClientList(int conn);
  void SendClientsAdded(client_protocol_t client_prot, int conn_to_ignore, std::vector<ClientEntryData> &entries);
  void SendClientsRemoved(client_protocol_t client_prot, std::vector<ClientEntryData> &entries);
  void SendStatus(RPTController *controller, const RptHeader &header, rpt_status_code_t status_code,
                  const std::string &status_str = std::string());
};

#endif  // _BROKER_CORE_H_