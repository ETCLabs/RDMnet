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

/// @file rdmnet/cpp/message_types/rdm_response.h
/// @brief Definitions for RDMnet RDM response message types.

#ifndef RDMNET_CPP_MESSAGE_TYPES_RDM_RESPONSE_H_
#define RDMNET_CPP_MESSAGE_TYPES_RDM_RESPONSE_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include "etcpal/common.h"
#include "etcpal/cpp/error.h"
#include "etcpal/pack.h"
#include "rdm/cpp/message.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/defs.h"
#include "rdmnet/message.h"

namespace rdmnet
{
class SavedRdmResponse;

/// @ingroup rdmnet_cpp_common
/// @brief An RDM response received over RDMnet and delivered to an RDMnet callback function.
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

  constexpr RdmResponse(const RdmnetRdmResponse& c_resp) noexcept;

  constexpr rdm::Uid rdmnet_source_uid() const noexcept;
  constexpr uint16_t source_endpoint() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rdm::Uid           original_cmd_source_uid() const noexcept;
  constexpr rdm::Uid           original_cmd_dest_uid() const noexcept;
  constexpr rdm::CommandHeader original_cmd_header() const noexcept;
  constexpr const uint8_t*     original_cmd_data() const noexcept;
  constexpr uint8_t            original_cmd_data_len() const noexcept;

  constexpr rdm::Uid            rdm_source_uid() const noexcept;
  constexpr rdm::Uid            rdm_dest_uid() const noexcept;
  constexpr rdm_response_type_t response_type() const noexcept;
  constexpr uint16_t            subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t            param_id() const noexcept;
  constexpr rdm::ResponseHeader rdm_header() const noexcept;
  constexpr const uint8_t*      data() const noexcept;
  constexpr size_t              data_len() const noexcept;
  constexpr bool                more_coming() const noexcept;

  constexpr bool OriginalCommandIncluded() const noexcept;
  constexpr bool HasData() const noexcept;
  constexpr bool IsFromDefaultResponder() const noexcept;
  constexpr bool IsResponseToMe() const noexcept;

  constexpr bool IsAck() const noexcept;
  constexpr bool IsNack() const noexcept;

  constexpr bool IsGetResponse() const noexcept;
  constexpr bool IsSetResponse() const noexcept;

  etcpal::Expected<rdm::NackReason> GetNackReason() const noexcept;
  std::vector<uint8_t>              GetData() const;
  std::vector<uint8_t>              GetOriginalCmdData() const;

  constexpr const RdmnetRdmResponse& get() const noexcept;

  rdm::Command     OriginalCommandToRdm() const;
  rdm::Response    ToRdm() const;
  SavedRdmResponse Save() const;

private:
  const RdmnetRdmResponse& resp_;
};

/// @ingroup rdmnet_cpp_common
/// @brief An RDM response received over RDMnet and saved for later processing.
///
/// This type is not used by the library API, but can come in handy if an application wants to
/// queue or copy RDM responses before acting on them. This type does heap allocation to hold the
/// response parameter data.
class SavedRdmResponse
{
public:
  /// Constructs an empty, invalid RDM response by default.
  SavedRdmResponse() = default;
  SavedRdmResponse(const RdmnetSavedRdmResponse& c_resp);
  SavedRdmResponse& operator=(const RdmnetSavedRdmResponse& c_resp);
  SavedRdmResponse(const RdmResponse& resp);
  SavedRdmResponse& operator=(const RdmResponse& resp);

  const rdm::Uid& rdmnet_source_uid() const noexcept;
  uint16_t        source_endpoint() const noexcept;
  uint32_t        seq_num() const noexcept;

  rdm::Uid                  original_cmd_source_uid() const noexcept;
  rdm::Uid                  original_cmd_dest_uid() const noexcept;
  const rdm::CommandHeader& original_cmd_header() const noexcept;
  const uint8_t*            original_cmd_data() const noexcept;
  uint8_t                   original_cmd_data_len() const noexcept;
  const rdm::Command&       original_cmd() const noexcept;

  rdm::Uid                   rdm_source_uid() const noexcept;
  rdm::Uid                   rdm_dest_uid() const noexcept;
  rdm_response_type_t        response_type() const noexcept;
  uint16_t                   subdevice() const noexcept;
  rdm_command_class_t        command_class() const noexcept;
  uint16_t                   param_id() const noexcept;
  const rdm::ResponseHeader& rdm_header() const noexcept;
  const uint8_t*             data() const noexcept;
  size_t                     data_len() const noexcept;
  const rdm::Response&       rdm() const noexcept;

  bool IsValid() const noexcept;
  bool OriginalCommandIncluded() const noexcept;
  bool HasData() const noexcept;
  bool IsFromDefaultResponder() const noexcept;
  bool IsResponseToMe() const noexcept;

  bool IsAck() const noexcept;
  bool IsNack() const noexcept;

  bool IsGetResponse() const noexcept;
  bool IsSetResponse() const noexcept;

  etcpal::Expected<rdm::NackReason> GetNackReason() const noexcept;
  std::vector<uint8_t>              GetData() const;

  void AppendData(const RdmResponse& new_resp);
  void AppendData(const uint8_t* data, size_t size);

private:
  rdm::Uid      rdmnet_source_uid_;
  uint16_t      source_endpoint_{0};
  uint32_t      seq_num_{0};
  bool          is_response_to_me_{false};
  rdm::Command  original_cmd_;
  rdm::Response rdm_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// RdmResponse function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a RdmResponse copied from an instance of the C RdmnetRdmResponse type.
constexpr RdmResponse::RdmResponse(const RdmnetRdmResponse& c_resp) noexcept : resp_(c_resp)
{
}

/// Get the UID of the RDMnet component that sent this response.
constexpr rdm::Uid RdmResponse::rdmnet_source_uid() const noexcept
{
  return resp_.rdmnet_source_uid;
}

/// Get the endpoint from which this response was sent.
constexpr uint16_t RdmResponse::source_endpoint() const noexcept
{
  return resp_.source_endpoint;
}

/// Get the RDMnet sequence number of this response, for matching with a corresponding command.
constexpr uint32_t RdmResponse::seq_num() const noexcept
{
  return resp_.seq_num;
}

/// @brief Get the RDM source UID of the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid RDM source UID.
/// @return If !OriginalCommandIncluded(), an empty/invalid RDM UID.
constexpr rdm::Uid RdmResponse::original_cmd_source_uid() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_header.source_uid : rdm::Uid{});
}

/// @brief Get the RDM destination UID of the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid RDM destination UID.
/// @return If !OriginalCommandIncluded(), an empty/invalid RDM UID.
constexpr rdm::Uid RdmResponse::original_cmd_dest_uid() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_header.dest_uid : rdm::Uid{});
}

/// @brief Get the RDM protocol header of the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid RDM header.
/// @return If !OriginalCommandIncluded(), an empty/invalid RDM header.
constexpr rdm::CommandHeader RdmResponse::original_cmd_header() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_header : rdm::CommandHeader{});
}

/// @brief Get the RDM parameter data of the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid RDM parameter data.
/// @return If !OriginalCommandIncluded(), nullptr.
constexpr const uint8_t* RdmResponse::original_cmd_data() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_data : nullptr);
}

/// @brief Get the length of the RDM parameter data accompanying the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid length.
/// @return If !OriginalCommandIncluded(), 0.
constexpr uint8_t RdmResponse::original_cmd_data_len() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_data_len : 0);
}

/// Get the UID of the RDM responder that sent this response.
constexpr rdm::Uid RdmResponse::rdm_source_uid() const noexcept
{
  return resp_.rdm_header.source_uid;
}

/// Get the UID of the RDM controller to which this response is addressed.
constexpr rdm::Uid RdmResponse::rdm_dest_uid() const noexcept
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
constexpr size_t RdmResponse::data_len() const noexcept
{
  return resp_.rdm_data_len;
}

/// @brief This message contains partial RDM data.
///
/// This can be set when the library runs out of static memory in which to store RDM response data
/// and must deliver a partial data buffer before continuing (this only applies to the data buffer
/// within the RDM response). The application should store the partial data but should not act on
/// it until another RdmResponse is received with more_coming set to false.
constexpr bool RdmResponse::more_coming() const noexcept
{
  return resp_.more_coming;
}

/// @brief Whether the original RDM command is included.
///
/// In RDMnet, a response to an RDM command includes the original command data. An exception to
/// this rule is unsolicited RDM responses, which are not in response to a command and thus do not
/// include the original command data.
constexpr bool RdmResponse::OriginalCommandIncluded() const noexcept
{
  return (resp_.seq_num != 0);
}

/// Whether this RDM response includes any RDM parameter data.
constexpr bool RdmResponse::HasData() const noexcept
{
  return (data_len() != 0);
}

/// @brief Whether this RDM response is from a default responder.
/// @details See @ref devices_and_gateways for more information.
constexpr bool RdmResponse::IsFromDefaultResponder() const noexcept
{
  return (resp_.source_endpoint == E133_NULL_ENDPOINT);
}

/// @brief Whether the response was sent in response to a command previously sent by this controller.
/// @details If this is false, the command was a broadcast sent to all controllers.
constexpr bool RdmResponse::IsResponseToMe() const noexcept
{
  return resp_.is_response_to_me;
}

/// @brief Whether this command has an RDM response type of ACK.
///
/// If this is false, it implies that IsNack() is true (ACK_TIMER is not allowed in RDMnet, and the
/// library recombines ACK_OVERFLOW responses automatically).
constexpr bool RdmResponse::IsAck() const noexcept
{
  return (resp_.rdm_header.resp_type == kRdmResponseTypeAck);
}

/// @brief Whether this command has an RDM response type of NACK_REASON.
///
/// If this is false, it implies that IsAck() is true (ACK_TIMER is not allowed in RDMnet, and the
/// library recombines ACK_OVERFLOW responses automatically).
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

/// @brief Get the NACK reason code of this RDM response.
/// @return If IsNack(), the valid NackReason instance.
/// @return If !IsNack(), kEtcPalErrInvalid.
inline etcpal::Expected<rdm::NackReason> RdmResponse::GetNackReason() const noexcept
{
  if (IsNack() && data_len() >= 2)
    return etcpal_unpack_u16b(data());
  else
    return kEtcPalErrInvalid;
}

/// @brief Copy out the data in a RdmResponse.
/// @return A copied vector containing any parameter data associated with this response.
inline std::vector<uint8_t> RdmResponse::GetData() const
{
  return std::vector<uint8_t>(resp_.rdm_data, resp_.rdm_data + resp_.rdm_data_len);
}

/// @brief Copy out the original RDM command data in a RdmResponse.
/// @return A copied vector containing the parameter data associated with the original RDM command
///         that generated this response.
inline std::vector<uint8_t> RdmResponse::GetOriginalCmdData() const
{
  return std::vector<uint8_t>(resp_.original_cmd_data, resp_.original_cmd_data + resp_.original_cmd_data_len);
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetRdmResponse& RdmResponse::get() const noexcept
{
  return resp_;
}

/// @brief Convert the original RDM command associated with this response to an RDM command type.
/// @return If OriginalCommandIncluded(), the valid RDM command.
/// @return If !OriginalCommandIncluded(), an empty/invalid RDM command.
inline rdm::Command RdmResponse::OriginalCommandToRdm() const
{
  return (OriginalCommandIncluded()
              ? rdm::Command(resp_.original_cmd_header, resp_.original_cmd_data, resp_.original_cmd_data_len)
              : rdm::Command{});
}

/// Convert the RDM data in this response to an RDM response type.
inline rdm::Response RdmResponse::ToRdm() const
{
  return rdm::Response(resp_.rdm_header, resp_.rdm_data, resp_.rdm_data_len);
}

/// @brief Save the data in this response for later use from a different context.
/// @return A SavedRdmResponse containing the copied data.
inline SavedRdmResponse RdmResponse::Save() const
{
  return SavedRdmResponse(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SavedRdmResponse function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a SavedRdmResponse copied from an instance of the C RdmnetSavedRdmResponse type.
inline SavedRdmResponse::SavedRdmResponse(const RdmnetSavedRdmResponse& c_resp)
    : rdmnet_source_uid_(c_resp.rdmnet_source_uid)
    , source_endpoint_(c_resp.source_endpoint)
    , seq_num_(c_resp.seq_num)
    , original_cmd_(c_resp.original_cmd_header, c_resp.original_cmd_data, c_resp.original_cmd_data_len)
    , rdm_(c_resp.rdm_header, c_resp.rdm_data, c_resp.rdm_data_len)
{
}

/// Assign an instance of the C RdmnetSavedRdmResponse type to an instance of this class.
inline SavedRdmResponse& SavedRdmResponse::operator=(const RdmnetSavedRdmResponse& c_resp)
{
  rdmnet_source_uid_ = c_resp.rdmnet_source_uid;
  source_endpoint_ = c_resp.source_endpoint;
  seq_num_ = c_resp.seq_num;
  original_cmd_ = rdm::Command(c_resp.original_cmd_header, c_resp.original_cmd_data, c_resp.original_cmd_data_len);
  rdm_ = rdm::Response(c_resp.rdm_header, c_resp.rdm_data, c_resp.rdm_data_len);
  return *this;
}

/// Construct a SavedRdmResponse from an RdmResponse.
inline SavedRdmResponse::SavedRdmResponse(const RdmResponse& resp)
    : rdmnet_source_uid_(resp.rdmnet_source_uid())
    , source_endpoint_(resp.source_endpoint())
    , seq_num_(resp.seq_num())
    , original_cmd_(resp.OriginalCommandToRdm())
    , rdm_(resp.ToRdm())
{
}

/// Assign an RdmResponse to an instance of this class.
inline SavedRdmResponse& SavedRdmResponse::operator=(const RdmResponse& resp)
{
  rdmnet_source_uid_ = resp.rdmnet_source_uid();
  source_endpoint_ = resp.source_endpoint();
  seq_num_ = resp.seq_num();
  original_cmd_ = resp.OriginalCommandToRdm();
  rdm_ = resp.ToRdm();
  return *this;
}

/// Get the UID of the RDMnet component that sent this response.
inline const rdm::Uid& SavedRdmResponse::rdmnet_source_uid() const noexcept
{
  return rdmnet_source_uid_;
}

/// Get the endpoint from which this response was sent.
inline uint16_t SavedRdmResponse::source_endpoint() const noexcept
{
  return source_endpoint_;
}

/// Get the RDMnet sequence number of this response, for matching with a corresponding command.
inline uint32_t SavedRdmResponse::seq_num() const noexcept
{
  return seq_num_;
}

/// @brief Get the RDM source UID of the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid RDM source UID.
/// @return If !OriginalCommandIncluded(), an empty/invalid RDM UID.
inline rdm::Uid SavedRdmResponse::original_cmd_source_uid() const noexcept
{
  return (OriginalCommandIncluded() ? original_cmd_.source_uid() : rdm::Uid{});
}

/// @brief Get the RDM destination UID of the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid RDM destination UID.
/// @return If !OriginalCommandIncluded(), an empty/invalid RDM UID.
inline rdm::Uid SavedRdmResponse::original_cmd_dest_uid() const noexcept
{
  return (OriginalCommandIncluded() ? original_cmd_.dest_uid() : rdm::Uid{});
}

/// @brief Get the RDM protocol header of the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid RDM header.
/// @return If !OriginalCommandIncluded(), an empty/invalid RDM header.
inline const rdm::CommandHeader& SavedRdmResponse::original_cmd_header() const noexcept
{
  return original_cmd_.header();
}

/// @brief Get the RDM parameter data of the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid RDM parameter data.
/// @return If !OriginalCommandIncluded(), nullptr.
inline const uint8_t* SavedRdmResponse::original_cmd_data() const noexcept
{
  return (OriginalCommandIncluded() ? original_cmd_.data() : nullptr);
}

/// @brief Get the length of the RDM parameter data accompanying the original RDM command, if available.
/// @return If OriginalCommandIncluded(), the valid length.
/// @return If !OriginalCommandIncluded(), 0.
inline uint8_t SavedRdmResponse::original_cmd_data_len() const noexcept
{
  return (OriginalCommandIncluded() ? original_cmd_.data_len() : 0);
}

/// @brief Get the original RDM command that resulted in this RDM response, if available.
/// @return If OriginalCommandIncluded(), the valid RDM command.
/// return If !OriginalCommandIncluded(), an empty/invalid RDM command.
inline const rdm::Command& SavedRdmResponse::original_cmd() const noexcept
{
  return original_cmd_;
}

/// Get the UID of the RDM responder that sent this response.
inline rdm::Uid SavedRdmResponse::rdm_source_uid() const noexcept
{
  return rdm_.source_uid();
}

/// Get the UID of the RDM controller to which this response is addressed.
inline rdm::Uid SavedRdmResponse::rdm_dest_uid() const noexcept
{
  return rdm_.dest_uid();
}

/// Get the RDM response type of this response.
inline rdm_response_type_t SavedRdmResponse::response_type() const noexcept
{
  return rdm_.response_type();
}

/// Get the RDM subdevice from which this response originated (0 means the root device).
inline uint16_t SavedRdmResponse::subdevice() const noexcept
{
  return rdm_.subdevice();
}

/// Get the RDM response class of this response.
inline rdm_command_class_t SavedRdmResponse::command_class() const noexcept
{
  return rdm_.command_class();
}

/// Get the RDM parameter ID (PID) of this response.
inline uint16_t SavedRdmResponse::param_id() const noexcept
{
  return rdm_.param_id();
}

/// Get the RDM protocol header contained within this response.
inline const rdm::ResponseHeader& SavedRdmResponse::rdm_header() const noexcept
{
  return rdm_.header();
}

/// Get a pointer to the RDM parameter data buffer contained within this response.
inline const uint8_t* SavedRdmResponse::data() const noexcept
{
  return rdm_.data();
}

/// Get the length of the RDM parameter data contained within this response.
inline size_t SavedRdmResponse::data_len() const noexcept
{
  return rdm_.data_len();
}

/// Get the RDM data in this response as an RDM response type.
inline const rdm::Response& SavedRdmResponse::rdm() const noexcept
{
  return rdm_;
}

/// @brief Whether the values contained in this response are valid for an RDM response.
/// @details In particular, a default-constructed SavedRdmResponse is not valid.
inline bool SavedRdmResponse::IsValid() const noexcept
{
  return rdm_.IsValid();
}

/// @brief Whether the original RDM command is included.
///
/// In RDMnet, a response to an RDM command includes the original command data. An exception to
/// this rule is unsolicited RDM responses, which are not in response to a command and thus do not
/// include the original command data.
inline bool SavedRdmResponse::OriginalCommandIncluded() const noexcept
{
  return (seq_num_ != 0);
}

/// Whether this RDM response includes any RDM parameter data.
inline bool SavedRdmResponse::HasData() const noexcept
{
  return rdm_.HasData();
}

/// @brief Whether this RDM response is from a default responder.
/// @details See @ref devices_and_gateways for more information.
inline bool SavedRdmResponse::IsFromDefaultResponder() const noexcept
{
  return (source_endpoint_ == E133_NULL_ENDPOINT);
}

/// @brief Whether the response was sent in response to a command previously sent by this controller.
/// @details If this is false, the command was a broadcast sent to all controllers.
inline bool SavedRdmResponse::IsResponseToMe() const noexcept
{
  return is_response_to_me_;
}

/// @brief Whether this command has an RDM response type of ACK.
///
/// If this is false, it implies that IsNack() is true (ACK_TIMER is not allowed in RDMnet, and the
/// library recombines ACK_OVERFLOW responses automatically).
inline bool SavedRdmResponse::IsAck() const noexcept
{
  return rdm_.IsAck();
}

/// @brief Whether this command has an RDM response type of NACK_REASON.
///
/// If this is false, it implies that IsAck() is true (ACK_TIMER is not allowed in RDMnet, and the
/// library recombines ACK_OVERFLOW responses automatically).
inline bool SavedRdmResponse::IsNack() const noexcept
{
  return rdm_.IsNack();
}

/// Whether this response is an RDM GET response.
inline bool SavedRdmResponse::IsGetResponse() const noexcept
{
  return rdm_.IsGetResponse();
}

/// Whether this response is an RDM SET response.
inline bool SavedRdmResponse::IsSetResponse() const noexcept
{
  return rdm_.IsSetResponse();
}

/// @brief Get the NACK reason code of this RDM response.
/// @return If IsNack(), the valid NackReason instance.
/// @return If !IsNack(), kEtcPalErrInvalid.
inline etcpal::Expected<rdm::NackReason> SavedRdmResponse::GetNackReason() const noexcept
{
  return rdm_.GetNackReason();
}

/// @brief Copy out the data in a SavedRdmResponse.
/// @return A copied vector containing any parameter data associated with this response.
inline std::vector<uint8_t> SavedRdmResponse::GetData() const
{
  return rdm_.GetData();
}

/// @brief Append more data to this response's parameter data.
/// @param new_resp An RdmResponse delivered to an RDMnet callback function as a continuation of a previous response.
inline void SavedRdmResponse::AppendData(const RdmResponse& new_resp)
{
  rdm_.AppendData(new_resp.data(), new_resp.data_len());
}

/// @brief Append more data to this response's parameter data.
/// @param data Pointer to data buffer to append.
/// @param data_len Size of data buffer to append.
inline void SavedRdmResponse::AppendData(const uint8_t* data, size_t data_len)
{
  rdm_.AppendData(data, data_len);
}
};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_RDM_RESPONSE_H_
