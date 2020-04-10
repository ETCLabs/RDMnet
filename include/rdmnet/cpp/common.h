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

#ifndef RDMNET_CPP_COMMON_H_
#define RDMNET_CPP_COMMON_H_

/// \file rdmnet/cpp/common.h
/// \brief C++ wrapper for the RDMnet init/deinit functions

#include "etcpal/cpp/error.h"
#include "etcpal/cpp/log.h"
#include "rdmnet/common.h"

/// \defgroup rdmnet_cpp_api RDMnet C++ Language APIs
/// \brief Native C++ APIs for interfacing with the RDMnet library.
///
/// These wrap the corresponding C modules in a nicer syntax for C++ application developers.

/// \defgroup rdmnet_cpp_common Common Definitions
/// \ingroup rdmnet_cpp_api
/// \brief Definitions shared by other APIs in this module.

/// \brief A namespace which contains all C++ language definitions in the RDMnet library.
namespace rdmnet
{
/// \ingroup rdmnet_cpp_common
/// \brief Initialize the RDMnet library.
///
/// Wraps rdmnet_init(). Does all initialization required before the RDMnet API modules can be
/// used. Starts the message dispatch thread.
///
/// \param log_params (optional) Log parameters for the RDMnet library to use to log messages. If
///                   not provided, no logging will be performed.
/// \param mcast_netints (optional) A set of network interfaces to which to restrict multicast
///                      operation.
/// \return etcpal::Error::Ok(): Initialization successful.
/// \return Errors from rdmnet_init().
inline etcpal::Error Init(const EtcPalLogParams* log_params = nullptr,
                          const std::vector<RdmnetMcastNetintId>& mcast_netints = std::vector<RdmnetMcastNetintId>{})
{
  RdmnetNetintConfig config = {mcast_netints.data(), mcast_netints.size()};
  return rdmnet_init(log_params, &config);
}

/// \ingroup rdmnet_cpp_common
/// \brief Initialize the RDMnet library.
///
/// Wraps rdmnet_init(). Does all initialization required before the RDMnet API modules can be
/// used. Starts the message dispatch thread.
///
/// \param logger Logger instance for the RDMnet library to use to log messages.
/// \param mcast_netints (optional) A set of network interfaces to which to restrict multicast
///                      operation.
/// \return etcpal::Error::Ok(): Initialization successful.
/// \return Errors from rdmnet_init().
inline etcpal::Error Init(const etcpal::Logger& logger,
                          const std::vector<RdmnetMcastNetintId>& mcast_netints = std::vector<RdmnetMcastNetintId>{})
{
  RdmnetNetintConfig config = {mcast_netints.data(), mcast_netints.size()};
  return rdmnet_init(&logger.log_params(), &config);
}

/// \ingroup rdmnet_cpp_common
/// \brief Deinitialize the RDMnet library.
///
/// Closes all connections, deallocates all resources and joins the background thread. No RDMnet
/// API functions are usable after this function is called.
inline void Deinit()
{
  return rdmnet_deinit();
}

/// \ingroup rdmnet_cpp_common
/// \brief A class representing a synchronous action to take in response to a received RDM command.
class ResponseAction
{
public:
  static ResponseAction SendAck(size_t response_data_len = 0);
  static ResponseAction SendNack(rdm_nack_reason_t nack_reason);
  static ResponseAction SendNack(uint16_t raw_nack_reason);
  static ResponseAction DeferResponse();

  constexpr const RdmnetSyncRdmResponse& get() const;

private:
  RdmnetSyncRdmResponse response_;
};

/// \brief Send an RDM ACK, optionally including some response data.
/// \param response_data_len Length of the RDM response parameter data provided.
/// \pre If response_data_len != 0, data has been copied to the buffer provided at initialization time.
inline ResponseAction ResponseAction::SendAck(size_t response_data_len)
{
  ResponseAction to_return;
  RDMNET_SYNC_SEND_RDM_ACK(&to_return.response_, response_data_len);
  return to_return;
}

/// \brief Send an RDM NACK with a reason code.
/// \param nack_reason The RDM NACK reason code to send with the NACK response.
inline ResponseAction ResponseAction::SendNack(rdm_nack_reason_t nack_reason)
{
  ResponseAction to_return;
  RDMNET_SYNC_SEND_RDM_NACK(&to_return.response_, nack_reason);
  return to_return;
}

/// \brief Send an RDM NACK with a reason code.
/// \param raw_nack_reason The NACK reason (either standard or manufacturer-specific) to send with
///                        the NACK response.
inline ResponseAction ResponseAction::SendNack(uint16_t raw_nack_reason)
{
  ResponseAction to_return;
  RDMNET_SYNC_SEND_RDM_NACK(&to_return.response_, static_cast<rdm_nack_reason_t>(raw_nack_reason));
  return to_return;
}

/// \brief Defer the RDM response to be sent later from another context.
/// \details Make sure to Save() any RDM command data for later processing.
inline ResponseAction ResponseAction::DeferResponse()
{
  ResponseAction to_return;
  RDMNET_SYNC_DEFER_RDM_RESPONSE(&to_return.response_);
  return to_return;
}

/// \brief Get a const reference to the underlying C type.
constexpr const RdmnetSyncRdmResponse& ResponseAction::get() const
{
  return response_;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_COMMON_H_
