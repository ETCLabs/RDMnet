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

/// \file rdmnet/broker/settings.h
/// \brief Defines a struct to hold the configuration settings that an rdmnet::Broker instance
///        takes on startup.

#ifndef RDMNET_BROKER_SETTINGS_H_
#define RDMNET_BROKER_SETTINGS_H_

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"

namespace rdmnet
{
/// \ingroup rdmnet_broker
/// \brief Settings for the Broker's DNS Discovery functionality.
struct BrokerDiscoveryAttributes
{
  /// Your unique name for this broker DNS-SD service instance. The discovery library uses
  /// standard mechanisms to ensure that this service instance name is actually unique;
  /// however, the application should make a reasonable effort to provide a name that will
  /// not conflict with other brokers.
  std::string service_instance_name;

  /// A string to identify the manufacturer of this broker instance.
  std::string manufacturer{"Generic Manufacturer"};
  /// A string to identify the model of product in which the broker instance is included.
  std::string model{"Generic RDMnet Broker"};
};

/// \ingroup rdmnet_broker
/// \brief A group of settings for Broker operation.
struct BrokerSettings
{
  etcpal::Uuid cid;  ///< The Broker's CID.

  enum UidType
  {
    kStaticUid,
    kDynamicUid
  } uid_type{kDynamicUid};
  RdmUid uid{0, 0};  ///< The Broker's UID.

  BrokerDiscoveryAttributes dns;

  /// The Scope on which this broker should operate. If empty, the default RDMnet scope is used.
  std::string scope{E133_DEFAULT_SCOPE};

  /// The port on which this broker should listen for incoming connections (and advertise via DNS).
  /// 0 means use an ephemeral port.
  uint16_t listen_port{0};
  /// A list of MAC addresses representing network interfaces to listen on. If both this and
  /// listen_addrs are empty, the broker will listen on all available interfaces. Otherwise
  /// listening will be restricted to the interfaces specified.
  std::set<etcpal::MacAddr> listen_macs;
  /// A list of IP addresses representing network interfaces to listen on. If both this and
  /// listen_macs are empty, the broker will listen on all available interfaces. Otherwise
  /// listening will be restricted to the interfaces specified.
  std::set<etcpal::IpAddr> listen_addrs;

  /// The maximum number of client connections supported. 0 means infinite.
  unsigned int max_connections{0};
  /// The maximum number of controllers allowed. 0 means infinite.
  unsigned int max_controllers{0};
  /// The maximum number of queued messages per controller. 0 means infinite.
  unsigned int max_controller_messages{500};
  /// The maximum number of devices allowed.  0 means infinite.
  unsigned int max_devices{0};
  /// The maximum number of queued messages per device. 0 means infinite.
  unsigned int max_device_messages{500};
  /// If you reach the number of max connections, this number of tcp-level connections are still
  /// supported to reject the connection request.
  unsigned int max_reject_connections{1000};

  BrokerSettings() = default;
  BrokerSettings(const etcpal::Uuid& cid_in, const RdmUid& static_uid_in);
  BrokerSettings(const etcpal::Uuid& cid_in, uint16_t rdm_manu_id_in);

  void SetDynamicUid(uint16_t manufacturer_id);
  void SetStaticUid(const RdmUid& uid_in);
  void SetDefaultServiceInstanceName();

  bool valid() const;
};

inline void BrokerSettings::SetDynamicUid(uint16_t manufacturer_id)
{
  RDMNET_INIT_DYNAMIC_UID_REQUEST(&uid, manufacturer_id);
  uid_type = kDynamicUid;
}

inline void BrokerSettings::SetStaticUid(const RdmUid& uid_in)
{
  uid = uid_in;
  uid_type = kStaticUid;
}

inline void BrokerSettings::SetDefaultServiceInstanceName()
{
  dns.service_instance_name = "RDMnet Broker Instance " + cid.ToString();
}

/// Initialize a BrokerSettings with a CID and static UID.
inline BrokerSettings::BrokerSettings(const etcpal::Uuid& cid_in, const RdmUid& static_uid_in) : cid(cid_in)
{
  SetStaticUid(static_uid_in);
  SetDefaultServiceInstanceName();
}

/// Initialize a BrokerSettings with a CID and dynamic UID (provide the manufacturer ID).
inline BrokerSettings::BrokerSettings(const etcpal::Uuid& cid_in, uint16_t rdm_manu_id_in) : cid(cid_in)
{
  SetDynamicUid(rdm_manu_id_in);
  SetDefaultServiceInstanceName();
}

inline bool BrokerSettings::valid() const
{
  // clang-format off
  return (
          !cid.IsNull() &&
          (!scope.empty() && scope.length() < (E133_SCOPE_STRING_PADDED_LENGTH - 1)) &&
          (!dns.manufacturer.empty() && dns.manufacturer.length() < (E133_MANUFACTURER_STRING_PADDED_LENGTH - 1)) &&
          (!dns.model.empty() && dns.model.length() < (E133_MODEL_STRING_PADDED_LENGTH - 1)) &&
          (!dns.service_instance_name.empty() && dns.service_instance_name.length() < (E133_SERVICE_NAME_STRING_PADDED_LENGTH - 1)) &&
          (listen_port == 0 || listen_port >= 1024) &&
          (RDM_GET_MANUFACTURER_ID(&uid) > 0 && RDM_GET_MANUFACTURER_ID(&uid) < 0x8000) &&
          !(uid_type == rdmnet::BrokerSettings::kStaticUid && !RDMNET_UID_IS_STATIC(&uid)) &&
          !(uid_type == rdmnet::BrokerSettings::kDynamicUid && !RDMNET_UID_IS_DYNAMIC(&uid))
         );
  // clang-format on
}

};  // namespace rdmnet

#endif  // RDMNET_BROKER_SETTINGS_H_
