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
 * @file rdmnet/discovery.h
 * @brief RDMnet Discovery API definitions
 *
 * Functions to discover a Broker and/or register a Broker for discovery. Uses mDNS and DNS-SD under
 * the hood.
 */

#ifndef RDMNET_DISCOVERY_H_
#define RDMNET_DISCOVERY_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "etcpal/error.h"
#include "etcpal/uuid.h"
#include "etcpal/socket.h"
#include "rdmnet/common.h"
#include "rdmnet/defs.h"

/**
 * @defgroup rdmnet_disc Discovery
 * @ingroup rdmnet_api
 * @brief Handle RDMnet discovery using mDNS and DNS-SD.
 *
 * RDMnet uses DNS-SD (aka Bonjour) as its network discovery method. These functions encapsulate
 * system DNS-SD and mDNS functionality (Bonjour, Avahi, etc.) and provide functions for doing
 * broker discovery and service registration.
 *
 * Typically, this API is called automatically when using the role APIs:
 *
 * * @ref rdmnet_controller
 * * @ref rdmnet_device
 * * @ref rdmnet_broker
 * * @ref rdmnet_ept_client
 *
 * And thus these functions should not typically need to be used directly.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief An extra key/value pair in a broker's DNS TXT record that does not have a standard RDMnet use.
 *
 * DNS-SD TXT records are key/value pairs where the key is printable ASCII and the value is opaque
 * binary data. The total length of the key plus the value cannot exceed 255 bytes.
 */
typedef struct RdmnetDnsTxtRecordItem
{
  const char*    key;       /**< The key for this item. */
  const uint8_t* value;     /**< The value data for this item. */
  uint8_t        value_len; /**< The length of the value data. */
} RdmnetDnsTxtRecordItem;

/** A handle to an RDMnet broker's DNS-SD registration. */
typedef struct RdmnetBrokerRegisterRef* rdmnet_registered_broker_t;
/** An invalid registered broker value. */
#define RDMNET_REGISTERED_BROKER_INVALID NULL

/** Information about a broker discovered or registered using DNS-SD. */
typedef struct RdmnetBrokerDiscInfo
{
  /** The broker's CID. */
  EtcPalUuid cid;
  /** The broker's UID. */
  RdmUid uid;
  /** The E1.33 version that the broker supports. */
  int e133_version;
  /**
   * @brief The broker's service instance name.
   *
   * A service instance name uniquely identifies a specific broker on a given network segment. They
   * are a maximum of 63 bytes in length, can contain any UTF-8 character, and should be
   * configurable by a user.
   */
  const char* service_instance_name;
  /** The port on which the broker is listening for RDMnet connections. */
  uint16_t port;
  /** An array of IP addresses at which the broker is listening for RDMnet connections. */
  const EtcPalIpAddr* listen_addrs;
  /** An array of local network interface IDs for reaching each respective address in listen_addrs. */
  const unsigned int* listen_addr_netints;
  /** Size of the listen_addrs and listen_addr_netints arrays. */
  size_t num_listen_addrs;
  /** The broker's RDMnet scope. */
  const char* scope;
  /** The broker's product model name. */
  const char* model;
  /** The name of the broker's manufacturer. */
  const char* manufacturer;

  /** Any additional non-standard items that were present in the discovered broker's TXT record. */
  const RdmnetDnsTxtRecordItem* additional_txt_items;
  /** Size of the additional_txt_items array. */
  size_t num_additional_txt_items;
} RdmnetBrokerDiscInfo;

/**
 * @name Registered Broker Callbacks
 * @{
 */

/**
 * @brief A broker has been registered successfully with the DNS-SD service.
 * @param[in] handle Handle to the registered broker instance.
 * @param[in] assigned_service_instance_name The broker's service instance name. Note that this might be different from
 *                                           the one given at config time because of DNS-SD's uniqueness negotiation.
 * @param[in] context Context pointer that was given at the creation of the registered broker instance.
 */
typedef void (*RdmnetDiscBrokerRegisteredCallback)(rdmnet_registered_broker_t handle,
                                                   const char*                assigned_service_instance_name,
                                                   void*                      context);

/**
 * @brief Broker registration has failed.
 * @param[in] handle Handle to the registered broker instance which has failed to register.
 * @param[in] platform_error Platform-specific error code from the underlying DNS-SD service (e.g. Bonjour or Avahi).
 * @param[in] context Context pointer that was given at the creation of the registered broker instance.
 */
typedef void (*RdmnetDiscBrokerRegisterFailedCallback)(rdmnet_registered_broker_t handle,
                                                       int                        platform_error,
                                                       void*                      context);

/**
 * @brief Another broker has been found on the scope on which this broker is registered.
 * @param[in] handle Handle to the registered broker instance.
 * @param[in] broker_info Information about the other broker that has been found on the same scope.
 * @param[in] context Context pointer that was given at the creation of the registered broker instance.
 */
typedef void (*RdmnetDiscOtherBrokerFoundCallback)(rdmnet_registered_broker_t  handle,
                                                   const RdmnetBrokerDiscInfo* broker_info,
                                                   void*                       context);

/**
 * @brief A broker which was previously detected on the same scope as a registered broker has been lost.
 * @param[in] handle Handle to the registered broker instance.
 * @param[in] scope Scope string of the scope on which the broker is registered.
 * @param[in] service_instance_name Service instance name of the other broker that has been lost.
 * @param[in] context Context pointer that was given at the creation of the registered broker instance.
 */
typedef void (*RdmnetDiscOtherBrokerLostCallback)(rdmnet_registered_broker_t handle,
                                                  const char*                scope,
                                                  const char*                service_instance_name,
                                                  void*                      context);

/**
 * @}
 */

/** A set of notification callbacks received by a registered broker instance. */
typedef struct RdmnetDiscBrokerCallbacks
{
  RdmnetDiscBrokerRegisteredCallback     broker_registered;      /**< Required. */
  RdmnetDiscBrokerRegisterFailedCallback broker_register_failed; /**< Required. */
  RdmnetDiscOtherBrokerFoundCallback     other_broker_found;     /**< Required. */
  RdmnetDiscOtherBrokerLostCallback      other_broker_lost;      /**< Required. */
  void* context; /**< (optional) Pointer to opaque data passed back with each callback. */
} RdmnetDiscBrokerCallbacks;

/** A set of information that defines the parameters of an RDMnet broker registered with DNS-SD. */
typedef struct RdmnetBrokerRegisterConfig
{
  /************************************************************************************************
   * Required Values
   ***********************************************************************************************/

  /** The broker's CID. */
  EtcPalUuid cid;
  /** The broker's UID. */
  RdmUid uid;
  /**
   * @brief The broker's requested service instance name.
   *
   * A service instance name uniquely identifies a specific broker on a given network segment. They
   * are a maximum of 63 bytes in length, can contain any UTF-8 character, and should be
   * configurable by a user. The underlying DNS-SD library will do a standard uniqueness check and
   * may register the broker with a different name if this one already exists.
   */
  const char* service_instance_name;
  /** The port on which the broker is listening for RDMnet connections. */
  uint16_t port;
  /**
   * @brief An array of network interface indexes on which the broker should respond to mDNS queries.
   * @details NULL = use all interfaces.
   */
  const unsigned int* netints;
  /** Size of the netints array. */
  size_t num_netints;
  /** The broker's RDMnet scope. */
  const char* scope;
  /** The broker's product model name. */
  const char* model;
  /** The name of the broker's manufacturer. */
  const char* manufacturer;

  /** Any additional non-standard items to add to the broker's TXT record. */
  const RdmnetDnsTxtRecordItem* additional_txt_items;
  /** Size of the additional_txt_items array. */
  size_t num_additional_txt_items;

  /** A set of callbacks to receive notifications about the registered broker. */
  RdmnetDiscBrokerCallbacks callbacks;
} RdmnetBrokerRegisterConfig;

/**
 * @brief A default-value initializer for an RdmnetBrokerRegisterConfig struct.
 *
 * Usage:
 * @code
 * RdmnetBrokerRegisterConfig config = RDMNET_BROKER_REGISTER_CONFIG_DEFAULT_INIT;
 * // Now fill in the required portions as necessary with your data...
 * @endcode
 */
#define RDMNET_BROKER_REGISTER_CONFIG_DEFAULT_INIT                                            \
  {                                                                                           \
    {{0}}, {0}, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL, 0, { NULL, NULL, NULL, NULL, NULL } \
  }

/** A handle to a monitored RDMnet scope. */
typedef struct RdmnetScopeMonitorRef* rdmnet_scope_monitor_t;
/** An invalid monitored scope handle value. */
#define RDMNET_SCOPE_MONITOR_INVALID NULL

/**
 * @name Scope Monitor Callbacks
 * @{
 */

/**
 * @brief An RDMnet broker has been found on a monitored scope.
 * @param handle Handle to the monitored scope on which the broker was found.
 * @param broker_info Information about the broker that was found.
 * @param context Context pointer that was given at the creation of the scope monitor instance.
 */
typedef void (*RdmnetDiscBrokerFoundCallback)(rdmnet_scope_monitor_t      handle,
                                              const RdmnetBrokerDiscInfo* broker_info,
                                              void*                       context);

/**
 * @brief Updated information has been received for a previously-discovered RDMnet broker.
 * @param handle Handle to the monitored scope on which the broker was updated.
 * @param updated_broker_info Updated broker information.
 * @param context Context pointer that was given at the creation of the scope monitor instance.
 */
typedef void (*RdmnetDiscBrokerUpdatedCallback)(rdmnet_scope_monitor_t      handle,
                                                const RdmnetBrokerDiscInfo* updated_broker_info,
                                                void*                       context);

/**
 * @brief A previously-discovered RDMnet broker has been lost on a monitored scope.
 * @param handle Handle to the monitored scope on which the broker was lost.
 * @param scope Scope string of the monitored scope.
 * @param service_instance_name Service instance name of the broker that has been lost.
 * @param context Context pointer that was given at the creation of the scope monitor instance.
 */
typedef void (*RdmnetDiscBrokerLostCallback)(rdmnet_scope_monitor_t handle,
                                             const char*            scope,
                                             const char*            service_instance_name,
                                             void*                  context);

/**
 * @}
 */

/** A set of notification callbacks received by a scope monitor instance. */
typedef struct RdmnetScopeMonitorCallbacks
{
  RdmnetDiscBrokerFoundCallback   broker_found;   /**< Required. */
  RdmnetDiscBrokerUpdatedCallback broker_updated; /**< Required. */
  RdmnetDiscBrokerLostCallback    broker_lost;    /**< Required. */
  void*                           context; /**< (optional) Pointer to opaque data passed back with each callback. */
} RdmnetScopeMonitorCallbacks;

/** A set of information that defines the parameters of an RDMnet scope to be monitored using DNS-SD. */
typedef struct RdmnetScopeMonitorConfig
{
  /************************************************************************************************
   * Required Values
   ***********************************************************************************************/

  /** Scope string of the scope to be monitored. */
  const char* scope;
  /** A set of callbacks to receive notifications about the monitored scope. */
  RdmnetScopeMonitorCallbacks callbacks;

  /************************************************************************************************
   * Optional Values
   ***********************************************************************************************/

  /** (optional) The search domain to use for DNS discovery. NULL to use the default search domain(s). */
  const char* domain;
} RdmnetScopeMonitorConfig;

/**
 * @brief A default-value initializer for an RdmnetScopeMonitorConfig struct.
 *
 * Usage:
 * @code
 * RdmnetScopeMonitorConfig config = RDMNET_SCOPE_MONITOR_CONFIG_DEFAULT_INIT;
 * // Now fill in the required portions as necessary with your data...
 * @endcode
 */
#define RDMNET_SCOPE_MONITOR_CONFIG_DEFAULT_INIT \
  {                                              \
    E133_DEFAULT_SCOPE, {NULL, NULL, NULL}, NULL \
  }

void rdmnet_broker_register_config_init(RdmnetBrokerRegisterConfig* config);
void rdmnet_broker_register_config_set_callbacks(RdmnetBrokerRegisterConfig*            config,
                                                 RdmnetDiscBrokerRegisteredCallback     broker_registered,
                                                 RdmnetDiscBrokerRegisterFailedCallback broker_register_failed,
                                                 RdmnetDiscOtherBrokerFoundCallback     other_broker_found,
                                                 RdmnetDiscOtherBrokerLostCallback      other_broker_lost,
                                                 void*                                  context);

void rdmnet_scope_monitor_config_init(RdmnetScopeMonitorConfig* config);
void rdmnet_scope_monitor_config_set_callbacks(RdmnetScopeMonitorConfig*       config,
                                               RdmnetDiscBrokerFoundCallback   broker_found,
                                               RdmnetDiscBrokerUpdatedCallback broker_udpated,
                                               RdmnetDiscBrokerLostCallback    broker_lost,
                                               void*                           context);

etcpal_error_t rdmnet_disc_start_monitoring(const RdmnetScopeMonitorConfig* config,
                                            rdmnet_scope_monitor_t*         handle,
                                            int*                            platform_specific_error);
void           rdmnet_disc_stop_monitoring(rdmnet_scope_monitor_t handle);
void           rdmnet_disc_stop_monitoring_all(void);

etcpal_error_t rdmnet_disc_register_broker(const RdmnetBrokerRegisterConfig* config,
                                           rdmnet_registered_broker_t*       handle);
void           rdmnet_disc_unregister_broker(rdmnet_registered_broker_t handle);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* RDMNET_DISCOVERY_H_ */
