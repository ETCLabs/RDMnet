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

#ifndef BROKER_CORE_H_
#define BROKER_CORE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/lock.h"
#include "etcpal/socket.h"
#include "rdmnet/broker.h"
#include "broker_client.h"
#include "broker_discovery.h"
#include "broker_responder.h"
#include "broker_socket_manager.h"
#include "broker_threads.h"
#include "broker_uid_manager.h"
#include "rdmnet_conn_wrapper.h"

class BrokerComponentNotify : public RdmnetConnNotify,
                              public BrokerSocketNotify,
                              public BrokerThreadNotify,
                              public BrokerDiscoveryNotify
{
};

// A set of components of broker functionality, separated to facilitate testing and dependency
// injection.
struct BrokerComponents final
{
  // Manages the interface to the lower-level RDMnet library
  std::unique_ptr<RdmnetConnInterface> conn_interface;
  // Manages the broker's readable sockets
  std::unique_ptr<BrokerSocketManager> socket_mgr;
  // Manages the broker's worker threads
  std::unique_ptr<BrokerThreadInterface> threads;
  // Handles DNS discovery of the broker
  std::unique_ptr<BrokerDiscoveryInterface> disc;
  // Handles the Broker's dynamic UID assignment functionality
  BrokerUidManager uids;
  // The Broker's RDM responder
  BrokerResponder responder;

  void SetNotify(BrokerComponentNotify* notify)
  {
    conn_interface->SetNotify(notify);
    socket_mgr->SetNotify(notify);
    threads->SetNotify(notify);
    disc->SetNotify(notify);
  }

  BrokerComponents(std::unique_ptr<RdmnetConnInterface> conn_interface_in = std::make_unique<RdmnetConnWrapper>(),
                   std::unique_ptr<BrokerSocketManager> socket_mgr_in = CreateBrokerSocketManager(),
                   std::unique_ptr<BrokerThreadInterface> threads_in = std::make_unique<BrokerThreadManager>(),
                   std::unique_ptr<BrokerDiscoveryInterface> disc_in = std::make_unique<BrokerDiscoveryManager>())
      : conn_interface(std::move(conn_interface_in))
      , socket_mgr(std::move(socket_mgr_in))
      , threads(std::move(threads_in))
      , disc(std::move(disc_in))
  {
  }
};

class BrokerCore final : public BrokerComponentNotify
{
public:
  BrokerCore();
  virtual ~BrokerCore();

  // Some utility functions
  static constexpr bool IsControllerBroadcastUID(const RdmUid& uid);
  static constexpr bool IsDeviceBroadcastUID(const RdmUid& uid);
  static bool IsDeviceManuBroadcastUID(const RdmUid& uid, uint16_t& manu);
  bool IsValidControllerDestinationUID(const RdmUid& uid) const;
  bool IsValidDeviceDestinationUID(const RdmUid& uid) const;

  rdmnet::BrokerLog* GetLog() const { return log_; }
  rdmnet::BrokerSettings GetSettings() const { return settings_; }

  bool Startup(const rdmnet::BrokerSettings& settings, rdmnet::BrokerNotify* notify, rdmnet::BrokerLog* log,
               BrokerComponents components = BrokerComponents());
  void Shutdown();
  void Tick();

private:
  // These are never modified between startup and shutdown, so they don't need to be locked.
  bool started_{false};
  bool service_registered_{false};

  // Attributes of this broker instance
  rdmnet::BrokerSettings settings_;
  RdmUid my_uid_{};

  // External (non-owned) components
  rdmnet::BrokerLog* log_{nullptr};        // Enables the broker to log messages
  rdmnet::BrokerNotify* notify_{nullptr};  // Enables the broker to notify application code when something has changed

  // Owned components
  BrokerComponents components_;

  // The list of connected clients, indexed by the connection handle
  std::map<rdmnet_conn_t, std::shared_ptr<BrokerClient>> clients_;
  // Protects the list of clients and uid lookup, but not the data in the clients themselves.
  mutable etcpal::RwLock client_lock_;

  // The state data for each controller, indexed by its connection handle.
  std::map<rdmnet_conn_t, std::shared_ptr<RPTController>> controllers_;
  // The set of devices, indexed by the connection handle.
  std::map<rdmnet_conn_t, std::shared_ptr<RPTDevice>> devices_;

  std::set<rdmnet_conn_t> clients_to_destroy_;

  static std::set<etcpal::IpAddr> CombineMacsAndInterfaces(const std::set<etcpal::IpAddr>& interfaces,
                                                           const std::set<etcpal::MacAddr>& macs);
  etcpal_socket_t StartListening(const etcpal::IpAddr& ip, uint16_t& port);
  bool StartBrokerServices();
  void StopBrokerServices();

  // RdmnetCoreLibraryNotify messages
  virtual void HandleRdmnetConnDisconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo& disconn_info) override;
  virtual void HandleRdmnetConnMsgReceived(rdmnet_conn_t handle, const RdmnetMessage& msg) override;

  // BrokerSocketNotify messages
  virtual void HandleSocketDataReceived(rdmnet_conn_t conn_handle, const uint8_t* data, size_t data_size) override;
  virtual void HandleSocketClosed(rdmnet_conn_t conn_handle, bool graceful) override;

  // BrokerThreadNotify messages
  virtual bool HandleNewConnection(etcpal_socket_t new_sock, const etcpal::SockAddr& addr) override;
  virtual bool ServiceClients() override;

  // BrokerDiscoveryManagerNotify messages
  virtual void HandleBrokerRegistered(const std::string& scope, const std::string& requested_service_name,
                                      const std::string& assigned_service_name) override;
  virtual void HandleOtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info) override;
  virtual void HandleOtherBrokerLost(const std::string& scope, const std::string& service_name) override;
  virtual void HandleBrokerRegisterError(const std::string& scope, const std::string& requested_service_name,
                                         int platform_error) override;
  virtual void HandleScopeMonitorError(const std::string& scope, int platform_error) override;

  void GetConnSnapshot(std::vector<rdmnet_conn_t>& conns, bool include_devices, bool include_controllers,
                       bool include_unknown, uint16_t manufacturer_filter = 0xffff);

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

#endif  // BROKER_CORE_H_
