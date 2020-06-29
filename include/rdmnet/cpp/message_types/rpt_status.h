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

/// @file rdmnet/cpp/message_types/rpt_status.h
/// @brief Definitions for RPT status message types in RDMnet.

#ifndef RDMNET_CPP_MESSAGE_TYPES_RPT_STATUS_H_
#define RDMNET_CPP_MESSAGE_TYPES_RPT_STATUS_H_

#include <cstdint>
#include <string>
#include "rdm/cpp/uid.h"
#include "rdmnet/common.h"
#include "rdmnet/message.h"

namespace rdmnet
{
class SavedRptStatus;

/// @ingroup rdmnet_cpp_common
/// @brief An RPT status message received over RDMnet and delivered to an RDMnet callback function.
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
  constexpr const char*       status_c_str() const noexcept;
  std::string                 status_string() const;

  const char*    CodeToCString() const noexcept;
  std::string    CodeToString() const;
  constexpr bool HasStatusString() const noexcept;

  constexpr const RdmnetRptStatus& get() const noexcept;

  SavedRptStatus Save() const;

private:
  const RdmnetRptStatus& status_;
};

/// @ingroup rdmnet_cpp_common
/// @brief An RPT status message received over RDMnet and saved for later processing.
class SavedRptStatus
{
public:
  /// Constructs an empty, invalid RPT status by default.
  SavedRptStatus() = default;
  SavedRptStatus(const RdmnetSavedRptStatus& c_status);
  SavedRptStatus& operator=(const RdmnetSavedRptStatus& c_status);
  SavedRptStatus(const RptStatus& status);
  SavedRptStatus& operator=(const RptStatus& status);

  const rdm::Uid& source_uid() const noexcept;
  uint16_t        source_endpoint() const noexcept;
  uint32_t        seq_num() const noexcept;

  rpt_status_code_t  status_code() const noexcept;
  const std::string& status_string() const noexcept;

  bool        IsValid() const noexcept;
  const char* CodeToCString() const noexcept;
  std::string CodeToString() const;
  bool        HasStatusString() const noexcept;

private:
  rdm::Uid          source_uid_;
  uint16_t          source_endpoint_{E133_NULL_ENDPOINT};
  uint32_t          seq_num_{0};
  rpt_status_code_t status_code_{};
  std::string       status_string_;
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

/// @brief Save the data in this status message for later use from a different context.
/// @return A SavedRptStatus containing the copied data.
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
inline const rdm::Uid& SavedRptStatus::source_uid() const noexcept
{
  return source_uid_;
}

/// Get the endpoint from which this RPT status message was sent.
inline uint16_t SavedRptStatus::source_endpoint() const noexcept
{
  return source_endpoint_;
}

/// Get the RDMnet sequence number of this RPT status message, for matching with a corresponding command.
inline uint32_t SavedRptStatus::seq_num() const noexcept
{
  return seq_num_;
}

/// Get the RPT status code of this status message.
inline rpt_status_code_t SavedRptStatus::status_code() const noexcept
{
  return status_code_;
}

/// Get the optional status string accompanying this status message.
inline const std::string& SavedRptStatus::status_string() const noexcept
{
  return status_string_;
}

/// Whether the values contained in this class are valid for an RPT Status message.
inline bool SavedRptStatus::IsValid() const noexcept
{
  return seq_num_ != 0;
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

};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_RPT_STATUS_H_
