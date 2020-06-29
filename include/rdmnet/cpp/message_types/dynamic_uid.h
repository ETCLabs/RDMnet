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

/// @file rdmnet/cpp/message_types/dynamic_uid.h
/// @brief Definitions for message types representing dynamic UID assignment lists and mappings in RDMnet.

#ifndef RDMNET_CPP_MESSAGE_TYPES_DYNAMIC_UID_H_
#define RDMNET_CPP_MESSAGE_TYPES_DYNAMIC_UID_H_

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/common.h"
#include "rdmnet/message.h"

namespace rdmnet
{
/// @ingroup rdmnet_cpp_common
/// @brief A mapping from a dynamic UID to a responder ID (RID).
struct DynamicUidMapping
{
  DynamicUidMapping() = default;
  DynamicUidMapping(const RdmnetDynamicUidMapping& c_mapping);
  DynamicUidMapping& operator=(const RdmnetDynamicUidMapping& c_mapping);

  constexpr bool IsOk() const noexcept;
  const char*    CodeToCString() const noexcept;
  std::string    CodeToString() const;

  /// The response code - indicating whether the broker was able to assign or look up this dynamic UID.
  rdmnet_dynamic_uid_status_t status_code{kRdmnetDynamicUidStatusOk};
  /// The dynamic UID.
  rdm::Uid uid;
  /// The corresponding RID to which the dynamic UID is mapped.
  etcpal::Uuid rid;
};

/// Construct an DynamicUidMapping copied from an instance of the C RdmnetDynamicUidMapping type.
inline DynamicUidMapping::DynamicUidMapping(const RdmnetDynamicUidMapping& c_mapping)
    : status_code(c_mapping.status_code), uid(c_mapping.uid), rid(c_mapping.rid)
{
}

/// Assign an instance of the C RdmnetDynamicUidMapping type to an instance of this class.
inline DynamicUidMapping& DynamicUidMapping::operator=(const RdmnetDynamicUidMapping& c_mapping)
{
  status_code = c_mapping.status_code;
  uid = c_mapping.uid;
  rid = c_mapping.rid;
  return *this;
}

/// @brief Whether a DynamicUidMapping has a status code of OK.
/// @details An OK status code indicates a successful UID assignment or RID lookup.
constexpr bool DynamicUidMapping::IsOk() const noexcept
{
  return (status_code == kRdmnetDynamicUidStatusOk);
}

/// Convert the mapping status code to a string representation.
inline const char* DynamicUidMapping::CodeToCString() const noexcept
{
  return rdmnet_dynamic_uid_status_to_string(status_code);
}

/// Convert the mapping status code to a string representation.
inline std::string DynamicUidMapping::CodeToString() const
{
  return rdmnet_dynamic_uid_status_to_string(status_code);
}

/// @ingroup rdmnet_cpp_common
/// @brief A list of mappings from dynamic UIDs to responder IDs received from an RDMnet broker.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// DynamicUidAssignmentList::GetMappings() to copy out the data.
class DynamicUidAssignmentList
{
public:
  /// Not default-constructible.
  DynamicUidAssignmentList() = delete;
  /// Not copyable - use GetMappings() to copy out the data.
  DynamicUidAssignmentList(const DynamicUidAssignmentList& other) = delete;
  /// Not copyable - use GetMappings() to copy out the data.
  DynamicUidAssignmentList& operator=(const DynamicUidAssignmentList& other) = delete;

  constexpr DynamicUidAssignmentList(const RdmnetDynamicUidAssignmentList& c_list) noexcept;

  std::vector<DynamicUidMapping> GetMappings() const;

  constexpr bool                           more_coming() const noexcept;
  constexpr const RdmnetDynamicUidMapping* raw_mapping_array() const noexcept;
  constexpr size_t                         raw_mapping_array_size() const noexcept;

private:
  const RdmnetDynamicUidAssignmentList& list_;
};

/// Construct a DynamicUidAssignmentList which references an instance of the C RdmnetDynamicUidAssignmentList type.
constexpr DynamicUidAssignmentList::DynamicUidAssignmentList(const RdmnetDynamicUidAssignmentList& c_list) noexcept
    : list_(c_list)
{
}

/// @brief Copy out the list of dynamic UID mappings.
///
/// This function copies and translates the list delivered to a callback function into C++ native
/// types.
inline std::vector<DynamicUidMapping> DynamicUidAssignmentList::GetMappings() const
{
  std::vector<DynamicUidMapping> to_return;
  to_return.reserve(list_.num_mappings);
  std::transform(list_.mappings, list_.mappings + list_.num_mappings, std::back_inserter(to_return),
                 [](const RdmnetDynamicUidMapping& mapping) { return DynamicUidMapping(mapping); });
  return to_return;
}

/// @brief This message contains a partial list.
///
/// This can be set when the library runs out of static memory in which to store Client Entries and
/// must deliver the partial list before continuing. The application should store the entries in
/// the list but should not act on the list until another DynamicUidAssignmentList is received with
/// more_coming() == false.
constexpr bool DynamicUidAssignmentList::more_coming() const noexcept
{
  return list_.more_coming;
}

/// @brief Get a pointer to the raw array of dynamic UID mapping C structures.
constexpr const RdmnetDynamicUidMapping* DynamicUidAssignmentList::raw_mapping_array() const noexcept
{
  return list_.mappings;
}

/// @brief Get the size of the raw array of dynamic UID mapping C structures.
constexpr size_t DynamicUidAssignmentList::raw_mapping_array_size() const noexcept
{
  return list_.num_mappings;
}
};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_DYNAMIC_UID_H_
