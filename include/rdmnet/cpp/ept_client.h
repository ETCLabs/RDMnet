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

/// @file rdmnet/cpp/ept_client.h
/// @brief C++ wrapper for the RDMnet EPT Client API

#ifndef RDMNET_CPP_EPT_CLIENT_H_
#define RDMNET_CPP_EPT_CLIENT_H_

#include "etcpal/common.h"
#include "rdmnet/cpp/client.h"
#include "rdmnet/cpp/common.h"
#include "rdmnet/cpp/message.h"
#include "rdmnet/ept_client.h"

namespace rdmnet
{
/// @defgroup rdmnet_ept_client_cpp EPT Client API
/// @ingroup rdmnet_cpp_api
/// @brief Implementation of RDMnet EPT Client functionality; see @ref using_ept_client.
///
/// EPT clients use the Extensible Packet Transport protocol to exchange opaque,
/// manufacturer-specific non-RDM data across the network topology defined by RDMnet. EPT clients
/// participate in RDMnet scopes and exchange messages through an RDMnet broker, similarly to
/// RDMnet controllers and devices.
///
/// See @ref using_ept_client for a detailed description of how to use this API.

/// @ingroup rdmnet_ept_client_cpp
/// @brief An instance of RDMnet EPT client functionality.
///
/// See @ref using_ept_client for details of how to use this API.
class EptClient
{
public:
  /// A handle type used by the RDMnet library to identify EPT client instances.
  using Handle = rdmnet_ept_client_t;
  /// An invalid Handle value.
  static constexpr Handle kInvalidHandle = RDMNET_EPT_CLIENT_INVALID;

  /// @ingroup rdmnet_ept_client_cpp
  /// @brief A base class for a class that receives notification callbacks from an EPT client.
  ///
  /// See @ref using_ept_client for details of how to use this API.
  class NotifyHandler
  {
  public:
    virtual ~NotifyHandler() = default;

    /// @brief An EPT client has successfully connected to a broker.
    /// @param client_handle Handle to EPT client instance which has connected.
    /// @param scope_handle Handle to the scope on which the EPT client has connected.
    /// @param info More information about the successful connection.
    virtual void HandleConnectedToBroker(Handle                     client_handle,
                                         ScopeHandle                scope_handle,
                                         const ClientConnectedInfo& info) = 0;

    /// @brief A connection attempt failed between an EPT client and a broker.
    /// @param client_handle Handle to EPT client instance which has failed to connect.
    /// @param scope_handle Handle to the scope on which the connection failed.
    /// @param info More information about the failed connection.
    virtual void HandleBrokerConnectFailed(Handle                         client_handle,
                                           ScopeHandle                    scope_handle,
                                           const ClientConnectFailedInfo& info) = 0;

    /// @brief An EPT client which was previously connected to a broker has disconnected.
    /// @param client_handle Handle to EPT client instance which has disconnected.
    /// @param scope_handle Handle to the scope on which the disconnect occurred.
    /// @param info More information about the disconnect event.
    virtual void HandleDisconnectedFromBroker(Handle                        client_handle,
                                              ScopeHandle                   scope_handle,
                                              const ClientDisconnectedInfo& info) = 0;

    /// @brief A client list update has been received from a broker.
    /// @param client_handle Handle to EPT client instance which has received the client list update.
    /// @param scope_handle Handle to the scope on which the client list update was received.
    /// @param list_action The way the updates in client_list should be applied to the EPT client's
    ///                    cached list.
    /// @param list The list of updates.
    virtual void HandleClientListUpdate(Handle               client_handle,
                                        ScopeHandle          scope_handle,
                                        client_list_action_t list_action,
                                        const EptClientList& list) = 0;

    /// @brief EPT data has been received addressed to an EPT client.
    /// @param client_handle Handle to EPT client instance which has received the data.
    /// @param scope_handle Handle to the scope on which the EPT data was received.
    /// @param data The EPT data.
    /// @return The action to take in response to this EPT data message.
    virtual EptResponseAction HandleEptData(Handle client_handle, ScopeHandle scope_handle, const EptData& data) = 0;

    /// @brief An EPT status message has been received in response to a previously-sent EPT data message.
    /// @param client_handle Handle to EPT client instance which has received the data.
    /// @param scope_handle Handle to the scope on which the EPT status message was received.
    /// @param status The EPT status message.
    virtual void HandleEptStatus(Handle client_handle, ScopeHandle scope_handle, const EptStatus& status) = 0;
  };

  /// @ingroup rdmnet_ept_client_cpp
  /// @brief A set of configuration settings that an EPT client needs to initialize.
  struct Settings
  {
    etcpal::Uuid                cid;            ///< The EPT client's CID.
    std::vector<EptSubProtocol> protocols;      ///< The list of EPT sub-protocols that this EPT client supports.
    std::string                 search_domain;  ///< (optional) The EPT client's search domain for discovering brokers.
    /// (optional) A data buffer to be used to respond synchronously to EPT data noficiations.
    const uint8_t* response_buf{nullptr};

    /// Create an empty, invalid data structure by default.
    Settings() = default;
    Settings(const etcpal::Uuid& new_cid, const std::vector<EptSubProtocol>& new_protocols);

    bool IsValid() const;
  };

  EptClient() = default;
  EptClient(const EptClient& other) = delete;
  EptClient& operator=(const EptClient& other) = delete;
  EptClient(EptClient&& other) = default;             ///< Move an EPT client instance.
  EptClient& operator=(EptClient&& other) = default;  ///< Move an EPT client instance.

  etcpal::Error Startup(NotifyHandler& notify_handler, const Settings& settings);
  void          Shutdown(rdmnet_disconnect_reason_t disconnect_reason = kRdmnetDisconnectShutdown);

  etcpal::Expected<ScopeHandle> AddScope(const char*             id,
                                         const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Expected<ScopeHandle> AddScope(const Scope& scope_config);
  etcpal::Expected<ScopeHandle> AddDefaultScope(const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  etcpal::Error                 RemoveScope(ScopeHandle scope_handle, rdmnet_disconnect_reason_t disconnect_reason);

  etcpal::Error RequestClientList(ScopeHandle scope_handle);

  etcpal::Error SendData(ScopeHandle         scope_handle,
                         const etcpal::Uuid& dest_cid,
                         uint16_t            manufacturer_id,
                         uint16_t            protocol_id,
                         const uint8_t*      data,
                         size_t              data_len);
  etcpal::Error SendStatus(ScopeHandle         scope_handle,
                           const etcpal::Uuid& dest_cid,
                           ept_status_code_t   status_code,
                           const char*         status_string = nullptr);

  Handle                  handle() const;
  NotifyHandler*          notify_handler() const;
  etcpal::Expected<Scope> scope(ScopeHandle scope_handle) const;

private:
  Handle         handle_{kInvalidHandle};
  NotifyHandler* notify_{nullptr};
};

/// Create an EPT client Settings instance by passing the required members explicitly.
inline EptClient::Settings::Settings(const etcpal::Uuid& new_cid, const std::vector<EptSubProtocol>& new_protocols)
    : cid(new_cid), protocols(new_protocols)
{
}

/// Determine whether an EPT client Settings instance contains valid data for RDMnet operation.
inline bool EptClient::Settings::IsValid() const
{
  return (!cid.IsNull() && !protocols.empty());
}

/// @brief Allocate resources and start up this EPT client with the given configuration.
/// @param notify_handler A class instance to handle callback notifications from this EPT client.
/// @param settings Configuration settings used by this EPT client.
/// @return etcpal::Error::Ok(): EPT client started successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return Errors forwarded from rdmnet_ept_client_create().
inline etcpal::Error EptClient::Startup(NotifyHandler& notify_handler, const Settings& settings)
{
  ETCPAL_UNUSED_ARG(notify_handler);
  ETCPAL_UNUSED_ARG(settings);
  return kEtcPalErrNotImpl;
}

/// @brief Shut down this EPT client and deallocate resources.
///
/// Will disconnect all scopes to which this EPT client is currently connected, sending the
/// disconnect reason provided in the disconnect_reason parameter.
///
/// @param disconnect_reason Reason code for disconnecting from each scope.
inline void EptClient::Shutdown(rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(disconnect_reason);
}

/// @brief Add a new scope to this EPT client instance.
///
/// The library will attempt to discover and connect to a broker for the scope (or just connect if
/// a static broker address is given); the status of these attempts will be communicated via the
/// associated NotifyHandler.
///
/// @param id The scope ID string.
/// @param static_broker_addr [optional] A static IP address and port at which to connect to the
///                           broker for this scope.
/// @return On success, a handle to the new scope, to be used with subsequent API calls.
/// @return On failure, error codes from rdmnet_ept_client_add_scope().
inline etcpal::Expected<ScopeHandle> EptClient::AddScope(const char* id, const etcpal::SockAddr& static_broker_addr)
{
  ETCPAL_UNUSED_ARG(id);
  ETCPAL_UNUSED_ARG(static_broker_addr);
  return kEtcPalErrNotImpl;
}

/// @brief Add a new scope to this EPT client instance.
///
/// The library will attempt to discover and connect to a broker for the scope (or just connect if
/// a static broker address is given); the status of these attempts will be communicated via the
/// associated NotifyHandler.
///
/// @param scope_config Configuration information for the new scope.
/// @return On success, a handle to the new scope, to be used with subsequent API calls.
/// @return On failure, error codes from rdmnet_ept_client_add_scope().
inline etcpal::Expected<ScopeHandle> EptClient::AddScope(const Scope& scope_config)
{
  ETCPAL_UNUSED_ARG(scope_config);
  return kEtcPalErrNotImpl;
}

/// @brief Shortcut to add the default RDMnet scope to an EPT client instance.
///
/// The library will attempt to discover and connect to a broker for the default scope (or just
/// connect if a static broker address is given); the status of these attempts will be communicated
/// via the associated NotifyHandler.
///
/// @param static_broker_addr [optional] A static broker address to configure for the default scope.
/// @return On success, a handle to the new scope, to be used with subsequent API calls.
/// @return On failure, error codes from rdmnet_ept_client_add_scope().
inline etcpal::Expected<ScopeHandle> EptClient::AddDefaultScope(const etcpal::SockAddr& static_broker_addr)
{
  ETCPAL_UNUSED_ARG(static_broker_addr);
  return kEtcPalErrNotImpl;
}

/// @brief Remove a previously-added scope from this EPT client instance.
///
/// After this call completes, scope_handle will no longer be valid.
///
/// @param scope_handle Handle to the scope to remove.
/// @param disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
/// @return etcpal::Error::Ok(): Scope removed successfully.
/// @return Error codes from from rdmnet_ept_client_remove_scope().
inline etcpal::Error EptClient::RemoveScope(ScopeHandle scope_handle, rdmnet_disconnect_reason_t disconnect_reason)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(disconnect_reason);
  return kEtcPalErrNotImpl;
}

/// @brief Request a client list from a broker.
///
/// The response will be delivered via the NotifyHandler::HandleClientListUpdate() callback.
///
/// @param scope_handle Handle to the scope on which to request the client list.
/// @return etcpal::Error::Ok(): Request sent successfully.
/// @return Error codes from rdmnet_ept_client_request_client_list().
inline etcpal::Error EptClient::RequestClientList(ScopeHandle scope_handle)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  return kEtcPalErrNotImpl;
}

/// @brief Send data from an EPT client on a scope.
/// @param scope_handle Handle to the scope on which to send data.
/// @param dest_cid CID of the EPT client to which to send the data.
/// @param manufacturer_id Manufacturer ID portion of the EPT sub-protocol identifier.
/// @param protocol_id Protocol ID portion of the EPT sub-protocol identifier.
/// @param data The data to send.
/// @param data_len Size in bytes of data.
/// @return etcpal::Error::Ok(): Data sent successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return #kEtcPalErrNotInit: Module not initialized.
/// @return #kEtcPalErrNotFound: Client not started, or scope_handle is not associated with a valid
///         scope instance.
/// @return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error EptClient::SendData(ScopeHandle         scope_handle,
                                         const etcpal::Uuid& dest_cid,
                                         uint16_t            manufacturer_id,
                                         uint16_t            protocol_id,
                                         const uint8_t*      data,
                                         size_t              data_len)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(dest_cid);
  ETCPAL_UNUSED_ARG(manufacturer_id);
  ETCPAL_UNUSED_ARG(protocol_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/// @brief Send a status message from an EPT client on a scope.
///
/// @param scope_handle Handle to the scope on which to send the status message.
/// @param dest_cid CID of the EPT client to which to send the status message.
/// @param status_code EPT status code to send.
/// @param status_string Optional status string accompanying the code.
/// @return etcpal::Error::Ok(): Status sent successfully.
/// @return #kEtcPalErrInvalid: Invalid argument.
/// @return #kEtcPalErrNotInit: Module not initialized.
/// @return #kEtcPalErrNotFound: Client not started, or scope_handle is not associated with a
///         valid scope instance.
/// @return #kEtcPalErrSys: An internal library or system call error occurred.
inline etcpal::Error EptClient::SendStatus(ScopeHandle         scope_handle,
                                           const etcpal::Uuid& dest_cid,
                                           ept_status_code_t   status_code,
                                           const char*         status_string)
{
  ETCPAL_UNUSED_ARG(scope_handle);
  ETCPAL_UNUSED_ARG(dest_cid);
  ETCPAL_UNUSED_ARG(status_code);
  ETCPAL_UNUSED_ARG(status_string);
  return kEtcPalErrNotImpl;
}

/// @brief Retrieve the handle of an EPT client instance.
inline EptClient::Handle EptClient::handle() const
{
  return kInvalidHandle;
}

/// @brief Retrieve the NotifyHandler reference that this EPT client was configured with.
inline EptClient::NotifyHandler* EptClient::notify_handler() const
{
  return nullptr;
}

/// @brief Retrieve the scope configuration associated with a given scope handle.
/// @return The scope configuration on success.
/// @return #kEtcPalErrNotInit: Module not initialized.
/// @return #kEtcPalErrNotFound: EPT client not started, or scope handle not found.
inline etcpal::Expected<Scope> EptClient::scope(ScopeHandle scope_handle) const
{
  ETCPAL_UNUSED_ARG(scope_handle);
  return kEtcPalErrNotImpl;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_EPT_CLIENT_H_
