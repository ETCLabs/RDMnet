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

/// \file rdmnet/cpp/message_types/ept_misc.h
/// \brief Definitions for miscellaneous EPT message types in RDMnet.

#ifndef RDMNET_CPP_MESSAGE_TYPES_EPT_MISC_H_
#define RDMNET_CPP_MESSAGE_TYPES_EPT_MISC_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "etcpal/cpp/uuid.h"
#include "rdmnet/message.h"

namespace rdmnet
{
class SavedEptStatus;

/// \ingroup rdmnet_cpp_common
/// \brief An EPT status message received over RDMnet and delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// EptStatus::Save() to create a copyable version.
class EptStatus
{
public:
  /// Not default-constructible.
  EptStatus() = delete;
  /// Not copyable - use Save() to create a copyable version.
  EptStatus(const EptStatus& other) = delete;
  /// Not copyable - use Save() to create a copyable version.
  EptStatus& operator=(const EptStatus& other) = delete;
  constexpr EptStatus(const RdmnetEptStatus& c_status);

  constexpr etcpal::Uuid source_cid() const noexcept;
  constexpr ept_status_code_t status_code() const noexcept;
  constexpr const char* status_c_str() const noexcept;
  std::string status_string() const;

  const char* CodeToCString() const noexcept;
  std::string CodeToString() const;
  constexpr bool HasStatusString() const noexcept;

  constexpr const RdmnetEptStatus& get() const noexcept;

  SavedEptStatus Save() const;

private:
  const RdmnetEptStatus& status_;
};

/// \ingroup rdmnet_cpp_common
/// \brief An EPT status message received over RDMnet and saved for later processing.
class SavedEptStatus
{
public:
  /// Constructs an empty, invalid EPT status by default.
  SavedEptStatus() = default;
  SavedEptStatus(const RdmnetSavedEptStatus& c_resp);
  SavedEptStatus& operator=(const RdmnetSavedEptStatus& c_resp);
  SavedEptStatus(const EptStatus& status);
  SavedEptStatus& operator=(const EptStatus& status);

  constexpr const etcpal::Uuid& source_cid() const noexcept;
  constexpr ept_status_code_t status_code() const noexcept;
  constexpr const std::string& status_string() const noexcept;

  constexpr bool IsValid() const noexcept;
  const char* CodeToCString() const noexcept;
  std::string CodeToString() const;
  bool HasStatusString() const noexcept;

private:
  etcpal::Uuid source_cid_;
  ept_status_code_t status_code_{kEptNumStatusCodes};
  std::string status_string_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// EptStatus function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct an EptStatus from an instance of the C RdmnetRptStatus type.
constexpr EptStatus::EptStatus(const RdmnetEptStatus& c_status) : status_(c_status)
{
}

/// Get the CID of the EPT client that sent this status message.
constexpr etcpal::Uuid EptStatus::source_cid() const noexcept
{
  return status_.source_cid;
}

/// Get the EPT status code of this status message.
constexpr ept_status_code_t EptStatus::status_code() const noexcept
{
  return status_.status_code;
}

/// Get the optional status string accompanying this status message.
constexpr const char* EptStatus::status_c_str() const noexcept
{
  return status_.status_string;
}

/// Get the optional status string accompanying this status message.
inline std::string EptStatus::status_string() const
{
  return status_.status_string;
}

/// Convert the status message's code to a string representation.
inline const char* EptStatus::CodeToCString() const noexcept
{
  return status_.status_string;
}

/// Convert the status message's code to a string representation.
inline std::string EptStatus::CodeToString() const
{
  return status_.status_string;
}

/// Determine whether the optional EPT status string is present.
constexpr bool EptStatus::HasStatusString() const noexcept
{
  return (status_.status_string != nullptr);
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetEptStatus& EptStatus::get() const noexcept
{
  return status_;
}

/// \brief Save the data in this status message for later use from a different context.
/// \return A SavedEptStatus containing the copied data.
inline SavedEptStatus EptStatus::Save() const
{
  return SavedEptStatus(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SavedEptStatus function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a SavedEptStatus from an instance of the C RdmnetSavedEptStatus type.
inline SavedEptStatus::SavedEptStatus(const RdmnetSavedEptStatus& c_status)
    : source_cid_(c_status.source_cid), status_code_(c_status.status_code)
{
  if (c_status.status_string)
    status_string_.assign(c_status.status_string);
}

/// Assign an instance of the C RdmnetSavedEptStatus type to an instance of this class.
inline SavedEptStatus& SavedEptStatus::operator=(const RdmnetSavedEptStatus& c_status)
{
  source_cid_ = c_status.source_cid;
  status_code_ = c_status.status_code;
  if (c_status.status_string)
    status_string_.assign(c_status.status_string);
  return *this;
}

/// Construct a SavedEptStatus from an EptStatus.
inline SavedEptStatus::SavedEptStatus(const EptStatus& status)
    : source_cid_(status.source_cid()), status_code_(status.status_code()), status_string_(status.status_string())
{
}

/// Assign an EptStatus to an instance of this class.
inline SavedEptStatus& SavedEptStatus::operator=(const EptStatus& status)
{
  source_cid_ = status.source_cid();
  status_code_ = status.status_code();
  status_string_ = status.status_string();
  return *this;
}

/// Get the CID of the EPT client that sent this EPT status message.
constexpr const etcpal::Uuid& SavedEptStatus::source_cid() const noexcept
{
  return source_cid_;
}

/// Get the EPT status code of this status message.
constexpr ept_status_code_t SavedEptStatus::status_code() const noexcept
{
  return status_code_;
}

/// Get the optional status string accompanying this status message.
constexpr const std::string& SavedEptStatus::status_string() const noexcept
{
  return status_string_;
}

/// Whether the values contained in this class are valid for an EPT status message.
constexpr bool SavedEptStatus::IsValid() const noexcept
{
  return (static_cast<int>(status_code_) < static_cast<int>(kEptNumStatusCodes));
}

/// Convert the status message's code to a string representation.
inline const char* SavedEptStatus::CodeToCString() const noexcept
{
  return rdmnet_ept_status_code_to_string(status_code_);
}

/// Convert the status message's code to a string representation.
inline std::string SavedEptStatus::CodeToString() const
{
  return rdmnet_ept_status_code_to_string(status_code_);
}

/// Determine whether the optional EPT status string is present.
inline bool SavedEptStatus::HasStatusString() const noexcept
{
  return !status_string_.empty();
}

/// \ingroup rdmnet_cpp_common
/// \brief A description of an EPT sub-protocol.
///
/// EPT clients can implement multiple protocols, each of which is identified by a two-part
/// identifier including an ESTA manufacturer ID and a protocol ID.
struct EptSubProtocol
{
  EptSubProtocol() = default;
  EptSubProtocol(uint16_t new_manufacturer_id, uint16_t new_protocol_id, const std::string& new_protocol_string);
  EptSubProtocol(uint16_t new_manufacturer_id, uint16_t new_protocol_id, const char* new_protocol_string);
  EptSubProtocol(const RdmnetEptSubProtocol& c_prot);
  EptSubProtocol& operator=(const RdmnetEptSubProtocol& c_prot);

  uint16_t manufacturer_id{0};  ///< The ESTA manufacturer ID under which this protocol is namespaced.
  uint16_t protocol_id{0};      ///< The identifier for this protocol.
  std::string protocol_string;  ///< A descriptive string for the protocol.
};

/// Construct an EptSubProtocol from the required values.
inline EptSubProtocol::EptSubProtocol(uint16_t new_manufacturer_id, uint16_t new_protocol_id,
                                      const std::string& new_protocol_string)
    : manufacturer_id(new_manufacturer_id), protocol_id(new_protocol_id), protocol_string(new_protocol_string)
{
}

/// Construct an EptSubProtocol from the required values.
inline EptSubProtocol::EptSubProtocol(uint16_t new_manufacturer_id, uint16_t new_protocol_id,
                                      const char* new_protocol_string)
    : manufacturer_id(new_manufacturer_id), protocol_id(new_protocol_id), protocol_string(new_protocol_string)
{
}

/// Construct an EptSubProtocol copied from an instance of the C RdmnetEptSubProtocol type.
inline EptSubProtocol::EptSubProtocol(const RdmnetEptSubProtocol& c_prot)
    : manufacturer_id(c_prot.manufacturer_id), protocol_id(c_prot.protocol_id), protocol_string(c_prot.protocol_string)
{
}

/// Assign an instance of the C RdmnetEptSubProtocol type to an instance of this class.
inline EptSubProtocol& EptSubProtocol::operator=(const RdmnetEptSubProtocol& c_prot)
{
  manufacturer_id = c_prot.manufacturer_id;
  protocol_id = c_prot.protocol_id;
  protocol_string = c_prot.protocol_string;
}

/// \ingroup rdmnet_cpp_common
/// \brief A descriptive structure for an EPT client.
struct EptClientEntry
{
  EptClientEntry() = default;
  EptClientEntry(const RdmnetEptClientEntry& c_entry);
  EptClientEntry& operator=(const RdmnetEptClientEntry& c_entry);

  etcpal::Uuid cid;                       ///< The client's Component Identifier (CID).
  std::vector<EptSubProtocol> protocols;  ///< A list of EPT protocols that this client implements.
};

/// Construct an EptClientEntry copied from an instance of the C RdmnetEptClientEntry type.
inline EptClientEntry::EptClientEntry(const RdmnetEptClientEntry& c_entry) : cid(c_entry.cid)
{
  protocols.reserve(c_entry.num_protocols);
  std::transform(c_entry.protocols, c_entry.protocols + c_entry.num_protocols, std::back_inserter(protocols),
                 [](const RdmnetEptSubProtocol& protocol) { return EptSubProtocol(protocol); });
}

/// Assign an instance of the C RdmnetEptClientEntry type to an instance of this class.
inline EptClientEntry& EptClientEntry::operator=(const RdmnetEptClientEntry& c_entry)
{
  cid = c_entry.cid;
  protocols.clear();
  protocols.reserve(c_entry.num_protocols);
  std::transform(c_entry.protocols, c_entry.protocols + c_entry.num_protocols, std::back_inserter(protocols),
                 [](const RdmnetEptSubProtocol& protocol) { return EptSubProtocol(protocol); });
  return *this;
}

/// \ingroup rdmnet_cpp_common
/// \brief A list of EPT client entries.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// EptClientList::GetClientEntries() to copy out the data.
class EptClientList
{
public:
  /// Not default-constructible.
  EptClientList() = delete;
  /// Not copyable - use Save() to create a copyable version.
  EptClientList(const EptClientList& other) = delete;
  /// Not copyable - use Save() to create a copyable version.
  EptClientList& operator=(const EptClientList& other) = delete;

  constexpr EptClientList(const RdmnetEptClientList& c_list) noexcept;

  std::vector<EptClientEntry> GetClientEntries() const;

  constexpr bool more_coming() const noexcept;
  constexpr const RdmnetEptClientEntry* raw_entry_array() const noexcept;
  constexpr size_t raw_entry_array_size() const noexcept;

private:
  const RdmnetEptClientList& list_;
};

/// Construct an EptClientList which references an instance of the C RdmnetEptClientList type.
constexpr EptClientList::EptClientList(const RdmnetEptClientList& c_list) noexcept : list_(c_list)
{
}

/// \brief Copy out the list of client entries.
///
/// This function copies and translates the list delivered to a callback function into C++ native
/// types. These types use C++ heap-allocating containers to store the client entry data and
/// sub-protocol entries.
inline std::vector<EptClientEntry> EptClientList::GetClientEntries() const
{
  std::vector<EptClientEntry> to_return;
  to_return.reserve(list_.num_client_entries);
  std::transform(list_.client_entries, list_.client_entries + list_.num_client_entries, std::back_inserter(to_return),
                 [](const RdmnetEptClientEntry& entry) { return EptClientEntry(entry); });
  return to_return;
}

/// \brief This message contains a partial list.
///
/// This can be set when the library runs out of static memory in which to store Client Entries and
/// must deliver the partial list before continuing. The application should store the entries in
/// the list but should not act on the list until another EptClientList is received with
/// more_coming() == false.
constexpr bool EptClientList::more_coming() const noexcept
{
  return list_.more_coming;
}

/// \brief Get a pointer to the raw array of client entry C structures.
constexpr const RdmnetEptClientEntry* EptClientList::raw_entry_array() const noexcept
{
  return list_.client_entries;
}

/// \brief Get the size of the raw array of client entry C structures.
constexpr size_t EptClientList::raw_entry_array_size() const noexcept
{
  return list_.num_client_entries;
}
};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_EPT_MISC_H_
