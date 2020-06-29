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

/// @file rdmnet/cpp/message_types/ept_status.h
/// @brief Definitions for EPT status message types in RDMnet.

#ifndef RDMNET_CPP_MESSAGE_TYPES_EPT_STATUS_H_
#define RDMNET_CPP_MESSAGE_TYPES_EPT_STATUS_H_

#include <string>
#include "etcpal/cpp/uuid.h"
#include "rdmnet/common.h"
#include "rdmnet/message.h"

namespace rdmnet
{
class SavedEptStatus;

/// @ingroup rdmnet_cpp_common
/// @brief An EPT status message received over RDMnet and delivered to an RDMnet callback function.
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

  constexpr etcpal::Uuid      source_cid() const noexcept;
  constexpr ept_status_code_t status_code() const noexcept;
  constexpr const char*       status_c_str() const noexcept;
  std::string                 status_string() const;

  const char*    CodeToCString() const noexcept;
  std::string    CodeToString() const;
  constexpr bool HasStatusString() const noexcept;

  constexpr const RdmnetEptStatus& get() const noexcept;

  SavedEptStatus Save() const;

private:
  const RdmnetEptStatus& status_;
};

/// @ingroup rdmnet_cpp_common
/// @brief An EPT status message received over RDMnet and saved for later processing.
class SavedEptStatus
{
public:
  /// Constructs an empty, invalid EPT status by default.
  SavedEptStatus() = default;
  SavedEptStatus(const RdmnetSavedEptStatus& c_resp);
  SavedEptStatus& operator=(const RdmnetSavedEptStatus& c_resp);
  SavedEptStatus(const EptStatus& status);
  SavedEptStatus& operator=(const EptStatus& status);

  const etcpal::Uuid& source_cid() const noexcept;
  ept_status_code_t   status_code() const noexcept;
  const std::string&  status_string() const noexcept;

  bool        IsValid() const noexcept;
  const char* CodeToCString() const noexcept;
  std::string CodeToString() const;
  bool        HasStatusString() const noexcept;

private:
  etcpal::Uuid      source_cid_;
  ept_status_code_t status_code_;
  std::string       status_string_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// EptStatus function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct an EptStatus from an instance of the C RdmnetEptStatus type.
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

/// @brief Save the data in this status message for later use from a different context.
/// @return A SavedEptStatus containing the copied data.
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
inline const etcpal::Uuid& SavedEptStatus::source_cid() const noexcept
{
  return source_cid_;
}

/// Get the EPT status code of this status message.
inline ept_status_code_t SavedEptStatus::status_code() const noexcept
{
  return status_code_;
}

/// Get the optional status string accompanying this status message.
inline const std::string& SavedEptStatus::status_string() const noexcept
{
  return status_string_;
}

/// Whether the values contained in this class are valid for an EPT status message.
inline bool SavedEptStatus::IsValid() const noexcept
{
  return !source_cid_.IsNull();
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
};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_EPT_STATUS_H_
