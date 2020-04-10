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

/// \file rdmnet/cpp/message.h

#ifndef RDMNET_CPP_MESSAGE_H_
#define RDMNET_CPP_MESSAGE_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "etcpal/cpp/common.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/pack.h"
#include "rdm/cpp/uid.h"
#include "rdm/cpp/message.h"
#include "rdmnet/common.h"
#include "rdmnet/llrp.h"
#include "rdmnet/message.h"

namespace rdmnet
{
///////////////////////////////////////////////////////////////////////////////////////////////////
// RDMnet RDM Command message types
///////////////////////////////////////////////////////////////////////////////////////////////////

class SavedRdmCommand;

/// \ingroup rdmnet_cpp_common
/// \brief An RDM command received over RDMnet and delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// RdmCommand::Save() to create a copyable version.
class RdmCommand
{
public:
  /// Not default-constructible.
  RdmCommand() = delete;
  /// Not copyable - use Save() to create a copyable version.
  RdmCommand(const RdmCommand& other) = delete;
  /// Not copyable - use Save() to create a copyable version.
  RdmCommand& operator=(const RdmCommand& other) = delete;

  constexpr RdmCommand(const RdmnetRdmCommand& c_cmd) noexcept;

  constexpr rdm::Uid rdmnet_source_uid() const noexcept;
  constexpr uint16_t dest_endpoint() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rdm::Uid rdm_source_uid() const noexcept;
  constexpr rdm::Uid rdm_dest_uid() const noexcept;
  constexpr uint16_t subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t param_id() const noexcept;

  constexpr rdm::CommandHeader rdm_header() const noexcept;

  constexpr const uint8_t* data() const noexcept;
  constexpr uint8_t data_len() const noexcept;

  constexpr bool HasData() const noexcept;
  constexpr bool IsToDefaultResponder() const noexcept;

  constexpr bool IsGet() const noexcept;
  constexpr bool IsSet() const noexcept;

  constexpr const RdmnetRdmCommand& get() const noexcept;

  rdm::Command ToRdm() const;
  SavedRdmCommand Save() const;

private:
  const RdmnetRdmCommand& cmd_;
};

/// \ingroup rdmnet_cpp_common
/// \brief An RDM command received over RDMnet by a local component and saved for a later response.
class SavedRdmCommand
{
public:
  /// Construct an empty, invalid SavedRdmCommand by default.
  SavedRdmCommand() = default;
  constexpr SavedRdmCommand(const RdmnetSavedRdmCommand& c_cmd) noexcept;
  SavedRdmCommand& operator=(const RdmnetSavedRdmCommand& c_cmd) noexcept;
  SavedRdmCommand(const RdmCommand& command) noexcept;
  SavedRdmCommand& operator=(const RdmCommand& command) noexcept;

  constexpr rdm::Uid rdmnet_source_uid() const noexcept;
  constexpr uint16_t dest_endpoint() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rdm::Uid rdm_source_uid() const noexcept;
  constexpr rdm::Uid rdm_dest_uid() const noexcept;
  constexpr uint16_t subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t param_id() const noexcept;

  constexpr rdm::CommandHeader rdm_header() const noexcept;

  constexpr const uint8_t* data() const noexcept;
  constexpr uint8_t data_len() const noexcept;

  bool IsValid() const noexcept;
  constexpr bool HasData() const noexcept;
  constexpr bool IsToDefaultResponder() const noexcept;

  constexpr bool IsGet() const noexcept;
  constexpr bool IsSet() const noexcept;

  ETCPAL_CONSTEXPR_14 RdmnetSavedRdmCommand& get() noexcept;
  constexpr const RdmnetSavedRdmCommand& get() const noexcept;

  rdm::Command ToRdm() const;

private:
  RdmnetSavedRdmCommand cmd_{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// RdmCommand function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct an RdmCommand which references an instance of the C RdmnetRdmCommand type.
constexpr RdmCommand::RdmCommand(const RdmnetRdmCommand& c_cmd) noexcept : cmd_(c_cmd)
{
}

/// Get the UID of the RDMnet controller that sent this command.
constexpr rdm::Uid RdmCommand::rdmnet_source_uid() const noexcept
{
  return cmd_.rdmnet_source_uid;
}

/// Get the endpoint to which this command is addressed.
constexpr uint16_t RdmCommand::dest_endpoint() const noexcept
{
  return cmd_.dest_endpoint;
}

/// Get the RDMnet sequence number of this command.
constexpr uint32_t RdmCommand::seq_num() const noexcept
{
  return cmd_.seq_num;
}

/// Get the UID of the RDM controller that has sent this command.
constexpr rdm::Uid RdmCommand::rdm_source_uid() const noexcept
{
  return cmd_.rdm_header.source_uid;
}

/// Get the UID of the RDM responder to which this command is addressed.
constexpr rdm::Uid RdmCommand::rdm_dest_uid() const noexcept
{
  return cmd_.rdm_header.dest_uid;
}

/// Get the RDM subdevice to which this command is addressed (0 means the root device).
constexpr uint16_t RdmCommand::subdevice() const noexcept
{
  return cmd_.rdm_header.subdevice;
}

/// Get the RDM command class of this command.
constexpr rdm_command_class_t RdmCommand::command_class() const noexcept
{
  return cmd_.rdm_header.command_class;
}

/// Get the RDM parameter ID (PID) of this command.
constexpr uint16_t RdmCommand::param_id() const noexcept
{
  return cmd_.rdm_header.param_id;
}

/// Get the RDM protocol header contained within this command.
constexpr rdm::CommandHeader RdmCommand::rdm_header() const noexcept
{
  return cmd_.rdm_header;
}

/// Get a pointer to the RDM parameter data buffer contained within this command.
constexpr const uint8_t* RdmCommand::data() const noexcept
{
  return cmd_.data;
}

/// Get the length of the RDM parameter data contained within this command.
constexpr uint8_t RdmCommand::data_len() const noexcept
{
  return cmd_.data_len;
}

/// Whether this command has any associated RDM parameter data.
constexpr bool RdmCommand::HasData() const noexcept
{
  return (data_len() != 0);
}

/// Whether this command is an RDM GET command.
constexpr bool RdmCommand::IsGet() const noexcept
{
  return (cmd_.rdm_header.command_class == kRdmCCGetCommand);
}

/// Whether this command is an RDM SET command.
constexpr bool RdmCommand::IsSet() const noexcept
{
  return (cmd_.rdm_header.command_class == kRdmCCSetCommand);
}

/// \brief Whether this command is addressed to the RDMnet default responder.
/// \details See \ref devices_and_gateways for more information.
constexpr bool RdmCommand::IsToDefaultResponder() const noexcept
{
  return (cmd_.dest_endpoint == E133_NULL_ENDPOINT);
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetRdmCommand& RdmCommand::get() const noexcept
{
  return cmd_;
}

/// Convert the RDM data in this command to an RDM command type.
inline rdm::Command RdmCommand::ToRdm() const
{
  return rdm::Command(cmd_.rdm_header, cmd_.data, cmd_.data_len);
}

/// \brief Save the data in this command for later use with API functions from a different context.
/// \return A SavedRdmCommand containing the copied data.
inline SavedRdmCommand RdmCommand::Save() const
{
  return SavedRdmCommand(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SavedRdmCommand function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a SavedRdmCommand copied from an instance of the C RdmnetSavedRdmCommand type.
constexpr SavedRdmCommand::SavedRdmCommand(const RdmnetSavedRdmCommand& c_cmd) noexcept : cmd_(c_cmd)
{
}

/// Assign an instance of the C RdmnetSavedRdmCommand type to an instance of this class.
inline SavedRdmCommand& SavedRdmCommand::operator=(const RdmnetSavedRdmCommand& c_cmd) noexcept
{
  cmd_ = c_cmd;
  return *this;
}

/// Construct a SavedRdmCommand from an RdmCommand.
inline SavedRdmCommand::SavedRdmCommand(const RdmCommand& command) noexcept
{
  rdmnet_save_rdm_command(&command.get(), &cmd_);
}

/// Assign an RdmCommand to an instance of this class.
inline SavedRdmCommand& SavedRdmCommand::operator=(const RdmCommand& command) noexcept
{
  rdmnet_save_rdm_command(&command.get(), &cmd_);
}

/// Get the UID of the RDMnet controller that sent this command.
constexpr rdm::Uid SavedRdmCommand::rdmnet_source_uid() const noexcept
{
  return cmd_.rdmnet_source_uid;
}

/// Get the endpoint to which this command is addressed.
constexpr uint16_t SavedRdmCommand::dest_endpoint() const noexcept
{
  return cmd_.dest_endpoint;
}

/// Get the RDMnet sequence number of this command.
constexpr uint32_t SavedRdmCommand::seq_num() const noexcept
{
  return cmd_.seq_num;
}

/// Get the UID of the RDM controller that sent this command.
constexpr rdm::Uid SavedRdmCommand::rdm_source_uid() const noexcept
{
  return cmd_.rdm_header.source_uid;
}

/// Get the UID of the RDM responder to which this command is addressed.
constexpr rdm::Uid SavedRdmCommand::rdm_dest_uid() const noexcept
{
  return cmd_.rdm_header.dest_uid;
}

/// Get the RDM subdevice to which this command is addressed (0 means the root device).
constexpr uint16_t SavedRdmCommand::subdevice() const noexcept
{
  return cmd_.rdm_header.subdevice;
}

/// Get the RDM command class of this command.
constexpr rdm_command_class_t SavedRdmCommand::command_class() const noexcept
{
  return cmd_.rdm_header.command_class;
}

/// Get the RDM parameter ID (PID) of this command.
constexpr uint16_t SavedRdmCommand::param_id() const noexcept
{
  return cmd_.rdm_header.param_id;
}

/// Get the RDM protocol header contained within this command.
constexpr rdm::CommandHeader SavedRdmCommand::rdm_header() const noexcept
{
  return cmd_.rdm_header;
}

/// Get a pointer to the RDM parameter data buffer contained within this command.
constexpr const uint8_t* SavedRdmCommand::data() const noexcept
{
  return cmd_.data;
}

/// Get the length of the RDM parameter data contained within this command.
constexpr uint8_t SavedRdmCommand::data_len() const noexcept
{
  return cmd_.data_len;
}

/// \brief Whether the values contained in this command are valid for an RDM command.
/// \details In particular, a default-constructed SavedRdmCommand is not valid.
inline bool SavedRdmCommand::IsValid() const noexcept
{
  return rdm_command_header_is_valid(&cmd_.rdm_header);
}

/// Whether this command has any associated RDM parameter data.
constexpr bool SavedRdmCommand::HasData() const noexcept
{
  return (data_len() != 0);
}

/// \brief Whether this command is addressed to the RDMnet default responder.
/// \details See \ref devices_and_gateways for more information.
constexpr bool SavedRdmCommand::IsToDefaultResponder() const noexcept
{
  return (cmd_.dest_endpoint == E133_NULL_ENDPOINT);
}

/// Whether this command is an RDM GET command.
constexpr bool SavedRdmCommand::IsGet() const noexcept
{
  return (cmd_.rdm_header.command_class == kRdmCCGetCommand);
}

/// Whether this command is an RDM SET command.
constexpr bool SavedRdmCommand::IsSet() const noexcept
{
  return (cmd_.rdm_header.command_class == kRdmCCSetCommand);
}

/// Get a mutable reference to the underlying C type.
ETCPAL_CONSTEXPR_14_OR_INLINE RdmnetSavedRdmCommand& SavedRdmCommand::get() noexcept
{
  return cmd_;
}

/// Get a const reference to the underlying C type.
constexpr const RdmnetSavedRdmCommand& SavedRdmCommand::get() const noexcept
{
  return cmd_;
}

/// Convert the RDM data in this command to an RDM command type.
inline rdm::Command SavedRdmCommand::ToRdm() const
{
  return rdm::Command(cmd_.rdm_header, cmd_.data, cmd_.data_len);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// RDMnet RDM response message types
///////////////////////////////////////////////////////////////////////////////////////////////////

class SavedRdmResponse;

/// \ingroup rdmnet_cpp_common
/// \brief An RDM response received over RDMnet and delivered to an RDMnet callback function.
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

  constexpr rdm::Uid original_cmd_source_uid() const noexcept;
  constexpr rdm::Uid original_cmd_dest_uid() const noexcept;
  constexpr rdm::CommandHeader original_cmd_header() const noexcept;
  constexpr const uint8_t* original_cmd_data() const noexcept;
  constexpr uint8_t original_cmd_data_len() const noexcept;

  constexpr rdm::Uid rdm_source_uid() const noexcept;
  constexpr rdm::Uid rdm_dest_uid() const noexcept;
  constexpr rdm_response_type_t response_type() const noexcept;
  constexpr uint16_t subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t param_id() const noexcept;
  constexpr rdm::ResponseHeader rdm_header() const noexcept;
  constexpr const uint8_t* data() const noexcept;
  constexpr size_t data_len() const noexcept;

  constexpr bool OriginalCommandIncluded() const noexcept;
  constexpr bool HasData() const noexcept;
  constexpr bool IsFromDefaultResponder() const noexcept;

  constexpr bool IsAck() const noexcept;
  constexpr bool IsNack() const noexcept;

  constexpr bool IsGetResponse() const noexcept;
  constexpr bool IsSetResponse() const noexcept;

  etcpal::Expected<rdm::NackReason> NackReason() const noexcept;

  constexpr const RdmnetRdmResponse& get() const noexcept;

  rdm::Command OriginalCommandToRdm() const;
  rdm::Response ToRdm() const;
  SavedRdmResponse Save() const;

private:
  const RdmnetRdmResponse& resp_;
};

/// \ingroup rdmnet_cpp_common
/// \brief An RDM response received over RDMnet and saved for later processing.
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

  constexpr const rdm::Uid& rdmnet_source_uid() const noexcept;
  constexpr uint16_t source_endpoint() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rdm::Uid original_cmd_source_uid() const noexcept;
  constexpr rdm::Uid original_cmd_dest_uid() const noexcept;
  constexpr const rdm::CommandHeader& original_cmd_header() const noexcept;
  constexpr const uint8_t* original_cmd_data() const noexcept;
  constexpr uint8_t original_cmd_data_len() const noexcept;
  constexpr const rdm::Command& original_cmd() const noexcept;

  constexpr rdm::Uid rdm_source_uid() const noexcept;
  constexpr rdm::Uid rdm_dest_uid() const noexcept;
  constexpr rdm_response_type_t response_type() const noexcept;
  constexpr uint16_t subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t param_id() const noexcept;
  constexpr const rdm::ResponseHeader& rdm_header() const noexcept;
  const uint8_t* data() const noexcept;
  size_t data_len() const noexcept;
  constexpr const rdm::Response& rdm() const noexcept;

  bool IsValid() const noexcept;
  constexpr bool OriginalCommandIncluded() const noexcept;
  bool HasData() const noexcept;
  constexpr bool IsFromDefaultResponder() const noexcept;

  constexpr bool IsAck() const noexcept;
  constexpr bool IsNack() const noexcept;

  constexpr bool IsGetResponse() const noexcept;
  constexpr bool IsSetResponse() const noexcept;

  etcpal::Expected<rdm::NackReason> NackReason() const noexcept;

private:
  rdm::Uid rdmnet_source_uid_;
  uint16_t source_endpoint_{0};
  uint32_t seq_num_{0};
  rdm::Command original_cmd_;
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

/// \brief Get the RDM source UID of the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid RDM source UID.
/// \return If !OriginalCommandIncluded(), an empty/invalid RDM UID.
constexpr rdm::Uid RdmResponse::original_cmd_source_uid() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_header.source_uid : rdm::Uid{});
}

/// \brief Get the RDM destination UID of the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid RDM destination UID.
/// \return If !OriginalCommandIncluded(), an empty/invalid RDM UID.
constexpr rdm::Uid RdmResponse::original_cmd_dest_uid() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_header.dest_uid : rdm::Uid{});
}

/// \brief Get the RDM protocol header of the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid RDM header.
/// \return If !OriginalCommandIncluded(), an empty/invalid RDM header.
constexpr rdm::CommandHeader RdmResponse::original_cmd_header() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_header : rdm::CommandHeader{});
}

/// \brief Get the RDM parameter data of the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid RDM parameter data.
/// \return If !OriginalCommandIncluded(), nullptr.
constexpr const uint8_t* RdmResponse::original_cmd_data() const noexcept
{
  return (OriginalCommandIncluded() ? resp_.original_cmd_data : nullptr);
}

/// \brief Get the length of the RDM parameter data accompanying the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid length.
/// \return If !OriginalCommandIncluded(), 0.
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

/// \brief Whether the original RDM command is included.
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

/// \brief Whether this RDM response is from a default responder.
/// \details See \ref devices_and_gateways for more information.
constexpr bool RdmResponse::IsFromDefaultResponder() const noexcept
{
  return (resp_.source_endpoint == E133_NULL_ENDPOINT);
}

/// \brief Whether this command has an RDM response type of ACK.
///
/// If this is false, it implies that IsNack() is true (ACK_TIMER is not allowed in RDMnet, and the
/// library recombines ACK_OVERFLOW responses automatically).
constexpr bool RdmResponse::IsAck() const noexcept
{
  return (resp_.rdm_header.resp_type == kRdmResponseTypeAck);
}

/// \brief Whether this command has an RDM response type of NACK_REASON.
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
constexpr const RdmnetRdmResponse& RdmResponse::get() const noexcept
{
  return resp_;
}

/// \brief Convert the original RDM command associated with this response to an RDM command type.
/// \return If OriginalCommandIncluded(), the valid RDM command.
/// \return If !OriginalCommandIncluded(), an empty/invalid RDM command.
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

/// \brief Save the data in this response for later use from a different context.
/// \return A SavedRdmResponse containing the copied data.
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
constexpr const rdm::Uid& SavedRdmResponse::rdmnet_source_uid() const noexcept
{
  return rdmnet_source_uid_;
}

/// Get the endpoint from which this response was sent.
constexpr uint16_t SavedRdmResponse::source_endpoint() const noexcept
{
  return source_endpoint_;
}

/// Get the RDMnet sequence number of this response, for matching with a corresponding command.
constexpr uint32_t SavedRdmResponse::seq_num() const noexcept
{
  return seq_num_;
}

/// \brief Get the RDM source UID of the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid RDM source UID.
/// \return If !OriginalCommandIncluded(), an empty/invalid RDM UID.
constexpr rdm::Uid SavedRdmResponse::original_cmd_source_uid() const noexcept
{
  return (OriginalCommandIncluded() ? original_cmd_.source_uid() : rdm::Uid{});
}

/// \brief Get the RDM destination UID of the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid RDM destination UID.
/// \return If !OriginalCommandIncluded(), an empty/invalid RDM UID.
constexpr rdm::Uid SavedRdmResponse::original_cmd_dest_uid() const noexcept
{
  return (OriginalCommandIncluded() ? original_cmd_.dest_uid() : rdm::Uid{});
}

/// \brief Get the RDM protocol header of the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid RDM header.
/// \return If !OriginalCommandIncluded(), an empty/invalid RDM header.
constexpr const rdm::CommandHeader& SavedRdmResponse::original_cmd_header() const noexcept
{
  return original_cmd_.header();
}

/// \brief Get the RDM parameter data of the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid RDM parameter data.
/// \return If !OriginalCommandIncluded(), nullptr.
constexpr const uint8_t* SavedRdmResponse::original_cmd_data() const noexcept
{
  return (OriginalCommandIncluded() ? original_cmd_.data() : nullptr);
}

/// \brief Get the length of the RDM parameter data accompanying the original RDM command, if available.
/// \return If OriginalCommandIncluded(), the valid length.
/// \return If !OriginalCommandIncluded(), 0.
constexpr uint8_t SavedRdmResponse::original_cmd_data_len() const noexcept
{
  return (OriginalCommandIncluded() ? original_cmd_.data_len() : 0);
}

/// \brief Get the original RDM command that resulted in this RDM response, if available.
/// \return If OriginalCommandIncluded(), the valid RDM command.
/// return If !OriginalCommandIncluded(), an empty/invalid RDM command.
constexpr const rdm::Command& SavedRdmResponse::original_cmd() const noexcept
{
  return original_cmd_;
}

/// Get the UID of the RDM responder that sent this response.
constexpr rdm::Uid SavedRdmResponse::rdm_source_uid() const noexcept
{
  return rdm_.source_uid();
}

/// Get the UID of the RDM controller to which this response is addressed.
constexpr rdm::Uid SavedRdmResponse::rdm_dest_uid() const noexcept
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
inline size_t SavedRdmResponse::data_len() const noexcept
{
  return rdm_.data_len();
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

/// \brief Whether the original RDM command is included.
///
/// In RDMnet, a response to an RDM command includes the original command data. An exception to
/// this rule is unsolicited RDM responses, which are not in response to a command and thus do not
/// include the original command data.
constexpr bool SavedRdmResponse::OriginalCommandIncluded() const noexcept
{
  return (seq_num_ != 0);
}

/// Whether this RDM response includes any RDM parameter data.
inline bool SavedRdmResponse::HasData() const noexcept
{
  return rdm_.HasData();
}

/// \brief Whether this RDM response is from a default responder.
/// \details See \ref devices_and_gateways for more information.
constexpr bool SavedRdmResponse::IsFromDefaultResponder() const noexcept
{
  return (source_endpoint_ == E133_NULL_ENDPOINT);
}

/// \brief Whether this command has an RDM response type of ACK.
///
/// If this is false, it implies that IsNack() is true (ACK_TIMER is not allowed in RDMnet, and the
/// library recombines ACK_OVERFLOW responses automatically).
constexpr bool SavedRdmResponse::IsAck() const noexcept
{
  return rdm_.IsAck();
}

/// \brief Whether this command has an RDM response type of NACK_REASON.
///
/// If this is false, it implies that IsAck() is true (ACK_TIMER is not allowed in RDMnet, and the
/// library recombines ACK_OVERFLOW responses automatically).
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

///////////////////////////////////////////////////////////////////////////////////////////////////
// RPT Status Message Types
///////////////////////////////////////////////////////////////////////////////////////////////////

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

  constexpr rdm::Uid rdmnet_source_uid() const noexcept;
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

  constexpr const rdm::Uid& rdmnet_source_uid() const noexcept;
  constexpr uint16_t source_endpoint() const noexcept;
  constexpr uint32_t seq_num() const noexcept;

  constexpr rpt_status_code_t status_code() const noexcept;
  constexpr const std::string& status_string() const noexcept;

  constexpr bool IsValid() const noexcept;
  const char* CodeToCString() const noexcept;
  std::string CodeToString() const;
  bool HasStatusString() const noexcept;

private:
  rdm::Uid rdmnet_source_uid_;
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
constexpr rdm::Uid RptStatus::rdmnet_source_uid() const noexcept
{
  return status_.rdmnet_source_uid;
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
    : rdmnet_source_uid_(c_status.rdmnet_source_uid)
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
  rdmnet_source_uid_ = c_status.rdmnet_source_uid;
  source_endpoint_ = c_status.source_endpoint;
  seq_num_ = c_status.seq_num;
  status_code_ = c_status.status_code;
  if (c_status.status_string)
    status_string_.assign(c_status.status_string);
  return *this;
}

/// Construct a SavedRptStatus from an RptStatus.
inline SavedRptStatus::SavedRptStatus(const RptStatus& status)
    : rdmnet_source_uid_(status.rdmnet_source_uid())
    , source_endpoint_(status.source_endpoint())
    , seq_num_(status.seq_num())
    , status_code_(status.status_code())
    , status_string_(status.status_string())
{
}

/// Assign an RptStatus to an instance of this class.
inline SavedRptStatus& SavedRptStatus::operator=(const RptStatus& status)
{
  rdmnet_source_uid_ = status.rdmnet_source_uid();
  source_endpoint_ = status.source_endpoint();
  seq_num_ = status.seq_num();
  status_code_ = status.status_code();
  status_string_ = status.status_string();
  return *this;
}

/// Get the UID of the RDMnet component that sent this RPT status message.
constexpr const rdm::Uid& SavedRptStatus::rdmnet_source_uid() const noexcept
{
  return rdmnet_source_uid_;
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

/// \brief Copy a list of RPT client entries delivered to an RDMnet callback function.
///
/// This function copies and translates the list delivered to a callback function into C++ native
/// types.
inline std::vector<RptClientEntry> GetRptClientEntries(const RdmnetRptClientList& list)
{
  std::vector<RptClientEntry> to_return;
  to_return.reserve(list.num_client_entries);
  std::transform(list.client_entries, list.client_entries + list.num_client_entries, std::back_inserter(to_return),
                 [](const RdmnetRptClientEntry& entry) { return RptClientEntry(entry); });
  return to_return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// EPT message types
///////////////////////////////////////////////////////////////////////////////////////////////////

class SavedEptData;

/// \ingroup rdmnet_cpp_common
/// \brief An EPT data message received over RDMnet and delivered to an RDMnet callback function.
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

  constexpr etcpal::Uuid source_cid() const noexcept;
  constexpr uint16_t manufacturer_id() const noexcept;
  constexpr uint16_t protocol_id() const noexcept;
  constexpr uint32_t sub_protocol() const noexcept;
  constexpr const uint8_t* data() const noexcept;
  constexpr size_t data_len() const noexcept;

  std::vector<uint8_t> CopyData() const;

  constexpr const RdmnetEptData& get() const noexcept;

  SavedEptData Save() const;

private:
  const RdmnetEptData& data_;
};

/// \ingroup rdmnet_cpp_common
/// \brief An EPT data message received over RDMnet and saved for later processing.
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

  constexpr const etcpal::Uuid& source_cid() const noexcept;
  constexpr uint16_t manufacturer_id() const noexcept;
  constexpr uint16_t protocol_id() const noexcept;
  constexpr uint32_t sub_protocol() const noexcept;
  const uint8_t* data() const noexcept;
  size_t data_len() const noexcept;

  bool IsValid() const noexcept;

private:
  etcpal::Uuid source_cid_;
  uint16_t manufacturer_id_;
  uint16_t protocol_id_;
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

/// \brief Get the full EPT sub-protocol identifier.
/// \details Equivalent to (manufacturer_id() << 16 | protocol_id())
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

/// \brief Copy the data out of an EPT data message.
/// \return A new vector of bytes representing the EPT data.
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

/// \brief Save this data message for later use from a different context.
/// \return A SavedEptData containing the copied data.
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
constexpr const etcpal::Uuid& SavedEptData::source_cid() const noexcept
{
  return source_cid_;
}

/// Get the ESTA manufacturer ID that identifies the EPT sub-protocol.
constexpr uint16_t SavedEptData::manufacturer_id() const noexcept
{
  return manufacturer_id_;
}

/// Get the protocol ID that identifies the EPT sub-protocol.
constexpr uint16_t SavedEptData::protocol_id() const noexcept
{
  return protocol_id_;
}

/// \brief Get the full EPT sub-protocol identifier.
/// \details Equivalent to (manufacturer_id() << 16 | protocol_id())
constexpr uint32_t SavedEptData::sub_protocol() const noexcept
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

///////////////////////////////////////////////////////////////////////////////////////////////////
// EPT Status Message Types
///////////////////////////////////////////////////////////////////////////////////////////////////

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

/// \brief Copy a list of EPT client entries delivered to an RDMnet callback function.
///
/// This function copies and translates the list delivered to a callback function into C++ native
/// types. These types use C++ heap-allocating containers to store the client entry data and
/// sub-protocol entries.
inline std::vector<EptClientEntry> GetEptClientEntries(const RdmnetEptClientList& list)
{
  std::vector<EptClientEntry> to_return;
  to_return.reserve(list.num_client_entries);
  std::transform(list.client_entries, list.client_entries + list.num_client_entries, std::back_inserter(to_return),
                 [](const RdmnetEptClientEntry& entry) { return EptClientEntry(entry); });
  return to_return;
}

namespace llrp
{
///////////////////////////////////////////////////////////////////////////////////////////////////
// LLRP RDM Command message types
///////////////////////////////////////////////////////////////////////////////////////////////////

class SavedRdmCommand;

/// \ingroup rdmnet_cpp_common
/// \brief An RDM command received over LLRP and delivered to an RDMnet callback function.
///
/// Not valid for use other than as a parameter to an RDMnet callback function; use
/// RdmCommand::Save() to create a copyable version.
class RdmCommand
{
public:
  /// Not default-constructible.
  RdmCommand() = delete;
  /// Not copyable - use Save() to create a copyable version.
  RdmCommand(const RdmCommand& other) = delete;
  /// Not copyable - use Save() to create a copyable version.
  RdmCommand& operator=(const RdmCommand& other) = delete;

  constexpr RdmCommand(const LlrpRdmCommand& c_cmd) noexcept;

  constexpr etcpal::Uuid source_cid() const noexcept;
  constexpr uint32_t seq_num() const noexcept;
  constexpr RdmnetMcastNetintId netint_id() const noexcept;
  constexpr etcpal_iptype_t netint_ip_type() const noexcept;
  constexpr unsigned int netint_index() const noexcept;

  constexpr rdm::Uid source_uid() const noexcept;
  constexpr rdm::Uid dest_uid() const noexcept;
  constexpr uint16_t subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t param_id() const noexcept;

  constexpr rdm::CommandHeader rdm_header() const noexcept;

  constexpr const uint8_t* data() const noexcept;
  constexpr uint8_t data_len() const noexcept;

  constexpr bool HasData() const noexcept;

  constexpr bool IsGet() const noexcept;
  constexpr bool IsSet() const noexcept;

  constexpr const LlrpRdmCommand& get() const noexcept;

  rdm::Command ToRdm() const;
  SavedRdmCommand Save() const;

private:
  const LlrpRdmCommand& cmd_;
};

/// \ingroup rdmnet_cpp_common
/// \brief An RDM command received over RDMnet by a local component and saved for a later response.
class SavedRdmCommand
{
public:
  /// Create an empty, invalid SavedRdmCommand by default.
  SavedRdmCommand() = default;
  constexpr SavedRdmCommand(const LlrpSavedRdmCommand& c_cmd) noexcept;
  SavedRdmCommand& operator=(const LlrpSavedRdmCommand& c_cmd) noexcept;
  SavedRdmCommand(const RdmCommand& command) noexcept;
  SavedRdmCommand& operator=(const RdmCommand& command) noexcept;

  constexpr etcpal::Uuid source_cid() const noexcept;
  constexpr uint32_t seq_num() const noexcept;
  constexpr RdmnetMcastNetintId netint_id() const noexcept;
  constexpr etcpal_iptype_t netint_ip_type() const noexcept;
  constexpr unsigned int netint_index() const noexcept;

  constexpr rdm::Uid source_uid() const noexcept;
  constexpr rdm::Uid dest_uid() const noexcept;
  constexpr uint16_t subdevice() const noexcept;
  constexpr rdm_command_class_t command_class() const noexcept;
  constexpr uint16_t param_id() const noexcept;

  constexpr rdm::CommandHeader rdm_header() const noexcept;

  constexpr const uint8_t* data() const noexcept;
  constexpr uint8_t data_len() const noexcept;

  constexpr bool HasData() const noexcept;

  constexpr bool IsGet() const noexcept;
  constexpr bool IsSet() const noexcept;

  ETCPAL_CONSTEXPR_14 LlrpSavedRdmCommand& get() noexcept;
  constexpr const LlrpSavedRdmCommand& get() const noexcept;

  rdm::Command ToRdm() const;

private:
  LlrpSavedRdmCommand cmd_{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// llrp::RdmCommand function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct an RdmCommand which references an instance of the C LlrpRdmCommand type.
constexpr RdmCommand::RdmCommand(const LlrpRdmCommand& c_cmd) noexcept : cmd_(c_cmd)
{
}

/// Get the CID of the LLRP manager that sent this command.
constexpr etcpal::Uuid RdmCommand::source_cid() const noexcept
{
  return cmd_.source_cid;
}

/// Get the LLRP sequence number of this command.
constexpr uint32_t RdmCommand::seq_num() const noexcept
{
  return cmd_.seq_num;
}

/// \brief Get the network interface ID on which this command was received.
/// \details This helps the LLRP library send the response on the same interface.
constexpr RdmnetMcastNetintId RdmCommand::netint_id() const noexcept
{
  return cmd_.netint_id;
}

/// Get the IP protocol type of the network interface on which this command was received.
constexpr etcpal_iptype_t RdmCommand::netint_ip_type() const noexcept
{
  return cmd_.netint_id.ip_type;
}

/// Get the index of the network interface on which this command was received.
constexpr unsigned int RdmCommand::netint_index() const noexcept
{
  return cmd_.netint_id.index;
}

/// Get the UID of the LLRP manager that sent this command.
constexpr rdm::Uid RdmCommand::source_uid() const noexcept
{
  return cmd_.rdm_header.source_uid;
}

/// Get the UID of the LLRP target to which this command is addressed.
constexpr rdm::Uid RdmCommand::dest_uid() const noexcept
{
  return cmd_.rdm_header.dest_uid;
}

/// Get the RDM subdevice to which this command is addressed (0 means the root device).
constexpr uint16_t RdmCommand::subdevice() const noexcept
{
  return cmd_.rdm_header.subdevice;
}

/// Get the RDM command class of this command.
constexpr rdm_command_class_t RdmCommand::command_class() const noexcept
{
  return cmd_.rdm_header.command_class;
}

/// Get the RDM parameter ID (PID) of this command.
constexpr uint16_t RdmCommand::param_id() const noexcept
{
  return cmd_.rdm_header.param_id;
}

/// Get the RDM protocol header contained within this command.
constexpr rdm::CommandHeader RdmCommand::rdm_header() const noexcept
{
  return cmd_.rdm_header;
}

/// Get a pointer to the RDM parameter data buffer contained within this command.
constexpr const uint8_t* RdmCommand::data() const noexcept
{
  return cmd_.data;
}

/// Get the length of the RDM parameter data contained within this command.
constexpr uint8_t RdmCommand::data_len() const noexcept
{
  return cmd_.data_len;
}

/// Whether this command has any associated RDM parameter data.
constexpr bool RdmCommand::HasData() const noexcept
{
  return (data_len() != 0);
}

/// Whether this command is an RDM GET command.
constexpr bool RdmCommand::IsGet() const noexcept
{
  return (cmd_.rdm_header.command_class == kRdmCCGetCommand);
}

/// Whether this command is an RDM SET command.
constexpr bool RdmCommand::IsSet() const noexcept
{
  return (cmd_.rdm_header.command_class == kRdmCCSetCommand);
}

/// Get a const reference to the underlying C type.
constexpr const LlrpRdmCommand& RdmCommand::get() const noexcept
{
  return cmd_;
}

/// Convert the RDM data in this command to an RDM command type.
inline rdm::Command RdmCommand::ToRdm() const
{
  return rdm::Command(cmd_.rdm_header, cmd_.data, cmd_.data_len);
}

/// \brief Save the data in this command for later use with API functions from a different context.
/// \return A SavedRdmCommand containing the copied data.
inline SavedRdmCommand RdmCommand::Save() const
{
  return SavedRdmCommand(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// llrp::SavedRdmCommand function definitions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Construct a SavedRdmCommand copied from an instance of the C LlrpSavedRdmCommand type.
constexpr SavedRdmCommand::SavedRdmCommand(const LlrpSavedRdmCommand& c_cmd) noexcept : cmd_(c_cmd)
{
}

/// Assign an instance of the C RdmnetSavedRdmCommand type to an instance of this class.
inline SavedRdmCommand& SavedRdmCommand::operator=(const LlrpSavedRdmCommand& c_cmd) noexcept
{
  cmd_ = c_cmd;
  return *this;
}

/// Construct a SavedRdmCommand from an RdmCommand.
inline SavedRdmCommand::SavedRdmCommand(const RdmCommand& command) noexcept
{
  rdmnet_save_llrp_rdm_command(&command.get(), &cmd_);
}

/// Assign an RdmCommand to an instance of this class.
inline SavedRdmCommand& SavedRdmCommand::operator=(const RdmCommand& command) noexcept
{
  rdmnet_save_llrp_rdm_command(&command.get(), &cmd_);
}

/// Get the CID of the LLRP manager that sent this command.
constexpr etcpal::Uuid SavedRdmCommand::source_cid() const noexcept
{
  return cmd_.source_cid;
}

/// Get the LLRP sequence number of this command.
constexpr uint32_t SavedRdmCommand::seq_num() const noexcept
{
  return cmd_.seq_num;
}

/// \brief Get the network interface ID on which this command was received.
/// \details This helps the LLRP library send the response on the same interface.
constexpr RdmnetMcastNetintId SavedRdmCommand::netint_id() const noexcept
{
  return cmd_.netint_id;
}

/// Get the IP protocol type of the network interface on which this command was received.
constexpr etcpal_iptype_t SavedRdmCommand::netint_ip_type() const noexcept
{
  return cmd_.netint_id.ip_type;
}

/// Get the index of the network interface on which this command was received.
constexpr unsigned int SavedRdmCommand::netint_index() const noexcept
{
  return cmd_.netint_id.index;
}

/// Get the UID of the LLRP manager that sent this command.
constexpr rdm::Uid SavedRdmCommand::source_uid() const noexcept
{
  return cmd_.rdm_header.source_uid;
}

/// Get the UID of the LLRP target to which this command is addressed.
constexpr rdm::Uid SavedRdmCommand::dest_uid() const noexcept
{
  return cmd_.rdm_header.dest_uid;
}

/// Get the RDM subdevice to which this command is addressed (0 means the root device).
constexpr uint16_t SavedRdmCommand::subdevice() const noexcept
{
  return cmd_.rdm_header.subdevice;
}

/// Get the RDM command class of this command.
constexpr rdm_command_class_t SavedRdmCommand::command_class() const noexcept
{
  return cmd_.rdm_header.command_class;
}

/// Get the RDM parameter ID (PID) of this command.
constexpr uint16_t SavedRdmCommand::param_id() const noexcept
{
  return cmd_.rdm_header.param_id;
}

/// Get the RDM protocol header contained within this command.
constexpr rdm::CommandHeader SavedRdmCommand::rdm_header() const noexcept
{
  return cmd_.rdm_header;
}

/// Get a pointer to the RDM parameter data buffer contained within this command.
constexpr const uint8_t* SavedRdmCommand::data() const noexcept
{
  return cmd_.data;
}

/// Get the length of the RDM parameter data contained within this command.
constexpr uint8_t SavedRdmCommand::data_len() const noexcept
{
  return cmd_.data_len;
}

/// Whether this command has any associated RDM parameter data.
constexpr bool SavedRdmCommand::HasData() const noexcept
{
  return (data_len() != 0);
}

/// Whether this command is an RDM GET command.
constexpr bool SavedRdmCommand::IsGet() const noexcept
{
  return (cmd_.rdm_header.command_class == kRdmCCGetCommand);
}

/// Whether this command is an RDM SET command.
constexpr bool SavedRdmCommand::IsSet() const noexcept
{
  return (cmd_.rdm_header.command_class == kRdmCCSetCommand);
}

/// Get a mutable reference to the underlying C type.
ETCPAL_CONSTEXPR_14_OR_INLINE LlrpSavedRdmCommand& SavedRdmCommand::get() noexcept
{
  return cmd_;
}

/// Get a const reference to the underlying C type.
constexpr const LlrpSavedRdmCommand& SavedRdmCommand::get() const noexcept
{
  return cmd_;
}

/// Convert the RDM data in this command to an RDM command type.
inline rdm::Command SavedRdmCommand::ToRdm() const
{
  return rdm::Command(cmd_.rdm_header, cmd_.data, cmd_.data_len);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// LLRP RDM response message types
///////////////////////////////////////////////////////////////////////////////////////////////////

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

#endif  // RDMNET_CPP_MESSAGE_H_
