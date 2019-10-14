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
#include "etcpal/inet.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"

namespace rdmnet
{
/// Settings for the Broker's DNS Discovery functionality.
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

/// A group of settings for Broker operation.
struct BrokerSettings
{
  using MacAddress = std::array<uint8_t, ETCPAL_NETINTINFO_MAC_LEN>;

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
  std::set<MacAddress> listen_macs;
  /// A list of IP addresses representing network interfaces to listen on. If both this and
  /// listen_macs are empty, the broker will listen on all available interfaces. Otherwise
  /// listening will be restricted to the interfaces specified.
  std::set<EtcPalIpAddr> listen_addrs;

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

  BrokerSettings();
  BrokerSettings(const RdmUid& static_uid);
  BrokerSettings(uint16_t rdm_manu_id);

  void SetDynamicUid(uint16_t manufacturer_id);
  void SetStaticUid(const RdmUid& uid_in);

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

inline BrokerSettings::BrokerSettings()
{
}

/// Initialize a BrokerSettings with a static UID.
inline BrokerSettings::BrokerSettings(const RdmUid& static_uid)
{
  SetStaticUid(static_uid);
}

/// Initialize a BrokerSettings with a dynamic UID (provide the manufacturer ID).
inline BrokerSettings::BrokerSettings(uint16_t rdm_manu_id)
{
  SetDynamicUid(rdm_manu_id);
}

inline bool BrokerSettings::valid() const
{
  return (!cid.IsNull() && (scope.length() < (E133_SCOPE_STRING_PADDED_LENGTH - 1)) && (listen_port >= 1024) &&
          !(uid_type == rdmnet::BrokerSettings::kStaticUid && !RDMNET_UID_IS_STATIC(&uid)) &&
          !(uid_type == rdmnet::BrokerSettings::kDynamicUid && !RDMNET_UID_IS_DYNAMIC(&uid)));
}

};  // namespace rdmnet

#endif  // RDMNET_BROKER_SETTINGS_H_
