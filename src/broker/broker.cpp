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

// The generic broker implementation

#include "broker/broker.h"

#include <cstring>
#include "lwpa_pack.h"
#include "lwpa_socket.h"
#include "brokerconsts.h"
#include "rdmnet/version.h"
#include "rdmnet/connection.h"
#include "broker/util.h"
#include "rdmnet/discovery.h"

/* Suppress strncpy() warning on Windows/MSVC. */
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/************************* The draft warning message *************************/

/* clang-format off */
#pragma message("************ THIS CODE IMPLEMENTS A DRAFT STANDARD ************")
#pragma message("*** PLEASE DO NOT INCLUDE THIS CODE IN ANY SHIPPING PRODUCT ***")
#pragma message("************* SEE THE README FOR MORE INFORMATION *************")
/* clang-format on */

/**************************** Private constants ******************************/

// The amount of time we'll block until we get something to read from a
// connection
#define READ_TIMEOUT_MS 200

/*************************** Function definitions ****************************/

Broker::Broker(BrokerLog *log, IBrokerNotify *notify)
    : IListenThread_Notify()
    , IClientServiceThread_Notify()
    , IConnPollThread_Notify()
    //  , IRDMnet_MDNS_Notify()
    , service_thread_(1)
    , started_(false)
    , log_(log)
    , notify_(notify)
//  , mdns_(nullptr)
{
}

Broker::~Broker()
{
  if (started_)
    Shutdown();
}

// This starts all Broker functionality and threads.
// If listen_addrs is empty, this returns false.  Otherwise, the broker
// uses the address fields to set up the listening sockets. If the
// listen_port is 0 and their is only one listen_addr, an ephemeral port is
// chosen. If there are more listen_addrs, listen_port must not be 0.
bool Broker::Startup(const BrokerSettings &settings, uint16_t listen_port, std::vector<LwpaIpAddr> &listen_addrs)
{
  if (!started_)
  {
    if (listen_addrs.empty() || ((listen_addrs.size() > 1) && (listen_port == 0)))
      return false;

    settings_ = settings;

    // Initialize DNS discovery.
    BrokerDiscInfo info;
    fill_default_broker_info(&info);

    if (settings_.disc_attributes.mdns_domain.empty())
      settings_.disc_attributes.mdns_domain = E133_DEFAULT_DOMAIN;
    info.cid = settings_.cid;
    strncpy(info.domain, settings_.disc_attributes.mdns_domain.c_str(), E133_DOMAIN_STRING_PADDED_LENGTH);
    for (size_t i = 0; i < listen_addrs.size(); i++)
      info.listen_addrs[i].ip =
          listen_addrs[i];  // TODO: make sure lwpa_sockaddr is what we want on the library's side of things
    info.listen_addrs_count = listen_addrs.size();
    strncpy(info.manufacturer, settings_.disc_attributes.mdns_manufacturer.c_str(),
            E133_MANUFACTURER_STRING_PADDED_LENGTH);
    strncpy(info.model, settings_.disc_attributes.mdns_model.c_str(), E133_MODEL_STRING_PADDED_LENGTH);
    info.port = listen_port;
    strncpy(info.scope, settings_.disc_attributes.scope.c_str(), E133_SCOPE_STRING_PADDED_LENGTH);
    strncpy(info.service_name, settings_.disc_attributes.mdns_service_instance_name.c_str(),
            E133_SERVICE_NAME_STRING_PADDED_LENGTH);

    if (LWPA_OK != rdmnet_init(log_->GetLogParams()))
      return false;

    if (!lwpa_rwlock_create(&client_lock_))
    {
      rdmnet_deinit();
      return false;
    }
    if (!lwpa_mutex_create(&poll_thread_lock_))
    {
      lwpa_rwlock_destroy(&client_lock_);
      rdmnet_deinit();
      return false;
    }
    if (!lwpa_mutex_create(&client_destroy_lock_))
    {
      lwpa_mutex_destroy(&poll_thread_lock_);
      lwpa_rwlock_destroy(&client_lock_);
      rdmnet_deinit();
      return false;
    }

    for (const auto &ip : listen_addrs)
    {
      LwpaSockaddr addr;
      addr.ip = ip;
      addr.port = listen_port;
      auto p = std::make_unique<ListenThread>(addr, this);
      if (p)
        listeners_.push_back(std::move(p));
    }

    started_ = true;

    StartListening();

    service_thread_.SetNotify(this);
    service_thread_.Start();

    SetCallbackFunctions(&callbacks_);
    rdmnetdisc_init(&callbacks_);
    if (rdmnetdisc_registerbroker(&info, this))  // previous code calls shutdown if true?
    {
      Shutdown();
      return false;
    }

    log_->Log(LWPA_LOG_INFO, std::string(settings_.disc_attributes.mdns_manufacturer +
                                         " Prototype RDMnet Broker Version " + RDMNET_VERSION_STRING)
                                 .c_str());
    log_->Log(LWPA_LOG_INFO, "Broker starting at scope \"%s\", listening on port %d, using network interfaces:",
              settings.disc_attributes.scope.c_str(), listen_port);
    for (auto addr : listen_addrs)
    {
      char addrbuf[LWPA_INET6_ADDRSTRLEN];
      lwpa_inet_ntop(&addr, addrbuf, LWPA_INET6_ADDRSTRLEN);
      log_->Log(LWPA_LOG_INFO, "%s", addrbuf);
    }
  }

  return started_;
}

// Call before destruction to gracefully close
void Broker::Shutdown()
{
  if (started_)
  {
    rdmnetdisc_unregisterbroker();

    StopListening();
    listeners_.clear();

    // No new connections coming in, manually shut down the existing ones.
    //    std::vector<int> conns;
    //    GetConnSnapshot(conns, true, true, true);

    //    for (auto it = conns.begin(); it != conns.end(); ++it)
    //      MarkSocketForDestruction(*it, true, DISCONNECT_SHUTDOWN);

    // Give the device sockets up to 1 second to send out the disconnect
    // notifications and shut down
    //    for (int i = 0; i < 10; ++i)
    //    {
    //      if device_destroy_lock_.ReadLock())
    //      {
    //        if devices_to_destroy_.size() == 0)
    //        {
    //         device_destroy_lock_.ReadUnlock();
    //          break;
    //        }
    //        lwpa_thread_sleep(100);
    //      }
    //    }

    service_thread_.Stop();

    // Cleanup remaining sockets manually
    //    DestroyMarkedControllerSockets();
    //    DestroyMarkedDeviceSockets();

    //    //Clean up any remaining messages
    //    RDMnetSocket::socket_msg msg;
    //    while socket_messages_.pop(msg))
    //      DeleteMessage(msg);

    lwpa_rwlock_destroy(&client_lock_);
    // rdmnet_deinit();

    started_ = false;

    //    RDMnetSocket::StackShutdown();

    //    // Shutdown LLRP socket and proxy
    //   llrpSocketProxy_.StopProxy();
    //   llrpSocket_.Shutdown();
  }
}

void Broker::Tick()
{
  rdmnetdisc_tick(this);
  DestroyMarkedClientSockets();
}

// Fills in the current settings the broker is using.  Can be called even
// after Shutdown. Useful if you want to shutdown & restart the broker for
// any reason.
void Broker::GetSettings(BrokerSettings &settings) const
{
  settings = settings_;
}

constexpr bool Broker::IsBroadcastUID(const LwpaUid &uid)
{
  return uid.id == 0xffffffff;
}

constexpr bool Broker::IsControllerBroadcastUID(const LwpaUid &uid)
{
  return (((uint64_t)uid.manu << 32 | uid.id) == E133_RPT_ALL_CONTROLLERS);
}

constexpr bool Broker::IsDeviceBroadcastUID(const LwpaUid &uid)
{
  return (((uint64_t)uid.manu << 32 | uid.id) == E133_RPT_ALL_DEVICES);
}

bool Broker::IsDeviceManuBroadcastUID(const LwpaUid &uid, uint16_t &manu)
{
  if ((uid.manu == ((uint16_t)((E133_RPT_ALL_DEVICES >> 16) & 0xffffu))) &&
      (((uint16_t)uid.id) == ((uint16_t)(E133_RPT_ALL_DEVICES & 0xffffu))) && (((uint16_t)(uid.id >> 16) != 0xffffu)))
  {
    manu = uid.id >> 16;
    return true;
  }
  return false;
}

bool Broker::IsValidControllerDestinationUID(const LwpaUid &uid) const
{
  if (IsDeviceBroadcastUID(uid) || (uid == settings_.uid))
    return true;

  // TODO this should only check devices
  int tmp;
  return UIDToHandle(uid, tmp);
}

bool Broker::IsValidDeviceDestinationUID(const LwpaUid &uid) const
{
  if (IsControllerBroadcastUID(uid))
    return true;

  // TODO this should only check controllers
  int tmp;
  return UIDToHandle(uid, tmp);
}

// If the uid is in the map, this returns true and fills in the cookie.
bool Broker::UIDToHandle(const LwpaUid &uid, int &conn_handle) const
{
  BrokerReadGuard client_read(client_lock_);

  bool result = false;
  auto it = uid_lookup_.find(uid);
  if (it != uid_lookup_.end())
  {
    result = true;
    conn_handle = it->second;
  }
  return result;
}

// The passed-in vector is cleared and filled with the cookies of connections
// that match the criteria.
void Broker::GetConnSnapshot(std::vector<int> &conns, bool include_devices, bool include_controllers,
                             bool include_unknown, uint16_t manufacturer_filter)
{
  conns.clear();

  BrokerReadGuard client_read(client_lock_);

  if (clients_.empty())
  {
    // We'll just do a bulk reserve.  The actual vector may take up less.
    conns.reserve(clients_.size());

    for (const auto &client : clients_)
    {
      // TODO EPT
      if (client.second)
      {
        RPTClient *rpt = static_cast<RPTClient *>(client.second.get());
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

// Gets the appropriate locks for writing controller state.  If something's
// wrong, returns nullptr. Be sure to call ReleaseControllerFromWriting if
// this returns a valid pointer.
RPTController *Broker::GetControllerForWriting(int conn) const
{
  RPTController *pdata = nullptr;
  if (ClientReadLock())
  {
    auto it = controllers_.find(conn);
    if (it != controllers_.end() && it->second && it->second->WriteLock())
    {
      pdata = it->second.get();
      // And keep the read lock
    }
    else
      ClientReadUnlock();
  }
  return pdata;
}

void Broker::ReleaseControllerFromWriting(RPTController *pdata) const
{
  if (pdata)
  {
    pdata->WriteUnlock();
    ClientReadUnlock();
  }
}

// Gets the appropriate locks for reading controller state.  If something's
// wrong, returns nullptr. Be sure to call ReleaseControllerFromReading if
// this returns a valid pointer.
const RPTController *Broker::GetControllerForReading(int conn) const
{
  const RPTController *pdata = nullptr;
  if (ClientReadLock())
  {
    auto it = controllers_.find(conn);
    if (it != controllers_.end() && it->second && it->second->ReadLock())
      pdata = it->second.get();
    else
      ClientReadUnlock();
  }

  return pdata;
}

void Broker::ReleaseControllerFromReading(const RPTController *pdata) const
{
  if (pdata)
  {
    pdata->ReadUnlock();
    ClientReadUnlock();
  }
}

// Gets the appropriate locks for writing device state.  If something's
// wrong, returns nullptr. Be sure to call ReleaseDeviceFromWriting if this
// returns a valid pointer.
RPTDevice *Broker::GetDeviceForWriting(int conn) const
{
  RPTDevice *pdata = nullptr;
  if (ClientReadLock())
  {
    auto it = devices_.find(conn);
    if (it != devices_.end() && it->second && it->second->WriteLock())
      pdata = it->second.get();
    else
      ClientReadUnlock();
  }

  return pdata;
}

void Broker::ReleaseDeviceFromWriting(RPTDevice *pdata) const
{
  if (pdata)
  {
    pdata->WriteUnlock();
    ClientReadUnlock();
  }
}

// Gets the appropriate locks for reading device state.  If something's
// wrong, returns nullptr. Be sure to call ReleaseDeviceFromReading if this
// returns a valid pointer.
const RPTDevice *Broker::GetDeviceForReading(int conn) const
{
  RPTDevice *pdata = nullptr;
  if (ClientReadLock())
  {
    auto it = devices_.find(conn);
    if (it != devices_.end() && it->second && it->second->ReadLock())
      pdata = it->second.get();
    else
      ClientReadUnlock();
  }

  return pdata;
}

void Broker::ReleaseDeviceFromReading(const RPTDevice *pdata) const
{
  if (pdata)
  {
    pdata->ReadUnlock();
    ClientReadUnlock();
  }
}

bool Broker::NewConnection(lwpa_socket_t new_sock, const LwpaSockaddr &addr)
{
  if (log_->CanLog(LWPA_LOG_INFO))
  {
    char addrstr[LWPA_INET6_ADDRSTRLEN];
    lwpa_inet_ntop(&addr.ip, addrstr, LWPA_INET6_ADDRSTRLEN);
    log_->Log(LWPA_LOG_INFO, "Creating a new connection for ip addr %s", addrstr);
  }

  int connhandle = -1;

  bool result = false;
  {  // Client write lock scope
    BrokerWriteGuard client_write(client_lock_);

    connhandle = rdmnet_new_connection(&settings_.cid);
    if (connhandle >= 0 && (settings_.max_connections == 0 ||
                            (clients_.size() <= settings_.max_connections + settings_.max_reject_connections)))
    {
      auto client = std::make_shared<BrokerClient>(connhandle);

      // Before inserting the connection, make sure we can attach the socket.
      if (client && LWPA_OK == rdmnet_attach_existing_socket(connhandle, new_sock, &addr))
      {
        client->addr = addr;
        AddConnToPollThread(connhandle, client->poll_thread);
        clients_.insert(std::make_pair(connhandle, std::move(client)));
        result = true;
      }
    }
  }

  if (result)
  {
    log_->Log(LWPA_LOG_INFO, "New connection created with handle %d", connhandle);
  }
  else
  {
    log_->Log(LWPA_LOG_INFO, "New connection failed");
  }

  return result;
}

// Called to log an error.  You may want to stop the listening thread if
// errors keep occurring, but you should NOT do it in this callback!
void Broker::LogError(const std::string &err)
{
  // TESTING TODO: For now, we'll just log, but we may want to mark this
  // listener for later stopping?
  log_->Log(LWPA_LOG_ERR, "%s", err.c_str());
}

void Broker::PollConnections(const std::vector<int> &conn_handles, RdmnetPoll *poll_arr)
{
  size_t poll_arr_size = 0;
  std::vector<int> conns;

  if (poll_arr)
  {
    BrokerReadGuard clients_read(client_lock_);
    for (auto conn_handle : conn_handles)
    {
      auto client = clients_.find(conn_handle);
      if (client != clients_.end())
      {
        bool poll_client = true;
        {
          ClientReadGuard client_read(*client->second);
          poll_client = !client->second->marked_for_destruction;
        }
        if (poll_client)
        {
          poll_arr[poll_arr_size].handle = client->first;
          ++poll_arr_size;
        }
      }
    }
  }

  int poll_res = 0;
  std::vector<int> ready_conns;
  if (poll_arr && poll_arr_size)
  {
    poll_res = rdmnet_poll(poll_arr, poll_arr_size, READ_TIMEOUT_MS);

    if (poll_res > 0)
    {
      for (size_t i = 0; i < poll_arr_size && poll_res; ++i)
      {
        if (poll_arr[i].err == LWPA_OK)
        {
          ready_conns.push_back(poll_arr[i].handle);
          --poll_res;
        }
        else if (poll_arr[i].err != LWPA_NODATA)
        {
          log_->Log(LWPA_LOG_INFO, "Connection %d encountered error: '%s'. Removing.", poll_arr[i].handle,
                    lwpa_strerror(poll_arr[i].err));
          MarkConnForDestruction(poll_arr[i].handle, false);
          --poll_res;
        }
      }
    }
    // TODO else handle error
  }

  for (auto conn : ready_conns)
  {
    bool found_connection = false;

    {  // Client read lock scope
      BrokerReadGuard clients_read(client_lock_);
      if (clients_.find(conn) != clients_.end())
        found_connection = true;
    }

    if (found_connection)
    {
      RdmnetData data;

      lwpa_error_t res = rdmnet_recv(conn, &data);
      switch (res)
      {
        case LWPA_OK:
          ProcessTCPMessage(conn, rdmnet_data_msg(&data));
          free_rdmnet_message(rdmnet_data_msg(&data));
          break;
        case LWPA_NODATA:
          break;
        case LWPA_CONNCLOSED:
        case LWPA_CONNRESET:
        case LWPA_TIMEDOUT:
        case LWPA_NOTCONN:
          if (res == LWPA_CONNCLOSED && rdmnet_data_is_code(&data))
          {
            log_->Log(LWPA_LOG_INFO, "Connection %d sent graceful RDMnet disconnect with reason %d.", conn,
                      rdmnet_data_code(&data));
          }
          else
          {
            log_->Log(LWPA_LOG_INFO, "Connection %d disconnected with error: '%s'.", conn, lwpa_strerror(res));
          }
          break;
        default:
          log_->Log(LWPA_LOG_WARNING, "rdmnet_recv() failed with unexpected error: '%s' after successful poll",
                    lwpa_strerror(res));
          break;
      }
      if (res != LWPA_OK && res != LWPA_NODATA)
        MarkConnForDestruction(conn, false);

      // TODO finish
    }
  }
}

///* ILLRPSocketProxy_Notify messages */
////llrp_data will point to the buffer data immediately after the UDP
/// preamble. /packet_data will point to the beginning of the packet. This
/// is the pointer you should call DeletePacket() on when finished.
/// /data_length will be the size of the buffer memory BELOW the UDP
/// preamble. /packet_length will be the size of the whole packet,
/// INCLUDING the UDP preamble. /Call E133::UnpackRoot with llrp_data to
/// start parsing the packet.
// void Broker::Received(const uint8_t* llrp_data, uint llrp_data_len,
// uint8_t* packet_data, uint packet_len)
//{
//  CID sender_cid;
//  uint32_t vector;
//  const uint8_t *header_data;
//  uint32_t header_data_len;
//  const uint8_t *vector_data;
//  uint32_t vector_data_len;
//  LLRP::llrp_data_struct header;

//  E133::UnpackRoot(llrp_data, llrp_data_len, sender_cid, vector,
//  header_data, header_data_len); LLRP::UnpackLLRPHeader(header_data,
//  header_data_len, header, vector_data, vector_data_len);

//  if ((header.dest_cid == CID::StringToCID(LLRP_BROADCAST_CID)) ||
//  (header.dest_cid ==settings_.cid))
//  {
//    if (header.vector == VECTOR_LLRP_PROBE_REQUEST)
//    {
//      E133_UID lower_uid;
//      E133_UID upper_uid;
//      uint8_t filter;
//      std::vector<E133_UID> known_uids;

//      // Process the probe request
//      LLRP::UnpackProbeRequest(vector_data, vector_data_len, lower_uid,
//      upper_uid, filter, known_uids);

//      if (settings_.uid >= lower_uid) && settings_.uid <= upper_uid))
//      {
//        if (settings_.uid.PresentInVector(known_uids))
//        {
//          LLRP::llrp_data_struct llrp_data_response;

//          if serv_)
//          {
//            IAsyncSocketServ::netintinfo info;
//            netintid id =serv_->GetDefaultInterface();

//            if serv_->CopyInterfaceInfo(id, info))
//            {
//              llrp_data_response.dest_cid = sender_cid;
//              llrp_data_response.transaction_number =
//              header.transaction_number; llrp_data_response.vector =
//              VECTOR_LLRP_PROBE_REPLY;

//              LLRP::SendProbeReply(llrpSocket_,settings_.cid,
//              llrp_data_response,settings_.uid,
//              reinterpret_cast<uint8_t*>(info.mac));

//              if log_)
//                lwpa_loglog_,log_context_, 2, LWPA_CAT_APP,
//                LWPA_SEV_INF, "Got a Probe Request I can respond to! Sent
//                a Probe Reply.");
//            }
//          }
//        }
//      }
//    }
//    else if (header.vector == VECTOR_LLRP_RDM_CMD)
//    {
//      rdmBuffer msg;
//      uid dest_uid;

//      // Process the LLRP RDM message

//      LLRP::UnpackRDMCommand(vector_data, vector_data_len, msg);

//      dest_uid = getDestinationId(const_cast<rdmBuffer*>(&msg));

//      if (settings_.uid.Getuid().id == dest_uid.id) &&
//      settings_.uid.Getuid().manu == dest_uid.manu))
//      {
//        printf("\nGot an LLRP RDM message.");

//       llrp_msg_proc_.ProcessLLRPRDMMessage(msg, sender_cid,
//        header.transaction_number);
//      }
//    }
//  }

// llrpSocket_.DeletePacket(packet_data, packet_len);
//}

////This means that the target reading socket has gone bad/closed. Call
/// Shutdown() on the LLRPSocket to make sure /that the bad socket is
/// destroyed. From there you can call Startup() again to recover with new
/// sockets.
// void Broker::TargetReadSocketBad()
//{
//  if log_)
//  {
//    lwpa_loglog_,log_context_, 1, LWPA_CAT_APP, LWPA_SEV_ERR, "Target
//    read socket went bad!");
//  }

// llrpSocket_.Shutdown();
//}

////This means that the target writing socket has gone bad/closed. Call
/// Shutdown() on the LLRPSocket to make sure /that the bad socket is
/// destroyed. From there you can call Startup() again to recover with new
/// sockets.
// void Broker::TargetWriteSocketBad()
//{
//  if log_)
//  {
//    lwpa_loglog_,log_context_, 1, LWPA_CAT_APP, LWPA_SEV_ERR, "Target
//    write socket went bad!");
//  }

// llrpSocket_.Shutdown();
//}

////This means that the  reading socket has gone bad/closed. Call
/// Shutdown()
/// on
/// the LLRPSocket to make sure /that the bad socket is destroyed. From
/// there you can call Startup() again to recover with new sockets.
// void Broker::ControllerReadSocketBad()
//{
//  if log_)
//  {
//    lwpa_loglog_,log_context_, 1, LWPA_CAT_APP, LWPA_SEV_ERR,
//    "Controller read socket went bad!");
//  }

// llrpSocket_.Shutdown();
//}

////This means that the controller writing socket has gone bad/closed. Call
/// Shutdown() on the LLRPSocket to make sure /that the bad socket is
/// destroyed. From there you can call Startup() again to recover with new
/// sockets.
// void Broker::ControllerWriteSocketBad()
//{
//  if log_)
//  {
//    lwpa_loglog_,log_context_, 1, LWPA_CAT_APP, LWPA_SEV_ERR,
//    "Controller write socket went bad!");
//  }

// llrpSocket_.Shutdown();
//}

// Process each controller queue, sending out the next message from each
// queue if devices are available. Also sends connect reply, error and
// status messages generated asynchronously to devices.  Return false if no
// controllers messages were sent.
bool Broker::ServiceClients()
{
  bool result = false;
  std::vector<int> client_conns;

  BrokerReadGuard clients_read(client_lock_);

  for (auto client : clients_)
  {
    ClientWriteGuard client_write(*client.second);
    result |= client.second->Send();
  }
  return result;
}

// Message processing functions
void Broker::ProcessTCPMessage(int conn, const RdmnetMessage *msg)
{
  switch (msg->vector)
  {
    case VECTOR_ROOT_BROKER:
    {
      const BrokerMessage *bmsg = get_broker_msg(msg);
      switch (bmsg->vector)
      {
        case VECTOR_BROKER_CONNECT:
          ProcessConnectRequest(conn, get_client_connect_msg(bmsg));
          break;
        case VECTOR_BROKER_FETCH_CLIENT_LIST:
          SendClientList(conn);
          log_->Log(LWPA_LOG_DEBUG, "Received Fetch Client List from Client %d; sending Client List.", conn);
          break;
        default:
          log_->Log(LWPA_LOG_ERR, "Received Broker PDU with unknown or unhandled vector %d", bmsg->vector);
          break;
      }
      break;
    }

    case VECTOR_ROOT_RPT:
      ProcessRPTMessage(conn, msg);
      break;

    default:
      log_->Log(LWPA_LOG_ERR, "Received Root Layer PDU with unknown or unhandled vector %d", msg->vector);
      break;
  }
}

void Broker::SendClientList(int conn)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CONNECTED_CLIENT_LIST;

  BrokerReadGuard clients_read(client_lock_);
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
          ClientEntryDataRpt *rpt_cli_data = get_rpt_client_entry_data(&cli_data);
          RPTClient *rptcli = static_cast<RPTClient *>(client.second.get());
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

void Broker::SendClientsAdded(client_protocol_t client_prot, int conn_to_ignore, std::vector<ClientEntryData> &entries)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CLIENT_ADD;
  get_client_list(&bmsg)->client_entry_list = entries.data();

  BrokerReadGuard controllers_read(client_lock_);
  for (const auto controller : controllers_)
  {
    if (controller.second->client_protocol == client_prot && controller.first != conn_to_ignore)
      controller.second->Push(settings_.cid, bmsg);
  }
}

void Broker::SendClientsRemoved(client_protocol_t client_prot, std::vector<ClientEntryData> &entries)
{
  BrokerMessage bmsg;
  bmsg.vector = VECTOR_BROKER_CLIENT_REMOVE;
  get_client_list(&bmsg)->client_entry_list = entries.data();

  BrokerReadGuard controllers_read(client_lock_);
  for (const auto controller : controllers_)
  {
    if (controller.second->client_protocol == client_prot)
      controller.second->Push(settings_.cid, bmsg);
  }
}

void Broker::SendStatus(RPTController *controller, const RptHeader &header, uint16_t status_code,
                        const std::string &status_str)
{
  RptHeader new_header;
  new_header.dest_endpoint_id = header.source_endpoint_id;
  new_header.dest_uid = header.source_uid;
  new_header.seqnum = header.seqnum;
  new_header.source_endpoint_id = header.dest_endpoint_id;
  new_header.source_uid = header.dest_uid;

  RptStatusMsg status;
  status.status_code = status_code;
#if RDMNET_DYNAMIC_MEM
  if (!status_str.empty())
    status.status_string = status_str.c_str();
  else
    status.status_string = nullptr;
#else
  if (!status_str.empty())
  {
    strncpy(status.status_string, status_str.c_str(), RPT_STATUS_STRING_MAXLEN - 1);
    status.status_string[RPT_STATUS_STRING_MAXLEN - 1] = '\0';
  }
  else
    status.status_string[0] = '\0';
#endif

  if (controller->Push(settings_.cid, new_header, status))
  {
    if (log_->CanLog(LWPA_LOG_INFO))
    {
      char cid_str[CID_STRING_BYTES];
      cid_to_string(cid_str, &controller->cid);
      log_->Log(LWPA_LOG_WARNING, "Sending RPT Status code %d to Controller %s", status_code, cid_str);
    }
  }
  else
  {
    // TODO disconnect
  }
}

void Broker::ProcessConnectRequest(int conn, const ClientConnectMsg *cmsg)
{
  bool deny_connection = true;
  rdmnet_connect_status_t connect_status = kRDMnetConnectScopeMismatch;

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
        connect_status = kRDMnetConnectInvalidClientEntry;
        break;
    }
  }

  if (deny_connection)
  {
    BrokerReadGuard client_read(client_lock_);

    auto it = clients_.find(conn);
    if (it != clients_.end() && it->second)
    {
      ConnectReplyMsg creply = {connect_status, E133_VERSION, settings_.uid};
      send_connect_reply(conn, &settings_.cid, &creply);
    }

    // Clean up this socket. TODO
    // MarkSocketForDestruction(cookie, false, 0);
  }
}

bool Broker::ProcessRPTConnectRequest(int conn, const ClientEntryData &data, rdmnet_connect_status_t &connect_status)
{
  bool continue_adding = true;
  const ClientEntryDataRpt *rptdata = get_rpt_client_entry_data(&data);

  if (LWPA_OK != rdmnet_set_blocking(conn, false))
  {
    log_->Log(LWPA_LOG_INFO, "Error translating socket into non-blocking socket for Client %d", conn);
    return false;
  }

  BrokerWriteGuard clients_write(client_lock_);
  RPTClient *new_client = nullptr;

  if ((settings_.max_connections > 0) && (clients_.size() >= settings_.max_connections))
  {
    connect_status = kRDMnetConnectCapacityExceeded;
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
        connect_status = kRDMnetConnectCapacityExceeded;
        continue_adding = false;
      }
      else
      {
        auto controller = std::make_shared<RPTController>(settings_.max_controller_messages, data, *clients_[conn]);
        if (controller)
        {
          new_client = controller.get();
          controllers_.insert(std::make_pair(conn, controller));
          clients_[conn] = std::move(controller);
        }
      }
    }
    // If it's a device, add it to the device states -- unless we've hit
    // our maximum number of devices
    else if (rptdata->client_type == kRPTClientTypeDevice)
    {
      if ((settings_.max_devices > 0) && (devices_.size() >= settings_.max_devices))
      {
        connect_status = kRDMnetConnectCapacityExceeded;
        continue_adding = false;
      }
      else
      {
        auto device = std::make_shared<RPTDevice>(settings_.max_device_messages, data, *clients_[conn]);
        if (device)
        {
          new_client = device.get();
          devices_.insert(std::make_pair(conn, device));
          clients_[conn] = std::move(device);
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

    // Update the UID lookup table
    uid_lookup_[rptdata->client_uid] = conn;

    // Send the connect reply
    BrokerMessage msg;
    msg.vector = VECTOR_BROKER_CONNECT_REPLY;
    ConnectReplyMsg *creply = get_connect_reply_msg(&msg);
    creply->connect_status = kRDMnetConnectOk;
    creply->e133_version = E133_VERSION;
    creply->broker_uid = settings_.uid;
    new_client->Push(settings_.cid, msg);

    if (log_->CanLog(LWPA_LOG_INFO))
    {
      log_->Log(LWPA_LOG_INFO, "Successfully processed RPT Connect request from %s (connection %d), UID %04x:%08x",
                new_client->client_type == kRPTClientTypeController ? "Controller" : "Device", conn,
                new_client->uid.manu, new_client->uid.id);
    }

    // Update everyone
    std::vector<ClientEntryData> entries;
    entries.push_back(data);
    entries[0].next = nullptr;
    SendClientsAdded(kClientProtocolRPT, conn, entries);
  }
  return continue_adding;
}

void Broker::ProcessRPTMessage(int conn, const RdmnetMessage *msg)
{
  BrokerReadGuard clients_read(client_lock_);

  const RptMessage *rptmsg = get_rpt_msg(msg);
  bool route_msg = false;
  auto client = clients_.find(conn);

  if ((client != clients_.end()) && client->second)
  {
    ClientWriteGuard client_write(*client->second);

    if (client->second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
    {
      RPTClient *rptcli = static_cast<RPTClient *>(client->second.get());

      switch (rptmsg->vector)
      {
        case VECTOR_RPT_REQUEST:
          if (rptcli->client_type == kRPTClientTypeController)
          {
            RPTController *controller = static_cast<RPTController *>(rptcli);
            if (!IsValidControllerDestinationUID(rptmsg->header.dest_uid))
            {
              SendStatus(controller, rptmsg->header, VECTOR_RPT_STATUS_UNKNOWN_RPT_UID);
              log_->Log(LWPA_LOG_DEBUG,
                        "Received Request PDU addressed to invalid or not found UID %04x:%08x from Controller %d",
                        rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
            else if (get_rdm_cmd_list(rptmsg)->list->next)
            {
              // There should only ever be one RDM command in an RPT request.
              SendStatus(controller, rptmsg->header, VECTOR_RPT_STATUS_INVALID_MESSAGE);
              log_->Log(LWPA_LOG_DEBUG,
                        "Received Request PDU from Controller %d which incorrectly contains multiple RDM Command PDUs",
                        conn);
            }
            else
              route_msg = true;
          }
          else
          {
            log_->Log(LWPA_LOG_DEBUG, "Received Request PDU from Client %d, which is not an RPT Controller", conn);
          }
          break;

        case VECTOR_RPT_STATUS:
          if (rptcli->client_type == kRPTClientTypeDevice)
          {
            if (IsValidDeviceDestinationUID(rptmsg->header.dest_uid))
            {
              if (get_status_msg(rptmsg)->status_code != VECTOR_RPT_STATUS_BROADCAST_COMPLETE)
                route_msg = true;
              else
                log_->Log(LWPA_LOG_DEBUG, "Device %d sent broadcast complete message.", conn);
            }
            else
            {
              log_->Log(LWPA_LOG_DEBUG,
                        "Received Status PDU addressed to invalid or not found UID %04x:%08x from Device %d",
                        rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
          }
          else
          {
            log_->Log(LWPA_LOG_DEBUG, "Received Status PDU from Client %d, which is not an RPT Device", conn);
          }
          break;

        case VECTOR_RPT_NOTIFICATION:
          if (rptcli->client_type == kRPTClientTypeDevice)
          {
            if (IsValidDeviceDestinationUID(rptmsg->header.dest_uid))
              route_msg = true;
            else
            {
              log_->Log(LWPA_LOG_DEBUG,
                        "Received Notification PDU addressed to invalid or not found UID %04x:%08x from Device %d",
                        rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
            }
          }
          else
          {
            log_->Log(LWPA_LOG_DEBUG, "Received Notification PDU from Client %d, which is not an RPT Device",
                      rptmsg->header.dest_uid.manu, rptmsg->header.dest_uid.id, conn);
          }
          break;

        default:
          log_->Log(LWPA_LOG_WARNING, "Received RPT PDU with unknown vector %d from Client %d", rptmsg->vector, conn);
          break;
      }
    }
  }

  if (route_msg)
  {
    uint16_t device_manu;
    int dest_conn;

    if (IsControllerBroadcastUID(rptmsg->header.dest_uid))
    {
      log_->Log(LWPA_LOG_DEBUG, "Broadcasting RPT message from Device %04x:%08x to all Controllers",
                rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id);
      for (auto controller : controllers_)
      {
        ClientWriteGuard client_write(*controller.second);
        if (!controller.second->Push(conn, msg->sender_cid, *rptmsg))
        {
          // TODO disconnect
          log_->Log(LWPA_LOG_ERR, "Error pushing to send queue for RPT Controller %d. DEBUG:NOT disconnecting...",
                    controller.first);
        }
      }
    }
    else if (IsDeviceBroadcastUID(rptmsg->header.dest_uid))
    {
      log_->Log(LWPA_LOG_DEBUG, "Broadcasting RPT message from Controller %04x:%08x to all Devices",
                rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id);
      for (auto device : devices_)
      {
        ClientWriteGuard client_write(*device.second);
        if (!device.second->Push(conn, msg->sender_cid, *rptmsg))
        {
          // TODO disconnect
          log_->Log(LWPA_LOG_ERR, "Error pushing to send queue for RPT Device %d. DEBUG:NOT disconnecting...",
                    device.first);
        }
      }
    }
    else if (IsDeviceManuBroadcastUID(rptmsg->header.dest_uid, device_manu))
    {
      log_->Log(LWPA_LOG_DEBUG,
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
            log_->Log(LWPA_LOG_ERR, "Error pushing to send queue for RPT Device %d. DEBUG:NOT disconnecting...",
                      device.first);
          }
        }
      }
    }
    else
    {
      bool found_dest_client = false;
      if (UIDToHandle(rptmsg->header.dest_uid, dest_conn))
      {
        auto dest_client = clients_.find(dest_conn);
        if (dest_client != clients_.end())
        {
          ClientWriteGuard client_write(*dest_client->second);
          if (static_cast<RPTClient *>(dest_client->second.get())->Push(conn, msg->sender_cid, *rptmsg))
          {
            found_dest_client = true;
            log_->Log(LWPA_LOG_DEBUG, "Routing RPT PDU from Client %04x:%08x to Client %04x:%08x",
                      rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, rptmsg->header.dest_uid.manu,
                      rptmsg->header.dest_uid.id);
          }
          else
          {
            // TODO disconnect
            log_->Log(LWPA_LOG_ERR, "Error pushing to send queue for RPT Client %d. DEBUG:NOT disconnecting...",
                      dest_client->first);
          }
        }
      }
      if (!found_dest_client)
      {
        log_->Log(LWPA_LOG_ERR,
                  "Could not route message from RPT Client %d (%04x:%08x): Destination UID %04x:%08x not found.", conn,
                  rptmsg->header.source_uid.manu, rptmsg->header.source_uid.id, rptmsg->header.dest_uid.manu,
                  rptmsg->header.dest_uid.id);
      }
    }
  }
}

void Broker::StartListening()
{
  for (const auto &listener : listeners_)
    listener->Start();
}

void Broker::StopListening()
{
  for (const auto &listener : listeners_)
    listener->Stop();
}

void Broker::AddConnToPollThread(int conn, std::shared_ptr<ConnPollThread> &thread)
{
  BrokerMutexGuard pt_guard(poll_thread_lock_);

  bool found = false;
  for (auto &poll_thread : poll_threads_)
  {
    if (poll_thread && poll_thread->AddConnection(conn))
    {
      found = true;
      thread = poll_thread;
      break;
    }
  }

  if (!found)
  {
    auto new_thread = std::make_shared<ConnPollThread>(LWPA_SOCKET_MAX_POLL_SIZE, this);
    if (new_thread)
    {
      new_thread->AddConnection(conn);
      if (new_thread->Start())
      {
        thread = new_thread;
        poll_threads_.insert(std::move(new_thread));
      }
    }
  }
}

// This function grabs a read lock on client_lock_.
// Optionally sends a RDMnet-level disconnect message.
void Broker::MarkConnForDestruction(int conn, bool send_disconnect, rdmnet_disconnect_reason_t reason)
{
  bool found = false;

  {  // Client read lock and destroy lock scope
    BrokerReadGuard clients_read(client_lock_);
    BrokerMutexGuard destroy_guard(client_destroy_lock_);

    auto client = clients_.find(conn);
    if ((client != clients_.end()) && client->second)
    {
      ClientWriteGuard client_write(*client->second);
      client->second->marked_for_destruction = true;
      found = true;
      clients_to_destroy_.insert(client->first);
    }
  }

  if (found)
  {
    rdmnet_disconnect(conn, send_disconnect, reason);
    rdmnet_destroy_connection(conn);
    log_->Log(LWPA_LOG_INFO, "Connection %d marked for destruction", conn);
  }
}

// These functions will take a write lock on client_lock_ and
// client_destroy_lock_.
void Broker::DestroyMarkedClientSockets()
{
  // Use a cache to avoid spending time in the lock while potentially waiting
  // for threads to stop. Using two vectors instead of a vector of pairs, so
  // the conn_cache can be passed to RemoveConnections.
  std::vector<int> conn_cache;
  std::vector<std::shared_ptr<ConnPollThread>> thread_cache;

  {  // read lock scope
    BrokerReadGuard clients_read(client_lock_);
    BrokerMutexGuard destroy_guard(client_destroy_lock_);
    if (!clients_to_destroy_.empty())
    {
      conn_cache.reserve(clients_to_destroy_.size());
      thread_cache.reserve(clients_to_destroy_.size());
      for (auto to_destroy : clients_to_destroy_)
      {
        auto client = clients_.find(to_destroy);
        if (client != clients_.end())
        {
          conn_cache.push_back(to_destroy);
          thread_cache.push_back(client->second->poll_thread);
        }
      }
    }
  }

  if (conn_cache.empty())
    return;

  {
    BrokerMutexGuard thread_guard(poll_thread_lock_);
    auto thread_it = thread_cache.begin();
    for (size_t i = 0; i < conn_cache.size(); ++i)
    {
      if (*thread_it)
      {
        if (0 != (*thread_it)->RemoveConnection(conn_cache[i]))
          thread_it = thread_cache.erase(thread_it);
        else
        {
          poll_threads_.erase(*thread_it);
          ++thread_it;
        }
      }
    }
  }

  for (auto thread_to_stop : thread_cache)
    thread_to_stop->Stop();

  RemoveConnections(conn_cache);

  BrokerMutexGuard destroy_guard(client_destroy_lock_);
  for (auto to_destroy : conn_cache)
    clients_to_destroy_.erase(to_destroy);
}

void Broker::RemoveConnections(const std::vector<int> &connections)
{
  std::vector<ClientEntryData> entries;

  {
    BrokerWriteGuard clients_write(client_lock_);
    for (auto conn : connections)
    {
      auto client = clients_.find(conn);
      if (client != clients_.end())
      {
        ClientEntryData entry;
        entry.client_protocol = client->second->client_protocol;
        entry.client_cid = client->second->cid;

        if (client->second->client_protocol == E133_CLIENT_PROTOCOL_RPT)
        {
          RPTClient *rptcli = static_cast<RPTClient *>(client->second.get());
          uid_lookup_.erase(rptcli->uid);
          if (rptcli->client_type == kRPTClientTypeController)
            controllers_.erase(conn);
          else if (rptcli->client_type == kRPTClientTypeDevice)
            devices_.erase(conn);

          ClientEntryDataRpt *rptdata = get_rpt_client_entry_data(&entry);
          rptdata->client_uid = rptcli->uid;
          rptdata->client_type = rptcli->client_type;
          rptdata->binding_cid = rptcli->binding_cid;
        }
        entries.push_back(entry);
        entries[entries.size() - 1].next = nullptr;
        if (entries.size() > 1)
          entries[entries.size() - 2].next = &entries[entries.size() - 1];
        clients_.erase(client);

        log_->Log(LWPA_LOG_INFO, "Removing connection %d marked for destruction.", conn);
        log_->Log(LWPA_LOG_DEBUG, "Clients: %zu Controllers: %zu Devices: %zu Poll Threads: %zu", clients_.size(),
                  controllers_.size(), devices_.size(), poll_threads_.size());
      }
    }
  }

  if (!entries.empty())
    SendClientsRemoved(entries[0].client_protocol, entries);
}

// Called when the broker has successfully registered. If there is some
// naming conflict, you may later get the BrokerFound notification. If some
// other service is already at that name, the mdns library may change it,
// rather than returning a BrokerRegisterError. Therefore, the new service
// name is passed back.
void Broker::BrokerRegistered(const BrokerDiscInfo *info_given, const char *assigned_service_name, void *context)
{
  Broker *broker = reinterpret_cast<Broker *>(context);
  if (context && broker->GetLog())
    broker->GetLog()->Log(LWPA_LOG_INFO, "Broker \"%s\" (now named \"%s\") successfully registered at scope \"%s\"",
                          info_given->service_name, assigned_service_name, info_given->scope);
}

// Called if the BrokerRegistration failed.  The platform-specific error
// is
// given for logging/debugging.
void Broker::BrokerRegisterError(const BrokerDiscInfo *info_given, int platform_specific_error, void *context)
{
  Broker *broker = reinterpret_cast<Broker *>(context);
  if (context && broker->GetLog())
    broker->GetLog()->Log(LWPA_LOG_INFO, "Broker \"%s\" register error %d at scope \"%s\"", info_given->service_name,
                          platform_specific_error, info_given->scope);
}

// Called whenever a broker was found. As this could be called multiple
// times for the same service (e.g. the ip address changes or more are
// discovered), you should always check the service_name field.  If you
// find more one broker for the scope, the user should be notified of an
// error!
void Broker::BrokerFound(const char *scope, const BrokerDiscInfo *broker_found, void *context)
{
  std::string addrs;
  for (unsigned int i = 0; i < broker_found->listen_addrs_count; i++)
  {
    // large enough to fit a uin32_t plus a null terminator
    char num_as_char_arr[11];
    if (lwpaip_is_v4(&broker_found->listen_addrs[i].ip))
    {
      sprintf(num_as_char_arr, "%d", lwpaip_v4_address(&broker_found->listen_addrs[i].ip));
      addrs.append(num_as_char_arr);
    }
    else if (lwpaip_is_v6(&broker_found->listen_addrs[i].ip))
    {
      for (int a = 0; a < 16; a++)
      {
        sprintf(num_as_char_arr, "%02X", lwpaip_v6_address(&broker_found->listen_addrs[i].ip)[a]);
        addrs.append(num_as_char_arr);
        if (a % 2 == 1 && a != 15)
          addrs.append(":");
      }
    }
    else  // LWPA_IP_INVALID
    {
      addrs.append("LWPA_IP_INVALID");
    }

    if (i + 1 < broker_found->listen_addrs_count)
      addrs.append(", ");
  }

  Broker *broker = reinterpret_cast<Broker *>(context);
  if (context && broker->GetLog())
    broker->GetLog()->Log(LWPA_LOG_INFO, "Broker \"%s\", ip[%s] found at same scope(\"%s\") as this broker.",
                          broker_found->service_name, addrs.c_str(), scope);
}

// Called whenever a broker went away.  The name corresponds to the
// service_name field given in BrokerFound.
void Broker::BrokerRemoved(const char *broker_service_name, void *context)
{
  Broker *broker = reinterpret_cast<Broker *>(context);
  if (context && broker->GetLog())
    broker->GetLog()->Log(LWPA_LOG_INFO, "Broker %s left", broker_service_name);
}

// Called when the query had a platform-specific error. Monitoring will
// attempt to continue.
void Broker::ScopeMonitorError(const ScopeMonitorInfo *info, int platform_specific_error, void *context)
{
  Broker *broker = reinterpret_cast<Broker *>(context);
  if (context && broker->GetLog())
    broker->GetLog()->Log(LWPA_LOG_INFO, "ScopeMonitorError %d for scope %s", platform_specific_error, info->scope);
}

BrokerLog *Broker::GetLog()
{
  return log_;
}

void Broker::SetCallbackFunctions(RdmnetDiscCallbacks *callbacks)
{
  callbacks->broker_found = &BrokerFound;
  callbacks->broker_lost = &BrokerRemoved;
  callbacks->scope_monitor_error = &ScopeMonitorError;
  callbacks->broker_registered = &BrokerRegistered;
  callbacks->broker_register_error = &BrokerRegisterError;
}
