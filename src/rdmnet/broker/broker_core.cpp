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

// The generic broker implementation

#include "broker_core.h"

#include <cstring>
#include <cstddef>

#include "etcpal/pack.h"
#include "rdmnet/version.h"
#include "rdmnet/core/connection.h"
#include "broker_client.h"
#include "broker_responder.h"
#include "broker_util.h"

/*************************** Function definitions ****************************/

RDMnet::Broker::Broker(BrokerLog* log, BrokerSocketManager* socket_manager, BrokerNotify* notify)
    : core_(std::make_unique<BrokerCore>(log, socket_manager, notify,
                                         std::unique_ptr<RdmnetConnInterface>(new RdmnetConnWrapper)))
{
}

RDMnet::Broker::~Broker()
{
}

/// \brief Start all %Broker functionality and threads.
///
/// If listen_addrs is empty, this returns false.  Otherwise, the broker uses the address fields to
/// set up the listening sockets. If the listen_port is 0 and their is only one listen_addr, an
/// ephemeral port is chosen. If there are more listen_addrs, listen_port must not be 0.
///
/// \param[in] settings Settings for the Broker to use for this session.
/// \param[in] listen_port Port for the Broker to listen on.
/// \param[in] listen_addrs Addresses of network interfaces for the Broker to listen on.
/// \return true (started %Broker successfully) or false (an error occurred starting %Broker).
bool RDMnet::Broker::Startup(const BrokerSettings& settings, uint16_t listen_port,
                             std::vector<EtcPalIpAddr>& listen_addrs)
{
  return core_->Startup(settings, listen_port, listen_addrs);
}

void RDMnet::Broker::Shutdown()
{
  core_->Shutdown();
}

void RDMnet::Broker::Tick()
{
  core_->Tick();
}

void RDMnet::Broker::GetSettings(BrokerSettings& settings) const
{
  core_->GetSettings(settings);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Begin BrokerCore Functions
// BrokerCore: Private implementation of Broker functionality.
///////////////////////////////////////////////////////////////////////////////////////////////////

BrokerCore::BrokerCore(RDMnet::BrokerLog* log, RDMnet::BrokerSocketManager* socket_manager,
                       RDMnet::BrokerNotify* notify, std::unique_ptr<RdmnetConnInterface> conn)
    : RdmnetConnNotify()
    , ListenThreadNotify()
    , ClientServiceThreadNotify()
    , BrokerDiscoveryManagerNotify()
    , log_(log)
    , socket_manager_(socket_manager)
    , notify_(notify)
    , conn_interface_(std::move(conn))
    , service_thread_(1)
    , disc_(this)
{
  etcpal_rwlock_create(&client_lock_);
}

BrokerCore::~BrokerCore()
{
  if (started_)
    Shutdown();

  etcpal_rwlock_destroy(&client_lock_);
}

bool BrokerCore::Startup(const RDMnet::BrokerSettings& settings, uint16_t listen_port,
                         const std::vector<EtcPalIpAddr>& listen_addrs)
{
  if (!started_)
  {
    // Check the settings for validity
    if (ETCPAL_UUID_IS_NULL(&settings.cid) ||
        (settings.uid_type == RDMnet::BrokerSettings::kStaticUid && !RDMNET_UID_IS_STATIC(&settings.uid)) ||
        (settings.uid_type == RDMnet::BrokerSettings::kDynamicUid && !RDMNET_UID_IS_DYNAMIC(&settings.uid)))
    {
      return false;
    }

    // Generate IDs if necessary
    my_uid_ = settings.uid;
    if (settings.uid_type == RDMnet::BrokerSettings::kDynamicUid)
    {
      my_uid_.id = 1;
      uid_manager_.SetNextDeviceId(2);
    }
    settings_ = settings;

    if (kEtcPalErrOk != conn_interface_->Startup(settings.cid, log_->GetLogParams(), this))
    {
      return false;
    }

    if (!socket_manager_->Startup(this))
    {
      rdmnet_core_deinit();
      return false;
    }

    listen_addrs_ = listen_addrs;
    listen_port_ = listen_port;
    if (!StartBrokerServices())
    {
      socket_manager_->Shutdown();
      rdmnet_core_deinit();
      return false;
    }

    started_ = true;

    service_thread_.SetNotify(this);
    service_thread_.Start();

    disc_.RegisterBroker(settings_.disc_attributes, settings_.cid, listen_addrs_, listen_port_);

    log_->Log(ETCPAL_LOG_INFO, "%s Prototype RDMnet Broker Version %s",
              settings.disc_attributes.dns_manufacturer.c_str(), RDMNET_VERSION_STRING);
    log_->Log(ETCPAL_LOG_INFO, "Broker starting at scope \"%s\", listening on port %d.", disc_.scope().c_str(),
              listen_port_);

    if (!listen_addrs.empty())
    {
      log_->Log(ETCPAL_LOG_INFO, "Listening on manually-specified network interfaces:");
      for (auto addr : listen_addrs)
      {
        char addrbuf[ETCPAL_INET6_ADDRSTRLEN];
        etcpal_inet_ntop(&addr, addrbuf, ETCPAL_INET6_ADDRSTRLEN);
        log_->Log(ETCPAL_LOG_INFO, "%s", addrbuf);
      }
    }
  }

  return started_;
}

// Call before destruction to gracefully close
void BrokerCore::Shutdown()
{
  if (started_)
  {
    disc_.UnregisterBroker();

    StopBrokerServices();
    listeners_.clear();

    service_thread_.Stop();

    socket_manager_->Shutdown();

    conn_interface_->Shutdown();

    started_ = false;
  }
}

void BrokerCore::Tick()
{
  DestroyMarkedClientSockets();
}

// Fills in the current settings the broker is using.  Can be called even after Shutdown. Useful if
// you want to shutdown & restart the broker for any reason.
void BrokerCore::GetSettings(RDMnet::BrokerSettings& settings) const
{
  settings = settings_;
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
  return uid_manager_.UidToHandle(uid, tmp);
}

bool BrokerCore::IsValidDeviceDestinationUID(const RdmUid& uid) const
{
  if (RDMNET_UID_IS_CONTROLLER_BROADCAST(&uid))
    return true;

  // TODO this should only check controllers
  int tmp;
  return uid_manager_.UidToHandle(uid, tmp);
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

bool BrokerCore::NewConnection(etcpal_socket_t new_sock, const EtcPalSockaddr& addr)
{
  if (log_->CanLog(ETCPAL_LOG_INFO))
  {
    char addrstr[ETCPAL_INET6_ADDRSTRLEN];
    etcpal_inet_ntop(&addr.ip, addrstr, ETCPAL_INET6_ADDRSTRLEN);
    log_->Log(ETCPAL_LOG_INFO, "Creating a new connection for ip addr %s", addrstr);
  }

  rdmnet_conn_t connhandle = RDMNET_CONN_INVALID;
  bool result = false;

  {  // Client write lock scope
    etcpal::WriteGuard client_write(client_lock_);

    if (settings_.max_connections == 0 ||
        (clients_.size() <= settings_.max_connections + settings_.max_reject_connections))
    {
      etcpal_error_t create_res = conn_interface_->CreateNewConnectionForSocket(new_sock, addr, connhandle);
      if (create_res == kEtcPalErrOk)
      {
        auto client = std::make_shared<BrokerClient>(connhandle);

        // Before inserting the connection, make sure we can attach the socket.
        if (client)
        {
          client->addr = addr;
          clients_.insert(std::make_pair(connhandle, std::move(client)));
          socket_manager_->AddSocket(connhandle, new_sock);
          result = true;
        }
        else
        {
          conn_interface_->DestroyConnection(connhandle);
        }
      }
    }
  }

  if (result)
  {
    log_->Log(ETCPAL_LOG_DEBUG, "New connection created with handle %d", connhandle);
  }
  else
  {
    log_->Log(ETCPAL_LOG_ERR, "New connection failed");
  }

  return result;
}

void BrokerCore::SocketDataReceived(rdmnet_conn_t conn_handle, const uint8_t* data, size_t data_size)
{
  conn_interface_->SocketDataReceived(conn_handle, data, data_size);
}

void BrokerCore::SocketClosed(rdmnet_conn_t conn_handle, bool graceful)
{
  conn_interface_->SocketError(conn_handle, graceful ? kEtcPalErrConnClosed : kEtcPalErrConnReset);
}

// Process each controller queue, sending out the next message from each queue if devices are
// available. Also sends connect reply, error and status messages generated asynchronously to
// devices.  Return false if no controllers messages were sent.
bool BrokerCore::ServiceClients()
{
  bool result = false;
  std::vector<int> client_conns;

  etcpal::ReadGuard clients_read(client_lock_);

  for (auto client : clients_)
  {
    ClientWriteGuard client_write(*client.second);
    result |= client.second->Send();
  }
  return result;
}

// Message processing functions
void BrokerCore::RdmnetConnMsgReceived(rdmnet_conn_t handle, const RdmnetMessage& msg)
{
  switch (msg.vector)
  {
    case ACN_VECTOR_ROOT_BROKER:
    {
      const BrokerMessage* bmsg = get_broker_msg(&msg);
      switch (bmsg->vector)
      {
        case VECTOR_BROKER_CONNECT:
          ProcessConnectRequest(handle, get_client_connect_msg(bmsg));
          break;
        case VECTOR_BROKER_FETCH_CLIENT_LIST:
          SendClientList(handle);
          log_->Log(ETCPAL_LOG_DEBUG, "Received Fetch Client List from Client %d; sending Client List.", handle);
          break;
        default:
          log_->Log(ETCPAL_LOG_ERR, "Received Broker PDU with unknown or unhandled vector %d", bmsg->vector);
          break;
      }
      break;
    }

    case ACN_VECTOR_ROOT_RPT:
      ProcessRPTMessage(handle, &msg);
      break;

    default:
      log_->Log(ETCPAL_LOG_WARNING, "Received Root Layer PDU with unknown or unhandled vector %d", msg.vector);
      break;
  }
}

void BrokerCore::RdmnetConnDisconnected(rdmnet_conn_t handle, const RdmnetDisconnectedInfo& /*disconn_info*/)
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
    std::vector<ClientEntryData> entries;
    entries.reserve(clients_.size());
    for (auto client : clients_)
    {
      if (client.second->client_protocol == to_client->second->client_protocol)
      {
        ClientEntryData cli_data;
        cli_data.client_cid = client.second->cid;
        cli_data.client_protocol = client.second->client_protocol;
        if (client.second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
        {
          ClientEntryDataRpt* rpt_cli_data = get_rpt_client_entry_data(&cli_data);
          RPTClient* rptcli = static_cast<RPTClient*>(client.second.get());
          rpt_cli_data->client_uid = rptcli->uid;
          rpt_cli_data->client_type = rptcli->client_type;
          rpt_cli_data->binding_cid = rptcli->binding_cid;
        }
        cli_data.next = nullptr;
        entries.push_back(cli_data);
        // Keep the list linked for the pack function.
        if (entries.size() > 1)
          entries[entries.size() - 2].next = &entries[entries.size() - 1];
      }
    }
    if (!entries.empty())
    {
      get_client_list(&bmsg)->client_entry_list = entries.data();
      to_client->second->Push(settings_.cid, bmsg);
    }
  }
}

void BrokerCore::SendClientsAdded(client_protocol_t client_prot, int conn_to_ignore,
                                  std::vector<ClientEntryData>& entries)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CLIENT_ADD;
  get_client_list(&bmsg)->client_entry_list = entries.data();

  for (const auto controller : controllers_)
  {
    if (controller.second->client_protocol == client_prot && controller.first != conn_to_ignore)
      controller.second->Push(settings_.cid, bmsg);
  }
}

void BrokerCore::SendClientsRemoved(client_protocol_t client_prot, std::vector<ClientEntryData>& entries)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CLIENT_REMOVE;
  get_client_list(&bmsg)->client_entry_list = entries.data();

  for (const auto controller : controllers_)
  {
    if (controller.second->client_protocol == client_prot)
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
    if (log_->CanLog(ETCPAL_LOG_INFO))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(cid_str, &controller->cid);
      log_->Log(ETCPAL_LOG_WARNING, "Sending RPT Status code %d to Controller %s", status_code, cid_str);
    }
  }
  else
  {
    // TODO disconnect
  }
}

void BrokerCore::ProcessConnectRequest(int conn, const ClientConnectMsg* cmsg)
{
  bool deny_connection = true;
  rdmnet_connect_status_t connect_status = kRdmnetConnectScopeMismatch;

  // TESTING
  // auto it =clients_.find(cookie);
  // if(it !=clients_.end())
  //{
  // RDMnet::SendRedirect(it->second->sock.get(), my_cid,
  //    CIPAddr::StringToAddr("192.168.6.12:8888"));
  // MarkSocketForDestruction(cookie, false, 0);
  // return;
  //}

  if ((cmsg->e133_version <= E133_VERSION) && (cmsg->scope == settings_.disc_attributes.scope))
  {
    switch (cmsg->client_entry.client_protocol)
    {
      case E133_CLIENT_PROTOCOL_RPT:
        deny_connection = !ProcessRPTConnectRequest(conn, cmsg->client_entry, connect_status);
        break;
      default:
        connect_status = kRdmnetConnectInvalidClientEntry;
        break;
    }
  }

  if (deny_connection)
  {
    etcpal::ReadGuard client_read(client_lock_);

    auto it = clients_.find(conn);
    if (it != clients_.end() && it->second)
    {
      ConnectReplyMsg creply = {connect_status, E133_VERSION, my_uid_, {}};
      send_connect_reply(conn, &settings_.cid, &creply);
    }

    // Clean up this socket. TODO
    // MarkSocketForDestruction(cookie, false, 0);
  }
}

bool BrokerCore::ProcessRPTConnectRequest(rdmnet_conn_t handle, const ClientEntryData& data,
                                          rdmnet_connect_status_t& connect_status)
{
  bool continue_adding = true;
  // We need to make a copy of the data because we might be changing the UID value
  ClientEntryData updated_data = data;
  ClientEntryDataRpt* rptdata = get_rpt_client_entry_data(&updated_data);

  if (kEtcPalErrOk != conn_interface_->SetBlocking(handle, false))
  {
    log_->Log(ETCPAL_LOG_INFO, "Error translating socket into non-blocking socket for Client %d", handle);
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
  if (RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&rptdata->client_uid))
  {
    BrokerUidManager::AddResult add_result =
        uid_manager_.AddDynamicUid(handle, updated_data.client_cid, rptdata->client_uid);
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
  else if (RDMNET_UID_IS_STATIC(&rptdata->client_uid))
  {
    BrokerUidManager::AddResult add_result = uid_manager_.AddStaticUid(handle, rptdata->client_uid);
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
    if (rptdata->client_type == kRPTClientTypeController)
    {
      if ((settings_.max_controllers > 0) && (controllers_.size() >= settings_.max_controllers))
      {
        connect_status = kRdmnetConnectCapacityExceeded;
        continue_adding = false;
        uid_manager_.RemoveUid(rptdata->client_uid);
      }
      else
      {
        auto controller =
            std::make_shared<RPTController>(settings_.max_controller_messages, updated_data, *clients_[handle]);
        if (controller)
        {
          new_client = controller.get();
          controllers_.insert(std::make_pair(handle, controller));
          clients_[handle] = std::move(controller);
        }
      }
    }
    // If it's a device, add it to the device states -- unless we've hit
    // our maximum number of devices
    else if (rptdata->client_type == kRPTClientTypeDevice)
    {
      if ((settings_.max_devices > 0) && (devices_.size() >= settings_.max_devices))
      {
        connect_status = kRdmnetConnectCapacityExceeded;
        continue_adding = false;
        uid_manager_.RemoveUid(rptdata->client_uid);
      }
      else
      {
        auto device = std::make_shared<RPTDevice>(settings_.max_device_messages, updated_data, *clients_[handle]);
        if (device)
        {
          new_client = device.get();
          devices_.insert(std::make_pair(handle, device));
          clients_[handle] = std::move(device);
        }
      }
    }
  }

  // The client is already part of our connections, but we need to update
  // it or check if capacity is exceeded
  if (continue_adding && new_client)
  {
    new_client->client_type = rptdata->client_type;
    new_client->uid = rptdata->client_uid;
    new_client->binding_cid = rptdata->binding_cid;

    // Send the connect reply
    BrokerMessage msg;
    msg.vector = VECTOR_BROKER_CONNECT_REPLY;
    ConnectReplyMsg* creply = get_connect_reply_msg(&msg);
    creply->connect_status = kRdmnetConnectOk;
    creply->e133_version = E133_VERSION;
    creply->broker_uid = my_uid_;
    creply->client_uid = rptdata->client_uid;
    new_client->Push(settings_.cid, msg);

    if (log_->CanLog(ETCPAL_LOG_INFO))
    {
      log_->Log(ETCPAL_LOG_INFO, "Successfully processed RPT Connect request from %s (connection %d), UID %04x:%08x",
                new_client->client_type == kRPTClientTypeController ? "Controller" : "Device", handle,
                new_client->uid.manu, new_client->uid.id);
    }

    // Update everyone
    std::vector<ClientEntryData> entries;
    entries.push_back(updated_data);
    entries[0].next = nullptr;
    SendClientsAdded(kClientProtocolRPT, handle, entries);
  }
  return continue_adding;
}

void BrokerCore::ProcessRPTMessage(int conn, const RdmnetMessage* msg)
{
  etcpal::ReadGuard clients_read(client_lock_);

  const RptMessage* rptmsg = get_rpt_msg(msg);
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
              log_->Log(ETCPAL_LOG_DEBUG,
                        "Received Request PDU addressed to invalid or not found UID %04x:%08x from Controller %d",
                        rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
            else if (get_rdm_buf_list(rptmsg)->list->next)
            {
              // There should only ever be one RDM command in an RPT request.
              SendStatus(controller, rptmsg->header, kRptStatusInvalidMessage);
              log_->Log(ETCPAL_LOG_DEBUG,
                        "Received Request PDU from Controller %d which incorrectly contains multiple RDM Command PDUs",
                        conn);
            }
            else
            {
              route_msg = true;
            }
          }
          else
          {
            log_->Log(ETCPAL_LOG_DEBUG, "Received Request PDU from Client %d, which is not an RPT Controller", conn);
          }
          break;

        case VECTOR_RPT_STATUS:
          if (rptcli->client_type == kRPTClientTypeDevice)
          {
            if (IsValidDeviceDestinationUID(rptmsg->header.dest_uid))
            {
              if (get_rpt_status_msg(rptmsg)->status_code != kRptStatusBroadcastComplete)
                route_msg = true;
              else
                log_->Log(ETCPAL_LOG_DEBUG, "Device %d sent broadcast complete message.", conn);
            }
            else
            {
              log_->Log(ETCPAL_LOG_DEBUG,
                        "Received Status PDU addressed to invalid or not found UID %04x:%08x from Device %d",
                        rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
          }
          else
          {
            log_->Log(ETCPAL_LOG_DEBUG, "Received Status PDU from Client %d, which is not an RPT Device", conn);
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
              log_->Log(ETCPAL_LOG_DEBUG,
                        "Received Notification PDU addressed to invalid or not found UID %04x:%08x from Device %d",
                        rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
          }
          else
          {
            log_->Log(ETCPAL_LOG_DEBUG, "Received Notification PDU from Client %d of unknown client type",
                      rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
          }
          break;

        default:
          log_->Log(ETCPAL_LOG_WARNING, "Received RPT PDU with unknown vector %d from Client %d", rptmsg->vector, conn);
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
      log_->Log(ETCPAL_LOG_DEBUG, "Broadcasting RPT message from Device %04x:%08x to all Controllers",
                rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id);
      for (auto controller : controllers_)
      {
        ClientWriteGuard client_write(*controller.second);
        if (!controller.second->Push(conn, msg->sender_cid, *rptmsg))
        {
          // TODO disconnect
          log_->Log(ETCPAL_LOG_ERR, "Error pushing to send queue for RPT Controller %d. DEBUG:NOT disconnecting...",
                    controller.first);
        }
      }
    }
    else if (RDMNET_UID_IS_DEVICE_BROADCAST(&rptmsg->header.dest_uid))
    {
      log_->Log(ETCPAL_LOG_DEBUG, "Broadcasting RPT message from Controller %04x:%08x to all Devices",
                rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id);
      for (auto device : devices_)
      {
        ClientWriteGuard client_write(*device.second);
        if (!device.second->Push(conn, msg->sender_cid, *rptmsg))
        {
          // TODO disconnect
          log_->Log(ETCPAL_LOG_ERR, "Error pushing to send queue for RPT Device %d. DEBUG:NOT disconnecting...",
                    device.first);
        }
      }
    }
    else if (IsDeviceManuBroadcastUID(rptmsg->header.dest_uid, device_manu))
    {
      log_->Log(ETCPAL_LOG_DEBUG,
                "Broadcasting RPT message from Controller %04x:%08x to all Devices from manufacturer %04x",
                rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, device_manu);
      for (auto device : devices_)
      {
        if (device.second->uid.manu == device_manu)
        {
          ClientWriteGuard client_write(*device.second);
          if (!device.second->Push(conn, msg->sender_cid, *rptmsg))
          {
            // TODO disconnect
            log_->Log(ETCPAL_LOG_ERR, "Error pushing to send queue for RPT Device %d. DEBUG:NOT disconnecting...",
                      device.first);
          }
        }
      }
    }
    else
    {
      bool found_dest_client = false;
      if (uid_manager_.UidToHandle(rptmsg->header.dest_uid, dest_conn))
      {
        auto dest_client = clients_.find(dest_conn);
        if (dest_client != clients_.end())
        {
          ClientWriteGuard client_write(*dest_client->second);
          if (static_cast<RPTClient*>(dest_client->second.get())->Push(conn, msg->sender_cid, *rptmsg))
          {
            found_dest_client = true;
            log_->Log(ETCPAL_LOG_DEBUG, "Routing RPT PDU from Client %04x:%08x to Client %04x:%08x",
                      rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, rptmsg->header.dest_uid.manu,
                      rptmsg->header.dest_uid.id);
          }
          else
          {
            // TODO disconnect
            log_->Log(ETCPAL_LOG_ERR, "Error pushing to send queue for RPT Client %d. DEBUG:NOT disconnecting...",
                      dest_client->first);
          }
        }
      }
      if (!found_dest_client)
      {
        log_->Log(ETCPAL_LOG_ERR,
                  "Could not route message from RPT Client %d (%04x:%08x): Destination UID %04x:%08x not found.", conn,
                  rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, rptmsg->header.dest_uid.manu,
                  rptmsg->header.dest_uid.id);
      }
    }
  }
}

etcpal_socket_t BrokerCore::StartListening(const EtcPalIpAddr& ip, uint16_t& port)
{
  EtcPalSockaddr addr;
  addr.ip = ip;
  addr.port = port;

  etcpal_socket_t listen_sock;
  etcpal_error_t err =
      etcpal_socket(ETCPAL_IP_IS_V4(&addr.ip) ? ETCPAL_AF_INET : ETCPAL_AF_INET6, ETCPAL_STREAM, &listen_sock);
  if (err != kEtcPalErrOk)
  {
    if (log_)
    {
      log_->Log(ETCPAL_LOG_WARNING, "Broker: Failed to create listen socket with error: %s.", etcpal_strerror(err));
    }
    return ETCPAL_SOCKET_INVALID;
  }

  int sockopt_val = 0;
  err = etcpal_setsockopt(listen_sock, ETCPAL_IPPROTO_IPV6, ETCPAL_IPV6_V6ONLY, &sockopt_val, sizeof(int));
  if (err != kEtcPalErrOk)
  {
    etcpal_close(listen_sock);
    if (log_)
    {
      log_->Log(ETCPAL_LOG_WARNING, "Broker: Failed to set V6ONLY socket option on listen socket: %s.",
                etcpal_strerror(err));
    }
    return ETCPAL_SOCKET_INVALID;
  }

  err = etcpal_bind(listen_sock, &addr);
  if (err != kEtcPalErrOk)
  {
    etcpal_close(listen_sock);
    if (log_ && log_->CanLog(ETCPAL_LOG_WARNING))
    {
      char addrstr[ETCPAL_INET6_ADDRSTRLEN];
      etcpal_inet_ntop(&addr.ip, addrstr, ETCPAL_INET6_ADDRSTRLEN);
      log_->Log(ETCPAL_LOG_WARNING, "Broker: Bind to %s failed on listen socket with error: %s.", addrstr,
                etcpal_strerror(err));
    }
    return ETCPAL_SOCKET_INVALID;
  }

  if (port == 0)
  {
    // Get the ephemeral port number we were assigned and which we will use for all other
    // applicable network interfaces.
    err = etcpal_getsockname(listen_sock, &addr);
    if (err == kEtcPalErrOk)
    {
      port = addr.port;
    }
    else
    {
      etcpal_close(listen_sock);
      if (log_)
      {
        log_->Log(ETCPAL_LOG_WARNING, "Broker: Failed to get ephemeral port assigned to listen socket: %s",
                  etcpal_strerror(err));
      }
      return ETCPAL_SOCKET_INVALID;
    }
  }

  err = etcpal_listen(listen_sock, 0);
  if (err != kEtcPalErrOk)
  {
    etcpal_close(listen_sock);
    if (log_)
    {
      log_->Log(ETCPAL_LOG_WARNING, "Broker: Listen failed on listen socket with error: %s.", etcpal_strerror(err));
    }
    return ETCPAL_SOCKET_INVALID;
  }
  return listen_sock;
}

bool BrokerCore::StartBrokerServices()
{
  bool success = true;

  if (listen_addrs_.empty())
  {
    // Listen on in6addr_any
    EtcPalIpAddr any_addr;
    etcpal_ip_set_wildcard(kEtcPalIpTypeV6, &any_addr);

    etcpal_socket_t listen_sock = StartListening(any_addr, listen_port_);
    if (listen_sock != ETCPAL_SOCKET_INVALID)
    {
      auto p = std::make_unique<ListenThread>(listen_sock, this, log_);
      listeners_.push_back(std::move(p));
    }
    else
    {
      success = false;
    }
  }
  else
  {
    // Listen on a specific set of interfaces supplied by the library user
    auto addr_iter = listen_addrs_.begin();
    while (addr_iter != listen_addrs_.end())
    {
      etcpal_socket_t listen_sock = StartListening(*addr_iter, listen_port_);
      if (listen_sock != ETCPAL_SOCKET_INVALID)
      {
        auto p = std::make_unique<ListenThread>(listen_sock, this, log_);
        listeners_.push_back(std::move(p));
        ++addr_iter;
      }
      else
      {
        addr_iter = listen_addrs_.erase(addr_iter);
      }
    }

    // Errors on some interfaces are tolerated as long as we have at least one to listen on.
    success = (!listen_addrs_.empty());
  }

  if (success)
  {
    for (const auto& listener : listeners_)
      listener->Start();
  }
  else
  {
    listeners_.clear();
  }
  return success;
}

void BrokerCore::StopBrokerServices()
{
  for (const auto& listener : listeners_)
    listener->Stop();

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
    conn_interface_->DestroyConnection(conn, send_disconnect);
    log_->Log(ETCPAL_LOG_DEBUG, "Connection %d marked for destruction", conn);
  }
}

// These functions will take a write lock on client_lock_ and client_destroy_lock_.
void BrokerCore::DestroyMarkedClientSockets()
{
  etcpal::WriteGuard clients_write(client_lock_);
  std::vector<ClientEntryData> entries;

  if (!clients_to_destroy_.empty())
  {
    for (auto to_destroy : clients_to_destroy_)
    {
      auto client = clients_.find(to_destroy);
      if (client != clients_.end())
      {
        ClientEntryData entry;
        entry.client_protocol = client->second->client_protocol;
        entry.client_cid = client->second->cid;

        if (client->second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
        {
          RPTClient* rptcli = static_cast<RPTClient*>(client->second.get());
          uid_manager_.RemoveUid(rptcli->uid);
          if (rptcli->client_type == kRPTClientTypeController)
            controllers_.erase(to_destroy);
          else if (rptcli->client_type == kRPTClientTypeDevice)
            devices_.erase(to_destroy);

          ClientEntryDataRpt* rptdata = get_rpt_client_entry_data(&entry);
          rptdata->client_uid = rptcli->uid;
          rptdata->client_type = rptcli->client_type;
          rptdata->binding_cid = rptcli->binding_cid;
        }
        entries.push_back(entry);
        entries[entries.size() - 1].next = nullptr;
        if (entries.size() > 1)
          entries[entries.size() - 2].next = &entries[entries.size() - 1];
        clients_.erase(client);

        log_->Log(ETCPAL_LOG_INFO, "Removing connection %d marked for destruction.", to_destroy);
        log_->Log(ETCPAL_LOG_DEBUG, "Clients: %zu Controllers: %zu Devices: %zu", clients_.size(), controllers_.size(),
                  devices_.size());
      }
    }
    clients_to_destroy_.clear();
  }

  if (!entries.empty())
    SendClientsRemoved(entries[0].client_protocol, entries);
}

void BrokerCore::BrokerRegistered(const std::string& assigned_service_name)
{
  service_registered_ = true;
  log_->Log(ETCPAL_LOG_INFO, "Broker \"%s\" (now named \"%s\") successfully registered at scope \"%s\"",
            disc_.requested_service_name().c_str(), assigned_service_name.c_str(), disc_.scope().c_str());
}

void BrokerCore::BrokerRegisterError(int platform_specific_error)
{
  log_->Log(ETCPAL_LOG_ERR, "Broker \"%s\" register error %d at scope \"%s\"", disc_.requested_service_name().c_str(),
            platform_specific_error, disc_.scope().c_str());
}

void BrokerCore::OtherBrokerFound(const RdmnetBrokerDiscInfo& broker_info)
{
  if (log_->CanLog(ETCPAL_LOG_WARNING))
  {
    std::string addrs;
    for (const BrokerListenAddr* listen_addr = broker_info.listen_addr_list; listen_addr;
         listen_addr = listen_addr->next)
    {
      char addr_string[ETCPAL_INET6_ADDRSTRLEN];
      if (kEtcPalErrOk == etcpal_inet_ntop(&listen_addr->addr, addr_string, ETCPAL_INET6_ADDRSTRLEN))
      {
        addrs.append(addr_string);
        if (listen_addr->next)
          addrs.append(", ");
      }
    }

    log_->Log(ETCPAL_LOG_WARNING, "Broker \"%s\", ip[%s] found at same scope(\"%s\") as this broker.",
              broker_info.service_name, addrs.c_str(), broker_info.scope);
  }
  if (!service_registered_)
  {
    log_->Log(ETCPAL_LOG_WARNING,
              "This broker will remain unregistered with DNS-SD until all conflicting brokers are removed.");
    // StopBrokerServices();
  }
}

void BrokerCore::OtherBrokerLost(const std::string& service_name)
{
  log_->Log(ETCPAL_LOG_WARNING, "Conflicting broker %s no longer discovered.", service_name.c_str());
}
