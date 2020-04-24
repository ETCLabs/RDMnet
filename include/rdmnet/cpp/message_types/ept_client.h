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

/// \file rdmnet/cpp/message_types/ept_client.h
/// \brief Definitions for EPT client list and client entry message types in RDMnet.

#ifndef RDMNET_CPP_MESSAGE_TYPES_EPT_CLIENT_H_
#define RDMNET_CPP_MESSAGE_TYPES_EPT_CLIENT_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "etcpal/cpp/uuid.h"
#include "rdmnet/message.h"

namespace rdmnet
{
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
  /// Not copyable - use GetClientEntries() to copy out the data.
  EptClientList(const EptClientList& other) = delete;
  /// Not copyable - use GetClientEntries() to copy out the data.
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

#endif  // RDMNET_CPP_MESSAGE_TYPES_EPT_CLIENT_H_
