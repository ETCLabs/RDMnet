/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "broker_uid_manager.h"

bool BrokerUidManager::AddStaticUid(int conn_handle, const RdmUid &static_uid)
{
  if (uid_lookup_.find(static_uid) != uid_lookup_.end())
    return false;

  uid_lookup_.insert(std::make_pair(static_uid, conn_handle));
  return true;
}

bool BrokerUidManager::AddDynamicUid(int conn_handle, const LwpaUuid &cid_or_rid, RdmUid &new_dynamic_uid)
{
  if (uid_lookup_.size() >= 0xffffffff)
    return false;

  auto reservation = reservations_.find(cid_or_rid);
  if (reservation != reservations_.end())
  {
    new_dynamic_uid = reservation->second;
  }
  else
  {
    do
    {
      new_dynamic_uid.id = next_device_id_++;
    } while (uid_lookup_.find(new_dynamic_uid) != uid_lookup_.end());
    reservations_.insert(std::make_pair(cid_or_rid, new_dynamic_uid));
  }
  uid_lookup_.insert(std::make_pair(new_dynamic_uid, conn_handle));
  return true;
}

bool BrokerUidManager::UidToHandle(const RdmUid &uid, int &conn_handle) const
{
  const auto handle_pair = uid_lookup_.find(uid);
  if (handle_pair != uid_lookup_.end())
  {
    conn_handle = handle_pair->second;
    return true;
  }
  else
  {
    return false;
  }
}