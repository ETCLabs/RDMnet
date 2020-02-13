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
/// \defgroup rdmnet_cpp_api RDMnet C++ Language API
/// \brief RDMnet C++-language API

/// \ingroup rdmnet_cpp_api
/// \copydoc rdmnet_client_scope_t
using ScopeHandle = rdmnet_client_scope_t;

constexpr uint16_t kNullEndpoint = E133_NULL_ENDPOINT;

/// \ingroup rdmnet_cpp_api
/// \brief An RDMnet scope configuration.
///
/// Includes the scope string, which can be from 1 to 62 characters of UTF-8. Also includes an
/// optional hardcoded ("static") IP address and port for a broker to connect to for this scope. If
/// this is absent, DNS-SD will be used to dynamically discover a broker.
class Scope
{
public:
  constexpr Scope(const std::string& scope_str, const etcpal::SockAddr& static_broker_addr = etcpal::SockAddr{});
  constexpr Scope(const RdmnetScopeConfig& scope_config);

  constexpr bool IsStatic() const noexcept;
  bool IsDefault() const noexcept;
  constexpr const std::string& id() const noexcept;
  constexpr const etcpal::SockAddr& static_broker_addr() const noexcept;

  void SetId(const std::string& id);
  void SetStaticBrokerAddr(const etcpal::SockAddr& static_broker_addr);

private:
  std::string id_;
  etcpal::SockAddr static_broker_addr_;
};

/// Construct a scope config from its id string and an optional static broker IP address and port.
constexpr Scope::Scope(const std::string& scope_str, const etcpal::SockAddr& static_broker_addr)
    : id_(scope_str.substr(0, E133_SCOPE_STRING_PADDED_LENGTH - 1)), static_broker_addr_(static_broker_addr)
{
}

/// Construct a scope config from the RdmnetScopeConfig type which is used with the
/// \ref rdmnet_client_api.
constexpr Scope::Scope(const RdmnetScopeConfig& scope_config) : id_(scope_config.scope)
{
  if (scope_config.has_static_broker_addr)
    static_broker_addr_ = scope_config.static_broker_addr;
}

/// Whether this scope has been configured with a static IP address and port for a broker.
constexpr bool Scope::IsStatic() const noexcept
{
  return static_broker_addr_.ip().IsValid();
}

/// Whether this scope represents the default RDMnet scope.
inline bool Scope::IsDefault() const noexcept
{
  return id_ == E133_DEFAULT_SCOPE;
}

/// The ID string of this scope.
constexpr const std::string& Scope::id() const noexcept
{
  return id_;
}

/// \brief The static broker address associated with this scope.
///
/// If no static broker address is configured, returns an invalid address (SockAddr::ip().IsValid()
/// returns false).
constexpr const etcpal::SockAddr& Scope::static_broker_addr() const noexcept
{
  return static_broker_addr_;
}

/// \brief Set a new ID string for this scope.
/// \param id The ID string. Will be truncated to a maximum of 62 UTF-8 bytes.
inline void Scope::SetId(const std::string& id)
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
