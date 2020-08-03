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

// The core broker implementation. Contains the private implementation of Broker functionality.

#include "broker_core.h"

#include <algorithm>
#include <cstring>
#include <cstddef>
#include <iterator>
#include "etcpal/cpp/error.h"
#include "etcpal/netint.h"
#include "etcpal/pack.h"
#include "rdmnet/version.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/connection.h"
#include "broker_client.h"
#include "broker_responder.h"
#include "broker_util.h"

#define BROKER_LOG_PREFIX "RDMnet Broker: "

// Log only if the log_ instance is present.
#define BROKER_LOG(pri, ...) \
  if (log_)                  \
  log_->Log(pri, BROKER_LOG_PREFIX __VA_ARGS__)

#define BROKER_LOG_EMERG(...) BROKER_LOG(ETCPAL_LOG_EMERG, __VA_ARGS__)
#define BROKER_LOG_ALERT(...) BROKER_LOG(ETCPAL_LOG_ALERT, __VA_ARGS__)
#define BROKER_LOG_CRIT(...) BROKER_LOG(ETCPAL_LOG_CRIT, __VA_ARGS__)
#define BROKER_LOG_ERR(...) BROKER_LOG(ETCPAL_LOG_ERR, __VA_ARGS__)
#define BROKER_LOG_WARNING(...) BROKER_LOG(ETCPAL_LOG_WARNING, __VA_ARGS__)
#define BROKER_LOG_NOTICE(...) BROKER_LOG(ETCPAL_LOG_NOTICE, __VA_ARGS__)
#define BROKER_LOG_INFO(...) BROKER_LOG(ETCPAL_LOG_INFO, __VA_ARGS__)
#define BROKER_LOG_DEBUG(...) BROKER_LOG(ETCPAL_LOG_DEBUG, __VA_ARGS__)

#define BROKER_CAN_LOG(pri) (log_ && log_->CanLog(pri))

/*************************** Function definitions ****************************/

BrokerCore::BrokerCore()
{
}

BrokerCore::~BrokerCore()
{
  if (started_)
    Shutdown(kRdmnetDisconnectShutdown);
}

etcpal::Error BrokerCore::Startup(const rdmnet::Broker::Settings& settings,
                                  rdmnet::Broker::NotifyHandler*  notify,
                                  etcpal::Logger*                 logger,
                                  BrokerComponents                components)
{
  if (!started_)
  {
    // Check the settings for validity
    if (!settings.IsValid())
      return kEtcPalErrInvalid;

    if (!rc_initialized())
      return kEtcPalErrNotInit;

    // Save members
    settings_ = settings;
    notify_ = notify;
    log_ = logger;
    components_ = std::move(components);
    components_.SetNotify(this);
    components_.handle_generator.SetValueInUseFunc(
        [&](BrokerClient::Handle handle) { return clients_.find(handle) != clients_.end(); });

    // Generate IDs if necessary
    my_uid_ = settings.uid;
    if (settings.uid.IsDynamicUidRequest())
    {
      my_uid_.SetDeviceId(1);
      components_.uids.SetNextDeviceId(2);
    }

    if (!components_.socket_mgr->Startup())
    {
      return kEtcPalErrSys;
    }

    auto err = StartBrokerServices();
    if (!err)
    {
      components_.socket_mgr->Shutdown();
      return err;
    }

    started_ = true;

    components_.disc->RegisterBroker(settings_, listen_interfaces_);

    BROKER_LOG_INFO("%s RDMnet Broker Version %s", settings_.dns.manufacturer.c_str(), RDMNET_VERSION_STRING);
    BROKER_LOG_INFO("Broker starting at scope \"%s\", listening on port %d.", settings_.scope.c_str(),
                    settings_.listen_port);

    if (!settings_.listen_interfaces.empty())
    {
      BROKER_LOG_INFO("Listening on manually-specified network interfaces:");
      for (const auto& netint : settings.listen_interfaces)
      {
        BROKER_LOG_INFO("%s", netint.c_str());
      }
    }
  }

  return kEtcPalErrOk;
}

// Call before destruction to gracefully close
void BrokerCore::Shutdown(rdmnet_disconnect_reason_t disconnect_reason)
{
  if (started_)
  {
    components_.disc->UnregisterBroker();
    StopBrokerServices(disconnect_reason);
    components_.socket_mgr->Shutdown();

    started_ = false;
  }
}

etcpal::Error BrokerCore::ChangeScope(const std::string& /*new_scope*/,
                                      rdmnet_disconnect_reason_t /*disconnect_reason*/)
{
  // TODO
  return kEtcPalErrNotImpl;
}

bool BrokerCore::IsDeviceManuBroadcastUID(const RdmUid& uid, uint16_t& manu)
{
  if (RDMNET_UID_IS_DEVICE_MANU_BROADCAST(&uid))
  {
    manu = RDMNET_DEVICE_BROADCAST_MANU_ID(&uid);
    return true;
  }
  return false;
}

bool BrokerCore::IsValidControllerDestinationUID(const RdmUid& uid) const
{
  if (RDMNET_UID_IS_CONTROLLER_BROADCAST(&uid) || (uid == my_uid_))
    return true;

  // TODO this should only check devices
  BrokerClient::Handle tmp;
  return components_.uids.UidToHandle(uid, tmp);
}

bool BrokerCore::IsValidDeviceDestinationUID(const RdmUid& uid) const
{
  if (RDMNET_UID_IS_CONTROLLER_BROADCAST(&uid))
    return true;

  // TODO this should only check controllers
  BrokerClient::Handle tmp;
  return components_.uids.UidToHandle(uid, tmp);
}

size_t BrokerCore::GetNumClients() const
{
  etcpal::ReadGuard client_read(client_lock_);
  return clients_.size();
}

// Convert a set of strings representing network interface names to a set of all IP addresses
// currently assigned to those interfaces.
//
// Side-effects: Sets the listen_interfaces_ member with a list of interface indexes also resolved
// from the interfaces parameter.
std::set<etcpal::IpAddr> BrokerCore::GetInterfaceAddrs(const std::vector<std::string>& interfaces)
{
  std::set<etcpal::IpAddr> to_return;
  std::set<unsigned int>   interface_indexes;

  size_t num_netints = etcpal_netint_get_num_interfaces();
  for (const auto& netint_name : interfaces)
  {
    const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();
    for (const EtcPalNetintInfo* netint = netint_list; netint < netint_list + num_netints; ++netint)
    {
      if (std::strcmp(netint_name.c_str(), netint->id) == 0)
      {
        to_return.insert(netint->addr);
        interface_indexes.insert(netint->index);
        // There could be multiple addresses that have this name, we don't break here so we listen
        // on all of them.
      }
    }
  }

  if (!interface_indexes.empty())
  {
    listen_interfaces_.reserve(interface_indexes.size());
    std::transform(interface_indexes.begin(), interface_indexes.end(), std::back_inserter(listen_interfaces_),
                   [](unsigned int val) { return val; });
  }

  return to_return;
}

etcpal::Expected<etcpal_socket_t> BrokerCore::StartListening(const etcpal::IpAddr& ip, uint16_t& port)
{
  etcpal::SockAddr addr(ip, port);

  etcpal_socket_t listen_sock;
  etcpal::Error   res = etcpal_socket(addr.IsV4() ? ETCPAL_AF_INET : ETCPAL_AF_INET6, ETCPAL_SOCK_STREAM, &listen_sock);
  if (!res)
  {
    BROKER_LOG_ERR("Broker: Failed to create listen socket with error: %s.", res.ToCString());
    return res.code();
  }

  if (ip.IsV6())
  {
    int sockopt_val = (ip.IsWildcard() ? 0 : 1);
    res = etcpal_setsockopt(listen_sock, ETCPAL_IPPROTO_IPV6, ETCPAL_IPV6_V6ONLY, &sockopt_val, sizeof(int));
    if (!res)
    {
      etcpal_close(listen_sock);
      BROKER_LOG_ERR("Broker: Failed to set V6ONLY socket option on listen socket: %s.", res.ToCString());
      return res.code();
    }
  }

  res = etcpal_bind(listen_sock, &addr.get());
  if (!res)
  {
    etcpal_close(listen_sock);
    if (BROKER_CAN_LOG(ETCPAL_LOG_ERR))
    {
      log_->Error("Broker: Bind to %s failed on listen socket with error: %s.", addr.ToString().c_str(),
                  res.ToCString());
    }
    return res.code();
  }

  if (port == 0)
  {
    // Get the ephemeral port number we were assigned and which we will use for all other
    // applicable network interfaces.
    res = etcpal_getsockname(listen_sock, &addr.get());
    if (res)
    {
      port = addr.port();
    }
    else
    {
      etcpal_close(listen_sock);
      BROKER_LOG_ERR("Broker: Failed to get ephemeral port assigned to listen socket: %s", res.ToCString());
      return res.code();
    }
  }

  res = etcpal_listen(listen_sock, 0);
  if (!res)
  {
    etcpal_close(listen_sock);
    BROKER_LOG_ERR("Broker: Listen failed on listen socket with error: %s.", res.ToCString());
    return res.code();
  }
  return listen_sock;
}

etcpal::Error BrokerCore::StartBrokerServices()
{
  etcpal::Error res = components_.threads->AddClientServiceThread();
  if (!res)
    return res;

  auto final_listen_addrs = GetInterfaceAddrs(settings_.listen_interfaces);
  if (final_listen_addrs.empty())
  {
    // Listen on in6addr_any
    const auto any_addr = etcpal::IpAddr::WildcardV6();
    auto       listen_sock = StartListening(any_addr, settings_.listen_port);
    if (listen_sock)
    {
      res = components_.threads->AddListenThread(*listen_sock);
      if (!res)
        etcpal_close(*listen_sock);
    }
    else
    {
      BROKER_LOG_CRIT("Could not bind a wildcard listening socket.");
      res = listen_sock.error();
    }
  }
  else
  {
    // Listen on a specific set of interfaces supplied by the library user
    auto addr_iter = final_listen_addrs.begin();
    while (addr_iter != final_listen_addrs.end())
    {
      auto listen_sock = StartListening(*addr_iter, settings_.listen_port);
      if (listen_sock)
      {
        if (components_.threads->AddListenThread(*listen_sock))
        {
          ++addr_iter;
        }
        else
        {
          etcpal_close(*listen_sock);
          addr_iter = final_listen_addrs.erase(addr_iter);
        }
      }
      else
      {
        addr_iter = final_listen_addrs.erase(addr_iter);
      }
    }

    // Errors on some interfaces are tolerated as long as we have at least one to listen on.
    if (final_listen_addrs.empty())
    {
      BROKER_LOG_CRIT("Could not listen on any provided IP addresses.");
      res = kEtcPalErrSys;
    }
  }

  return res;
}

void BrokerCore::StopBrokerServices(rdmnet_disconnect_reason_t disconnect_reason)
{
  components_.threads->StopThreads();

  // No new connections coming in, manually shut down the existing ones.
  auto clients = GetClientSnapshot(true, true, true);

  for (auto& client : clients)
    MarkClientForDestruction(client, ClientDestroyAction::SendDisconnect(disconnect_reason));

  DestroyMarkedClients();
}

bool BrokerCore::HandleNewConnection(etcpal_socket_t new_sock, const etcpal::SockAddr& addr)
{
  if (etcpal_setblocking(new_sock, false) != kEtcPalErrOk)
  {
    if (BROKER_CAN_LOG(ETCPAL_LOG_ERR))
    {
      log_->Error("Error translating socket into non-blocking socket for new connection from %s",
                  addr.ToString().c_str());
    }
    return false;
  }

  if (BROKER_CAN_LOG(ETCPAL_LOG_INFO))
    log_->Info("Creating a new connection for address %s", addr.ToString().c_str());

  BrokerClient::Handle new_handle = BrokerClient::kInvalidHandle;
  bool                 result = false;

  {  // Client write lock scope
    etcpal::WriteGuard client_write(client_lock_);

    new_handle = components_.handle_generator.GetClientHandle();

    if (settings_.limits.connections == 0 ||
        (clients_.size() <= settings_.limits.connections + settings_.limits.reject_connections))
    {
      std::unique_ptr<BrokerClient> client(new BrokerClient(new_handle, new_sock));

      // Before inserting the connection, make sure we can attach the socket.
      if (client)
      {
        client->addr = addr;
        clients_.insert(std::make_pair(new_handle, std::move(client)));
        components_.socket_mgr->AddSocket(new_handle, new_sock);
        result = true;
      }
    }
  }

  if (result)
  {
    BROKER_LOG_DEBUG("New connection created with handle %d", new_handle);
  }
  else
  {
    BROKER_LOG_ERR("New connection failed");
  }

  return result;
}

// Process each controller queue, sending out the next message from each queue if devices are
// available. Also sends connect reply, error and status messages generated asynchronously to
// devices.  Return false if no controllers messages were sent.
bool BrokerCore::ServiceClients()
{
  bool result = false;

  {
    etcpal::ReadGuard clients_read(client_lock_);

    for (auto& client : clients_)
    {
      ClientWriteGuard client_write(*client.second);
      if (client.second->TcpConnExpired())
        MarkLockedClientForDestruction(*client.second);
      else
        result |= client.second->Send(settings_.cid);
    }
  }

  if (client_destroy_timer_.IsExpired())
  {
    DestroyMarkedClients();
    client_destroy_timer_.Reset();
  }

  return result;
}

void BrokerCore::HandleBrokerRegistered(const std::string& assigned_service_name)
{
  service_registered_ = true;
  if (settings_.dns.service_instance_name == assigned_service_name)
  {
    BROKER_LOG_INFO("Broker \"%s\" successfully registered at scope \"%s\"", assigned_service_name.c_str(),
                    settings_.scope.c_str());
  }
  else
  {
    BROKER_LOG_INFO("Broker \"%s\" (now named \"%s\") successfully registered at scope \"%s\"",
                    settings_.dns.service_instance_name.c_str(), assigned_service_name.c_str(),
                    settings_.scope.c_str());
  }
}

void BrokerCore::HandleBrokerRegisterError(int platform_specific_error)
{
  BROKER_LOG_CRIT("Broker \"%s\" register error %d at scope \"%s\"", settings_.dns.service_instance_name.c_str(),
                  platform_specific_error, settings_.scope.c_str());
}

void BrokerCore::HandleOtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info)
{
  // If the broker is already registered with DNS-SD, the presence of another broker is an error
  // condition. Otherwise, the system is still usable (this broker will not register)
  int log_pri = (service_registered_ ? ETCPAL_LOG_ERR : ETCPAL_LOG_NOTICE);

  if (BROKER_CAN_LOG(log_pri))
  {
    std::string addrs;
    for (size_t i = 0; i < broker_info.num_listen_addrs; ++i)
    {
      char addr_string[ETCPAL_IP_STRING_BYTES];
      if (kEtcPalErrOk == etcpal_ip_to_string(&broker_info.listen_addrs[i], addr_string))
      {
        addrs.append(addr_string);
        if (i < broker_info.num_listen_addrs - 1)
          addrs.append(", ");
      }
    }

    log_->Log(log_pri, "Broker \"%s\", ip[%s] found at same scope(\"%s\") as this broker.",
              broker_info.service_instance_name, addrs.c_str(), broker_info.scope);
  }
  if (!service_registered_)
  {
    BROKER_LOG(log_pri, "This broker will remain unregistered with DNS-SD until all conflicting brokers are removed.");
    // StopBrokerServices();
  }
}

void BrokerCore::HandleOtherBrokerLost(const std::string& scope, const std::string& service_name)
{
  BROKER_LOG_NOTICE("Conflicting broker %s on scope \"%s\" no longer discovered.", service_name.c_str(), scope.c_str());
}

// Returns the handles of clients that match the criteria.
std::vector<BrokerClient::Handle> BrokerCore::GetClientSnapshot(bool     include_devices,
                                                                bool     include_controllers,
                                                                bool     include_unknown,
                                                                uint16_t manufacturer_filter)
{
  std::vector<BrokerClient::Handle> client_handles;

  etcpal::ReadGuard client_read(client_lock_);

  if (!clients_.empty())
  {
    // We'll just do a bulk reserve.  The actual vector may take up less.
    client_handles.reserve(clients_.size());

    for (const auto& client : clients_)
    {
      // TODO EPT
      if (client.second)
      {
        RPTClient* rpt = static_cast<RPTClient*>(client.second.get());
        if (((include_devices && (rpt->client_type == kRPTClientTypeDevice)) ||
             (include_controllers && (rpt->client_type == kRPTClientTypeController)) ||
             (include_unknown && (rpt->client_type == kRPTClientTypeUnknown))) &&
            ((manufacturer_filter == 0xffff) || (manufacturer_filter == rpt->uid.manu)))
        {
          client_handles.push_back(client.first);
        }
      }
    }
  }

  return client_handles;
}

// This function grabs a read lock on client_lock_.
// Optionally sends a RDMnet-level message to the client before destroying it.
// Also removes the client's UID from the BrokerUidManager if it's an RPT client.
void BrokerCore::MarkClientForDestruction(BrokerClient::Handle client_handle, ClientDestroyAction destroy_action)
{
  bool log_message = false;

  {  // Client read lock and destroy lock scope
    etcpal::ReadGuard clients_read(client_lock_);

    auto client = clients_.find(client_handle);
    if ((client != clients_.end()) && client->second)
    {
      ClientWriteGuard client_write(*client->second);
      log_message = MarkLockedClientForDestruction(*client->second, destroy_action);
    }
  }

  if (log_message)
  {
    BROKER_LOG_DEBUG("Client %d marked for destruction", client_handle);
  }
}

// This function marks a client for destruction when it is already write-locked.
// Optionally sends a RDMnet-level message to the client before destroying it.
// Also removes the client's UID from the BrokerUidManager if it's an RPT client.
bool BrokerCore::MarkLockedClientForDestruction(BrokerClient& client, ClientDestroyAction destroy_action)
{
  destroy_action.Apply(my_uid_, settings_.cid, client);

  if (client.client_protocol == E133_CLIENT_PROTOCOL_RPT)
  {
    RPTClient* rptcli = static_cast<RPTClient*>(&client);
    components_.uids.RemoveUid(rptcli->uid);
  }

  return clients_to_destroy_.insert(client.handle).second;
}

// These functions will take a write lock on client_lock_.
void BrokerCore::DestroyMarkedClients()
{
  etcpal::WriteGuard                clients_write(client_lock_);
  std::vector<RdmnetRptClientEntry> rpt_entries;

  if (!clients_to_destroy_.empty())
  {
    for (auto to_destroy : clients_to_destroy_)
    {
      auto client = clients_.find(to_destroy);
      if (client != clients_.end())
      {
        if (client->second->socket != ETCPAL_SOCKET_INVALID)
          components_.socket_mgr->RemoveSocket(client->second->handle);

        if (client->second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
        {
          RPTClient* rptcli = static_cast<RPTClient*>(client->second.get());
          rpt_clients_.erase(to_destroy);
          if (rptcli->client_type == kRPTClientTypeController)
            controllers_.erase(to_destroy);
          else if (rptcli->client_type == kRPTClientTypeDevice)
            devices_.erase(to_destroy);

          rpt_entries.emplace_back();
          RdmnetRptClientEntry& entry = rpt_entries.back();
          entry.cid = rptcli->cid.get();
          entry.uid = rptcli->uid;
          entry.type = rptcli->client_type;
          entry.binding_cid = rptcli->binding_cid.get();
        }
        clients_.erase(client);

        BROKER_LOG_INFO("Removing client %d marked for destruction.", to_destroy);
        BROKER_LOG_DEBUG("Clients: %zu Controllers: %zu Devices: %zu", clients_.size(), controllers_.size(),
                         devices_.size());
      }
    }
    clients_to_destroy_.clear();
  }

  if (!rpt_entries.empty())
    SendClientsRemoved(rpt_entries);
}

void BrokerCore::HandleSocketClosed(BrokerClient::Handle client_handle, bool /*graceful*/)
{
  MarkClientForDestruction(client_handle, ClientDestroyAction::MarkSocketInvalid());
}

void BrokerCore::HandleSocketMessageReceived(BrokerClient::Handle client_handle, const RdmnetMessage& message)
{
  // Any well-formed Root Layer PDU message resets the heartbeat timer.
  ResetClientHeartbeatTimer(client_handle);

  switch (message.vector)
  {
    case ACN_VECTOR_ROOT_BROKER: {
      const BrokerMessage* bmsg = RDMNET_GET_BROKER_MSG(&message);
      switch (bmsg->vector)
      {
        case VECTOR_BROKER_CONNECT:
          ProcessConnectRequest(client_handle, BROKER_GET_CLIENT_CONNECT_MSG(bmsg));
          break;
        case VECTOR_BROKER_FETCH_CLIENT_LIST:
          SendClientList(client_handle);
          BROKER_LOG_DEBUG("Received Fetch Client List from Client %d; sending Client List.", client_handle);
          break;
        case VECTOR_BROKER_DISCONNECT:
          BROKER_LOG_DEBUG("Client %d sent disconnect message with reason '%s'.", client_handle,
                           rdmnet_disconnect_reason_to_string(BROKER_GET_DISCONNECT_MSG(bmsg)->disconnect_reason));
          MarkClientForDestruction(client_handle);
        case VECTOR_BROKER_NULL:
          // Do nothing - the heartbeat timer is already reset
          break;
        default:
          BROKER_LOG_DEBUG("Received Broker PDU with unknown or unhandled vector %d", bmsg->vector);
          break;
      }
      break;
    }

    case ACN_VECTOR_ROOT_RPT:
      ProcessRPTMessage(client_handle, &message);
      break;

    default:
      BROKER_LOG_DEBUG("Received Root Layer PDU with unknown or unhandled vector %d", message.vector);
      break;
  }
}

void BrokerCore::ProcessConnectRequest(BrokerClient::Handle client_handle, const BrokerClientConnectMsg* cmsg)
{
  bool                    deny_connection = true;
  rdmnet_connect_status_t connect_status = kRdmnetConnectScopeMismatch;

  if ((cmsg->e133_version <= E133_VERSION) && (cmsg->scope == settings_.scope))
  {
    switch (cmsg->client_entry.client_protocol)
    {
      case E133_CLIENT_PROTOCOL_RPT:
        deny_connection =
            !ProcessRPTConnectRequest(client_handle, *(GET_RPT_CLIENT_ENTRY(&cmsg->client_entry)), connect_status);
        break;
      // TODO EPT
      default:
        connect_status = kRdmnetConnectInvalidClientEntry;
        break;
    }
  }

  if (deny_connection)
  {
    // Clean up this client.
    BROKER_LOG_INFO("Rejecting connection from client %d: %s", client_handle,
                    rdmnet_connect_status_to_string(connect_status));
    MarkClientForDestruction(client_handle, ClientDestroyAction::SendConnectReply(connect_status));
  }
}

bool BrokerCore::ProcessRPTConnectRequest(BrokerClient::Handle        client_handle,
                                          const RdmnetRptClientEntry& client_entry,
                                          rdmnet_connect_status_t&    connect_status)
{
  bool continue_adding = true;
  // We need to make a copy of the data because we might be changing the UID value
  RdmnetRptClientEntry updated_client_entry = client_entry;

  etcpal::WriteGuard clients_write(client_lock_);
  RPTClient*         new_client = nullptr;

  if ((settings_.limits.connections > 0) && (clients_.size() >= settings_.limits.connections))
  {
    connect_status = kRdmnetConnectCapacityExceeded;
    continue_adding = false;
  }

  continue_adding = ResolveNewClientUid(client_handle, updated_client_entry, connect_status);

  if (continue_adding)
  {
    // If it's a controller, add it to the controller queues -- unless
    // we've hit our maximum number of controllers
    if (updated_client_entry.type == kRPTClientTypeController)
    {
      if ((settings_.limits.controllers > 0) && (controllers_.size() >= settings_.limits.controllers))
      {
        connect_status = kRdmnetConnectCapacityExceeded;
        continue_adding = false;
        components_.uids.RemoveUid(updated_client_entry.uid);
      }
      else
      {
        std::unique_ptr<RPTController> controller(
            new RPTController(settings_.limits.controller_messages, updated_client_entry, *clients_[client_handle]));
        if (controller)
        {
          new_client = controller.get();
          controllers_.insert(std::make_pair(client_handle, controller.get()));
          rpt_clients_.insert(std::make_pair(client_handle, controller.get()));
          clients_[client_handle] = std::move(controller);
        }
      }
    }
    // If it's a device, add it to the device states -- unless we've hit our maximum number of
    // devices
    else if (updated_client_entry.type == kRPTClientTypeDevice)
    {
      if ((settings_.limits.devices > 0) && (devices_.size() >= settings_.limits.devices))
      {
        connect_status = kRdmnetConnectCapacityExceeded;
        continue_adding = false;
        components_.uids.RemoveUid(updated_client_entry.uid);
      }
      else
      {
        std::unique_ptr<RPTDevice> device(
            new RPTDevice(settings_.limits.device_messages, updated_client_entry, *clients_[client_handle]));
        if (device)
        {
          new_client = device.get();
          devices_.insert(std::make_pair(client_handle, device.get()));
          rpt_clients_.insert(std::make_pair(client_handle, device.get()));
          clients_[client_handle] = std::move(device);
        }
      }
    }
  }

  if (continue_adding && new_client)
  {
    // Send the connect reply
    BrokerMessage msg;
    msg.vector = VECTOR_BROKER_CONNECT_REPLY;
    BrokerConnectReplyMsg* creply = BROKER_GET_CONNECT_REPLY_MSG(&msg);
    creply->connect_status = kRdmnetConnectOk;
    creply->e133_version = E133_VERSION;
    creply->broker_uid = my_uid_.get();
    creply->client_uid = updated_client_entry.uid;
    new_client->Push(settings_.cid, msg);

    if (BROKER_CAN_LOG(ETCPAL_LOG_INFO))
    {
      BROKER_LOG_INFO("Successfully processed RPT Connect request from %s (connection %d), UID %04x:%08x",
                      new_client->client_type == kRPTClientTypeController ? "Controller" : "Device", client_handle,
                      new_client->uid.manu, new_client->uid.id);
    }

    // Update everyone
    std::vector<RdmnetRptClientEntry> entries;
    entries.push_back(updated_client_entry);
    SendClientsAdded(client_handle, entries);
  }
  return continue_adding;
}

bool BrokerCore::ResolveNewClientUid(BrokerClient::Handle     client_handle,
                                     RdmnetRptClientEntry&    client_entry,
                                     rdmnet_connect_status_t& connect_status)
{
  if (RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&client_entry.uid))
  {
    BrokerUidManager::AddResult add_result =
        components_.uids.AddDynamicUid(client_handle, client_entry.cid, client_entry.uid);
    switch (add_result)
    {
      case BrokerUidManager::AddResult::kOk:
        return true;
      case BrokerUidManager::AddResult::kDuplicateId:
        connect_status = kRdmnetConnectDuplicateUid;
        return false;
      case BrokerUidManager::AddResult::kCapacityExceeded:
      default:
        connect_status = kRdmnetConnectCapacityExceeded;
        return false;
    }
  }
  else if (RDMNET_UID_IS_STATIC(&client_entry.uid))
  {
    BrokerUidManager::AddResult add_result = components_.uids.AddStaticUid(client_handle, client_entry.uid);
    switch (add_result)
    {
      case BrokerUidManager::AddResult::kOk:
        return true;
      case BrokerUidManager::AddResult::kDuplicateId:
        connect_status = kRdmnetConnectDuplicateUid;
        return false;
      case BrokerUidManager::AddResult::kCapacityExceeded:
      default:
        connect_status = kRdmnetConnectCapacityExceeded;
        return false;
    }
  }
  else
  {
    // Client sent an invalid UID of some kind, like a bad dynamic UID request or a broadcast value
    connect_status = kRdmnetConnectInvalidUid;
    return false;
  }
}

void BrokerCore::ProcessRPTMessage(BrokerClient::Handle client_handle, const RdmnetMessage* msg)
{
  etcpal::ReadGuard clients_read(client_lock_);

  const RptMessage* rptmsg = RDMNET_GET_RPT_MSG(msg);
  bool              route_msg = false;
  auto              client = clients_.find(client_handle);

  if ((client != clients_.end()) && client->second)
  {
    ClientWriteGuard client_write(*client->second);

    client->second->MessageReceived();

    if (client->second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
    {
      RPTClient* rptcli = static_cast<RPTClient*>(client->second.get());

      switch (rptmsg->vector)
      {
        case VECTOR_RPT_REQUEST:
          if (rptcli->client_type == kRPTClientTypeController)
          {
            RPTController* controller = static_cast<RPTController*>(rptcli);
            if (!IsValidControllerDestinationUID(rptmsg->header.dest_uid))
            {
              SendStatus(controller, rptmsg->header, kRptStatusUnknownRptUid);
              BROKER_LOG_DEBUG(
                  "Received Request PDU addressed to invalid or not found UID %04x:%08x from Controller %d",
                  rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, client_handle);
            }
            else if (RPT_GET_RDM_BUF_LIST(rptmsg)->num_rdm_buffers > 1)
            {
              // There should only ever be one RDM command in an RPT request.
              SendStatus(controller, rptmsg->header, kRptStatusInvalidMessage);
              BROKER_LOG_DEBUG(
                  "Received Request PDU from Controller %d which incorrectly contains multiple RDM Command PDUs",
                  client_handle);
            }
            else
            {
              route_msg = true;
            }
          }
          else
          {
            BROKER_LOG_DEBUG("Received Request PDU from Client %d, which is not an RPT Controller", client_handle);
          }
          break;

        case VECTOR_RPT_STATUS:
          if (rptcli->client_type == kRPTClientTypeDevice)
          {
            if (IsValidDeviceDestinationUID(rptmsg->header.dest_uid))
            {
              if (RPT_GET_STATUS_MSG(rptmsg)->status_code != kRptStatusBroadcastComplete)
                route_msg = true;
              else
                BROKER_LOG_DEBUG("Device %d sent broadcast complete message.", client_handle);
            }
            else
            {
              BROKER_LOG_DEBUG("Received Status PDU addressed to invalid or not found UID %04x:%08x from Device %d",
                               rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, client_handle);
            }
          }
          else
          {
            BROKER_LOG_DEBUG("Received Status PDU from Client %d, which is not an RPT Device", client_handle);
          }
          break;

        case VECTOR_RPT_NOTIFICATION:
          if (rptcli->client_type != kRPTClientTypeUnknown)
          {
            if (IsValidDeviceDestinationUID(rptmsg->header.dest_uid))
            {
              route_msg = true;
            }
            else
            {
              BROKER_LOG_DEBUG(
                  "Received Notification PDU addressed to invalid or not found UID %04x:%08x from Device %d",
                  rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, client_handle);
            }
          }
          else
          {
            BROKER_LOG_DEBUG("Received Notification PDU from Client %d of unknown client type",
                             rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, client_handle);
          }
          break;

        default:
          BROKER_LOG_WARNING("Received RPT PDU with unknown vector %d from Client %d", rptmsg->vector, client_handle);
          break;
      }
    }
  }

  if (route_msg)
  {
    uint16_t             device_manu;
    BrokerClient::Handle dest_client_handle;

    if (RDMNET_UID_IS_CONTROLLER_BROADCAST(&rptmsg->header.dest_uid))
    {
      BROKER_LOG_DEBUG("Broadcasting RPT message from Device %04x:%08x to all Controllers",
                       rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id);
      for (auto controller : controllers_)
      {
        ClientWriteGuard client_write(*controller.second);
        if (!controller.second->Push(client_handle, msg->sender_cid, *rptmsg))
        {
          // TODO disconnect
          BROKER_LOG_ERR("Error pushing to send queue for RPT Controller %d. DEBUG:NOT disconnecting...",
                         controller.first);
        }
      }
    }
    else if (RDMNET_UID_IS_DEVICE_BROADCAST(&rptmsg->header.dest_uid))
    {
      BROKER_LOG_DEBUG("Broadcasting RPT message from Controller %04x:%08x to all Devices",
                       rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id);
      for (auto device : devices_)
      {
        ClientWriteGuard client_write(*device.second);
        if (!device.second->Push(client_handle, msg->sender_cid, *rptmsg))
        {
          // TODO disconnect
          BROKER_LOG_ERR("Error pushing to send queue for RPT Device %d. DEBUG:NOT disconnecting...", device.first);
        }
      }
    }
    else if (IsDeviceManuBroadcastUID(rptmsg->header.dest_uid, device_manu))
    {
      BROKER_LOG_DEBUG("Broadcasting RPT message from Controller %04x:%08x to all Devices from manufacturer %04x",
                       rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, device_manu);
      for (auto device : devices_)
      {
        if (device.second->uid.manu == device_manu)
        {
          ClientWriteGuard client_write(*device.second);
          if (!device.second->Push(client_handle, msg->sender_cid, *rptmsg))
          {
            // TODO disconnect
            BROKER_LOG_ERR("Error pushing to send queue for RPT Device %d. DEBUG:NOT disconnecting...", device.first);
          }
        }
      }
    }
    else
    {
      bool found_dest_client = false;
      if (components_.uids.UidToHandle(rptmsg->header.dest_uid, dest_client_handle))
      {
        auto dest_client = clients_.find(dest_client_handle);
        if (dest_client != clients_.end())
        {
          ClientWriteGuard client_write(*dest_client->second);
          if (static_cast<RPTClient*>(dest_client->second.get())->Push(client_handle, msg->sender_cid, *rptmsg))
          {
            found_dest_client = true;
            BROKER_LOG_DEBUG("Routing RPT PDU from Client %04x:%08x to Client %04x:%08x",
                             rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, rptmsg->header.dest_uid.manu,
                             rptmsg->header.dest_uid.id);
          }
          else
          {
            // TODO disconnect
            BROKER_LOG_ERR("Error pushing to send queue for RPT Client %d. DEBUG:NOT disconnecting...",
                           dest_client->first);
          }
        }
      }
      if (!found_dest_client)
      {
        BROKER_LOG_ERR("Could not route message from RPT Client %d (%04x:%08x): Destination UID %04x:%08x not found.",
                       client_handle, rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id,
                       rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id);
      }
    }
  }
}

void BrokerCore::ResetClientHeartbeatTimer(BrokerClient::Handle client_handle)
{
  etcpal::ReadGuard clients_read(client_lock_);
  auto              client = clients_.find(client_handle);
  if (client != clients_.end())
  {
    ClientWriteGuard client_write(*client->second);
    client->second->MessageReceived();
  }
}

void BrokerCore::SendClientList(BrokerClient::Handle client_handle)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CONNECTED_CLIENT_LIST;

  etcpal::ReadGuard clients_read(client_lock_);
  auto              to_client = clients_.find(client_handle);
  if (to_client != clients_.end())
  {
    if (to_client->second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
      SendRptClientList(bmsg, static_cast<RPTClient&>(*to_client->second));
    else
      SendEptClientList(bmsg, static_cast<EPTClient&>(*to_client->second));
  }
}

void BrokerCore::SendRptClientList(BrokerMessage& bmsg, RPTClient& to_cli)
{
  std::vector<RdmnetRptClientEntry> entries;
  entries.reserve(rpt_clients_.size());
  for (auto& client : rpt_clients_)
  {
    entries.emplace_back();
    RdmnetRptClientEntry& rpt_entry = entries.back();
    RPTClient&            rpt_cli = static_cast<RPTClient&>(*client.second);

    rpt_entry.cid = rpt_cli.cid.get();
    rpt_entry.uid = rpt_cli.uid;
    rpt_entry.type = rpt_cli.client_type;
    rpt_entry.binding_cid = rpt_cli.binding_cid.get();
  }
  if (!entries.empty())
  {
    BROKER_GET_CLIENT_LIST(&bmsg)->client_protocol = kClientProtocolRPT;
    BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->client_entries = entries.data();
    BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->num_client_entries = entries.size();
    to_cli.Push(settings_.cid, bmsg);
  }
}

void BrokerCore::SendEptClientList(BrokerMessage& /*bmsg*/, EPTClient& /*to_cli*/)
{
}

void BrokerCore::SendClientsAdded(BrokerClient::Handle handle_to_ignore, std::vector<RdmnetRptClientEntry>& entries)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CLIENT_ADD;
  BROKER_GET_CLIENT_LIST(&bmsg)->client_protocol = kClientProtocolRPT;
  BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->client_entries = entries.data();
  BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->num_client_entries = entries.size();

  for (const auto controller : controllers_)
  {
    if (controller.first != handle_to_ignore)
      controller.second->Push(settings_.cid, bmsg);
  }
}

void BrokerCore::SendClientsRemoved(std::vector<RdmnetRptClientEntry>& entries)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CLIENT_REMOVE;
  BROKER_GET_CLIENT_LIST(&bmsg)->client_protocol = kClientProtocolRPT;
  BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->client_entries = entries.data();
  BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->num_client_entries = entries.size();

  for (const auto controller : controllers_)
  {
    controller.second->Push(settings_.cid, bmsg);
  }
}

void BrokerCore::SendStatus(RPTController*     controller,
                            const RptHeader&   header,
                            rpt_status_code_t  status_code,
                            const std::string& status_str)
{
  RptHeader new_header;
  new_header.dest_endpoint_id = header.source_endpoint_id;
  new_header.dest_uid = header.source_uid;
  new_header.seqnum = header.seqnum;
  new_header.source_endpoint_id = header.dest_endpoint_id;
  new_header.source_uid = header.dest_uid;

  RptStatusMsg status;
  status.status_code = status_code;
  if (!status_str.empty())
    status.status_string = status_str.c_str();
  else
    status.status_string = nullptr;

  if (controller->Push(settings_.cid, new_header, status))
  {
    BROKER_LOG_WARNING("Sending RPT Status code %d to Controller %s", status_code, controller->cid.ToString().c_str());
  }
  else
  {
    // TODO disconnect
  }
}
