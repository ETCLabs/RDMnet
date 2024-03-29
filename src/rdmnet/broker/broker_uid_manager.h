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

#ifndef BROKER_UID_MANAGER_H_
#define BROKER_UID_MANAGER_H_

#include <map>
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/rwlock.h"
#include "rdm/uid.h"
#include "broker_client.h"

/// @brief Keeps track of all UIDs tracked by this Broker, and generates new Dynamic UIDs upon
///        request.
///
/// This class does very little validation of UIDs - that is expected to be done before this class
/// is used.
class BrokerUidManager
{
public:
  BrokerUidManager() = default;
  explicit BrokerUidManager(size_t max_uid_capacity) : max_uid_capacity_(max_uid_capacity) {}

  static constexpr size_t kDefaultMaxUidCapacity = 1000000;
  enum class AddResult
  {
    kOk,
    kCapacityExceeded,
    kDuplicateId
  };

  AddResult AddStaticUid(BrokerClient::Handle client_handle, const RdmUid& static_uid);
  AddResult AddDynamicUid(BrokerClient::Handle client_handle, const etcpal::Uuid& cid_or_rid, RdmUid& new_dynamic_uid);

  void RemoveUid(const RdmUid& uid);

  bool UidToHandle(const RdmUid& uid, BrokerClient::Handle& client_handle) const;

  void SetNextDeviceId(uint32_t next_device_id)
  {
    if (lock_)
    {
      etcpal::WriteGuard write_guard(*lock_);
      next_device_id_ = next_device_id;
    }
  }

private:
  struct ReservationData
  {
    explicit ReservationData(const RdmUid& uid) : assigned_uid(uid) {}

    RdmUid assigned_uid;
    bool   currently_connected{true};
  };
  struct UidData
  {
    explicit UidData(BrokerClient::Handle client_handle_in) : client_handle(client_handle_in) {}

    BrokerClient::Handle client_handle;
    ReservationData*     reservation{nullptr};
  };

  // The uid-keyed lookup table
  std::map<RdmUid, UidData> uid_lookup_;
  // We try to give the same components back their dynamic UIDs when they reconnect.
  // TODO: scalability/flushing to disk.
  std::map<etcpal::Uuid, ReservationData> reservations_;
  // The next dynamic RDM Device ID that will be assigned
  uint32_t next_device_id_{1};
  size_t   max_uid_capacity_{kDefaultMaxUidCapacity};
  // Protects the members of BrokerUidManager
  mutable std::unique_ptr<etcpal::RwLock> lock_{new etcpal::RwLock};
};

#endif  // BROKER_UID_MANAGER_H_
