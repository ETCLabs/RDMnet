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

/// \file rdmnet/cpp/message_types/rpt_client.h
/// \brief Definitions for RPT client list and client entry message types in RDMnet.

#ifndef RDMNET_CPP_MESSAGE_TYPES_RPT_CLIENT_H_
#define RDMNET_CPP_MESSAGE_TYPES_RPT_CLIENT_H_

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/message.h"

namespace rdmnet
{
/// \ingroup rdmnet_cpp_common
/// \brief A descriptive structure for an RPT client.
struct RptClientEntry
{
  RptClientEntry() = default;
  RptClientEntry(const RdmnetRptClientEntry& c_entry);
  RptClientEntry& operator=(const RdmnetRptClientEntry& c_entry);

  etcpal::Uuid cid;          ///< The client's Component Identifier (CID).
  rdm::Uid uid;              ///< The client's RDM UID.
  rpt_client_type_t type;    ///< Whether the client is a controller or a device.
  etcpal::Uuid binding_cid;  ///< An optional identifier for another component that the client is associated with.
};

/// Construct an RptClientEntry copied from an instance of the C RdmnetRptClientEntry type.
inline RptClientEntry::RptClientEntry(const RdmnetRptClientEntry& c_entry)
    : cid(c_entry.cid), uid(c_entry.uid), type(c_entry.type), binding_cid(c_entry.binding_cid)
{
}

/// Assign an instance of the C RdmnetRptClientEntry type to an instance of this class.
inline RptClientEntry& RptClientEntry::operator=(const RdmnetRptClientEntry& c_entry)
{
  cid = c_entry.cid;
  uid = c_entry.uid;
  type = c_entry.type;
  binding_cid = c_entry.binding_cid;
  return *this;
}

/// \ingroup rdmnet_cpp_common
/// \brief A list of RPT client entries.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// RptClientList::GetClientEntries() to copy out the data.
class RptClientList
{
public:
  /// Not default-constructible.
  RptClientList() = delete;
  /// Not copyable - use GetClientEntries() to copy out the data.
  RptClientList(const RptClientList& other) = delete;
  /// Not copyable - use GetClientEntries() to copy out the data.
  RptClientList& operator=(const RptClientList& other) = delete;

  constexpr RptClientList(const RdmnetRptClientList& c_list) noexcept;

  std::vector<RptClientEntry> GetClientEntries() const;

  constexpr bool more_coming() const noexcept;
  constexpr const RdmnetRptClientEntry* raw_entry_array() const noexcept;
  constexpr size_t raw_entry_array_size() const noexcept;

private:
  const RdmnetRptClientList& list_;
};

/// Construct an RptClientList which references an instance of the C RdmnetRptClientList type.
constexpr RptClientList::RptClientList(const RdmnetRptClientList& c_list) noexcept : list_(c_list)
{
}

/// \brief Copy out the list of client entries.
///
/// This function copies and translates the list delivered to a callback function into C++ native
/// types.
inline std::vector<RptClientEntry> RptClientList::GetClientEntries() const
{
  std::vector<RptClientEntry> to_return;
  to_return.reserve(list_.num_client_entries);
  std::transform(list_.client_entries, list_.client_entries + list_.num_client_entries, std::back_inserter(to_return),
                 [](const RdmnetRptClientEntry& entry) { return RptClientEntry(entry); });
  return to_return;
}

/// \brief This message contains a partial list.
///
/// This can be set when the library runs out of static memory in which to store Client Entries and
/// must deliver the partial list before continuing. The application should store the entries in
/// the list but should not act on the list until another RptClientList is received with
/// more_coming() == false.
constexpr bool RptClientList::more_coming() const noexcept
{
  return list_.more_coming;
}

/// \brief Get a pointer to the raw array of client entry C structures.
constexpr const RdmnetRptClientEntry* RptClientList::raw_entry_array() const noexcept
{
  return list_.client_entries;
}

/// \brief Get the size of the raw array of client entry C structures.
constexpr size_t RptClientList::raw_entry_array_size() const noexcept
{
  return list_.num_client_entries;
}
};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_RPT_CLIENT_H_
