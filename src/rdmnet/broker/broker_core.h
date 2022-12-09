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

#ifndef BROKER_CORE_H_
#define BROKER_CORE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/rwlock.h"
#include "etcpal/cpp/timer.h"
#include "etcpal/socket.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/cpp/broker.h"
#include "broker_client.h"
#include "broker_discovery.h"
#include "broker_responder.h"
#include "broker_socket_manager.h"
#include "broker_threads.h"
#include "broker_uid_manager.h"
#include "broker_util.h"

class BrokerComponentNotify : public BrokerSocketNotify, public BrokerThreadNotify, public BrokerDiscoveryNotify
{
};

// A set of components of broker functionality, separated to facilitate testing and dependency
// injection.
struct BrokerComponents final
{
  // Manages the broker's readable sockets
  std::unique_ptr<BrokerSocketManager> socket_mgr;
  // Manages the broker's worker threads
  std::unique_ptr<BrokerThreadInterface> threads;
  // Handles DNS discovery of the broker
  std::unique_ptr<BrokerDiscoveryInterface> disc;
  // Handles the Broker's dynamic UID assignment functionality
  BrokerUidManager uids;
  // Generates handles for broker clients
  ClientHandleGenerator handle_generator;
  // The Broker's RDM responder
  BrokerResponder responder;

  void SetNotify(BrokerComponentNotify* notify)
  {
    socket_mgr->SetNotify(notify);
    threads->SetNotify(notify);
    disc->SetNotify(notify);
  }

  BrokerComponents(std::unique_ptr<BrokerSocketManager>   socket_mgr_in = CreateBrokerSocketManager(),
                   std::unique_ptr<BrokerThreadInterface> threads_in =
                       std::unique_ptr<BrokerThreadInterface>(new BrokerThreadManager),
                   std::unique_ptr<BrokerDiscoveryInterface> disc_in =
                       std::unique_ptr<BrokerDiscoveryInterface>(new BrokerDiscoveryManager))
      : socket_mgr(std::move(socket_mgr_in)), threads(std::move(threads_in)), disc(std::move(disc_in))
  {
  }
};

class BrokerCore final : public BrokerComponentNotify
{
public:
  BrokerCore();
  virtual ~BrokerCore();

  etcpal::Logger*                 logger() const { return log_; }
  const rdmnet::Broker::Settings& settings() const { return settings_; }

  etcpal::Error Startup(const rdmnet::Broker::Settings& settings,
                        rdmnet::Broker::NotifyHandler*  notify,
                        etcpal::Logger*                 logger,
                        BrokerComponents                components = BrokerComponents());
  void          Shutdown(rdmnet_disconnect_reason_t disconnect_reason);
  etcpal::Error ChangeScope(const std::string& new_scope, rdmnet_disconnect_reason_t disconnect_reason);

  // Some utility functions
  static bool IsDeviceManuBroadcastUID(const RdmUid& uid, uint16_t& manu);
  bool        IsValidControllerDestinationUID(const RdmUid& uid) const;
  bool        IsValidDeviceDestinationUID(const RdmUid& uid) const;

  // Test/debug
  size_t GetNumClients() const;

private:
  using BrokerClientMap = std::unordered_map<BrokerClient::Handle, std::unique_ptr<BrokerClient>>;
  using RptClientMap = std::unordered_map<BrokerClient::Handle, RPTClient*>;
  using RptControllerMap = std::unordered_map<BrokerClient::Handle, RPTController*>;
  using RptDeviceMap = std::unordered_map<BrokerClient::Handle, RPTDevice*>;

  // These are never modified between startup and shutdown, so they don't need to be locked.
  bool started_{false};
  bool service_registered_{false};

  // Attributes of this broker instance
  rdmnet::Broker::Settings  settings_;
  std::vector<unsigned int> listen_interfaces_;
  std::vector<std::string>  listen_interface_ips_;
  rdm::Uid                  my_uid_;

  // External (non-owned) components

  // Enables the broker to log messages
  etcpal::Logger* log_{nullptr};
  // Enables the broker to notify application code when something has changed
  rdmnet::Broker::NotifyHandler* notify_{nullptr};

  // Owned components
  BrokerComponents components_;

  static constexpr uint32_t kClientDestroyIntervalMs = 200;
  etcpal::Timer             client_destroy_timer_{kClientDestroyIntervalMs};

  // The list of connected clients, indexed by the connection handle
  BrokerClientMap clients_;
  // Protects the list of clients and uid lookup, but not the data in the clients themselves.
  mutable etcpal::RwLock client_lock_;

  // We also divide the clients into subgroups by client protocol and by RPT role.
  RptClientMap     rpt_clients_;
  RptControllerMap controllers_;
  RptDeviceMap     devices_;

  std::unordered_set<BrokerClient::Handle> clients_to_destroy_;

  std::set<etcpal::IpAddr>          GetInterfaceAddrs(const std::vector<std::string>& interfaces);
  etcpal::Expected<etcpal_socket_t> StartListening(const etcpal::IpAddr& ip, uint16_t& port);
  etcpal::Error                     StartBrokerServices();
  void                              StopBrokerServices(rdmnet_disconnect_reason_t disconnect_reason);

  // BrokerThreadNotify messages
  virtual bool HandleNewConnection(etcpal_socket_t new_sock, const etcpal::SockAddr& addr) override;
  virtual bool ServiceClients() override;

  // BrokerDiscoveryManagerNotify messages
  virtual void HandleBrokerRegistered(const std::string& assigned_service_name) override;
  virtual void HandleOtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info) override;
  virtual void HandleOtherBrokerLost(const std::string& scope, const std::string& service_name) override;
  virtual void HandleBrokerRegisterError(int platform_error) override;

  std::vector<BrokerClient::Handle> GetClientSnapshot(bool     include_devices,
                                                      bool     include_controllers,
                                                      bool     include_unknown,
                                                      uint16_t manufacturer_filter = 0xffff);

  void MarkClientForDestruction(BrokerClient::Handle       client,
                                const ClientDestroyAction& destroy_action = ClientDestroyAction::DoNothing());
  bool MarkLockedClientForDestruction(BrokerClient&              client,
                                      const ClientDestroyAction& destroy_action = ClientDestroyAction::DoNothing());
  void DestroyMarkedClients();
  void DestroyMarkedClientsLocked();

  // BrokerSocketNotify messages
  virtual void                HandleSocketClosed(BrokerClient::Handle client_handle, bool graceful) override;
  virtual HandleMessageResult HandleSocketMessageReceived(BrokerClient::Handle client_handle,
                                                          const RdmnetMessage& message) override;

  // Message processing and sending functions
  void                   ProcessConnectRequest(BrokerClient::Handle client_handle, const BrokerClientConnectMsg* cmsg);
  bool                   ProcessRPTConnectRequest(BrokerClient::Handle        client_handle,
                                                  const RdmnetRptClientEntry& client_entry,
                                                  rdmnet_connect_status_t&    connect_status);
  bool                   ResolveNewClientUid(BrokerClient::Handle     client_handle,
                                             RdmnetRptClientEntry&    client_entry,
                                             rdmnet_connect_status_t& connect_status);
  HandleMessageResult    ProcessRPTMessage(BrokerClient::Handle client_handle, const RdmnetMessage* msg);
  HandleMessageResult    RouteRPTMessage(BrokerClient::Handle client_handle, const RdmnetMessage* msg);
  ClientPushResult       PushToAllControllers(BrokerClient::Handle sender_handle, const RdmnetMessage* msg);
  ClientPushResult       PushToAllDevices(BrokerClient::Handle sender_handle, const RdmnetMessage* msg);
  ClientPushResult       PushToManuSpecificDevices(BrokerClient::Handle sender_handle,
                                                   const RdmnetMessage* msg,
                                                   uint16_t             manu);
  ClientPushResult       PushToSpecificRptClient(BrokerClient::Handle sender_handle, const RdmnetMessage* msg);
  RptClientMap::iterator FindRptClient(const RdmUid& uid);
  HandleMessageResult    HandleRPTClientBadPushResult(const RptHeader& header, ClientPushResult result);
  void                   ResetClientHeartbeatTimer(BrokerClient::Handle client_handle);

  void SendRDMBrokerResponse(BrokerClient::Handle client_handle,
                             const RPTMessageRef& msg,
                             uint8_t              response_type,
                             uint8_t              command_class,
                             uint16_t             param_id,
                             uint8_t              packedlen,
                             uint8_t*             pdata);
  void SendClientList(BrokerClient::Handle client_handle);
  void SendRptClientList(BrokerMessage& bmsg, RPTClient& to_cli);
  void SendEptClientList(BrokerMessage& bmsg, EPTClient& to_cli);
  void SendClientsAdded(BrokerClient::Handle handle_to_ignore, std::vector<RdmnetRptClientEntry>& entries);
  void SendClientsRemoved(std::vector<RdmnetRptClientEntry>& entries);
  HandleMessageResult SendStatus(RPTController*     controller,
                                 const RptHeader&   header,
                                 rpt_status_code_t  status_code,
                                 const std::string& status_str = std::string());
};

#endif  // BROKER_CORE_H_
