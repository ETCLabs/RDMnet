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

#include "rdmnet/common.h"

#include "etcpal/common.h"

/*************************** Function definitions ****************************/

/*!
 * \brief Initialize the RDMnet library.
 *
 * Does all initialization required before the RDMnet API modules can be used. Starts the message
 * dispatch thread.
 *
 * \param[in] log_params Optional: log parameters for the RDMnet library to use to log messages. If
 *                       NULL, no logging will be performed.
 * \param[in] netint_config Optional: a set of network interfaces to which to restrict multicast
 *                          operation.
 * \return #kEtcPalErrOk: Initialization successful.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNoNetints: No network interfaces found on the system.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 * \return Other error codes are possible from the initialization of EtcPal.
 */
etcpal_error_t rdmnet_init(const EtcPalLogParams* log_params, const RdmnetNetintConfig* netint_config)
{
  ETCPAL_UNUSED_ARG(log_params);
  ETCPAL_UNUSED_ARG(netint_config);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Deinitialize the RDMnet library.
 *
 * Closes all connections, deallocates all resources and joins the background thread. No RDMnet API
 * functions are usable after this function is called.
 */
void rdmnet_deinit(void)
{
}

/*!
 * \brief Get a string representation of an RPT status code.
 */
const char* rdmnet_rpt_status_code_to_string(rpt_status_code_t code)
{
  ETCPAL_UNUSED_ARG(code);
  return NULL;
}

// clang-format off
static const char* kRdmnetConnectFailEventStrings[] =
{
  "Socket failure on connection initiation",
  "TCP connection failure",
  "No reply received to RDMnet handshake",
  "RDMnet connection rejected"
};
#define NUM_CONNECT_FAIL_EVENT_STRINGS (sizeof(kRdmnetConnectFailEventStrings) / sizeof(const char*))
// clang-format on

/*!
 * \brief Get a string description of an RDMnet connection failure event.
 *
 * An RDMnet connection failure event provides a high-level reason why an RDMnet connection failed.
 *
 * \param event Event code.
 * \return String, or NULL if event is invalid.
 */
const char* rdmnet_connect_fail_event_to_string(rdmnet_connect_fail_event_t event)
{
  if (event >= 0 && event < NUM_CONNECT_FAIL_EVENT_STRINGS)
    return kRdmnetConnectFailEventStrings[event];
  return NULL;
}

// clang-format off
static const char* kRdmnetDisconnectEventStrings[] =
{
  "Connection was closed abruptly",
  "No heartbeat message was received within the heartbeat timeout",
  "Connection was redirected to another Broker",
  "Remote component sent a disconnect message",
  "Local component sent a disconnect message"
};
#define NUM_DISCONNECT_EVENT_STRINGS (sizeof(kRdmnetDisconnectEventStrings) / sizeof(const char*))
// clang-format on

/*!
 * \brief Get a string description of an RDMnet disconnect event.
 *
 * An RDMnet disconnect event provides a high-level reason why an RDMnet connection was
 * disconnected.
 *
 * \param event Event code.
 * \return String, or NULL if event is invalid.
 */
const char* rdmnet_disconnect_event_to_string(rdmnet_disconnect_event_t event)
{
  if (event >= 0 && event < NUM_DISCONNECT_EVENT_STRINGS)
    return kRdmnetDisconnectEventStrings[event];
  return NULL;
}

// clang-format off
static const char* kRdmnetConnectStatusStrings[] =
{
  "Successful connection",
  "Broker/Client scope mismatch",
  "Broker connection capacity exceeded",
  "Duplicate UID detected",
  "Invalid client entry",
  "Invalid UID"
};
#define NUM_CONNECT_STATUS_STRINGS (sizeof(kRdmnetConnectStatusStrings) / sizeof(const char*))
// clang-format on

/*!
 * \brief Get a string description of an RDMnet connect status code.
 *
 * Connect status codes are returned by a broker in a connect reply message after a client attempts
 * to connect.
 *
 * \param code Connect status code.
 * \return String, or NULL if code is invalid.
 */
const char* rdmnet_connect_status_to_string(rdmnet_connect_status_t code)
{
  if (code >= 0 && code < NUM_CONNECT_STATUS_STRINGS)
    return kRdmnetConnectStatusStrings[code];
  return NULL;
}

// clang-format off
static const char* kRdmnetDisconnectReasonStrings[] =
{
  "Component shutting down",
  "Component can no longer support this connection",
  "Hardware fault",
  "Software fault",
  "Software reset",
  "Incorrect scope",
  "Component reconfigured via RPT",
  "Component reconfigured via LLRP",
  "Component reconfigured by non-RDMnet method"
};
#define NUM_DISCONNECT_REASON_STRINGS (sizeof(kRdmnetDisconnectReasonStrings) / sizeof(const char*))
// clang-format on

/*!
 * \brief Get a string description of an RDMnet disconnect reason code.
 *
 * Disconnect reason codes are sent by a broker or client that is disconnecting.
 *
 * \param code Disconnect reason code.
 * \return String, or NULL if code is invalid.
 */
const char* rdmnet_disconnect_reason_to_string(rdmnet_disconnect_reason_t code)
{
  if (code >= 0 && code < NUM_DISCONNECT_REASON_STRINGS)
    return kRdmnetDisconnectReasonStrings[code];
  return NULL;
}

// clang-format off
static const char* kRdmnetDynamicUidStatusStrings[] =
{
  "Dynamic UID fetched or assigned successfully",
  "The Dynamic UID request was malformed",
  "The requested Dynamic UID was not found",
  "This RID has already been assigned a Dynamic UID",
  "Dynamic UID capacity exhausted"
};
#define NUM_DYNAMIC_UID_STATUS_STRINGS (sizeof(kRdmnetDynamicUidStatusStrings) / sizeof(const char*))
// clang-format on

/*!
 * \brief Get a string description of an RDMnet Dynamic UID status code.
 *
 * Dynamic UID status codes are returned by a broker in response to a request for dynamic UIDs by a
 * client.
 *
 * \param code Dynamic UID status code.
 * \return String, or NULL if code is invalid.
 */
const char* rdmnet_dynamic_uid_status_to_string(rdmnet_dynamic_uid_status_t code)
{
  if (code >= 0 && code < NUM_DYNAMIC_UID_STATUS_STRINGS)
    return kRdmnetDynamicUidStatusStrings[code];
  return NULL;
}
