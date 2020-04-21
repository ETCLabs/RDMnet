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

/// \file rdmnet/cpp/message_types/llrp_rdm_command.h
/// \brief Definitions for RDMnet LLRP RDM command message types.

#ifndef RDMNET_CPP_MESSAGE_TYPES_LLRP_RDM_COMMAND_H_
#define RDMNET_CPP_MESSAGE_TYPES_LLRP_RDM_COMMAND_H_

#include <cstdint>
#include "etcpal/common.h"
#include "etcpal/inet.h"
#include "etcpal/cpp/uuid.h"
#include "rdm/cpp/message.h"
#include "rdm/cpp/uid.h"
#include "rdmnet/message.h"

namespace rdmnet
{
namespace llrp
{
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

  bool IsValid() const noexcept;
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

};  // namespace llrp
};  // namespace rdmnet

#endif  // RDMNET_CPP_MESSAGE_TYPES_LLRP_RDM_COMMAND_H_
