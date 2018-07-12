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

/*! \file broker/broker.h
 *  \brief A platform-neutral RDMnet Broker implementation.
 *  \author Nick Ballhorn-Wagner and Sam Kearney */
#ifndef _BROKER_BROKER_H_
#define _BROKER_BROKER_H_

#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <memory>
#include <cstddef>

#include "lwpa_int.h"
#include "lwpa_log.h"
#include "lwpa_cid.h"
#include "estardm.h"
#include "estardmnet.h"
#include "rdmnet/rdmtypes.h"
#include "rdmnet/client.h"
#include "rdmnet/cpputil.h"
#include "rdmnet/discovery.h"
#include "rdmnet/connection.h"
#include "broker/client.h"
#include "broker/threads.h"
#include "broker/responder.h"
#include "broker/discovery.h"

/*! \defgroup rdmnet_broker Broker
 *  \brief A platform-neutral RDMnet %Broker implementation.
 *  @{
 */

// These are the settings the broker needs to run.
struct BrokerSettings
{
  // Identification
  LwpaCid cid;
  LwpaUid uid;

  BrokerDiscoveryAttributes disc_attributes;

  // The maximum number of client connections supported.  0 means infinite.
  unsigned int max_connections;

  // The maximum number of controllers allowed.  0 means infinite
  unsigned int max_controllers;

  // The maximum number of queued messages per controller.  0 means infinite
  unsigned int max_controller_messages;

  // The maximum number of devices allowed.  0 means infinite
  unsigned int max_devices;

  unsigned int max_device_messages;

  // If you reach the number of max connections, This number of tcp-level
  // connections are still supported to reject the connection request.
  unsigned int max_reject_connections;

  /*THESE ARE FOR DEBUGGING PURPOSES ONLY.  When not debugging, use
    the defaults provided by the constructor. */

  // Each read thread can support many sockets, up to the maximum allowed by
  // your socket
  unsigned int max_socket_per_read_thread;

  BrokerSettings()
      : max_connections(0)
      , max_controllers(0)
      , max_controller_messages(500)
      , max_devices(0)
      , max_device_messages(500)
      , max_reject_connections(1000)
      , max_socket_per_read_thread(LWPA_SOCKET_MAX_POLL_SIZE)
  {
  }
};

// The callback interface for Broker notifications
class IBrokerNotify
{
public:
  virtual void ScopeChanged(const std::string &new_scope) = 0;
};

/*! \brief Defines an instance of RDMnet %Broker functionality. */
class Broker : public IListenThread_Notify, public IConnPollThread_Notify, public IClientServiceThread_Notify
{
public:
  Broker(BrokerLog *log, IBrokerNotify *notify);
  virtual ~Broker();

  bool Startup(const BrokerSettings &settings, uint16_t listen_port, std::vector<LwpaIpAddr> &listen_addrs);
  void Shutdown();
  void Tick();

  void GetSettings(BrokerSettings &settings) const;

  // Some utility functions
  static constexpr bool IsBroadcastUID(const LwpaUid &uid);
  static constexpr bool IsControllerBroadcastUID(const LwpaUid &uid);
  static constexpr bool IsDeviceBroadcastUID(const LwpaUid &uid);
  static bool IsDeviceManuBroadcastUID(const LwpaUid &uid, uint16_t &manu);
  bool IsValidControllerDestinationUID(const LwpaUid &uid) const;
  bool IsValidDeviceDestinationUID(const LwpaUid &uid) const;

  BrokerLog *GetLog();

protected:
  // These are never modified between startup and shutdown, so they don't
  // need to be locked.
  bool started_;
  BrokerLog *log_;
  IBrokerNotify *notify_;
  BrokerSettings settings_;
  std::vector<std::unique_ptr<ListenThread>> listeners_;

  // The Broker's RDM responder
  BrokerResponder responder_;

  // LLRP data
  // LLRPSocket m_llrpSocket;
  // LLRPSocketProxy m_llrpSocketProxy;
  // IWinAsyncSocketServ *m_serv;
  // LLRPMessageProcessor m_llrp_msg_proc;

  // IRDMnet_MDNS* m_mdns;

  // If you have a maximum number of connections, we may be stopping and
  // starting the listen threads.
  // TESTING TODO:  We don't ever stop listening yet.
  void StartListening();
  void StopListening();

  // IListenThread_Notify messages
  virtual bool NewConnection(lwpa_socket_t new_sock, const LwpaSockaddr &addr) override;
  virtual void LogError(const std::string &err) override;

  // IConnPollThread_Notify messages
  virtual bool PollConnections(const std::vector<int> &conn_handles, RdmnetPoll *poll_arr) override;

  // ILLRPSocketProxy_Notify messages
  //  virtual void Received(const uint8_t *llrp_data, size_t llrp_data_len,
  //                        uint8_t *packet_data, size_t packet_len) override;
  //  virtual void TargetReadSocketBad() override;
  //  virtual void TargetWriteSocketBad() override;
  //  virtual void ControllerReadSocketBad() override;
  //  virtual void ControllerWriteSocketBad() override;

  // IClientServiceThread_Notify messages
  virtual bool ServiceClients() override;

  // IBrokerDiscoveryManager_Notify messages
  // virtual void BrokerRegistered(const mdns_broker_info &info_given,
  //                             const std::string & assigned_service_name)
  //                             override;
  // virtual void BrokerRegisterError(const mdns_broker_info &info_given,
  //                                 int platform_specific_error) override;
  // virtual void BrokerFound(const std::string &     scope,
  //                         const mdns_broker_info &broker_found) override;
  // virtual void BrokerRemoved(const std::string &broker_service_name)
  // override; virtual void ScopeMonitorError(const mdns_monitor_info &info,
  //                               int platform_specific_error) override;

  ClientServiceThread service_thread_;

  // The poll operation has a maximum size, so we need a pool of threads to do
  // the poll operation.
  lwpa_mutex_t poll_thread_lock_;
  std::set<std::shared_ptr<ConnPollThread>> poll_threads_;

  void AddConnToPollThread(int conn, std::shared_ptr<ConnPollThread> &thread);

  // The list of connected clients, indexed by the connection handle
  std::map<int, std::shared_ptr<BrokerClient>> clients_;
  // The uid->handle lookup table
  std::map<LwpaUid, int> uid_lookup_;
  // Protects the list of clients and uid lookup, but not the data in the
  // clients themselves.
  mutable lwpa_rwlock_t client_lock_;

  bool ClientReadLock() const { return lwpa_rwlock_readlock(&client_lock_, LWPA_WAIT_FOREVER); }
  void ClientReadUnlock() const { lwpa_rwlock_readunlock(&client_lock_); }
  bool ClientWriteLock() const { return lwpa_rwlock_writelock(&client_lock_, LWPA_WAIT_FOREVER); }
  void ClientWriteUnlock() const { lwpa_rwlock_writeunlock(&client_lock_); }

  bool UIDToHandle(const LwpaUid &uid, int &conn_handle) const;

  void GetConnSnapshot(std::vector<int> &conns, bool include_devices, bool include_controllers, bool include_unknown,
                       uint16_t manufacturer_filter = 0xffff);

  // The state data for each controller, indexed by its connection handle.
  std::map<int, std::shared_ptr<RPTController>> controllers_;

  RPTController *GetControllerForWriting(int conn) const;
  void ReleaseControllerFromWriting(RPTController *pdata) const;
  const RPTController *GetControllerForReading(int conn) const;
  void ReleaseControllerFromReading(const RPTController *pdata) const;

  // The set of devices, indexed by the connection handle.
  std::map<int, std::shared_ptr<RPTDevice>> devices_;

  RPTDevice *GetDeviceForWriting(int conn) const;
  void ReleaseDeviceFromWriting(RPTDevice *pdata) const;
  const RPTDevice *GetDeviceForReading(int conn) const;
  void ReleaseDeviceFromReading(const RPTDevice *pdata) const;

  lwpa_mutex_t client_destroy_lock_;
  std::set<int> clients_to_destroy_;

  void MarkConnForDestruction(int conn, bool send_disconnect,
                              rdmnet_disconnect_reason_t reason = kRDMnetDisconnectShutdown);
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
  void SendStatus(RPTClient *rptcli, const RptHeader &header, uint16_t status_code,
                  const std::string &status_str = std::string());

  // broker callback functions
  static void BrokerRegistered(const BrokerDiscInfo *info_given, const char *assigned_service_name, void *context);
  static void BrokerRegisterError(const BrokerDiscInfo *info_given, int platform_specific_error, void *context);
  static void BrokerFound(const char *scope, const BrokerDiscInfo *broker_found, void *context);
  static void BrokerRemoved(const char *broker_service_name, void *context);
  static void ScopeMonitorError(const ScopeMonitorInfo *info, int platform_specific_error, void *context);

  void SetCallbackFunctions(RdmnetDiscCallbacks *callbacks);

  RdmnetDiscCallbacks callbacks_;
};

/*!@}*/

#endif  // _BROKER_BROKER_H_