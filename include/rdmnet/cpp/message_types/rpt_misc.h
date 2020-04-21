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

/// \file rdmnet/cpp/message_types/rpt_misc.h
/// \brief Definitions for miscellaneous RPT message types in RDMnet.

#ifndef RDMNET_CPP_MESSAGE_TYPES_RPT_MISC_H_
#define RDMNET_CPP_MESSAGE_TYPES_RPT_MISC_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/message.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/common.h"
#include "rdmnet/message.h"

namespace rdmnet
{
class SavedRptStatus;

/// \ingroup rdmnet_cpp_common
/// \brief An RPT status message received over RDMnet and delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// RptStatus::Save() to create a copyable version.
class RptStatus
{
public:
  /// Not default-constructible.
  RptStatus() = delete;
  constexpr RptStatus(const RdmnetRptStatus& c_status);

  constexpr rdm::Uid source_uid() const noexcept;
  constexpr uint16_t source_endpoint() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rpt_status_code_t status_code() const noexcept;
  constexpr const char* status_c_str() const noexcept;
  std::string status_string() const;

  const char* CodeToCString() const noexcept;
  std::string CodeToString() const;
  constexpr bool HasStatusString() const noexcept;

  constexpr const RdmnetRptStatus& get() const noexcept;

  SavedRptStatus Save() const;

private:
  const RdmnetRptStatus& status_;
};

/// \ingroup rdmnet_cpp_common
/// \brief An RPT status message received over RDMnet and saved for later processing.
class SavedRptStatus
{
public:
  /// Constructs an empty, invalid RPT status by default.
  SavedRptStatus() = default;
  SavedRptStatus(const RdmnetSavedRptStatus& c_status);
  SavedRptStatus& operator=(const RdmnetSavedRptStatus& c_status);
  SavedRptStatus(const RptStatus& status);
  SavedRptStatus& operator=(const RptStatus& status);

  constexpr const rdm::Uid& source_uid() const noexcept;
  constexpr uint16_t source_endpoint() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rpt_status_code_t status_code() const noexcept;
  constexpr const std::string& status_string() const noexcept;

  constexpr bool IsValid() const noexcept;
  const char* CodeToCString() const noexcept;
  std::string CodeToString() const;
  bool HasStatusString() const noexcept;

private:
  rdm::Uid source_uid_;
  uint16_t source_endpoint_{E133_NULL_ENDPOINT};
  uint32_t seq_num_{0};
  rpt_status_code_t status_code_{kRptNumStatusCodes};
  std::string status_string_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// RptStatus function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct an RptStatus from an instance of the C RdmnetRptStatus type.
constexpr RptStatus::RptStatus(const RdmnetRptStatus& c_status) : status_(c_status)
{
}

/// Get the UID of the RDMnet component that sent this RPT status message.
constexpr rdm::Uid RptStatus::source_uid() const noexcept
{
  return status_.source_uid;
}

/// Get the endpoint from which this RPT status message was sent.
constexpr uint16_t RptStatus::source_endpoint() const noexcept
{
  return status_.source_endpoint;
}

/// Get the RDMnet sequence number of this RPT status message, for matching with a corresponding command.
constexpr uint32_t RptStatus::seq_num() const noexcept
{
  return status_.seq_num;
}

/// Get the RPT status code of this status message.
constexpr rpt_status_code_t RptStatus::status_code() const noexcept
{
  return status_.status_code;
}

/// Get the optional status string accompanying this status message.
constexpr const char* RptStatus::status_c_str() const noexcept
{
  return status_.status_string;
}

/// Get the optional status string accompanying this status message.
inline std::string RptStatus::status_string() const
{
  return status_.status_string;
}

/// Convert the status message's code to a string representation.
inline const char* RptStatus::CodeToCString() const noexcept
{
  return rdmnet_rpt_status_code_to_string(status_.status_code);
}

/// Convert the status message's code to a string representation.
inline std::string RptStatus::CodeToString() const
{
  return rdmnet_rpt_status_code_to_string(status_.status_code);
}

/// Determine whether the optional RPT status string is present.
constexpr bool RptStatus::HasStatusString() const noexcept
{
  return (status_.status_string != nullptr);
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetRptStatus& RptStatus::get() const noexcept
{
  return status_;
}

/// \brief Save the data in this status message for later use from a different context.
/// \return A SavedRptStatus containing the copied data.
inline SavedRptStatus RptStatus::Save() const
{
  return SavedRptStatus(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SavedRptStatus function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a SavedRptStatus from an instance of the C RdmnetSavedRptStatus type.
inline SavedRptStatus::SavedRptStatus(const RdmnetSavedRptStatus& c_status)
    : source_uid_(c_status.source_uid)
    , source_endpoint_(c_status.source_endpoint)
    , seq_num_(c_status.seq_num)
    , status_code_(c_status.status_code)
{
  if (c_status.status_string)
    status_string_.assign(c_status.status_string);
}

/// Assign an instance of the C RdmnetSavedRptStatus type to an instance of this class.
inline SavedRptStatus& SavedRptStatus::operator=(const RdmnetSavedRptStatus& c_status)
{
  source_uid_ = c_status.source_uid;
  source_endpoint_ = c_status.source_endpoint;
  seq_num_ = c_status.seq_num;
  status_code_ = c_status.status_code;
  if (c_status.status_string)
    status_string_.assign(c_status.status_string);
  return *this;
}

/// Construct a SavedRptStatus from an RptStatus.
inline SavedRptStatus::SavedRptStatus(const RptStatus& status)
    : source_uid_(status.source_uid())
    , source_endpoint_(status.source_endpoint())
    , seq_num_(status.seq_num())
    , status_code_(status.status_code())
    , status_string_(status.status_string())
{
}

/// Assign an RptStatus to an instance of this class.
inline SavedRptStatus& SavedRptStatus::operator=(const RptStatus& status)
{
  source_uid_ = status.source_uid();
  source_endpoint_ = status.source_endpoint();
  seq_num_ = status.seq_num();
  status_code_ = status.status_code();
  status_string_ = status.status_string();
  return *this;
}

/// Get the UID of the RDMnet component that sent this RPT status message.
constexpr const rdm::Uid& SavedRptStatus::source_uid() const noexcept
{
  return source_uid_;
}

/// Get the endpoint from which this RPT status message was sent.
constexpr uint16_t SavedRptStatus::source_endpoint() const noexcept
{
  return source_endpoint_;
}

/// Get the RDMnet sequence number of this RPT status message, for matching with a corresponding command.
constexpr uint32_t SavedRptStatus::seq_num() const noexcept
{
  return seq_num_;
}

/// Get the RPT status code of this status message.
constexpr rpt_status_code_t SavedRptStatus::status_code() const noexcept
{
  return status_code_;
}

/// Get the optional status string accompanying this status message.
constexpr const std::string& SavedRptStatus::status_string() const noexcept
{
  return status_string_;
}

/// Whether the values contained in this class are valid for an RPT Status message.
constexpr bool SavedRptStatus::IsValid() const noexcept
{
  return (seq_num_ != 0 && static_cast<int>(status_code_) < static_cast<int>(kRptNumStatusCodes));
}

/// Convert the status message's code to a string representation.
inline const char* SavedRptStatus::CodeToCString() const noexcept
{
  return rdmnet_rpt_status_code_to_string(status_code_);
}

/// Convert the status message's code to a string representation.
inline std::string SavedRptStatus::CodeToString() const
{
  return rdmnet_rpt_status_code_to_string(status_code_);
}

/// Determine whether the optional RPT status string is present.
inline bool SavedRptStatus::HasStatusString() const noexcept
{
  return !status_string_.empty();
}

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
  /// Not copyable - use Save() to create a copyable version.
  RptClientList(const RptClientList& other) = delete;
  /// Not copyable - use Save() to create a copyable version.
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

/// \ingroup rdmnet_cpp_common
/// \brief A mapping from a dynamic UID to a responder ID (RID).
struct DynamicUidMapping
{
  DynamicUidMapping() = default;
  DynamicUidMapping(const RdmnetDynamicUidMapping& c_mapping);
  DynamicUidMapping& operator=(const RdmnetDynamicUidMapping& c_mapping);

  constexpr bool IsOk() const noexcept;
  const char* CodeToCString() const noexcept;
  std::string CodeToString() const;

  /// The response code - indicating whether the broker was able to assign or look up this dynamic UID.
  rdmnet_dynamic_uid_status_t status_code;
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

/// \brief Whether a DynamicUidMapping has a status code of OK.
/// \details An OK status code indicates a successful UID assignment or RID lookup.
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

/// \ingroup rdmnet_cpp_common
/// \brief A list of mappings from dynamic UIDs to responder IDs received from an RDMnet broker.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// DynamicUidAssignmentList::GetMappings() to copy out the data.
class DynamicUidAssignmentList
{
public:
  /// Not default-constructible.
  DynamicUidAssignmentList() = delete;
  /// Not copyable - use Save() to create a copyable version.
  DynamicUidAssignmentList(const DynamicUidAssignmentList& other) = delete;
  /// Not copyable - use Save() to create a copyable version.
  DynamicUidAssignmentList& operator=(const DynamicUidAssignmentList& other) = delete;

  constexpr DynamicUidAssignmentList(const RdmnetDynamicUidAssignmentList& c_list) noexcept;

  std::vector<DynamicUidMapping> GetMappings() const;

  constexpr bool more_coming() const noexcept;
  constexpr const RdmnetDynamicUidMapping* raw_mapping_array() const noexcept;
  constexpr size_t raw_mapping_array_size() const noexcept;

private:
  const RdmnetDynamicUidAssignmentList& list_;
};

/// Construct an DynamicUidAssignmentList which references an instance of the C RdmnetDynamicUidAssignmentList type.
constexpr DynamicUidAssignmentList::DynamicUidAssignmentList(const RdmnetDynamicUidAssignmentList& c_list) noexcept
    : list_(c_list)
{
}

/// \brief Copy out the list of dynamic UID mappings.
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

/// \brief This message contains a partial list.
///
/// This can be set when the library runs out of static memory in which to store Client Entries and
/// must deliver the partial list before continuing. The application should store the entries in
/// the list but should not act on the list until another DynamicUidAssignmentList is received with
/// more_coming() == false.
constexpr bool DynamicUidAssignmentList::more_coming() const noexcept
{
  return list_.more_coming;
}

/// \brief Get a pointer to the raw array of client entry C structures.
constexpr const RdmnetDynamicUidMapping* DynamicUidAssignmentList::raw_mapping_array() const noexcept
{
  return list_.mappings;
}

/// \brief Get the size of the raw array of client entry C structures.
constexpr size_t DynamicUidAssignmentList::raw_mapping_array_size() const noexcept
{
  return list_.num_mappings;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_RPT_MISC_H_
