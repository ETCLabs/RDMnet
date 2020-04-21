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

/// \file rdmnet/cpp/message_types/llrp_rdm_response.h
/// \brief Definitions for RDMnet LLRP RDM response message types.

#ifndef RDMNET_CPP_MESSAGE_TYPES_LLRP_RDM_RESPONSE_H_
#define RDMNET_CPP_MESSAGE_TYPES_LLRP_RDM_RESPONSE_H_

#include <cstdint>
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/message.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/message.h"

namespace rdmnet
{
namespace llrp
{
class SavedRdmResponse;

/// \ingroup rdmnet_cpp_common
/// \brief An RDM response received over LLRP and delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// RdmResponse::Save() to create a copyable version.
class RdmResponse
{
public:
  /// Not default-constructible.
  RdmResponse() = delete;
  /// Not copyable - use Save() to create a copyable version.
  RdmResponse(const RdmResponse& other) = delete;
  /// Not copyable - use Save() to create a copyable version.
  RdmResponse& operator=(const RdmResponse& other) = delete;

  constexpr RdmResponse(const LlrpRdmResponse& c_resp) noexcept;

  constexpr etcpal::Uuid source_cid() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rdm::Uid source_uid() const noexcept;
  constexpr rdm::Uid dest_uid() const noexcept;
  constexpr rdm_response_type_t response_type() const noexcept;
  constexpr uint16_t subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t param_id() const noexcept;
  constexpr rdm::ResponseHeader rdm_header() const noexcept;
  constexpr const uint8_t* data() const noexcept;
  constexpr uint8_t data_len() const noexcept;

  constexpr bool HasData() const noexcept;

  constexpr bool IsAck() const noexcept;
  constexpr bool IsNack() const noexcept;

  constexpr bool IsGetResponse() const noexcept;
  constexpr bool IsSetResponse() const noexcept;

  constexpr const LlrpRdmResponse& get() const noexcept;

  etcpal::Expected<rdm::NackReason> NackReason() const noexcept;

  rdm::Response ToRdm() const;
  SavedRdmResponse Save() const;

private:
  const LlrpRdmResponse& resp_;
};

/// \ingroup rdmnet_cpp_common
/// \brief An RDM response received over LLRP and saved for later processing.
///
/// This type is not used by the library API, but can come in handy if an application wants to
/// queue or copy RDM responses before acting on them. This type does heap allocation to hold the
/// response parameter data.
class SavedRdmResponse
{
public:
  /// Constructs an empty, invalid RDM response by default.
  SavedRdmResponse() = default;
  SavedRdmResponse(const LlrpSavedRdmResponse& c_resp);
  SavedRdmResponse& operator=(const LlrpSavedRdmResponse& c_resp);
  SavedRdmResponse(const RdmResponse& resp);
  SavedRdmResponse& operator=(const RdmResponse& resp);

  constexpr const etcpal::Uuid& source_cid() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rdm::Uid source_uid() const noexcept;
  constexpr rdm::Uid dest_uid() const noexcept;
  constexpr rdm_response_type_t response_type() const noexcept;
  constexpr uint16_t subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t param_id() const noexcept;
  constexpr const rdm::ResponseHeader& rdm_header() const noexcept;
  const uint8_t* data() const noexcept;
  uint8_t data_len() const noexcept;
  constexpr const rdm::Response& rdm() const noexcept;

  bool IsValid() const noexcept;
  bool HasData() const noexcept;

  constexpr bool IsAck() const noexcept;
  constexpr bool IsNack() const noexcept;

  constexpr bool IsGetResponse() const noexcept;
  constexpr bool IsSetResponse() const noexcept;

  etcpal::Expected<rdm::NackReason> NackReason() const noexcept;

private:
  etcpal::Uuid source_cid_;
  uint32_t seq_num_;
  rdm::Response rdm_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// llrp::RdmResponse function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a RdmResponse copied from an instance of the C LlrpRdmResponse type.
constexpr RdmResponse::RdmResponse(const LlrpRdmResponse& c_resp) noexcept : resp_(c_resp)
{
}

/// Get the CID of the LLRP target that sent this response.
constexpr etcpal::Uuid RdmResponse::source_cid() const noexcept
{
  return resp_.source_cid;
}

/// Get the LLRP sequence number of this response, for matching with a corresponding command.
constexpr uint32_t RdmResponse::seq_num() const noexcept
{
  return resp_.seq_num;
}

/// Get the UID of the LLRP target that sent this response.
constexpr rdm::Uid RdmResponse::source_uid() const noexcept
{
  return resp_.rdm_header.source_uid;
}

/// Get the UID of the LLRP manager to which this response is addressed.
constexpr rdm::Uid RdmResponse::dest_uid() const noexcept
{
  return resp_.rdm_header.dest_uid;
}

/// Get the RDM response type of this response.
constexpr rdm_response_type_t RdmResponse::response_type() const noexcept
{
  return resp_.rdm_header.resp_type;
}

/// Get the RDM subdevice from which this response originated (0 means the root device).
constexpr uint16_t RdmResponse::subdevice() const noexcept
{
  return resp_.rdm_header.subdevice;
}

/// Get the RDM response class of this response.
constexpr rdm_command_class_t RdmResponse::command_class() const noexcept
{
  return resp_.rdm_header.command_class;
}

/// Get the RDM parameter ID (PID) of this response.
constexpr uint16_t RdmResponse::param_id() const noexcept
{
  return resp_.rdm_header.param_id;
}

/// Get the RDM protocol header contained within this response.
constexpr rdm::ResponseHeader RdmResponse::rdm_header() const noexcept
{
  return resp_.rdm_header;
}

/// Get a pointer to the RDM parameter data buffer contained within this response.
constexpr const uint8_t* RdmResponse::data() const noexcept
{
  return resp_.rdm_data;
}

/// Get the length of the RDM parameter data contained within this response.
constexpr uint8_t RdmResponse::data_len() const noexcept
{
  return resp_.rdm_data_len;
}

/// Whether this RDM response includes any RDM parameter data.
constexpr bool RdmResponse::HasData() const noexcept
{
  return (data_len() != 0);
}

/// \brief Whether this command has an RDM response type of ACK.
///
/// If this is false, it implies that IsNack() is true (ACK_TIMER and ACK_OVERFLOW are not allowed
/// in LLRP).
constexpr bool RdmResponse::IsAck() const noexcept
{
  return (resp_.rdm_header.resp_type == kRdmResponseTypeAck);
}

/// \brief Whether this command has an RDM response type of NACK_REASON.
///
/// If this is false, it implies that IsAck() is true (ACK_TIMER and ACK_OVERFLOW are not allowed
/// in LLRP).
constexpr bool RdmResponse::IsNack() const noexcept
{
  return (resp_.rdm_header.resp_type == kRdmResponseTypeNackReason);
}

/// Whether this response is an RDM GET response.
constexpr bool RdmResponse::IsGetResponse() const noexcept
{
  return (resp_.rdm_header.command_class == kRdmCCGetCommandResponse);
}

/// Whether this response is an RDM SET response.
constexpr bool RdmResponse::IsSetResponse() const noexcept
{
  return (resp_.rdm_header.command_class == kRdmCCSetCommandResponse);
}

/// \brief Get the NACK reason code of this RDM response.
/// \return If IsNack(), the valid NackReason instance.
/// \return If !IsNack(), kEtcPalErrInvalid.
inline etcpal::Expected<rdm::NackReason> RdmResponse::NackReason() const noexcept
{
  if (IsNack() && data_len() >= 2)
    return etcpal_unpack_u16b(data());
  else
    return kEtcPalErrInvalid;
}

/// Get a const reference to the underlying C type.
constexpr const LlrpRdmResponse& RdmResponse::get() const noexcept
{
  return resp_;
}

/// Convert the RDM data in this response to an RDM response type.
inline rdm::Response RdmResponse::ToRdm() const
{
  return rdm::Response(resp_.rdm_header, resp_.rdm_data, resp_.rdm_data_len);
}

/// \brief Save the data in this response for later use from a different context.
/// \return A SavedRdmResponse containing the copied data.
inline SavedRdmResponse RdmResponse::Save() const
{
  return SavedRdmResponse(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// llrp::SavedRdmResponse function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a SavedRdmResponse copied from an instance of the C LlrpSavedRdmResponse type.
inline SavedRdmResponse::SavedRdmResponse(const LlrpSavedRdmResponse& c_resp)
    : source_cid_(c_resp.source_cid)
    , seq_num_(c_resp.seq_num)
    , rdm_(c_resp.rdm_header, c_resp.rdm_data, c_resp.rdm_data_len)
{
}

/// Assign an instance of the C LlrpSavedRdmResponse type to an instance of this class.
inline SavedRdmResponse& SavedRdmResponse::operator=(const LlrpSavedRdmResponse& c_resp)
{
  source_cid_ = c_resp.source_cid;
  seq_num_ = c_resp.seq_num;
  rdm_ = rdm::Response(c_resp.rdm_header, c_resp.rdm_data, c_resp.rdm_data_len);
  return *this;
}

/// Construct a SavedRdmResponse from an RdmResponse.
inline SavedRdmResponse::SavedRdmResponse(const RdmResponse& resp)
    : source_cid_(resp.source_cid()), seq_num_(resp.seq_num()), rdm_(resp.ToRdm())
{
}

/// Assign an RdmResponse to an instance of this class.
inline SavedRdmResponse& SavedRdmResponse::operator=(const RdmResponse& resp)
{
  source_cid_ = resp.source_cid();
  seq_num_ = resp.seq_num();
  rdm_ = resp.ToRdm();
  return *this;
}

/// Get the CID of the LLRP target that sent this response.
constexpr const etcpal::Uuid& SavedRdmResponse::source_cid() const noexcept
{
  return source_cid_;
}

/// Get the LLRP sequence number of this response, for matching with a corresponding command.
constexpr uint32_t SavedRdmResponse::seq_num() const noexcept
{
  return seq_num_;
}

/// Get the UID of the LLRP target that sent this response.
constexpr rdm::Uid SavedRdmResponse::source_uid() const noexcept
{
  return rdm_.source_uid();
}

/// Get the UID of the LLRP manager to which this response is addressed.
constexpr rdm::Uid SavedRdmResponse::dest_uid() const noexcept
{
  return rdm_.dest_uid();
}

/// Get the RDM response type of this response.
constexpr rdm_response_type_t SavedRdmResponse::response_type() const noexcept
{
  return rdm_.response_type();
}

/// Get the RDM subdevice from which this response originated (0 means the root device).
constexpr uint16_t SavedRdmResponse::subdevice() const noexcept
{
  return rdm_.subdevice();
}

/// Get the RDM response class of this response.
constexpr rdm_command_class_t SavedRdmResponse::command_class() const noexcept
{
  return rdm_.command_class();
}

/// Get the RDM parameter ID (PID) of this response.
constexpr uint16_t SavedRdmResponse::param_id() const noexcept
{
  return rdm_.param_id();
}

/// Get the RDM protocol header contained within this response.
constexpr const rdm::ResponseHeader& SavedRdmResponse::rdm_header() const noexcept
{
  return rdm_.header();
}

/// Get a pointer to the RDM parameter data buffer contained within this response.
inline const uint8_t* SavedRdmResponse::data() const noexcept
{
  return rdm_.data();
}

/// Get the length of the RDM parameter data contained within this response.
inline uint8_t SavedRdmResponse::data_len() const noexcept
{
  return static_cast<uint8_t>(rdm_.data_len());
}

/// Get the RDM data in this response as an RDM response type.
constexpr const rdm::Response& SavedRdmResponse::rdm() const noexcept
{
  return rdm_;
}

/// \brief Whether the values contained in this response are valid for an RDM response.
/// \details In particular, a default-constructed SavedRdmResponse is not valid.
inline bool SavedRdmResponse::IsValid() const noexcept
{
  return rdm_.IsValid();
}

/// Whether this RDM response includes any RDM parameter data.
inline bool SavedRdmResponse::HasData() const noexcept
{
  return rdm_.HasData();
}

/// \brief Whether this command has an RDM response type of ACK.
///
/// If this is false, it implies that IsNack() is true (ACK_TIMER and ACK_OVERFLOW are not allowed
/// in LLRP).
constexpr bool SavedRdmResponse::IsAck() const noexcept
{
  return rdm_.IsAck();
}

/// \brief Whether this command has an RDM response type of NACK_REASON.
///
/// If this is false, it implies that IsAck() is true (ACK_TIMER and ACK_OVERFLOW are not allowed
/// in LLRP).
constexpr bool SavedRdmResponse::IsNack() const noexcept
{
  return rdm_.IsNack();
}

/// Whether this response is an RDM GET response.
constexpr bool SavedRdmResponse::IsGetResponse() const noexcept
{
  return rdm_.IsGetResponse();
}

/// Whether this response is an RDM SET response.
constexpr bool SavedRdmResponse::IsSetResponse() const noexcept
{
  return rdm_.IsSetResponse();
}

/// \brief Get the NACK reason code of this RDM response.
/// \return If IsNack(), the valid NackReason instance.
/// \return If !IsNack(), kEtcPalErrInvalid.
inline etcpal::Expected<rdm::NackReason> SavedRdmResponse::NackReason() const noexcept
{
  return rdm_.NackReason();
}

};  // namespace llrp
};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_LLRP_RDM_RESPONSE_H_
