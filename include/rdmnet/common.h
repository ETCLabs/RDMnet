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

/**
 * @file rdmnet/common.h
 * @brief Functions and definitions common to all RDMnet API modules.
 */

#ifndef RDMNET_COMMON_H_
#define RDMNET_COMMON_H_

#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/log.h"
#include "rdm/message.h"
#include "rdmnet/defs.h"

/**
 * @defgroup rdmnet_api RDMnet C Language APIs
 * @brief Core C-language APIs for interfacting with the RDMnet library.
 */

/**
 * @defgroup rdmnet_api_common Common Definitions
 * @ingroup rdmnet_api
 * @brief Definitions shared by other APIs in this module.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** RPT status code definitions */
typedef enum
{
  /** The Destination UID in the RPT PDU could not be found. */
  kRptStatusUnknownRptUid = VECTOR_RPT_STATUS_UNKNOWN_RPT_UID,
  /** No RDM response was received from a Gateway's RDM responder. */
  kRptStatusRdmTimeout = VECTOR_RPT_STATUS_RDM_TIMEOUT,
  /** An invalid RDM response was received from a Gateway's RDM responder. */
  kRptStatusInvalidRdmResponse = VECTOR_RPT_STATUS_RDM_INVALID_RESPONSE,
  /** The Destination UID in an encapsulated RDM Command could not be found. */
  kRptStatusUnknownRdmUid = VECTOR_RPT_STATUS_UNKNOWN_RDM_UID,
  /** The Destination Endpoint ID in the RPT PDU could not be found. */
  kRptStatusUnknownEndpoint = VECTOR_RPT_STATUS_UNKNOWN_ENDPOINT,
  /** A Broadcasted RPT Request was sent to at least one Device. */
  kRptStatusBroadcastComplete = VECTOR_RPT_STATUS_BROADCAST_COMPLETE,
  /** An RPT PDU was received with an unsupported Vector. */
  kRptStatusUnknownVector = VECTOR_RPT_STATUS_UNKNOWN_VECTOR,
  /** The inner PDU contained by the RPT PDU was malformed. */
  kRptStatusInvalidMessage = VECTOR_RPT_STATUS_INVALID_MESSAGE,
  /** The Command Class of an encapsulated RDM Command was invalid. */
  kRptStatusInvalidCommandClass = VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS
} rpt_status_code_t;

/** EPT status code definitions */
typedef enum
{
  /** The destination CID in the EPT PDU could not be found. */
  kEptStatusUnknownCid = VECTOR_EPT_STATUS_UNKNOWN_CID,
  /** An EPT PDU was received with an unsupported Vector. */
  kEptStatusUnknownVector = VECTOR_EPT_STATUS_UNKNOWN_VECTOR
} ept_status_code_t;

/** Disconnect reason defines for the BrokerDisconnectMsg. */
typedef enum
{
  /** The remote component is shutting down. */
  kRdmnetDisconnectShutdown = E133_DISCONNECT_SHUTDOWN,
  /** The remote component no longer has the ability to support this connection. */
  kRdmnetDisconnectCapacityExhausted = E133_DISCONNECT_CAPACITY_EXHAUSTED,
  /** The component must disconnect due to an internal hardware fault. */
  kRdmnetDisconnectHardwareFault = E133_DISCONNECT_HARDWARE_FAULT,
  /** The component must disconnect due to a software fault. */
  kRdmnetDisconnectSoftwareFault = E133_DISCONNECT_SOFTWARE_FAULT,
  /** The component must terminated because of a software reset. */
  kRdmnetDisconnectSoftwareReset = E133_DISCONNECT_SOFTWARE_RESET,
  /** Sent by brokers that are not on the desired Scope. */
  kRdmnetDisconnectIncorrectScope = E133_DISCONNECT_INCORRECT_SCOPE,
  /**
   * The component was reconfigured using RPT, and the new configuration requires connection
   * termination.
   */
  kRdmnetDisconnectRptReconfigure = E133_DISCONNECT_RPT_RECONFIGURE,
  /**
   * The component was reconfigured using LLRP, and the new configuration requires connection
   * termination.
   */
  kRdmnetDisconnectLlrpReconfigure = E133_DISCONNECT_LLRP_RECONFIGURE,
  /**
   * The component was reconfigured via some other means, and the new configuration requires
   * connection termination.
   */
  kRdmnetDisconnectUserReconfigure = E133_DISCONNECT_USER_RECONFIGURE
} rdmnet_disconnect_reason_t;

/** Connect status defines for the BrokerConnectReplyMsg. */
typedef enum
{
  /** Connection completed successfully. */
  kRdmnetConnectOk = E133_CONNECT_OK,
  /** The client's scope does not match the broker's scope. */
  kRdmnetConnectScopeMismatch = E133_CONNECT_SCOPE_MISMATCH,
  /** The broker has no further capacity for new clients. */
  kRdmnetConnectCapacityExceeded = E133_CONNECT_CAPACITY_EXCEEDED,
  /** The client's static UID matches another connected client's static UID. */
  kRdmnetConnectDuplicateUid = E133_CONNECT_DUPLICATE_UID,
  /** The client's Client Entry is invalid. */
  kRdmnetConnectInvalidClientEntry = E133_CONNECT_INVALID_CLIENT_ENTRY,
  /** The UID sent in the Client Entry PDU is malformed. */
  kRdmnetConnectInvalidUid = E133_CONNECT_INVALID_UID
} rdmnet_connect_status_t;

/** Dynamic UID Status Codes for the BrokerDynamicUidMapping struct. */
typedef enum
{
  /** The Dynamic UID Mapping was fetched or assigned successfully. */
  kRdmnetDynamicUidStatusOk = E133_DYNAMIC_UID_STATUS_OK,
  /** The corresponding request contained a malformed UID value. */
  kRdmnetDynamicUidStatusInvalidRequest = E133_DYNAMIC_UID_STATUS_INVALID_REQUEST,
  /** The requested Dynamic UID was not found in the broker's Dynamic UID mapping table. */
  kRdmnetDynamicUidStatusUidNotFound = E133_DYNAMIC_UID_STATUS_UID_NOT_FOUND,
  /** This RID has already been assigned a Dynamic UID by this broker. */
  kRdmnetDynamicUidStatusDuplicateRid = E133_DYNAMIC_UID_STATUS_DUPLICATE_RID,
  /** The broker has exhausted its capacity to generate Dynamic UIDs. */
  kRdmnetDynamicUidStatusCapacityExhausted = E133_DYNAMIC_UID_STATUS_CAPACITY_EXHAUSTED
} rdmnet_dynamic_uid_status_t;

/** A high-level reason for RDMnet connection failure. */
typedef enum
{
  /**
   * The connection was unable to be started because of an error returned from the system during a
   * lower-level socket call.
   */
  kRdmnetConnectFailSocketFailure,
  /**
   * The connection started but the TCP connection was never established. This could be because of
   * an incorrect address or port for the remote host or a network issue.
   */
  kRdmnetConnectFailTcpLevel,
  /**
   * The TCP connection was established, but no reply was received from the RDMnet protocol
   * handshake. This probably indicates an error in the remote broker.
   */
  kRdmnetConnectFailNoReply,
  /**
   * The remote broker rejected the connection at the RDMnet protocol level. A reason is provided
   * in the form of an rdmnet_connect_status_t.
   */
  kRdmnetConnectFailRejected
} rdmnet_connect_fail_event_t;

/** A high-level reason for RDMnet connection to be disconnected after successful connection. */
typedef enum
{
  /** The TCP connection was closed without an RDMnet disconnect message being sent. */
  kRdmnetDisconnectAbruptClose,
  /**
   * The TCP connection was deemed unhealthy due to no heartbeat message being received before the
   * heartbeat timeout.
   */
  kRdmnetDisconnectNoHeartbeat,
  /** The client was redirected to another broker address. */
  kRdmnetDisconnectRedirected,
  /** The remote component sent an RDMnet disconnect message with a reason code. */
  kRdmnetDisconnectGracefulRemoteInitiated,
  /** A disconnect was requested locally. */
  kRdmnetDisconnectGracefulLocalInitiated
} rdmnet_disconnect_event_t;

/**
 * Enumeration representing an action to take after an "RDM command received" callback completes.
 */
typedef enum
{
  /** Send an RDM ACK to the originating controller. */
  kRdmnetRdmResponseActionSendAck,
  /** Send an RDM NACK with reason to the originating controller. */
  kRdmnetRdmResponseActionSendNack,
  /** Do nothing; the application will send the response later. Be sure to save the command. */
  kRdmnetRdmResponseActionDefer,
  /** The command cannot be processed at this time - trigger another notification for this (non-LLRP) command later. */
  kRdmnetRdmResponseActionRetryLater
} rdmnet_rdm_response_action_t;

/**
 * This structure should not be manipulated directly - use the macros to access it:
 *
 * - RDMNET_SYNC_SEND_RDM_ACK()
 * - RDMNET_SYNC_SEND_RDM_NACK()
 * - RDMNET_SYNC_DEFER_RDM_RESPONSE()
 * - RDMNET_SYNC_RETRY_LATER()
 *
 * Contains information about an RDMnet RDM response to be sent synchronously from an RDMnet
 * callback, or the notification that the (non-LLRP) command notification must be retried later.
 */
typedef struct RdmnetSyncRdmResponse
{
  /** Represents the response action to take. */
  rdmnet_rdm_response_action_t response_action;

  /** Data associated with certain response actions (use the macros to access). */
  union
  {
    /**
     * The length of the response data which has been copied into the buffer given at initialization
     * time. Set to 0 for no data. Valid if response_action is #kRdmnetRdmResponseActionSendAck.
     */
    size_t response_data_len;
    /** The NACK reason code. Valid if response_action is #kRdmnetRdmResponseActionSendNack. */
    rdm_nack_reason_t nack_reason;
  } response_data;
} RdmnetSyncRdmResponse;

/**
 * @brief Indicate that an RDM ACK should be sent when this callback returns.
 *
 * If response_data_len_in != 0, data must be copied to the buffer provided at initialization time
 * before the callback returns.
 *
 * @param response_ptr Pointer to RdmnetSyncRdmResponse in which to set data.
 * @param response_data_len_in Length of the RDM response parameter data provided.
 */
#define RDMNET_SYNC_SEND_RDM_ACK(response_ptr, response_data_len_in)          \
  do                                                                          \
  {                                                                           \
    (response_ptr)->response_action = kRdmnetRdmResponseActionSendAck;        \
    (response_ptr)->response_data.response_data_len = (response_data_len_in); \
  } while (0)

/**
 * @brief Indicate that an RDM NACK should be sent when this callback returns.
 * @param response_ptr Pointer to RdmnetSyncRdmResponse in which to set data.
 * @param nack_reason_in RDM NACK reason code to send with the NACK response.
 */
#define RDMNET_SYNC_SEND_RDM_NACK(response_ptr, nack_reason_in)         \
  do                                                                    \
  {                                                                     \
    (response_ptr)->response_action = kRdmnetRdmResponseActionSendNack; \
    (response_ptr)->response_data.nack_reason = (nack_reason_in);       \
  } while (0)

/**
 * @brief Defer the RDM response to be sent later from another context.
 * @details Make sure to save any RDM command data for later processing using the appropriate API function.
 * @param response_ptr Pointer to RdmnetSyncRdmResponse in which to set data.
 */
#define RDMNET_SYNC_DEFER_RDM_RESPONSE(response_ptr) ((response_ptr)->response_action = kRdmnetRdmResponseActionDefer)

/**
 * @brief Trigger another notification for the (non-LLRP) RDM command on the next tick.
 * @param response_ptr Pointer to RdmnetSyncRdmResponse in which to set data.
 */
#define RDMNET_SYNC_RETRY_LATER(response_ptr) ((response_ptr)->response_action = kRdmnetRdmResponseActionRetryLater)

/**
 * Enumeration representing an action to take after an "EPT data received" callback completes.
 */
typedef enum
{
  /** Send an EPT data message to the originating EPT client. */
  kRdmnetEptResponseActionSendData,
  /** Send an EPT status message to the originating EPT client. */
  kRdmnetEptResponseActionSendStatus,
  /**
   * Do nothing; either the application will send the response later or no response is required. If
   * sending a respone later, be sure to save the data message.
   */
  kRdmnetEptResponseActionDefer
} rdmnet_ept_response_action_t;

/**
 * This structure should not be manipulated directly - use the macros to access it:
 *
 * - RDMNET_SYNC_SEND_EPT_DATA()
 * - RDMNET_SYNC_SEND_EPT_STATUS()
 * - RDMNET_SYNC_DEFER_EPT_RESPONSE()
 *
 * Contains information about an RDMnet EPT response to be sent synchronously from an RDMnet
 * callback.
 */
typedef struct RdmnetSyncEptResponse
{
  /** Represents the response action to take. */
  rdmnet_ept_response_action_t response_action;

  /** Data associated with certain response actions (use the macros to access). */
  union
  {
    /**
     * The length of the response data which has been copied into the buffer given at initialization
     * time. Valid if response_action is #kRdmnetEptResponseActionSendData.
     */
    size_t response_data_len;
    /** The EPT status code. Valid if response action is #kRdmnetEptResponseActionSendStatus. */
    ept_status_code_t status_code;
  } response_data;
} RdmnetSyncEptResponse;

/**
 * @brief Indicate that an EPT data message should be sent when this callback returns.
 *
 * Data must be copied to the buffer provided at initialization time before the callback returns.
 *
 * @param response_ptr Pointer to RdmnetSyncEptResponse in which to set data.
 * @param response_data_len_in Length of the EPT response data provided - must be nonzero.
 */
#define RDMNET_SYNC_SEND_EPT_DATA(response_ptr, response_data_len_in)         \
  do                                                                          \
  {                                                                           \
    (response_ptr)->response_action = kRdmnetEptResponseActionSendData;       \
    (response_ptr)->response_data.response_data_len = (response_data_len_in); \
  } while (0)

/**
 * @brief Indicate that an EPT status message should be sent when this callback returns.
 * @param response_ptr Pointer to RdmnetSyncEptResponse in which to set data.
 * @param status_code_in EPT status code to send with the response.
 */
#define RDMNET_SYNC_SEND_EPT_STATUS(response_ptr, status_code_in)         \
  do                                                                      \
  {                                                                       \
    (response_ptr)->response_action = kRdmnetEptResponseActionSendStatus; \
    (response_ptr)->response_data.status_code = (status_code_in);         \
  } while (0)

/**
 * @brief Defer the response to the EPT message, either to be sent later or because no response is necessary.
 * @param response_ptr Pointer to RdmnetSyncEptResponse in which to set data.
 */
#define RDMNET_SYNC_DEFER_EPT_RESPONSE(response_ptr) ((response_ptr)->response_action = kRdmnetEptResponseActionDefer)

/**
 * @brief An RDM command class, for RDMnet purposes.
 *
 * RDMnet disallows some RDM command classes. This type is used only with RDMnet APIs that
 * originate RDM commands.
 */
typedef enum
{
  /** An RDMnet RDM GET command. */
  kRdmnetCCGetCommand = 0x20,
  /** An RDMnet RDM SET command. */
  kRdmnetCCSetCommand = 0x30
} rdmnet_command_class_t;

/**
 * Network interface configuration information to give the RDMnet library at initialization. LLRP
 * multicast and discovery traffic will be restricted to the network interfaces given.
 */
typedef struct RdmnetNetintConfig
{
  /** An array of network interface IDs to which to restrict RDMnet traffic. If this is null, and no_netints is false,
      all system interfaces will be used. */
  const EtcPalMcastNetintId* netints;
  /** Size of netints array. Must be 0 if netints is null. */
  size_t num_netints;
  /** If this is true, no network interfaces will be used for multicast. If any are specified in netints, they will be
      ignored. */
  bool no_netints;
} RdmnetNetintConfig;

#define RDMNET_NETINT_CONFIG_DEFAULT_INIT \
  {                                       \
    NULL, 0, false                        \
  }

etcpal_error_t rdmnet_init(const EtcPalLogParams* log_params, const RdmnetNetintConfig* netint_config);
void           rdmnet_deinit(void);

const char* rdmnet_rpt_status_code_to_string(rpt_status_code_t code);
const char* rdmnet_ept_status_code_to_string(ept_status_code_t code);
const char* rdmnet_connect_fail_event_to_string(rdmnet_connect_fail_event_t event);
const char* rdmnet_disconnect_event_to_string(rdmnet_disconnect_event_t event);
const char* rdmnet_connect_status_to_string(rdmnet_connect_status_t code);
const char* rdmnet_disconnect_reason_to_string(rdmnet_disconnect_reason_t code);
const char* rdmnet_dynamic_uid_status_to_string(rdmnet_dynamic_uid_status_t code);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* RDMNET_COMMON_H_ */
