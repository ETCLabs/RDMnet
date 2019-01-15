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

/// \file broker_uid_manager.h
#ifndef _BROKER_UID_MANAGER_H_
#define _BROKER_UID_MANAGER_H_

#include <map>
#include "lwpa/uuid.h"
#include "rdm/uid.h"

/// \brief Keeps track of all UIDs tracked by this Broker, and generates new Dynamic UIDs upon
///        request.
class BrokerUidManager
{
public:
  bool AddStaticUid(int conn_handle, const RdmUid &static_uid);
  bool AddDynamicUid(int conn_handle, const LwpaUuid &cid_or_rid, RdmUid &new_dynamic_uid);

  void RemoveUid(const RdmUid &uid) { uid_lookup_.erase(uid); }

  bool UidToHandle(const RdmUid &uid, int &conn_handle) const;

  void SetNextDeviceId(uint32_t next_device_id) {next_device_id_ = next_device_id; }

private:
  // The uid->handle lookup table
  std::map<RdmUid, int> uid_lookup_;
  // We try to give the same components back their dynamic UIDs when they reconnect.
  // TODO: scalability/flushing to disk.
  std::map<LwpaUuid, RdmUid> reservations_;
  // The next dynamic RDM Device ID that will be assigned
  uint32_t next_device_id_{1};
};

#endif  // _BROKER_UID_MANAGER_H_