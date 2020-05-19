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

// The core broker implementation. Contains the private implementation of Broker functionality.

#include "broker_core.h"

#include <cstring>
#include <cstddef>
#include "etcpal/cpp/error.h"
#include "etcpal/netint.h"
#include "etcpal/pack.h"
#include "rdmnet/version.h"
#include "rdmnet/core/connection.h"
#include "broker_client.h"
#include "broker_responder.h"
#include "broker_util.h"

/*************************** Function definitions ****************************/

BrokerCore::BrokerCore() : BrokerComponentNotify()
{
}

BrokerCore::~BrokerCore()
{
  if (started_)
    Shutdown();
}

etcpal::Error BrokerCore::Startup(const rdmnet::Broker::Settings& settings, rdmnet::Broker::NotifyHandler* notify,
                                  etcpal::Logger* logger, BrokerComponents components)
{
  if (!started_)
  {
    // Check the settings for validity
    if (!settings.valid())
      return kEtcPalErrInvalid;

    // Save members
    settings_ = settings;
    notify_ = notify;
    log_ = logger;
    components_ = std::move(components);
    components_.SetNotify(this);

    // Generate IDs if necessary
    my_uid_ = settings.uid;
    if (settings.uid_type == rdmnet::Broker::Settings::kDynamicUid)
    {
      my_uid_.id = 1;
      components_.uids.SetNextDeviceId(2);
    }

    auto err = components_.conn_interface->Startup(settings.cid, &log_->log_params());
    if (!err)
    {
      return err;
    }

    if (!components_.socket_mgr->Startup())
    {
      return kEtcPalErrSys;
    }

    err = StartBrokerServices();
    if (!err)
    {
      components_.socket_mgr->Shutdown();
      return err;
    }

    started_ = true;

    components_.disc->RegisterBroker(settings_);

    log_->Info("%s RDMnet Broker Version %s", settings_.dns.manufacturer.c_str(), RDMNET_VERSION_STRING);
    log_->Info("Broker starting at scope \"%s\", listening on port %d.", settings_.scope.c_str(),
               settings_.listen_port);

    if (!settings_.listen_addrs.empty())
    {
      log_->Info("Listening on manually-specified network interfaces:");
      for (auto addr : settings.listen_addrs)
      {
        log_->Info("%s", addr.ToString().c_str());
      }
    }
  }

  return kEtcPalErrOk;
}

// Call before destruction to gracefully close
void BrokerCore::Shutdown()
{
  if (started_)
  {
    components_.disc->UnregisterBroker();
    StopBrokerServices();
    components_.socket_mgr->Shutdown();
    components_.conn_interface->Shutdown();

    started_ = false;
  }
}

void BrokerCore::Tick()
{
  DestroyMarkedClientSockets();
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
  int tmp;
  return components_.uids.UidToHandle(uid, tmp);
}

bool BrokerCore::IsValidDeviceDestinationUID(const RdmUid& uid) const
{
  if (RDMNET_UID_IS_CONTROLLER_BROADCAST(&uid))
    return true;

  // TODO this should only check controllers
  int tmp;
  return components_.uids.UidToHandle(uid, tmp);
}

// The passed-in vector is cleared and filled with the cookies of connections that match the
// criteria.
void BrokerCore::GetConnSnapshot(std::vector<rdmnet_conn_t>& conns, bool include_devices, bool include_controllers,
                                 bool include_unknown, uint16_t manufacturer_filter)
{
  conns.clear();

  etcpal::ReadGuard client_read(client_lock_);

  if (!clients_.empty())
  {
    // We'll just do a bulk reserve.  The actual vector may take up less.
    conns.reserve(clients_.size());

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
          conns.push_back(client.first);
        }
      }
    }
  }
}

bool BrokerCore::HandleNewConnection(etcpal_socket_t new_sock, const etcpal::SockAddr& addr)
{
  if (log_->CanLog(ETCPAL_LOG_INFO))
  {
    log_->Info("Creating a new connection for ip addr %s", addr.ip().ToString().c_str());
  }

  rdmnet_conn_t connhandle = RDMNET_CONN_INVALID;
  bool result = false;

  {  // Client write lock scope
    etcpal::WriteGuard client_write(client_lock_);

    if (settings_.max_connections == 0 ||
        (clients_.size() <= settings_.max_connections + settings_.max_reject_connections))
    {
      auto create_res = components_.conn_interface->CreateNewConnectionForSocket(new_sock, addr, connhandle);
      if (create_res)
      {
        auto client = std::make_unique<BrokerClient>(connhandle);

        // Before inserting the connection, make sure we can attach the socket.
        if (client)
        {
          client->addr = addr;
          clients_.insert(std::make_pair(connhandle, std::move(client)));
          components_.socket_mgr->AddSocket(connhandle, new_sock);
          result = true;
        }
        else
        {
          components_.conn_interface->DestroyConnection(connhandle);
        }
      }
    }
  }

  if (result)
  {
    log_->Debug("New connection created with handle %d", connhandle);
  }
  else
  {
    log_->Error("New connection failed");
  }

  return result;
}

void BrokerCore::HandleSocketDataReceived(rdmnet_conn_t conn_handle, const uint8_t* data, size_t data_size)
{
  components_.conn_interface->SocketDataReceived(conn_handle, data, data_size);
}

void BrokerCore::HandleSocketClosed(rdmnet_conn_t conn_handle, bool graceful)
{
  components_.conn_interface->SocketError(conn_handle, graceful ? kEtcPalErrConnClosed : kEtcPalErrConnReset);
}

// Process each controller queue, sending out the next message from each queue if devices are
// available. Also sends connect reply, error and status messages generated asynchronously to
// devices.  Return false if no controllers messages were sent.
bool BrokerCore::ServiceClients()
{
  bool result = false;
  std::vector<int> client_conns;

  etcpal::ReadGuard clients_read(client_lock_);

  for (auto& client : clients_)
  {
    ClientWriteGuard client_write(*client.second);
    result |= client.second->Send();
  }
  return result;
}

// Message processing functions
void BrokerCore::HandleRdmnetConnMsgReceived(rdmnet_conn_t handle, const RdmnetMessage& msg)
{
  switch (msg.vector)
  {
    case ACN_VECTOR_ROOT_BROKER:
    {
      const BrokerMessage* bmsg = RDMNET_GET_BROKER_MSG(&msg);
      switch (bmsg->vector)
      {
        case VECTOR_BROKER_CONNECT:
          ProcessConnectRequest(handle, BROKER_GET_CLIENT_CONNECT_MSG(bmsg));
          break;
        case VECTOR_BROKER_FETCH_CLIENT_LIST:
          SendClientList(handle);
          log_->Debug("Received Fetch Client List from Client %d; sending Client List.", handle);
          break;
        default:
          log_->Warning("Received Broker PDU with unknown or unhandled vector %d", bmsg->vector);
          break;
      }
      break;
    }

    case ACN_VECTOR_ROOT_RPT:
      ProcessRPTMessage(handle, &msg);
      break;

    default:
      log_->Warning("Received Root Layer PDU with unknown or unhandled vector %d", msg.vector);
      break;
  }
}

void BrokerCore::HandleRdmnetConnDisconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo& /*disconn_info*/)
{
  MarkConnForDestruction(handle);
}

void BrokerCore::SendClientList(int conn)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CONNECTED_CLIENT_LIST;

  etcpal::ReadGuard clients_read(client_lock_);
  auto to_client = clients_.find(conn);
  if (to_client != clients_.end())
  {
    if (to_client->second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
      SendRdmnetRptClientList(bmsg, static_cast<RPTClient&>(*to_client->second));
    else
      SendRdmnetEptClientList(bmsg, static_cast<EPTClient&>(*to_client->second));
  }
}

void BrokerCore::SendRdmnetRptClientList(BrokerMessage& bmsg, RPTClient& to_cli)
{
  std::vector<RdmnetRptClientEntry> entries;
  entries.reserve(rpt_clients_.size());
  for (auto& client : rpt_clients_)
  {
    entries.emplace_back();
    RdmnetRptClientEntry& rpt_entry = entries.back();
    RPTClient& rpt_cli = static_cast<RPTClient&>(*client.second);

    rpt_entry.cid = rpt_cli.cid.get();
    rpt_entry.uid = rpt_cli.uid;
    rpt_entry.type = rpt_cli.client_type;
    rpt_entry.binding_cid = rpt_cli.binding_cid.get();
  }
  if (!entries.empty())
  {
    BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->client_entries = entries.data();
    BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->num_client_entries = entries.size();
    to_cli.Push(settings_.cid, bmsg);
  }
}

void BrokerCore::SendRdmnetEptClientList(BrokerMessage& /*bmsg*/, EPTClient& /*to_cli*/)
{
}

void BrokerCore::SendClientsAdded(rdmnet_conn_t conn_to_ignore, std::vector<RdmnetRptClientEntry>& entries)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CLIENT_ADD;
  BROKER_GET_CLIENT_LIST(&bmsg)->client_protocol = kClientProtocolRPT;
  BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->client_entries = entries.data();
  BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(&bmsg))->num_client_entries = entries.size();

  for (const auto controller : controllers_)
  {
    if (controller.first != conn_to_ignore)
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

void BrokerCore::SendStatus(RPTController* controller, const RptHeader& header, rpt_status_code_t status_code,
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
    log_->Warning("Sending RPT Status code %d to Controller %s", status_code, controller->cid.ToString().c_str());
  }
  else
  {
    // TODO disconnect
  }
}

void BrokerCore::ProcessConnectRequest(int conn, const BrokerClientConnectMsg* cmsg)
{
  bool deny_connection = true;
  rdmnet_connect_status_t connect_status = kRdmnetConnectScopeMismatch;

  if ((cmsg->e133_version <= E133_VERSION) && (cmsg->scope == settings_.scope))
  {
    switch (cmsg->client_entry.client_protocol)
    {
      case E133_CLIENT_PROTOCOL_RPT:
        deny_connection = !ProcessRPTConnectRequest(conn, *(GET_RPT_CLIENT_ENTRY(&cmsg->client_entry)), connect_status);
        break;
      // TODO EPT
      default:
        connect_status = kRdmnetConnectInvalidClientEntry;
        break;
    }
  }

  if (deny_connection)
  {
    BrokerConnectReplyMsg creply = {connect_status, E133_VERSION, my_uid_, {}};
    broker_send_connect_reply(conn, &settings_.cid.get(), &creply);

    // Clean up this connection.
    MarkConnForDestruction(conn);
  }
}

bool BrokerCore::ProcessRPTConnectRequest(rdmnet_conn_t handle, const RdmnetRptClientEntry& client_entry,
                                          rdmnet_connect_status_t& connect_status)
{
  bool continue_adding = true;
  // We need to make a copy of the data because we might be changing the UID value
  RdmnetRptClientEntry updated_client_entry = client_entry;

  if (!components_.conn_interface->SetBlocking(handle, false))
  {
    log_->Error("Error translating socket into non-blocking socket for Client %d", handle);
    return false;
  }

  etcpal::WriteGuard clients_write(client_lock_);
  RPTClient* new_client = nullptr;

  if ((settings_.max_connections > 0) && (clients_.size() >= settings_.max_connections))
  {
    connect_status = kRdmnetConnectCapacityExceeded;
    continue_adding = false;
  }

  // Resolve the Client's UID
  if (RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&updated_client_entry.uid))
  {
    BrokerUidManager::AddResult add_result =
        components_.uids.AddDynamicUid(handle, updated_client_entry.cid, updated_client_entry.uid);
    switch (add_result)
    {
      case BrokerUidManager::AddResult::kOk:
        break;
      case BrokerUidManager::AddResult::kDuplicateId:
        connect_status = kRdmnetConnectDuplicateUid;
        continue_adding = false;
        break;
      case BrokerUidManager::AddResult::kCapacityExceeded:
      default:
        connect_status = kRdmnetConnectCapacityExceeded;
        continue_adding = false;
        break;
    }
  }
  else if (RDMNET_UID_IS_STATIC(&updated_client_entry.uid))
  {
    BrokerUidManager::AddResult add_result = components_.uids.AddStaticUid(handle, updated_client_entry.uid);
    switch (add_result)
    {
      case BrokerUidManager::AddResult::kOk:
        break;
      case BrokerUidManager::AddResult::kDuplicateId:
        connect_status = kRdmnetConnectDuplicateUid;
        continue_adding = false;
        break;
      case BrokerUidManager::AddResult::kCapacityExceeded:
      default:
        connect_status = kRdmnetConnectCapacityExceeded;
        continue_adding = false;
        break;
    }
  }
  else
  {
    // Client sent an invalid UID of some kind, like a bad dynamic UID request or a broadcast value
    connect_status = kRdmnetConnectInvalidUid;
    continue_adding = false;
  }

  if (continue_adding)
  {
    // If it's a controller, add it to the controller queues -- unless
    // we've hit our maximum number of controllers
    if (updated_client_entry.type == kRPTClientTypeController)
    {
      if ((settings_.max_controllers > 0) && (controllers_.size() >= settings_.max_controllers))
      {
        connect_status = kRdmnetConnectCapacityExceeded;
        continue_adding = false;
        components_.uids.RemoveUid(updated_client_entry.uid);
      }
      else
      {
        auto controller =
            std::make_unique<RPTController>(settings_.max_controller_messages, updated_client_entry, *clients_[handle]);
        if (controller)
        {
          new_client = controller.get();
          controllers_.insert(std::make_pair(handle, controller.get()));
          rpt_clients_.insert(std::make_pair(handle, controller.get()));
          clients_[handle] = std::move(controller);
        }
      }
    }
    // If it's a device, add it to the device states -- unless we've hit our maximum number of
    // devices
    else if (updated_client_entry.type == kRPTClientTypeDevice)
    {
      if ((settings_.max_devices > 0) && (devices_.size() >= settings_.max_devices))
      {
        connect_status = kRdmnetConnectCapacityExceeded;
        continue_adding = false;
        components_.uids.RemoveUid(updated_client_entry.uid);
      }
      else
      {
        auto device =
            std::make_unique<RPTDevice>(settings_.max_device_messages, updated_client_entry, *clients_[handle]);
        if (device)
        {
          new_client = device.get();
          devices_.insert(std::make_pair(handle, device.get()));
          rpt_clients_.insert(std::make_pair(handle, device.get()));
          clients_[handle] = std::move(device);
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
    creply->broker_uid = my_uid_;
    creply->client_uid = updated_client_entry.uid;
    new_client->Push(settings_.cid, msg);

    if (log_->CanLog(ETCPAL_LOG_INFO))
    {
      log_->Info("Successfully processed RPT Connect request from %s (connection %d), UID %04x:%08x",
                 new_client->client_type == kRPTClientTypeController ? "Controller" : "Device", handle,
                 new_client->uid.manu, new_client->uid.id);
    }

    // Update everyone
    std::vector<RdmnetRptClientEntry> entries;
    entries.push_back(updated_client_entry);
    SendClientsAdded(handle, entries);
  }
  return continue_adding;
}

void BrokerCore::ProcessRPTMessage(int conn, const RdmnetMessage* msg)
{
  etcpal::ReadGuard clients_read(client_lock_);

  const RptMessage* rptmsg = RDMNET_GET_RPT_MSG(msg);
  bool route_msg = false;
  auto client = clients_.find(conn);

  if ((client != clients_.end()) && client->second)
  {
    ClientWriteGuard client_write(*client->second);

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
              log_->Debug("Received Request PDU addressed to invalid or not found UID %04x:%08x from Controller %d",
                          rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
            else if (RPT_GET_RDM_BUF_LIST(rptmsg)->num_rdm_buffers > 1)
            {
              // There should only ever be one RDM command in an RPT request.
              SendStatus(controller, rptmsg->header, kRptStatusInvalidMessage);
              log_->Debug(
                  "Received Request PDU from Controller %d which incorrectly contains multiple RDM Command PDUs", conn);
            }
            else
            {
              route_msg = true;
            }
          }
          else
          {
            log_->Debug("Received Request PDU from Client %d, which is not an RPT Controller", conn);
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
                log_->Debug("Device %d sent broadcast complete message.", conn);
            }
            else
            {
              log_->Debug("Received Status PDU addressed to invalid or not found UID %04x:%08x from Device %d",
                          rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
          }
          else
          {
            log_->Debug("Received Status PDU from Client %d, which is not an RPT Device", conn);
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
              log_->Debug("Received Notification PDU addressed to invalid or not found UID %04x:%08x from Device %d",
                          rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
          }
          else
          {
            log_->Debug("Received Notification PDU from Client %d of unknown client type", rptmsg->header.dest_uid.manu,
                        rptmsg->header.dest_uid.id, conn);
          }
          break;

        default:
          log_->Warning("Received RPT PDU with unknown vector %d from Client %d", rptmsg->vector, conn);
          break;
      }
    }
  }

  if (route_msg)
  {
    uint16_t device_manu;
    int dest_conn;

    if (RDMNET_UID_IS_CONTROLLER_BROADCAST(&rptmsg->header.dest_uid))
    {
      log_->Debug("Broadcasting RPT message from Device %04x:%08x to all Controllers", rptmsg->header.source_uid.manu,
                  rptmsg->header.source_uid.id);
      for (auto controller : controllers_)
      {
        ClientWriteGuard client_write(*controller.second);
        if (!controller.second->Push(conn, msg->sender_cid, *rptmsg))
        {
          // TODO disconnect
          log_->Error("Error pushing to send queue for RPT Controller %d. DEBUG:NOT disconnecting...",
                      controller.first);
        }
      }
    }
    else if (RDMNET_UID_IS_DEVICE_BROADCAST(&rptmsg->header.dest_uid))
    {
      log_->Debug("Broadcasting RPT message from Controller %04x:%08x to all Devices", rptmsg->header.source_uid.manu,
                  rptmsg->header.source_uid.id);
      for (auto device : devices_)
      {
        ClientWriteGuard client_write(*device.second);
        if (!device.second->Push(conn, msg->sender_cid, *rptmsg))
        {
          // TODO disconnect
          log_->Error("Error pushing to send queue for RPT Device %d. DEBUG:NOT disconnecting...", device.first);
        }
      }
    }
    else if (IsDeviceManuBroadcastUID(rptmsg->header.dest_uid, device_manu))
    {
      log_->Debug("Broadcasting RPT message from Controller %04x:%08x to all Devices from manufacturer %04x",
                  rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, device_manu);
      for (auto device : devices_)
      {
        if (device.second->uid.manu == device_manu)
        {
          ClientWriteGuard client_write(*device.second);
          if (!device.second->Push(conn, msg->sender_cid, *rptmsg))
          {
            // TODO disconnect
            log_->Error("Error pushing to send queue for RPT Device %d. DEBUG:NOT disconnecting...", device.first);
          }
        }
      }
    }
    else
    {
      bool found_dest_client = false;
      if (components_.uids.UidToHandle(rptmsg->header.dest_uid, dest_conn))
      {
        auto dest_client = clients_.find(dest_conn);
        if (dest_client != clients_.end())
        {
          ClientWriteGuard client_write(*dest_client->second);
          if (static_cast<RPTClient*>(dest_client->second.get())->Push(conn, msg->sender_cid, *rptmsg))
          {
            found_dest_client = true;
            log_->Debug("Routing RPT PDU from Client %04x:%08x to Client %04x:%08x", rptmsg->header.source_uid.manu,
                        rptmsg->header.source_uid.id, rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id);
          }
          else
          {
            // TODO disconnect
            log_->Error("Error pushing to send queue for RPT Client %d. DEBUG:NOT disconnecting...",
                        dest_client->first);
          }
        }
      }
      if (!found_dest_client)
      {
        log_->Error("Could not route message from RPT Client %d (%04x:%08x): Destination UID %04x:%08x not found.",
                    conn, rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, rptmsg->header.dest_uid.manu,
                    rptmsg->header.dest_uid.id);
      }
    }
  }
}

std::set<etcpal::IpAddr> BrokerCore::CombineMacsAndInterfaces(const std::set<etcpal::IpAddr>& interfaces,
                                                              const std::set<etcpal::MacAddr>& macs)
{
  auto to_return = interfaces;

  size_t num_netints = etcpal_netint_get_num_interfaces();
  for (const auto& mac : macs)
  {
    const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();
    for (const EtcPalNetintInfo* netint = netint_list; netint < netint_list + num_netints; ++netint)
    {
      if (netint->mac == mac)
      {
        to_return.insert(netint->addr);
        // There could be multiple addresses that have this mac, we don't break here so we listen
        // on all of them.
      }
    }
  }

  return to_return;
}

etcpal::Expected<etcpal_socket_t> BrokerCore::StartListening(const etcpal::IpAddr& ip, uint16_t& port)
{
  etcpal::SockAddr addr(ip, port);

  etcpal_socket_t listen_sock;
  etcpal::Error res = etcpal_socket(addr.IsV4() ? ETCPAL_AF_INET : ETCPAL_AF_INET6, ETCPAL_STREAM, &listen_sock);
  if (!res)
  {
    if (log_)
    {
      log_->Error("Broker: Failed to create listen socket with error: %s.", res.ToCString());
    }
    return res;
  }

  if (ip.IsV6())
  {
    int sockopt_val = (ip.IsWildcard() ? 0 : 1);
    res = etcpal_setsockopt(listen_sock, ETCPAL_IPPROTO_IPV6, ETCPAL_IPV6_V6ONLY, &sockopt_val, sizeof(int));
    if (!res)
    {
      etcpal_close(listen_sock);
      if (log_)
      {
        log_->Error("Broker: Failed to set V6ONLY socket option on listen socket: %s.", res.ToCString());
      }
      return res;
    }
  }

  res = etcpal_bind(listen_sock, &addr.get());
  if (!res)
  {
    etcpal_close(listen_sock);
    if (log_ && log_->CanLog(ETCPAL_LOG_ERR))
    {
      log_->Error("Broker: Bind to %s failed on listen socket with error: %s.", addr.ToString().c_str(),
                  res.ToCString());
    }
    return res;
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
      if (log_)
      {
        log_->Error("Broker: Failed to get ephemeral port assigned to listen socket: %s", res.ToCString());
      }
      return res;
    }
  }

  res = etcpal_listen(listen_sock, 0);
  if (!res)
  {
    etcpal_close(listen_sock);
    if (log_)
    {
      log_->Error("Broker: Listen failed on listen socket with error: %s.", res.ToCString());
    }
    return res;
  }
  return listen_sock;
}

etcpal::Error BrokerCore::StartBrokerServices()
{
  etcpal::Error res = components_.threads->AddClientServiceThread();
  if (!res)
    return res;

  auto final_listen_addrs = CombineMacsAndInterfaces(settings_.listen_addrs, settings_.listen_macs);
  if (final_listen_addrs.empty())
  {
    // Listen on in6addr_any
    const auto any_addr = etcpal::IpAddr::WildcardV6();
    auto listen_sock = StartListening(any_addr, settings_.listen_port);
    if (listen_sock)
    {
      res = components_.threads->AddListenThread(*listen_sock);
      if (!res)
        etcpal_close(*listen_sock);
    }
    else
    {
      log_->Critical("Could not bind a wildcard listening socket.");
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
      log_->Critical("Could not listen on any provided IP addresses.");
      res = kEtcPalErrSys;
    }
  }

  return res;
}

void BrokerCore::StopBrokerServices()
{
  components_.threads->StopThreads();

  // No new connections coming in, manually shut down the existing ones.
  std::vector<rdmnet_conn_t> conns;
  GetConnSnapshot(conns, true, true, true);

  for (auto& conn : conns)
    MarkConnForDestruction(conn, SendDisconnect(kRdmnetDisconnectShutdown));

  DestroyMarkedClientSockets();
}

// This function grabs a read lock on client_lock_.
// Optionally sends a RDMnet-level disconnect message.
void BrokerCore::MarkConnForDestruction(rdmnet_conn_t conn, SendDisconnect send_disconnect)
{
  bool found = false;

  {  // Client read lock and destroy lock scope
    etcpal::ReadGuard clients_read(client_lock_);

    auto client = clients_.find(conn);
    if ((client != clients_.end()) && client->second)
    {
      ClientWriteGuard client_write(*client->second);
      found = true;
      clients_to_destroy_.insert(client->first);
    }
  }

  if (found)
  {
    components_.conn_interface->DestroyConnection(conn, send_disconnect);
    log_->Debug("Connection %d marked for destruction", conn);
  }
}

// These functions will take a write lock on client_lock_ and client_destroy_lock_.
void BrokerCore::DestroyMarkedClientSockets()
{
  etcpal::WriteGuard clients_write(client_lock_);
  std::vector<RdmnetRptClientEntry> rpt_entries;

  if (!clients_to_destroy_.empty())
  {
    for (auto to_destroy : clients_to_destroy_)
    {
      auto client = clients_.find(to_destroy);
      if (client != clients_.end())
      {
        if (client->second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
        {
          RPTClient* rptcli = static_cast<RPTClient*>(client->second.get());
          components_.uids.RemoveUid(rptcli->uid);
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

        log_->Info("Removing connection %d marked for destruction.", to_destroy);
        if (log_->CanLog(ETCPAL_LOG_DEBUG))
        {
          log_->Debug("Clients: %zu Controllers: %zu Devices: %zu", clients_.size(), controllers_.size(),
                      devices_.size());
        }
      }
    }
    clients_to_destroy_.clear();
  }

  if (!rpt_entries.empty())
    SendClientsRemoved(rpt_entries);
}

void BrokerCore::HandleBrokerRegistered(const std::string& scope, const std::string& requested_service_name,
                                        const std::string& assigned_service_name)
{
  service_registered_ = true;
  if (requested_service_name == assigned_service_name)
  {
    log_->Info("Broker \"%s\" successfully registered at scope \"%s\"", requested_service_name.c_str(), scope.c_str());
  }
  else
  {
    log_->Info("Broker \"%s\" (now named \"%s\") successfully registered at scope \"%s\"",
               requested_service_name.c_str(), assigned_service_name.c_str(), scope.c_str());
  }
}

void BrokerCore::HandleBrokerRegisterError(const std::string& scope, const std::string& requested_service_name,
                                           int platform_specific_error)
{
  log_->Critical("Broker \"%s\" register error %d at scope \"%s\"", requested_service_name.c_str(),
                 platform_specific_error, scope.c_str());
}

void BrokerCore::HandleOtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info)
{
  // If the broker is already registered with DNS-SD, the presence of another broker is an error
  // condition. Otherwise, the system is still usable (this broker will not register)
  int log_pri = (service_registered_ ? ETCPAL_LOG_ERR : ETCPAL_LOG_NOTICE);

  if (log_->CanLog(log_pri))
  {
    std::string addrs;
    for (size_t i = 0; i < broker_info.num_listen_addrs; ++i)
    {
      char addr_string[ETCPAL_INET6_ADDRSTRLEN];
      if (kEtcPalErrOk == etcpal_ip_to_string(&broker_info.listen_addrs[i], addr_string))
      {
        addrs.append(addr_string);
        if (i < broker_info.num_listen_addrs - 1)
          addrs.append(", ");
      }
    }

    log_->Log(log_pri, "Broker \"%s\", ip[%s] found at same scope(\"%s\") as this broker.", broker_info.service_name,
              addrs.c_str(), broker_info.scope);
  }
  if (!service_registered_)
  {
    log_->Log(log_pri, "This broker will remain unregistered with DNS-SD until all conflicting brokers are removed.");
    // StopBrokerServices();
  }
}

void BrokerCore::HandleOtherBrokerLost(const std::string& scope, const std::string& service_name)
{
  log_->Notice("Conflicting broker %s on scope \"%s\" no longer discovered.", service_name.c_str(), scope.c_str());
}

void BrokerCore::HandleScopeMonitorError(const std::string& scope, int platform_error)
{
  log_->Error("Error code %d encountered while monitoring broker's scope \"%s\" for other brokers.", platform_error,
              scope.c_str());
}
