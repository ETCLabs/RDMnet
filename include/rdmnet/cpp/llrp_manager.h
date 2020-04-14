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

#ifndef RDMNET_CPP_LLRP_MANAGER_H_
#define RDMNET_CPP_LLRP_MANAGER_H_

#include <cstdint>
#include "etcpal/common.h"
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "rdmnet/llrp_manager.h"
#include "rdmnet/cpp/common.h"
#include "rdmnet/cpp/message.h"

namespace rdmnet
{
/// \brief A namespace which contains all LLRP C++ language definitions.
namespace llrp
{
/// \defgroup llrp_manager_cpp LLRP Manager API
/// \ingroup rdmnet_cpp_api
/// \brief Implementation of LLRP manager functionality; see \ref using_llrp_manager.
///
/// LLRP managers perform the discovery and command functionality of RDMnet's Low Level Recovery
/// Protocol (LLRP). See \ref using_llrp_manager for details of how to use this API.
///
/// @{

/// A handle type used by the RDMnet library to identify LLRP manager instances.
using ManagerHandle = llrp_manager_t;
/// An invalid ManagerHandle value.
constexpr ManagerHandle kInvalidManagerHandle = LLRP_MANAGER_INVALID;

/// @}

/// \brief A destination address for an LLRP RDM command.
/// \details Represents an LLRP Target to which an RDM command is addressed.
class DestinationAddr
{
public:
  constexpr DestinationAddr(const etcpal::Uuid& cid, const rdm::Uid& uid, uint16_t subdevice = 0);

private:
  LlrpDestinationAddr addr_;
};

/// Construct a destination address from its component parts.
/// \param cid The target's CID.
/// \param uid The target's RDM UID.
/// \param subdevice The RDM subdevice to which this command is addressed (0 means the root device).
constexpr DestinationAddr::DestinationAddr(const etcpal::Uuid& cid, const rdm::Uid& uid, uint16_t subdevice)
    : addr_{cid.get(), uid.get(), subdevice}
{
}

/// \ingroup llrp_manager_cpp
/// \brief Represents an LLRP target discovered by a manager.
class DiscoveredTarget
{
public:
  /// Construct a target with null/empty values by default.
  DiscoveredTarget() = default;
  constexpr DiscoveredTarget(const DiscoveredLlrpTarget& c_target) noexcept;
  DiscoveredTarget& operator=(const DiscoveredLlrpTarget& c_target) noexcept;

  constexpr const etcpal::Uuid& cid() const noexcept;
  constexpr const rdm::Uid& uid() const noexcept;
  constexpr const etcpal::MacAddr& hardware_address() const noexcept;
  constexpr llrp_component_t component_type() const noexcept;

  constexpr DestinationAddr address(uint16_t subdevice = 0) const noexcept;

private:
  etcpal::Uuid cid_;
  rdm::Uid uid_;
  etcpal::MacAddr hardware_addr_;
  llrp_component_t component_type_;
};

/// \brief Construct a DiscoveredTarget copied from an instance of the C DiscoveredLlrpTarget type.
constexpr DiscoveredTarget::DiscoveredTarget(const DiscoveredLlrpTarget& c_target) noexcept
    : cid_(c_target.cid)
    , uid_(c_target.uid)
    , hardware_addr_(c_target.hardware_address)
    , component_type_(c_target.component_type)
{
}

/// \brief Assign an instance of the C DiscoveredLlrpTarget type to an instance of this class.
inline DiscoveredTarget& DiscoveredTarget::operator=(const DiscoveredLlrpTarget& c_target) noexcept
{
  cid_ = c_target.cid;
  uid_ = c_target.uid;
  hardware_addr_ = c_target.hardware_address;
  component_type_ = c_target.component_type;
  return *this;
}

/// \brief Get the target's CID.
constexpr const etcpal::Uuid& DiscoveredTarget::cid() const noexcept
{
  return cid_;
}

/// \brief Get the target's RDM UID.
constexpr const rdm::Uid& DiscoveredTarget::uid() const noexcept
{
  return uid_;
}

/// \brief Get the lowest hardware address of the machine the target is operating on.
constexpr const etcpal::MacAddr& DiscoveredTarget::hardware_address() const noexcept
{
  return hardware_addr_;
}

/// \brief Get the LLRP component type of the target.
constexpr llrp_component_t DiscoveredTarget::component_type() const noexcept
{
  return component_type_;
}

/// \brief Get the target's LLRP addressing information.
constexpr DestinationAddr DiscoveredTarget::address(uint16_t subdevice) const noexcept
{
  return DestinationAddr(cid_, uid_, subdevice);
}

/// \ingroup llrp_manager_cpp
/// \brief A class that receives notification callbacks from an LLRP manager.
///
/// See \ref using_llrp_manager for details of how to use this API.
class ManagerNotifyHandler
{
public:
  /// \brief An LLRP target has been discovered.
  /// \param handle Handle to LLRP manager instance which has discovered the target.
  /// \param target Information about the target which has been discovered.
  virtual void HandleLlrpTargetDiscovered(ManagerHandle handle, const DiscoveredTarget& target) = 0;

  /// \brief An RDM response has been received from an LLRP target.
  /// \param handle Handle to LLRP manager instance which has received the RDM response.
  /// \param resp The RDM response data.
  virtual void HandleLlrpRdmResponse(ManagerHandle handle, const RdmResponse& resp) = 0;

  /// \brief The previously-started LLRP discovery process has finished.
  /// \param handle Handle to LLRP manager instance which has finished discovery.
  virtual void HandleLlrpDiscoveryFinished(ManagerHandle handle) { ETCPAL_UNUSED_ARG(handle); }
};

/// \ingroup llrp_manager_cpp
/// \brief An instance of LLRP manager functionality.
///
/// See \ref using_llrp_manager for details of how to use this API.
class Manager
{
public:
  Manager() = default;
  Manager(const Manager& other) = delete;
  Manager& operator=(const Manager& other) = delete;
  Manager(Manager&& other) = default;             ///< Move a manager instance.
  Manager& operator=(Manager&& other) = default;  ///< Move a manager instance.

  etcpal::Error Startup(ManagerNotifyHandler& notify_handler, uint16_t manufacturer_id, unsigned int netint_index,
                        etcpal_iptype_t ip_type = kEtcPalIpTypeV4,
                        const etcpal::Uuid& cid = etcpal::Uuid::OsPreferred());
  void Shutdown();

  etcpal::Error StartDiscovery(uint16_t filter = 0);
  etcpal::Error StopDiscovery();
  etcpal::Expected<uint32_t> SendRdmCommand(const DestinationAddr& destination, rdmnet_command_class_t command_class,
                                            uint16_t param_id, const uint8_t* data = nullptr, uint8_t data_len = 0);
  etcpal::Expected<uint32_t> SendGetCommand(const DestinationAddr& destination, uint16_t param_id,
                                            const uint8_t* data = nullptr, uint8_t data_len = 0);
  etcpal::Expected<uint32_t> SendSetCommand(const DestinationAddr& destination, uint16_t param_id,
                                            const uint8_t* data = nullptr, uint8_t data_len = 0);

  constexpr ManagerHandle handle() const;
  constexpr ManagerNotifyHandler* notify_handler() const;

private:
  ManagerHandle handle_{kInvalidManagerHandle};
  ManagerNotifyHandler* notify_{nullptr};
};

/// \brief Allocate resources and startup this LLRP manager with the given configuration.
/// \param notify_handler A class instance to handle callback notifications from this manager.
/// \param manufacturer_id The LLRP manager's ESTA manufacturer ID.
/// \param netint_index The network interface index on which this manager should operate.
/// \param ip_type The IP protocol type with which this manager should operate.
/// \param cid The manager's Component Identifier (CID).
/// \return etcpal::Error::Ok(): LLRP manager started successfully.
/// \return Errors forwarded from llrp_manager_create().
inline etcpal::Error Manager::Startup(ManagerNotifyHandler& notify_handler, uint16_t manufacturer_id,
                                      unsigned int netint_index, etcpal_iptype_t ip_type, const etcpal::Uuid& cid)
{
  ETCPAL_UNUSED_ARG(notify_handler);
  ETCPAL_UNUSED_ARG(manufacturer_id);
  ETCPAL_UNUSED_ARG(netint_index);
  ETCPAL_UNUSED_ARG(ip_type);
  ETCPAL_UNUSED_ARG(cid);
  return kEtcPalErrNotImpl;
}

/// \brief Shut down this LLRP manager and deallocate resources.
inline void Manager::Shutdown()
{
}

/// \brief Start LLRP discovery.
///
/// Configure a manager to start discovery and send the first discovery message. Fails if a
/// previous discovery process is still ongoing.
///
/// \param filter Discovery filter, made up of one or more of the LLRP_FILTERVAL_* constants
///               defined in rdmnet/defs.h.
/// \return etcpal::Error::Ok(): Discovery started successfully.
/// \return Errors from llrp_manager_start_discovery().
inline etcpal::Error Manager::StartDiscovery(uint16_t filter)
{
  ETCPAL_UNUSED_ARG(filter);
  return kEtcPalErrNotImpl;
}

/// \brief Stop LLRP discovery.
///
/// Clears all discovery state and known discovered targets.
///
/// \return etcpal::Error::Ok(): Discovery stopped successfully.
/// \return Errors from llrp_manager_stop_discovery().
inline etcpal::Error Manager::StopDiscovery()
{
  return kEtcPalErrNotImpl;
}

/// \brief Send an RDM command from an LLRP manager.
///
/// The response will be delivered via the ManagerNotifyHandler::HandleLlrpRdmResponse() callback.
///
/// \param destination The destination addressing information for the RDM command.
/// \param command_class The command's RDM command class (GET or SET).
/// \param param_id The command's RDM parameter ID.
/// \param data [optional] The command's RDM parameter data, if it has any.
/// \param data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// \return On success, a sequence number which can be used to match the command with a response.
/// \return On failure, error codes from llrp_manager_send_rdm_command().
inline etcpal::Expected<uint32_t> Manager::SendRdmCommand(const DestinationAddr& destination,
                                                          rdmnet_command_class_t command_class, uint16_t param_id,
                                                          const uint8_t* data, uint8_t data_len)
{
  ETCPAL_UNUSED_ARG(destination);
  ETCPAL_UNUSED_ARG(command_class);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/// \brief Send an RDM GET command from an LLRP manager.
///
/// The response will be delivered via the ManagerNotifyHandler::HandleLlrpRdmResponse() callback.
///
/// \param destination The destination addressing information for the RDM command.
/// \param param_id The command's RDM parameter ID.
/// \param data [optional] The command's RDM parameter data, if it has any.
/// \param data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// \return On success, a sequence number which can be used to match the command with a response.
/// \return On failure, error codes from llrp_manager_send_rdm_command().
inline etcpal::Expected<uint32_t> Manager::SendGetCommand(const DestinationAddr& destination, uint16_t param_id,
                                                          const uint8_t* data, uint8_t data_len)
{
  ETCPAL_UNUSED_ARG(destination);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/// \brief Send an RDM SET command from an LLRP manager.
///
/// The response will be delivered via the ManagerNotifyHandler::HandleLlrpRdmResponse() callback.
///
/// \param destination The destination addressing information for the RDM command.
/// \param param_id The command's RDM parameter ID.
/// \param data [optional] The command's RDM parameter data, if it has any.
/// \param data_len [optional] The length of the RDM parameter data (or 0 if data is nullptr).
/// \return On success, a sequence number which can be used to match the command with a response.
/// \return On failure, error codes from llrp_manager_send_rdm_command().
inline etcpal::Expected<uint32_t> Manager::SendSetCommand(const DestinationAddr& destination, uint16_t param_id,
                                                          const uint8_t* data, uint8_t data_len)
{
  ETCPAL_UNUSED_ARG(destination);
  ETCPAL_UNUSED_ARG(param_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(data_len);
  return kEtcPalErrNotImpl;
}

/// Retrieve the handle of an LLRP manager instance.
constexpr ManagerHandle Manager::handle() const
{
  return handle_;
}

/// Retrieve the ManagerNotifyHandler reference that this LLRP manager was configured with.
constexpr ManagerNotifyHandler* Manager::notify_handler() const
{
  return notify_;
}

};  // namespace llrp
};  // namespace rdmnet

#endif  // RDMNET_CPP_LLRP_MANAGER_H_
