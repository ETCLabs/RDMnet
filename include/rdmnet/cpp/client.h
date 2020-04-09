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

/// \file rdmnet/cpp/client.h

#ifndef RDMNET_CPP_CLIENT_H_
#define RDMNET_CPP_CLIENT_H_

#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/client.h"

namespace rdmnet
{
/// \brief A destination address for an RDM command in RDMnet's RPT protocol.
/// \details See \ref roles_and_addressing and \ref devices_and_gateways for more information.
class DestinationAddr
{
public:
  DestinationAddr() = default;

  static constexpr DestinationAddr ToDefaultResponder(const rdm::Uid& rdmnet_uid, uint16_t subdevice = 0);
  static constexpr DestinationAddr ToDefaultResponder(uint16_t manufacturer_id, uint32_t device_id,
                                                      uint16_t subdevice = 0);
  static constexpr DestinationAddr ToSubResponder(const rdm::Uid& rdmnet_uid, uint16_t endpoint,
                                                  const rdm::Uid& rdm_uid, uint16_t subdevice = 0);

private:
  constexpr DestinationAddr(const RdmUid& rdmnet_uid, uint16_t endpoint, const RdmUid& rdm_uid, uint16_t subdevice);

  RdmnetDestinationAddr addr_{};
};

/// \brief Get a DestinationAddr representing a message addressed to a component's default responder.
/// \param rdmnet_uid The UID of the RDMnet component to which the command is addressed.
/// \param subdevice (optional) The subdevice to which the command is addressed (0 for the root device by default).
constexpr DestinationAddr DestinationAddr::ToDefaultResponder(const rdm::Uid& rdmnet_uid, uint16_t subdevice)
{
  return DestinationAddr(rdmnet_uid.get(), E133_NULL_ENDPOINT, rdmnet_uid.get(), subdevice);
}

/// \brief Get a DestinationAddr representing a message addressed to a component's default responder.
/// \param manufacturer_id The manufacturer ID portion of the destination RDMnet component's UID.
/// \param device_id The device ID portion of the destination RDMnet component's UID.
/// \param subdevice (optional) The subdevice to which the command is addressed (0 for the root device by default).
constexpr DestinationAddr DestinationAddr::ToDefaultResponder(uint16_t manufacturer_id, uint32_t device_id,
                                                              uint16_t subdevice)
{
  return DestinationAddr({manufacturer_id, device_id}, E133_NULL_ENDPOINT, {manufacturer_id, device_id}, subdevice);
}

/// \brief Get a DestinationAddr representing a message addressed to a sub-responder on a component.
///
/// Sub-responders can be physical or virtual but are always addressed by UID; see
/// \ref devices_and_gateways for more information.
///
/// \param rdmnet_uid The UID of the RDMnet component which contains the sub-responder to which the
///                   command is addressed.
/// \param endpoint The endpoint the sub-responder is associated with.
/// \param rdm_uid The sub-responders's UID.
/// \param subdevice (optional) The subdevice to which the command is addressed (0 for the root device by default).
constexpr DestinationAddr DestinationAddr::ToSubResponder(const rdm::Uid& rdmnet_uid, uint16_t endpoint,
                                                          const rdm::Uid& rdm_uid, uint16_t subdevice)
{
  return DestinationAddr(rdmnet_uid.get(), endpoint, rdm_uid.get(), subdevice);
}

constexpr DestinationAddr::DestinationAddr(const RdmUid& rdmnet_uid, uint16_t endpoint, const RdmUid& rdm_uid,
                                           uint16_t subdevice)
    : addr_{rdmnet_uid, endpoint, rdm_uid, subdevice}
{
}

/// \ingroup rdmnet_cpp_common
/// \brief Information about a successful connection to a broker delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function. Extract the members
/// to save them for later use.
class ClientConnectedInfo
{
public:
  /// Not default-constructible.
  ClientConnectedInfo() = delete;
  /// Not copyable.
  ClientConnectedInfo(const ClientConnectedInfo& other) = delete;
  /// Not copyable.
  ClientConnectedInfo& operator=(const ClientConnectedInfo& other) = delete;

  constexpr ClientConnectedInfo(const RdmnetClientConnectedInfo& c_info) noexcept;

  constexpr etcpal::SockAddr broker_addr() const noexcept;
  constexpr std::string broker_name() const;
  constexpr const char* broker_name_c_str() const noexcept;
  constexpr etcpal::Uuid broker_cid() const noexcept;
  constexpr rdm::Uid broker_uid() const noexcept;

  constexpr const RdmnetClientConnectedInfo& get() const noexcept;

private:
  const RdmnetClientConnectedInfo& info_;
};

/// Construct a ClientConnectedInfo which references an instance of the C RdmnetClientConnectedInfo type.
constexpr ClientConnectedInfo::ClientConnectedInfo(const RdmnetClientConnectedInfo& c_info) noexcept : info_(c_info)
{
}

/// Get the IP address and port of the remote broker to which we have connected.
constexpr etcpal::SockAddr ClientConnectedInfo::broker_addr() const noexcept
{
  return info_.broker_addr;
}

/// Get the DNS name of the broker (if it was discovered via DNS-SD; otherwise this will be an empty string)
constexpr std::string ClientConnectedInfo::broker_name() const
{
  return info_.broker_name;
}

/// Get the DNS name of the broker (if it was discovered via DNS-SD; otherwise this will be an empty string)
constexpr const char* ClientConnectedInfo::broker_name_c_str() const noexcept
{
  return info_.broker_name;
}

/// Get the CID of the connected broker.
constexpr etcpal::Uuid ClientConnectedInfo::broker_cid() const noexcept
{
  return info_.broker_cid;
}

/// Get the RDM UID of the connected broker.
constexpr rdm::Uid ClientConnectedInfo::broker_uid() const noexcept
{
  return info_.broker_uid;
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetClientConnectedInfo& ClientConnectedInfo::get() const noexcept
{
  return info_;
}

/// \ingroup rdmnet_cpp_common
/// \brief Information about a failed connection to a broker delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function. Extract the members
/// to save them for later use.
class ClientConnectFailedInfo
{
public:
  /// Not default-constructible.
  ClientConnectFailedInfo() = delete;
  /// Not copyable.
  ClientConnectFailedInfo(const ClientConnectFailedInfo& other) = delete;
  /// Not copyable.
  ClientConnectFailedInfo& operator=(const ClientConnectFailedInfo& other) = delete;

  constexpr ClientConnectFailedInfo(const RdmnetClientConnectFailedInfo& c_info) noexcept;

  constexpr rdmnet_connect_fail_event_t event() const noexcept;
  constexpr etcpal::Error socket_err() const noexcept;
  constexpr rdmnet_connect_status_t rdmnet_reason() const noexcept;
  constexpr bool will_retry() const noexcept;

  constexpr bool HasSocketErr() const noexcept;
  constexpr bool HasRdmnetReason() const noexcept;

  constexpr const RdmnetClientConnectFailedInfo& get() const noexcept;

private:
  const RdmnetClientConnectFailedInfo& info_;
};

/// Construct a ClientConnectFailedInfo which references an instance of the C RdmnetClientConnectFailedInfo type.
constexpr ClientConnectFailedInfo::ClientConnectFailedInfo(const RdmnetClientConnectFailedInfo& c_info) noexcept
    : info_(c_info)
{
}

/// Get the high-level reason that this connection failed.
constexpr rdmnet_connect_fail_event_t ClientConnectFailedInfo::event() const noexcept
{
  return info_.event;
}

/// \brief Get the system error code associated with the failure.
/// \details Valid if HasSocketErr() == true.
constexpr etcpal::Error ClientConnectFailedInfo::socket_err() const noexcept
{
  return info_.socket_err;
}

/// \brief Get the reason given in the RDMnet-level connection refuse message.
/// \details Valid if HasRdmnetReason() == true.
constexpr rdmnet_connect_status_t ClientConnectFailedInfo::rdmnet_reason() const noexcept
{
  return info_.rdmnet_reason;
}

/// \brief Whether the connection will be retried automatically.
///
/// If this is true, the connection will be retried on the relevant scope; expect further
/// notifications of connection success or failure. If false, the rdmnet_client_scope_t handle
/// associated with the scope is invalidated, and the scope must be created again. This indicates
/// that the connection failed for a reason that usually must be corrected by a user or application
/// developer. Some possible reasons for this to be false include:
/// - The wrong scope was specified for a statically-configured broker
/// - A static UID was given that was invalid or duplicate with another UID in the system
constexpr bool ClientConnectFailedInfo::will_retry() const noexcept
{
  return info_.will_retry;
}

/// Whether the value returned from socket_err() is valid.
constexpr bool ClientConnectFailedInfo::HasSocketErr() const noexcept
{
  return (info_.event == kRdmnetConnectFailSocketFailure || info_.event == kRdmnetConnectFailTcpLevel);
}

/// Whether the value returned from rdmnet_reason() is valid.
constexpr bool ClientConnectFailedInfo::HasRdmnetReason() const noexcept
{
  return (info_.event == kRdmnetConnectFailRejected);
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetClientConnectFailedInfo& ClientConnectFailedInfo::get() const noexcept
{
  return info_;
}

/// \ingroup rdmnet_cpp_common
/// \brief Information about a disconnect event from a broker delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function. Extract the members
/// to save them for later use.
class ClientDisconnectedInfo
{
public:
  /// Not default-constructible.
  ClientDisconnectedInfo() = delete;
  /// Not copyable.
  ClientDisconnectedInfo(const ClientDisconnectedInfo& other) = delete;
  /// Not copyable.
  ClientDisconnectedInfo& operator=(const ClientDisconnectedInfo& other) = delete;

  constexpr ClientDisconnectedInfo(const RdmnetClientDisconnectedInfo& c_info) noexcept;

  constexpr rdmnet_disconnect_event_t event() const noexcept;
  constexpr etcpal::Error socket_err() const noexcept;
  constexpr rdmnet_disconnect_reason_t rdmnet_reason() const noexcept;
  constexpr bool will_retry() const noexcept;

  constexpr bool HasSocketErr() const noexcept;
  constexpr bool HasRdmnetReason() const noexcept;

  constexpr const RdmnetClientDisconnectedInfo& get() const noexcept;

private:
  const RdmnetClientDisconnectedInfo& info_;
};

/// Construct a ClientDisconnectedInfo which references an instance of the C RdmnetClientDisconnectedInfo type.
constexpr ClientDisconnectedInfo::ClientDisconnectedInfo(const RdmnetClientDisconnectedInfo& c_info) noexcept
    : info_(c_info)
{
}

/// Get the high-level reason for this disconnect.
constexpr rdmnet_disconnect_event_t ClientDisconnectedInfo::event() const noexcept
{
  return info_.event;
}

/// \brief Get the system error code associated with the disconnect.
/// \details Valid if HasSocketErr() == true.
constexpr etcpal::Error ClientDisconnectedInfo::socket_err() const noexcept
{
  return info_.socket_err;
}

/// \brief Get the reason given in the RDMnet-level disconnect message.
/// \details Valid if HasRdmnetReason() == true.
constexpr rdmnet_disconnect_reason_t ClientDisconnectedInfo::rdmnet_reason() const noexcept
{
  return info_.rdmnet_reason;
}

/// \brief Whether the connection will be retried automatically.
///
/// There are currently no conditions that will cause this to be false; therefore, disconnection
/// events after a successful connection will always lead to the connection being retried
/// automatically. This accessor exists for potential future usage.
constexpr bool ClientDisconnectedInfo::will_retry() const noexcept
{
  return info_.will_retry;
}

/// Whether the value returned from socket_err() is valid.
constexpr bool ClientDisconnectedInfo::HasSocketErr() const noexcept
{
  return (info_.event == kRdmnetDisconnectAbruptClose);
}

/// Whether the value returned from rdmnet_reason() is valid.
constexpr bool ClientDisconnectedInfo::HasRdmnetReason() const noexcept
{
  return (info_.event == kRdmnetDisconnectGracefulRemoteInitiated);
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetClientDisconnectedInfo& ClientDisconnectedInfo::get() const noexcept
{
  return info_;
}

/// \ingroup rdmnet_cpp_common
/// \copydoc rdmnet_client_scope_t
using ScopeHandle = rdmnet_client_scope_t;

/// \ingroup rdmnet_cpp_common
/// \brief Identifies the NULL_ENDPOINT, the endpoint of the RDMnet default responder.
constexpr uint16_t kNullEndpoint = E133_NULL_ENDPOINT;

/// \ingroup rdmnet_cpp_common
/// \brief An RDMnet scope configuration.
///
/// Includes the scope string, which can be from 1 to 62 characters of UTF-8. Also includes an
/// optional hardcoded ("static") IP address and port for a broker to connect to for this scope. If
/// this is absent, DNS-SD will be used to dynamically discover a broker.
class Scope
{
public:
  Scope() = default;
  Scope(const std::string& scope_str, const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  Scope(const RdmnetScopeConfig& scope_config);

  constexpr bool IsStatic() const noexcept;
  bool IsDefault() const noexcept;
  constexpr const std::string& id_string() const noexcept;
  constexpr const etcpal::SockAddr& static_broker_addr() const noexcept;

  void SetIdString(const std::string& id);
  void SetStaticBrokerAddr(const etcpal::SockAddr& static_broker_addr);

private:
  std::string id_{E133_DEFAULT_SCOPE};
  etcpal::SockAddr static_broker_addr_{};
};

/// Construct a scope config from its id string and an optional static broker IP address and port.
inline Scope::Scope(const std::string& scope_str, const etcpal::SockAddr& static_broker_addr)
    : id_(scope_str.substr(0, E133_SCOPE_STRING_PADDED_LENGTH - 1)), static_broker_addr_(static_broker_addr)
{
}

/// Construct a scope config from an instance of the C RdmnetScopeConfig type.
inline Scope::Scope(const RdmnetScopeConfig& scope_config) : id_(scope_config.scope)
{
  if (!ETCPAL_IP_IS_INVALID(&scope_config.static_broker_addr.ip))
    static_broker_addr_ = scope_config.static_broker_addr;
}

/// Whether this scope has been configured with a static IP address and port for a broker.
constexpr bool Scope::IsStatic() const noexcept
{
  return static_broker_addr_.IsValid();
}

/// Whether this scope represents the default RDMnet scope.
inline bool Scope::IsDefault() const noexcept
{
  return id_ == E133_DEFAULT_SCOPE;
}

/// The ID string of this scope.
constexpr const std::string& Scope::id_string() const noexcept
{
  return id_;
}

/// \brief The static broker address associated with this scope.
///
/// If no static broker address is configured, returns an invalid address (SockAddr::IsValid()
/// returns false).
constexpr const etcpal::SockAddr& Scope::static_broker_addr() const noexcept
{
  return static_broker_addr_;
}

/// \brief Set a new ID string for this scope.
/// \param id The ID string. Will be truncated to a maximum of 62 UTF-8 bytes.
inline void Scope::SetIdString(const std::string& id)
{
  id_ = id.substr(0, E133_SCOPE_STRING_PADDED_LENGTH - 1);
}

/// Set a new static broker IP address and port for this scope.
inline void Scope::SetStaticBrokerAddr(const etcpal::SockAddr& static_broker_addr)
{
  static_broker_addr_ = static_broker_addr;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_CLIENT_H_
