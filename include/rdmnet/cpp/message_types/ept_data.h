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

/// @file rdmnet/cpp/message_types/ept_data.h
/// @brief Definitions for RDMnet EPT data message types.

#ifndef RDMNET_CPP_MESSAGE_TYPES_EPT_DATA_H_
#define RDMNET_CPP_MESSAGE_TYPES_EPT_DATA_H_

#include <cstdint>
#include <vector>
#include "etcpal/cpp/uuid.h"
#include "rdmnet/message.h"

namespace rdmnet
{
class SavedEptData;

/// @ingroup rdmnet_cpp_common
/// @brief An EPT data message received over RDMnet and delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use EptData::Save()
/// to create a copyable version.
class EptData
{
public:
  /// Not default-constructible.
  EptData() = delete;
  /// Not copyable - use Save() to create a copyable version.
  EptData(const EptData& other) = delete;
  /// Not copyable - use Save() to create a copyable version.
  EptData& operator=(const EptData& other) = delete;

  constexpr EptData(const RdmnetEptData& c_data) noexcept;

  constexpr etcpal::Uuid   source_cid() const noexcept;
  constexpr uint16_t       manufacturer_id() const noexcept;
  constexpr uint16_t       protocol_id() const noexcept;
  constexpr uint32_t       sub_protocol() const noexcept;
  constexpr const uint8_t* data() const noexcept;
  constexpr size_t         data_len() const noexcept;

  std::vector<uint8_t> CopyData() const;

  constexpr const RdmnetEptData& get() const noexcept;

  SavedEptData Save() const;

private:
  const RdmnetEptData& data_;
};

/// @ingroup rdmnet_cpp_common
/// @brief An EPT data message received over RDMnet and saved for later processing.
///
/// This type is not used by the library API, but can come in handy if an application wants to
/// queue or copy EPT data messages before acting on them. This type does heap allocation to hold
/// the data.
class SavedEptData
{
public:
  /// Constructs an empty, invalid EPT data structure by default.
  SavedEptData() = default;
  SavedEptData(const RdmnetSavedEptData& c_data);
  SavedEptData& operator=(const RdmnetSavedEptData& c_data);
  SavedEptData(const EptData& resp);
  SavedEptData& operator=(const EptData& resp);

  const etcpal::Uuid& source_cid() const noexcept;
  uint16_t            manufacturer_id() const noexcept;
  uint16_t            protocol_id() const noexcept;
  uint32_t            sub_protocol() const noexcept;
  const uint8_t*      data() const noexcept;
  size_t              data_len() const noexcept;

  bool IsValid() const noexcept;

private:
  etcpal::Uuid         source_cid_;
  uint16_t             manufacturer_id_;
  uint16_t             protocol_id_;
  std::vector<uint8_t> data_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// EptData function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct an EptData copied from an instance of the C RdmnetEptData type.
constexpr EptData::EptData(const RdmnetEptData& c_data) noexcept : data_(c_data)
{
}

/// Get the CID of the EPT client that sent this data.
constexpr etcpal::Uuid EptData::source_cid() const noexcept
{
  return data_.source_cid;
}

/// Get the ESTA manufacturer ID that identifies the EPT sub-protocol.
constexpr uint16_t EptData::manufacturer_id() const noexcept
{
  return data_.manufacturer_id;
}

/// Get the protocol ID that identifies the EPT sub-protocol.
constexpr uint16_t EptData::protocol_id() const noexcept
{
  return data_.protocol_id;
}

/// @brief Get the full EPT sub-protocol identifier.
/// @details Equivalent to (manufacturer_id() << 16 | protocol_id())
constexpr uint32_t EptData::sub_protocol() const noexcept
{
  return ((static_cast<uint32_t>(data_.manufacturer_id) << 16) | data_.protocol_id);
}

/// Get the data associated with this EPT message.
constexpr const uint8_t* EptData::data() const noexcept
{
  return data_.data;
}

/// Get the length of the data associated with this EPT message.
constexpr size_t EptData::data_len() const noexcept
{
  return data_.data_len;
}

/// @brief Copy the data out of an EPT data message.
/// @return A new vector of bytes representing the EPT data.
inline std::vector<uint8_t> EptData::CopyData() const
{
  if (data_.data && data_.data_len)
    return std::vector<uint8_t>(data_.data, data_.data + data_.data_len);
  else
    return std::vector<uint8_t>{};
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetEptData& EptData::get() const noexcept
{
  return data_;
}

/// @brief Save this data message for later use from a different context.
/// @return A SavedEptData containing the copied data.
inline SavedEptData EptData::Save() const
{
  return SavedEptData(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SavedEptData function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a SavedEptData copied from an instance of the C RdmnetSavedEptData type.
inline SavedEptData::SavedEptData(const RdmnetSavedEptData& c_data)
    : source_cid_(c_data.source_cid), manufacturer_id_(c_data.manufacturer_id), protocol_id_(c_data.protocol_id)
{
  if (c_data.data && c_data.data_len)
    data_.assign(c_data.data, c_data.data + c_data.data_len);
}

/// Assign an instance of the C RdmnetSavedEptData type to an instance of this class.
inline SavedEptData& SavedEptData::operator=(const RdmnetSavedEptData& c_data)
{
  source_cid_ = c_data.source_cid;
  manufacturer_id_ = c_data.manufacturer_id;
  protocol_id_ = c_data.protocol_id;
  if (c_data.data && c_data.data_len)
    data_.assign(c_data.data, c_data.data + c_data.data_len);
  return *this;
}

/// Construct a SavedEptData from an EptData.
inline SavedEptData::SavedEptData(const EptData& resp)
    : source_cid_(resp.source_cid())
    , manufacturer_id_(resp.manufacturer_id())
    , protocol_id_(resp.protocol_id())
    , data_(resp.CopyData())
{
}

/// Assign an EptData to an instance of this class.
inline SavedEptData& SavedEptData::operator=(const EptData& resp)
{
  source_cid_ = resp.source_cid();
  manufacturer_id_ = resp.manufacturer_id();
  protocol_id_ = resp.protocol_id();
  data_ = resp.CopyData();
  return *this;
}

/// Get the CID of the EPT client that sent this data.
inline const etcpal::Uuid& SavedEptData::source_cid() const noexcept
{
  return source_cid_;
}

/// Get the ESTA manufacturer ID that identifies the EPT sub-protocol.
inline uint16_t SavedEptData::manufacturer_id() const noexcept
{
  return manufacturer_id_;
}

/// Get the protocol ID that identifies the EPT sub-protocol.
inline uint16_t SavedEptData::protocol_id() const noexcept
{
  return protocol_id_;
}

/// @brief Get the full EPT sub-protocol identifier.
/// @details Equivalent to (manufacturer_id() << 16 | protocol_id())
inline uint32_t SavedEptData::sub_protocol() const noexcept
{
  return ((static_cast<uint32_t>(manufacturer_id_) << 16) | protocol_id_);
}

/// Get the data associated with this EPT message.
inline const uint8_t* SavedEptData::data() const noexcept
{
  return data_.data();
}

/// Get the length of the data associated with this EPT message.
inline size_t SavedEptData::data_len() const noexcept
{
  return data_.size();
}

/// Whether the values contained in this class are valid for an EPT data message.
inline bool SavedEptData::IsValid() const noexcept
{
  return (!source_cid_.IsNull() && !data_.empty());
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_EPT_DATA_H_
