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

#ifndef RDMNET_CPP_COMMON_H_
#define RDMNET_CPP_COMMON_H_

/// @file rdmnet/cpp/common.h
/// @brief C++ wrapper for the RDMnet init/deinit functions

#include "etcpal/cpp/error.h"
#include "etcpal/cpp/log.h"
#include "rdmnet/common.h"

/// @defgroup rdmnet_cpp_api RDMnet C++ Language APIs
/// @brief Native C++ APIs for interfacing with the RDMnet library.
///
/// These wrap the corresponding C modules in a nicer syntax for C++ application developers.

/// @defgroup rdmnet_cpp_common Common Definitions
/// @ingroup rdmnet_cpp_api
/// @brief Definitions shared by other APIs in this module.

/// @brief A namespace which contains all C++ language definitions in the RDMnet library.
namespace rdmnet
{
/// @brief Determines whether multicast traffic is allowed through all interfaces or none.
enum class McastMode
{
  kEnabledOnAllInterfaces,
  kDisabledOnAllInterfaces
};

/// @ingroup rdmnet_cpp_common
/// @brief Initialize the RDMnet library.
///
/// Wraps rdmnet_init(). Does all initialization required before the RDMnet API modules can be
/// used. Starts the message dispatch thread.
///
/// @param log_params (optional) Log parameters for the RDMnet library to use to log messages. If
///                   not provided, no logging will be performed.
/// @param mcast_netints (optional) A set of network interfaces to which to restrict multicast
///                      operation.
/// @return etcpal::Error::Ok(): Initialization successful.
/// @return Errors from rdmnet_init().
inline etcpal::Error Init(const EtcPalLogParams*                  log_params = nullptr,
                          const std::vector<EtcPalMcastNetintId>& mcast_netints = std::vector<EtcPalMcastNetintId>{})
{
  if (mcast_netints.empty())
  {
    return rdmnet_init(log_params, nullptr);
  }
  else
  {
    RdmnetNetintConfig config = {mcast_netints.data(), mcast_netints.size(), false};
    return rdmnet_init(log_params, &config);
  }
}

/// @ingroup rdmnet_cpp_common
/// @brief Initialize the RDMnet library.
///
/// Wraps rdmnet_init(). Does all initialization required before the RDMnet API modules can be
/// used. Starts the message dispatch thread.
///
/// @param logger Logger instance for the RDMnet library to use to log messages.
/// @param mcast_netints (optional) A set of network interfaces to which to restrict multicast
///                      operation.
/// @return etcpal::Error::Ok(): Initialization successful.
/// @return Errors from rdmnet_init().
inline etcpal::Error Init(const etcpal::Logger&                   logger,
                          const std::vector<EtcPalMcastNetintId>& mcast_netints = std::vector<EtcPalMcastNetintId>{})
{
  if (mcast_netints.empty())
  {
    return rdmnet_init(&logger.log_params(), nullptr);
  }
  else
  {
    RdmnetNetintConfig config = {mcast_netints.data(), mcast_netints.size(), false};
    return rdmnet_init(&logger.log_params(), &config);
  }
}

/// @ingroup rdmnet_cpp_common
/// @brief Initialize the RDMnet library.
///
/// Wraps rdmnet_init(). Does all initialization required before the RDMnet API modules can be
/// used. Starts the message dispatch thread.
///
/// @param log_params Log parameters for the RDMnet library to use to log messages. If nullptr, no logging will be
///                   performed.
/// @param mcast_mode This controls whether multicast traffic should be allowed on all interfaces or no interfaces.
/// @return etcpal::Error::Ok(): Initialization successful.
/// @return Errors from rdmnet_init().
inline etcpal::Error Init(const EtcPalLogParams* log_params, McastMode mcast_mode)
{
  RdmnetNetintConfig config = {nullptr, 0u, (mcast_mode == kDisabledOnAllInterfaces)};
  return rdmnet_init(log_params, &config);
}

/// @ingroup rdmnet_cpp_common
/// @brief Initialize the RDMnet library.
///
/// Wraps rdmnet_init(). Does all initialization required before the RDMnet API modules can be
/// used. Starts the message dispatch thread.
///
/// @param logger Logger instance for the RDMnet library to use to log messages.
/// @param mcast_mode This controls whether multicast traffic should be allowed on all interfaces or no interfaces.
/// @return etcpal::Error::Ok(): Initialization successful.
/// @return Errors from rdmnet_init().
inline etcpal::Error Init(const etcpal::Logger& logger, McastMode mcast_mode)
{
  RdmnetNetintConfig config = {nullptr, 0u, (mcast_mode == kDisabledOnAllInterfaces)};
  return rdmnet_init(&logger.log_params(), &config);
}

/// @ingroup rdmnet_cpp_common
/// @brief Deinitialize the RDMnet library.
///
/// Closes all connections, deallocates all resources and joins the background thread. No RDMnet
/// API functions are usable after this function is called.
inline void Deinit()
{
  return rdmnet_deinit();
}

/// @ingroup rdmnet_cpp_common
/// @brief A class representing a synchronous action to take in response to a received RDM command.
class RdmResponseAction
{
public:
  static RdmResponseAction SendAck(size_t response_data_len = 0);
  static RdmResponseAction SendNack(rdm_nack_reason_t nack_reason);
  static RdmResponseAction SendNack(uint16_t raw_nack_reason);
  static RdmResponseAction DeferResponse();
  static RdmResponseAction RetryLater();

  constexpr const RdmnetSyncRdmResponse& get() const;

private:
  RdmnetSyncRdmResponse response_;
};

/// @brief Send an RDM ACK, optionally including some response data.
/// @param response_data_len Length of the RDM response parameter data provided.
/// @pre If response_data_len != 0, data has been copied to the buffer provided at initialization time.
inline RdmResponseAction RdmResponseAction::SendAck(size_t response_data_len)
{
  RdmResponseAction to_return;
  RDMNET_SYNC_SEND_RDM_ACK(&to_return.response_, response_data_len);
  return to_return;
}

/// @brief Send an RDM NACK with a reason code.
/// @param nack_reason The RDM NACK reason code to send with the NACK response.
inline RdmResponseAction RdmResponseAction::SendNack(rdm_nack_reason_t nack_reason)
{
  RdmResponseAction to_return;
  RDMNET_SYNC_SEND_RDM_NACK(&to_return.response_, nack_reason);
  return to_return;
}

/// @brief Send an RDM NACK with a reason code.
/// @param raw_nack_reason The NACK reason (either standard or manufacturer-specific) to send with
///                        the NACK response.
inline RdmResponseAction RdmResponseAction::SendNack(uint16_t raw_nack_reason)
{
  RdmResponseAction to_return;
  RDMNET_SYNC_SEND_RDM_NACK(&to_return.response_, static_cast<rdm_nack_reason_t>(raw_nack_reason));
  return to_return;
}

/// @brief Defer the RDM response to be sent later from another context.
/// @details Make sure to Save() any RDM command data for later processing.
inline RdmResponseAction RdmResponseAction::DeferResponse()
{
  RdmResponseAction to_return;
  RDMNET_SYNC_DEFER_RDM_RESPONSE(&to_return.response_);
  return to_return;
}

/// @brief Trigger another notification for the (non-LLRP) RDM command on the next tick.
inline RdmResponseAction RdmResponseAction::RetryLater()
{
  RdmResponseAction to_return;
  RDMNET_SYNC_RETRY_LATER(&to_return.response_);
  return to_return;
}

/// @brief Get a const reference to the underlying C type.
constexpr const RdmnetSyncRdmResponse& RdmResponseAction::get() const
{
  return response_;
}

/// @ingroup rdmnet_cpp_common
/// @brief A class representing a synchronous action to take in response to a received EPT data message.
class EptResponseAction
{
public:
  static EptResponseAction SendData(size_t response_data_len);
  static EptResponseAction SendStatus(ept_status_code_t status_code);
  static EptResponseAction DeferResponse();

  constexpr const RdmnetSyncEptResponse& get() const;

private:
  RdmnetSyncEptResponse response_;
};

/// @brief Send an EPT data message in response.
/// @param response_data_len Length of the EPT response data provided.
/// @pre Data has been copied to the buffer provided at initialization time.
inline EptResponseAction EptResponseAction::SendData(size_t response_data_len)
{
  EptResponseAction to_return;
  RDMNET_SYNC_SEND_EPT_DATA(&to_return.response_, response_data_len);
  return to_return;
}

/// @brief Send an EPT status message.
/// @param status_code EPT status code to send.
inline EptResponseAction EptResponseAction::SendStatus(ept_status_code_t status_code)
{
  EptResponseAction to_return;
  RDMNET_SYNC_SEND_EPT_STATUS(&to_return.response_, status_code);
  return to_return;
}

/// @brief Defer the response to the EPT message, either to be sent later or because no response is necessary.
inline EptResponseAction EptResponseAction::DeferResponse()
{
  EptResponseAction to_return;
  RDMNET_SYNC_DEFER_EPT_RESPONSE(&to_return.response_);
  return to_return;
}

/// @brief Get a const reference to the underlying C type.
constexpr const RdmnetSyncEptResponse& EptResponseAction::get() const
{
  return response_;
}

};  // namespace rdmnet

#endif  // RDMNET_CPP_COMMON_H_
