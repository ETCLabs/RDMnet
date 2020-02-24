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

/*!
 * \file rdmnet/client.h
 * \brief API definitions used by RDMnet clients (controllers and devices)
 */

#ifndef RDMNET_CLIENT_H_
#define RDMNET_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/inet.h"
#include "rdm/uid.h"
#include "rdmnet/common.h"

/*!
 * \addtogroup rdmnet_api_common
 * @{
 */

/*! A handle to a scope that an RDMnet client participates in. */
typedef int rdmnet_client_scope_t;
/*! An invalid RDMnet client scope handle value. */
#define RDMNET_CLIENT_SCOPE_INVALID -1

/*!
 * \brief A destination address for an RDM command in RDMnet's RPT protocol.
 * \details See \ref roles_and_addressing and \ref devices_and_gateways for more information.
 */
typedef struct RdmnetDestinationAddr
{
  /*! The UID of the RDMnet component to which this command is addressed. */
  RdmUid rdmnet_uid;
  /*!
   * The endpoint on the RDMnet component to which this message is addressed. If addressing the
   * default (root) responder of an RDMnet device, set this to E133_NULL_ENDPOINT.
   */
  uint16_t endpoint;
  /*!
   * The UID of the RDM responder to which this message is addressed. If addressing the default
   * (root) responder of an RDMnet device, this should be the same as rdmnet_uid.
   */
  RdmUid rdm_uid;
  /*! The sub-device to which this command is addressed, or 0 for the root device. */
  uint16_t subdevice;
} RdmnetDestinationAddr;

/*! Information provided by the library about a successful RDMnet client connection. */
typedef struct RdmnetClientConnectedInfo
{
  /*! The IP address and port of the remote broker to which we have connected. */
  EtcPalSockAddr broker_addr;
  /*! The DNS name of the broker, if it was discovered using DNS-SD; otherwise an empty string. */
  const char* broker_name;
  /*! The CID of the connected broker. */
  EtcPalUuid broker_cid;
  /*! The RDM UID of the connected broker. */
  RdmUid broker_uid;
} RdmnetClientConnectedInfo;

/*! Information provided by the library about an unsuccessful RDMnet client connection. */
typedef struct RdmnetClientConnectFailedInfo
{
  /*! The high-level reason that this connection failed. */
  rdmnet_connect_fail_event_t event;
  /*!
   * The system error code associated with the failure; valid if event is
   * kRdmnetConnectFailSocketFailure or kRdmnetConnectFailTcpLevel.
   */
  etcpal_error_t socket_err;
  /*!
   * The reason given in the RDMnet-level connection refuse message. Valid if event is
   * kRdmnetConnectFailRejected.
   */
  rdmnet_connect_status_t rdmnet_reason;
  /*!
   * \brief Whether the connection will be retried automatically.
   *
   * If this is true, the connection will be retried on the relevant scope; expect further
   * notifications of connection success or failure. If false, the rdmnet_client_scope_t handle
   * associated with the scope is invalidated, and the scope must be created again. This indicates
   * that the connection failed for a reason that usually must be corrected by a user or
   * application developer. Some possible reasons for this to be false include:
   * - The wrong scope was specified for a statically-configured broker
   * - A static UID was given that was invalid or duplicate with another UID in the system
   */
  bool will_retry;
} RdmnetClientConnectFailedInfo;

/*! Information provided by the library about an RDMnet client connection that disconnected after a
 *  successful connection. */
typedef struct RdmnetClientDisconnectedInfo
{
  /*! The high-level reason for the disconnect. */
  rdmnet_disconnect_event_t event;
  /*!
   * The system error code associated with the disconnect; valid if event is
   * kRdmnetDisconnectAbruptClose.
   */
  etcpal_error_t socket_err;
  /*!
   * The reason given in the RDMnet-level disconnect message. Valid if event is
   * kRdmnetDisconnectGracefulRemoteInitiated.
   */
  rdmnet_disconnect_reason_t rdmnet_reason;
  /*!
   * \brief Whether the connection will be retried automatically.
   *
   * There are currently no conditions that will cause this to be false; therefore, disconnection
   * events after a successful connection will always lead to the connection being retried
   * automatically. This field exists for potential future usage.
   */
  bool will_retry;
} RdmnetClientDisconnectedInfo;

/*!
 * A set of configuration information for a single scope in which an RDMnet client is
 * participating.
 */
typedef struct RdmnetScopeConfig
{
  /*!
   * The scope string. Scope strings are UTF-8, and their maximum length including NULL terminator
   * is #E133_SCOPE_STRING_PADDED_LENGTH, which is derived from the requirements of DNS and DNS-SD.
   */
  const char* scope;
  /*!
   * The broker address to which to connect, if a static broker has been configured. If this member
   * contains a valid EtcPalIpAddr and port, discovery using DNS-SD will be bypassed and a
   * connection will be attempted directly to the address given.
   */
  EtcPalSockAddr static_broker_addr;
} RdmnetScopeConfig;

/*!
 * \brief A default-value initializer for an RdmnetScopeConfig struct.
 *
 * Usage:
 * \code
 * RdmnetScopeConfig scope_config = RDMNET_SCOPE_CONFIG_DEFAULT_INIT;
 * // scope_config holds the configuration for the default RDMnet scope with no statically-configured broker.
 * \endcode
 */
#define RDMNET_SCOPE_CONFIG_DEFAULT_INIT              \
  {                                                   \
    E133_DEFAULT_SCOPE, { 0, ETCPAL_IP_INVALID_INIT } \
  }

/*!
 * \brief Initialize an RdmnetScopeConfig struct with a scope string.
 *
 * Scopes are resolved using RDMnet discovery (DNS-SD) by default; to override this behavior with a
 * static broker address and port, use rdmnet_set_static_scope().
 *
 * \param configptr Pointer to RdmnetScopeConfig.
 * \param scope_str UTF-8 scope string (const char *), must remain valid for as long as this scope
 *                  config.
 */
#define RDMNET_CLIENT_SET_SCOPE(configptr, scope_str)           \
  do                                                            \
  {                                                             \
    (configptr)->scope = scope_str;                             \
    ETCPAL_IP_SET_INVALID(&(configptr)->static_broker_addr.ip); \
  } while (0)

/*!
 * \brief Initialize an RdmnetScopeConfig struct with the default RDMnet scope.
 *
 * Scopes are resolved using RDMnet discovery (DNS-SD) by default; to override this behavior with a
 * static broker address and port, use rdmnet_set_static_default_scope().
 *
 * \param configptr Pointer to RdmnetScopeConfig.
 */
#define RDMNET_CLIENT_SET_DEFAULT_SCOPE(configptr)              \
  do                                                            \
  {                                                             \
    (configptr)->scope = E133_DEFAULT_SCOPE;                    \
    ETCPAL_IP_SET_INVALID(&(configptr)->static_broker_addr.ip); \
  } while (0)

/*!
 * \brief Initialize an RdmnetScopeConfig struct with a scope string and static broker address.
 *
 * DNS-SD discovery will be bypassed and broker connection will be attempted using the address and
 * port given.
 *
 * \param configptr Pointer to RdmnetScopeConfig.
 * \param scope_str UTF-8 scope string (const char *), must remain valid for as long as this scope
 *                  config.
 * \param broker_addr Address and port for a static broker (EtcPalSockAddr).
 */
#define RDMNET_CLIENT_SET_STATIC_SCOPE(configptr, scope_str, broker_addr) \
  do                                                                      \
  {                                                                       \
    (configptr)->scope = (scope_str);                                     \
    (configptr)->static_broker_addr = (broker_addr);                      \
  } while (0)

/*!
 * \brief Initialize an RdmnetScopeConfig struct with the default RDMnet scope and a static broker
 *        address.
 *
 * DNS-SD discovery will be bypassed and broker connection will be attempted using the address and
 * port given.
 *
 * \param configptr Pointer to RdmnetScopeConfig.
 * \param broker_addr Address and port for a static broker (EtcPalSockAddr)
 */
#define RDMNET_CLIENT_SET_STATIC_DEFAULT_SCOPE(configptr, broker_addr) \
  do                                                                   \
  {                                                                    \
    (configptr)->scope = E133_DEFAULT_SCOPE;                           \
    (configptr)->static_broker_addr = (broker_addr);                   \
  } while (0)

/*!
 * @}
 */

#endif /* RDMNET_CLIENT_H_ */
