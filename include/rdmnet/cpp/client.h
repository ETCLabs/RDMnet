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

#include "etcpal/cpp/inet.h"
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
