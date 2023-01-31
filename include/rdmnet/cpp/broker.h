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

/// @file rdmnet/cpp/broker.h
/// @brief A platform-neutral RDMnet Broker implementation.

#ifndef RDMNET_CPP_BROKER_H_
#define RDMNET_CPP_BROKER_H_

#include <cstdint>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "etcpal/common.h"
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/log.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/common.h"
#include "rdmnet/defs.h"

class BrokerCore;

namespace rdmnet
{
/// @defgroup rdmnet_broker Broker API
/// @ingroup rdmnet_cpp_api
/// @brief Implementation of RDMnet broker functionality; see @ref using_broker.

/// @ingroup rdmnet_broker
/// @brief A key/value pair representing a DNS TXT record item.
/// @details The key and value combined cannot be more than 255 bytes in length.
struct DnsTxtRecordItem
{
  std::string          key;    ///< The key is an ASCII-only string.
  std::vector<uint8_t> value;  ///< The value is opaque binary data.

  DnsTxtRecordItem() = default;
  DnsTxtRecordItem(const char* new_key, const char* new_value);
  DnsTxtRecordItem(const char* new_key, const uint8_t* new_value, size_t new_value_len);
  DnsTxtRecordItem(const std::string& new_key, const std::string& new_value);
  DnsTxtRecordItem(const std::string& new_key, const uint8_t* new_value, size_t new_value_len);
  DnsTxtRecordItem(const std::string& new_key, const std::vector<uint8_t>& new_value);
};

/// Construct a DnsTxtRecordItem from a C-string key and value.
inline DnsTxtRecordItem::DnsTxtRecordItem(const char* new_key, const char* new_value)
{
  if (new_key)
    key = std::string(new_key);

  if (new_value)
    value = std::vector<uint8_t>(new_value, &new_value[std::strlen(new_value)]);
}

/// Construct a DnsTxtRecordItem from a C-string key and a binary value.
inline DnsTxtRecordItem::DnsTxtRecordItem(const char* new_key, const uint8_t* new_value, size_t new_value_len)
{
  if (new_key)
    key = std::string(new_key);

  if (new_value)
    value = std::vector<uint8_t>(new_value, new_value + new_value_len);
}

/// Construct a DnsTxtRecordItem from a string key and value.
inline DnsTxtRecordItem::DnsTxtRecordItem(const std::string& new_key, const std::string& new_value)
    : key(new_key), value(new_value.begin(), new_value.end())
{
}

/// Construct a DnsTxtRecordItem from a string key and a binary value.
inline DnsTxtRecordItem::DnsTxtRecordItem(const std::string& new_key, const uint8_t* new_value, size_t new_value_len)
    : key(new_key)
{
  if (new_value)
    value = std::vector<uint8_t>(new_value, new_value + new_value_len);
}

/// Construct a DnsTxtRecordItem from a string key and a binary value.
inline DnsTxtRecordItem::DnsTxtRecordItem(const std::string& new_key, const std::vector<uint8_t>& new_value)
    : key(new_key), value(new_value)
{
}

/// @ingroup rdmnet_broker
/// @brief Defines an instance of RDMnet broker functionality.
///
/// Use the Broker::Settings struct to configure the behavior of the broker. After
/// instantiatiation, call Startup() to start broker services on a set of network interfaces.
///
/// Starts some threads to handle messages and connections. The current breakdown (pending
/// concurrency optimization) is:
///   * Either:
///     + One thread per explicitly-specified network interface being listened on, or
///     + One thread, if listening on all interfaces
///   * A platform-dependent number of threads to receive messages from clients, depending on the
///     most efficient way to read large number of sockets on a given platform
///   * One thread to handle message routing between clients
///   * One thread to handle periodic cleanup and housekeeping.
///
/// Call Shutdown() at exit, when Broker services are no longer needed, or when a setting has
/// changed. The Broker may send notifications through the Broker::NotifyHandler interface.
class Broker
{
public:
  /// @ingroup rdmnet_broker
  /// @brief Settings for the Broker's DNS Discovery functionality.
  struct DnsAttributes
  {
    /// @brief Your unique name for this broker DNS-SD service instance.
    ///
    /// The discovery library uses standard mechanisms to ensure that this service instance name is
    /// actually unique; however, the application should make a reasonable effort to provide a name
    /// that will not conflict with other brokers.
    std::string service_instance_name;

    /// A string to identify the manufacturer of this broker instance.
    std::string manufacturer{"Generic Manufacturer"};
    /// A string to identify the model of product in which the broker instance is included.
    std::string model{"Generic RDMnet Broker"};

    /// Any additional non-standard items to add to the broker's DNS TXT record.
    std::vector<DnsTxtRecordItem> additional_txt_record_items;
  };

  /// @ingroup rdmnet_broker
  /// @brief A set of limits for broker operation.
  struct Limits
  {
    /// The maximum number of client connections supported. 0 means infinite.
    unsigned int connections{0};
    /// The maximum number of controllers allowed. 0 means infinite.
    unsigned int controllers{0};
    /// The maximum number of queued messages per controller. 0 means infinite.
    unsigned int controller_messages{500};
    /// The maximum number of devices allowed.  0 means infinite.
    unsigned int devices{0};
    /// The maximum number of queued messages per device. 0 means infinite.
    unsigned int device_messages{500};
    /// If you reach the number of max connections, this number of tcp-level connections are still
    /// supported to reject the connection request.
    unsigned int reject_connections{1000};
  };

  /// @ingroup rdmnet_broker
  /// @brief A group of settings for broker operation.
  struct Settings
  {
    etcpal::Uuid  cid;     ///< The broker's CID.
    rdm::Uid      uid;     ///< The broker's UID.
    DnsAttributes dns;     ///< The broker's DNS attributes.
    Limits        limits;  ///< The broker's limits.

    /// The RDMnet scope on which this broker should operate.
    std::string scope{E133_DEFAULT_SCOPE};
    /// Whether the broker should allow the scope to be changed via RDM commands.
    // bool allow_rdm_scope_change{false};  // (TODO: Not yet implemented)
    /// Whether the broker should allow being disabled and enabled via the BROKER_STATUS RDM command.
    // bool allow_rdm_disable{false};  // (TODO: Not yet implemented)

    /// The port on which this broker should listen for incoming connections (and advertise via DNS).
    /// 0 means use an ephemeral port.
    uint16_t listen_port{0};

    Settings() = default;
    Settings(const etcpal::Uuid& cid_in, const rdm::Uid& static_uid_in);
    Settings(const etcpal::Uuid& cid_in, uint16_t rdm_manu_id_in);

    void SetDefaultServiceInstanceName();

    bool IsValid() const;
  };

  /// @ingroup rdmnet_broker
  /// @brief A callback interface for notifications from the broker.
  class NotifyHandler
  {
  public:
    virtual ~NotifyHandler() = default;

    /// @brief The scope of the broker has changed via RDMnet configuration.
    ///
    /// This callback is informative; no action needs to be taken to adjust broker operation to the
    /// new scope. This callback will only be called if the associated
    /// Settings::allow_remote_scope_change was set to true.
    virtual void HandleScopeChanged(const std::string& new_scope) { ETCPAL_UNUSED_ARG(new_scope); }
  };

  Broker();
  virtual ~Broker();

  /// Brokers cannot be copied.
  Broker(const Broker& other) = delete;
  /// Brokers cannot be copied.
  Broker& operator=(const Broker& other) = delete;
  /// Move an instance of broker functionality.
  Broker(Broker&& other) = default;
  /// Move an instance of broker functionality.
  Broker& operator=(Broker&& other) = default;

  static etcpal::Error Init(const EtcPalLogParams*                  log_params = nullptr,
                            const std::vector<EtcPalMcastNetintId>& netints = std::vector<EtcPalMcastNetintId>{});
  static etcpal::Error Init(const etcpal::Logger&                   logger,
                            const std::vector<EtcPalMcastNetintId>& netints = std::vector<EtcPalMcastNetintId>{});
  static void          Deinit();

  etcpal::Error Startup(const Settings& settings, etcpal::Logger* logger = nullptr, NotifyHandler* notify = nullptr);
  void          Shutdown(rdmnet_disconnect_reason_t disconnect_reason = kRdmnetDisconnectShutdown);
  etcpal::Error ChangeScope(const std::string& new_scope, rdmnet_disconnect_reason_t disconnect_reason);

  const Settings& settings() const;

private:
  std::unique_ptr<BrokerCore> core_;
};

/// Generate a DNS service instance name based on the broker's current CID.
inline void Broker::Settings::SetDefaultServiceInstanceName()
{
  dns.service_instance_name = "RDMnet Broker Instance " + cid.ToString();
}

/// Initialize a broker Settings with a CID and static UID.
inline Broker::Settings::Settings(const etcpal::Uuid& cid_in, const rdm::Uid& static_uid_in)
    : cid(cid_in), uid(static_uid_in)
{
  SetDefaultServiceInstanceName();
}

/// Initialize a broker Settings with a CID and dynamic UID (provide the manufacturer ID).
inline Broker::Settings::Settings(const etcpal::Uuid& cid_in, uint16_t rdm_manu_id_in)
    : cid(cid_in), uid(rdm::Uid::DynamicUidRequest(rdm_manu_id_in))
{
  SetDefaultServiceInstanceName();
}

/// Whether this structure contains valid settings for broker operation.
inline bool Broker::Settings::IsValid() const
{
  // clang-format off
  return (
          !cid.IsNull() &&
          (!scope.empty() && scope.length() < (E133_SCOPE_STRING_PADDED_LENGTH - 1)) &&
          (!dns.manufacturer.empty() && dns.manufacturer.length() < (E133_MANUFACTURER_STRING_PADDED_LENGTH - 1)) &&
          (!dns.model.empty() && dns.model.length() < (E133_MODEL_STRING_PADDED_LENGTH - 1)) &&
          (!dns.service_instance_name.empty() && dns.service_instance_name.length() < (E133_SERVICE_NAME_STRING_PADDED_LENGTH - 1)) &&
          (listen_port == 0 || listen_port >= 1024) &&
          (uid.manufacturer_id() != 0) &&
          (uid.IsStatic() || uid.IsDynamicUidRequest())
         );
  // clang-format on
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_BROKER_H_
