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
using ScopeHandle = rdmnet_client_scope_t;

class Scope
{
public:
  constexpr Scope(const std::string& scope_str);
  constexpr Scope(const std::string& scope_str, const etcpal::SockAddr& static_broker_addr);

  constexpr bool IsStatic() const noexcept;
  constexpr bool IsDefault() const noexcept;
  constexpr const std::string& id() const noexcept;
  constexpr const etcpal::SockAddr& static_broker_addr() const noexcept;

  void SetId(const std::string& id);
  void SetStaticBrokerAddr(const etcpal::SockAddr& static_broker_addr);

private:
  std::string id_;
  etcpal::SockAddr static_broker_addr_;
};

constexpr Scope::Scope(const std::string& scope_str) : id_(scope_str)
{
}

constexpr Scope::Scope(const std::string& scope_str, const etcpal::SockAddr& static_broker_addr)
    : id_(scope_str), static_broker_addr_(static_broker_addr)
{
}

constexpr bool Scope::IsStatic() const
{
  return static_broker_addr_.ip().IsValid();
}

constexpr bool Scope::IsDefault() const
{
  return id_ == E133_DEFAULT_SCOPE;
}

constexpr const std::string& Scope::id() const
{
  return id_;
}

constexpr const etcpal::SockAddr& Scope::static_broker_addr() const
{
  return static_broker_addr_;
}

inline void Scope::SetId(const std::string& id)
{
  id_ = id;
}

inline void Scope::SetStaticBrokerAddr(const etcpal::SockAddr& static_broker_addr)
{
  static_broker_addr_ = static_broker_addr;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_CLIENT_H_
