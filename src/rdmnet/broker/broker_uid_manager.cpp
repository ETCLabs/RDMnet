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

#include "broker_uid_manager.h"

BrokerUidManager::AddResult BrokerUidManager::AddStaticUid(BrokerClient::Handle client_handle, const RdmUid& static_uid)
{
  etcpal::WriteGuard write_guard(uid_manager_lock_);

  if (uid_lookup_.size() >= max_uid_capacity_)
    return AddResult::kCapacityExceeded;

  if (uid_lookup_.find(static_uid) != uid_lookup_.end())
    return AddResult::kDuplicateId;

  uid_lookup_.insert(std::make_pair(static_uid, UidData(client_handle)));
  return AddResult::kOk;
}

BrokerUidManager::AddResult BrokerUidManager::AddDynamicUid(BrokerClient::Handle client_handle,
                                                            const etcpal::Uuid&  cid_or_rid,
                                                            RdmUid&              new_dynamic_uid)
{
  etcpal::WriteGuard write_guard(uid_manager_lock_);

  if (uid_lookup_.size() >= max_uid_capacity_)
    return AddResult::kCapacityExceeded;

  UidData new_uid_data(client_handle);

  auto reservation = reservations_.find(cid_or_rid);
  if (reservation != reservations_.end())
  {
    if (reservation->second.currently_connected)
    {
      return AddResult::kDuplicateId;
    }
    else
    {
      new_dynamic_uid = reservation->second.assigned_uid;
      reservation->second.currently_connected = true;
      new_uid_data.reservation = &reservation->second;
    }
  }
  else
  {
    // Find the next available ID - avoid assigning the reserved ID of 0.
    do
    {
      new_dynamic_uid.id = next_device_id_++;
    } while ((next_device_id_ == 1) || uid_lookup_.find(new_dynamic_uid) != uid_lookup_.end());
    auto ins_res = reservations_.insert(std::make_pair(cid_or_rid, ReservationData(new_dynamic_uid)));
    new_uid_data.reservation = &ins_res.first->second;
  }
  uid_lookup_.insert(std::make_pair(new_dynamic_uid, new_uid_data));
  return AddResult::kOk;
}

void BrokerUidManager::RemoveUid(const RdmUid& uid)
{
  etcpal::WriteGuard write_guard(uid_manager_lock_);
 
  auto uid_data = uid_lookup_.find(uid);
  if (uid_data != uid_lookup_.end())
  {
    if (uid_data->second.reservation)
    {
      uid_data->second.reservation->currently_connected = false;
    }
    uid_lookup_.erase(uid_data);
  }
}

bool BrokerUidManager::UidToHandle(const RdmUid& uid, BrokerClient::Handle& client_handle) const
{
  etcpal::ReadGuard read_guard(uid_manager_lock_);
 
  const auto uid_data = uid_lookup_.find(uid);
  if (uid_data != uid_lookup_.end())
  {
    client_handle = uid_data->second.client_handle;
    return true;
  }
  else
  {
    return false;
  }
}
