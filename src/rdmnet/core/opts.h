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
 * @file rdmnet/core/opts.h
 * @brief RDMnet configuration options.
 *
 * Default values for all of RDMnet's @ref rdmnetopts "compile-time configuration options".
 */

#ifndef RDMNET_CORE_OPTS_H_
#define RDMNET_CORE_OPTS_H_

/**
 * @defgroup rdmnetopts RDMnet Configuration Options
 * @brief Compile-time configuration options for RDMnet.
 *
 * Default values can be overriden in the compiler settings, or by defining RDMNET_HAVE_CONFIG_H in
 * the compiler settings and providing a file called rdmnet_config.h with overridden definitions in
 * it.
 */

#if RDMNET_HAVE_CONFIG_H
#include "rdmnet_config.h"
#endif

#include "etcpal/thread.h"

/* Some option hints based on well-known compile definitions */

/** @cond Internal definitions */

/* Are we being compiled for a full-featured OS? */
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION)
#define RDMNET_FULL_OS_AVAILABLE_HINT 1
#else
#define RDMNET_FULL_OS_AVAILABLE_HINT 0
#endif

/* Are we being compiled in/for a Microsoft Windows environment? */
#ifdef _WIN32
#define RDMNET_WINDOWS_HINT 1
#else
#define RDMNET_WINDOWS_HINT 0
#endif

/** @endcond */

/********************************* Global ************************************/

/**
 * @defgroup rdmnetopts_global Global
 * @ingroup rdmnetopts
 *
 * Global or library-wide configuration options. Any options with *_MAX_* in the name are
 * applicable only to compilations with dynamic memory disabled (#RDMNET_DYNAMIC_MEM = 0, most
 * common in embedded toolchains).
 *
 * @{
 */

/**
 * @brief Use dynamic memory allocation.
 *
 * If defined nonzero, RDMnet manages memory dynamically using malloc() and free() from stdlib.h.
 * Otherwise, RDMnet uses static arrays and fixed-size pools through EtcPal's @ref etcpal_mempool.
 * The size of the pools and arrays is controlled with other config options starting with
 * RDMNET_MAX_.
 *
 * If not defined in rdmnet_config.h, the library attempts to guess using standard OS predefined
 * macros whether it is being compiled for a full-featured OS, in which case this option is defined
 * to 1 (otherwise an embedded application is assumed and it is defined to 0).
 */
#ifndef RDMNET_DYNAMIC_MEM
#define RDMNET_DYNAMIC_MEM RDMNET_FULL_OS_AVAILABLE_HINT
#endif

/**
 * @brief A string which will be prepended to all log messages from the RDMnet library.
 */
#ifndef RDMNET_LOG_MSG_PREFIX
#define RDMNET_LOG_MSG_PREFIX "RDMnet: "
#endif

/**
 * @brief The debug assert used by the RDMnet library.
 *
 * By default, just uses the C library assert. If redefining this, it must be redefined as a macro
 * taking a single argument (the assertion expression).
 */
#ifndef RDMNET_ASSERT
#include <assert.h>
#define RDMNET_ASSERT(expr) assert(expr)
#endif

/**
 * @}
 */

/**
 * @defgroup rdmnetopts_client Client
 * @ingroup rdmnetopts
 *
 * Options that affect the RDMnet Client APIs. Any options with *_MAX_* in the name are applicable
 * only to compilations with dynamic memory disabled (#RDMNET_DYNAMIC_MEM = 0, most common in
 * embedded toolchains).
 *
 * @{
 */

/**
 * @brief The maximum number of RDMnet Controller instances that an application can create.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. A typical application will only need one
 * controller instance (which can communicate on an arbitrary number of scopes).
 */
#ifndef RDMNET_MAX_CONTROLLERS
#define RDMNET_MAX_CONTROLLERS 0
#endif

/**
 * @brief The maximum number of RDMnet Device instances that an application can create.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. A typical application will only need one
 * device instance.
 */
#ifndef RDMNET_MAX_DEVICES
#define RDMNET_MAX_DEVICES 1
#endif

/**
 * @brief The maximum number of EPT Client instances that an application can create.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_EPT_CLIENTS
#define RDMNET_MAX_EPT_CLIENTS 0
#endif

/**
 * @brief The maximum number of scopes on which each controller instance can communicate.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_SCOPES_PER_CONTROLLER
#define RDMNET_MAX_SCOPES_PER_CONTROLLER 1
#endif

/**
 * @brief The maximum number of nonzero endpoints that can be added to each device instance.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_ENDPOINTS_PER_DEVICE
#define RDMNET_MAX_ENDPOINTS_PER_DEVICE 1
#endif

/**
 * @brief The maximum number of responders that can be added to each device instance.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_RESPONDERS_PER_DEVICE
#define RDMNET_MAX_RESPONDERS_PER_DEVICE 1
#endif

/**
 * @brief The maximum number of responders that can be added to each device endpoint.
 *
 * This can be set to be lower than #RDMNET_MAX_RESPONDERS_PER_DEVICE to save some memory for certain buffers. Otherwise
 * this just defaults to the maximum for the whole device.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_RESPONDERS_PER_DEVICE_ENDPOINT
#define RDMNET_MAX_RESPONDERS_PER_DEVICE_ENDPOINT RDMNET_MAX_RESPONDERS_PER_DEVICE
#endif

/**
 * @brief The maximum number of EPT sub-protocols supported on a local EPT client instance.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_PROTOCOLS_PER_EPT_CLIENT
#define RDMNET_MAX_PROTOCOLS_PER_EPT_CLIENT 5
#endif

/**
 * @brief The maximum number of RDM responses that can be sent from an RPT Client at once in an
 *        ACK_OVERFLOW response.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. For applications which desire static
 * memory, this parameter should be set to the maximum number of RDM ACK_OVERFLOW responses the
 * application ever anticipates generating in response to an RDMnet request, based on the client's
 * parameter data. Since RDMnet gateways cannot anticipate how many ACK_OVERFLOW responses will be
 * received from a downstream RDM responder, a reasonable guess may need to be made based on the
 * RDM standard.
 */
#ifndef RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES
#define RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES 2
#endif

/**
 * @}
 */

/** @cond Internal/derived definitions */

#ifdef RDMNET_MAX_RPT_CLIENTS
#undef RDMNET_MAX_RPT_CLIENTS
#endif
#define RDMNET_MAX_RPT_CLIENTS (RDMNET_MAX_CONTROLLERS + RDMNET_MAX_DEVICES)

#ifdef RDMNET_MAX_CLIENTS
#undef RDMNET_MAX_CLIENTS
#endif
#define RDMNET_MAX_CLIENTS (RDMNET_MAX_RPT_CLIENTS + RDMNET_MAX_EPT_CLIENTS)

#if !RDMNET_MAX_CLIENTS
#undef RDMNET_MAX_CLIENTS
#define RDMNET_MAX_CLIENTS 1
#endif

#if RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES < 1
#undef RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES
#define RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES 1
#endif

#ifndef RDMNET_MAX_CONNECTIONS
#define RDMNET_MAX_CONNECTIONS RDMNET_MAX_CLIENTS
#endif

#if RDMNET_MAX_CONNECTIONS < 1
#undef RDMNET_MAX_CONNECTIONS
#define RDMNET_MAX_CONNECTIONS 1
#endif

/** @endcond */

/**
 * @defgroup rdmnetopts_core Core
 * @ingroup rdmnetopts
 *
 * Options that affect the RDMnet core library. Any options with *_MAX_* in the name are applicable
 * only to compilations with dynamic memory disabled (#RDMNET_DYNAMIC_MEM = 0, most common in
 * embedded toolchains).
 *
 * @{
 */

/**
 * @brief The maximum number of ClientEntryData structures that can be returned with a parsed
 *        message.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_PARSER_MAX_CLIENT_ENTRIES
#define RDMNET_PARSER_MAX_CLIENT_ENTRIES 5
#endif

#if RDMNET_PARSER_MAX_CLIENT_ENTRIES < 1
#undef RDMNET_PARSER_MAX_CLIENT_ENTRIES
#define RDMNET_PARSER_MAX_CLIENT_ENTRIES 1
#endif

/**
 * @brief The maximum number of RdmnetEptSubProtocol structures that can be returned with a parsed
 *        message.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_PARSER_MAX_EPT_SUBPROTS
#define RDMNET_PARSER_MAX_EPT_SUBPROTS 5
#endif

#if RDMNET_PARSER_MAX_EPT_SUBPROTS < 1
#undef RDMNET_PARSER_MAX_EPT_SUBPROTS
#define RDMNET_PARSER_MAX_EPT_SUBPROTS 1
#endif

/**
 * @brief The maximum number of Dynamic-UID-related structures that can be returned with a parsed
 *        message.
 *
 * This option applies to BrokerDynamicUidRequestListEntry, BrokerDynamicUidMapping, and
 * BrokerFetchUidAssignmentListEntry structures. Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_PARSER_MAX_DYNAMIC_UID_ENTRIES
#define RDMNET_PARSER_MAX_DYNAMIC_UID_ENTRIES 5
#endif

#if RDMNET_PARSER_MAX_DYNAMIC_UID_ENTRIES < 1
#undef RDMNET_PARSER_MAX_DYNAMIC_UID_ENTRIES
#define RDMNET_PARSER_MAX_DYNAMIC_UID_ENTRIES 1
#endif

/**
 * @brief The maximum number of RdmCmdListEntry structures that can be returned with a parsed
 *        ACK_OVERFLOW response (e.g. from an RPT Notification message).
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. If an RDMnet response is received with
 * more ACK_OVERFLOW responses than this number, they will be delivered in batches of this number
 * with the "partial" flag set to true on all but the last batch.
 */
#ifndef RDMNET_PARSER_MAX_ACK_OVERFLOW_RESPONSES
#define RDMNET_PARSER_MAX_ACK_OVERFLOW_RESPONSES 5
#endif

#if RDMNET_PARSER_MAX_ACK_OVERFLOW_RESPONSES < 1
#undef RDMNET_PARSER_MAX_ACK_OVERFLOW_RESPONSES
#define RDMNET_PARSER_MAX_ACK_OVERFLOW_RESPONSES 1
#endif

/**
 * @brief The maximum number of network interfaces usable for RDMnet's multicast protocols.
 *
 * RDMnet makes use of two multicast protocols, LLRP and mDNS. These protocols require tracking of
 * local network interfaces when creating network sockets.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_MCAST_NETINTS
#define RDMNET_MAX_MCAST_NETINTS 3
#endif

#if RDMNET_MAX_MCAST_NETINTS < 1
#undef RDMNET_MAX_MCAST_NETINTS
#define RDMNET_MAX_MCAST_NETINTS 1
#endif

/**
 * @brief For multicast protocols, whether to bind the underlying network socket directly to the
 *        multicast address.
 *
 * Otherwise, the socket is bound to the wildcard address. On some systems, binding directly to a
 * multicast address decreases traffic duplication. On other systems, it's not even allowed. Leave
 * this option at its default value unless you REALLY know what you're doing.
 */
#ifndef RDMNET_BIND_MCAST_SOCKETS_TO_MCAST_ADDRESS
#define RDMNET_BIND_MCAST_SOCKETS_TO_MCAST_ADDRESS !RDMNET_WINDOWS_HINT
#endif

/**
 * @brief The priority of the tick thread.
 *
 * This is usually only meaningful on real-time systems.
 */
#ifndef RDMNET_TICK_THREAD_PRIORITY
#define RDMNET_TICK_THREAD_PRIORITY ETCPAL_THREAD_DEFAULT_PRIORITY
#endif

/**
 * @brief The stack size of the tick thread.
 *
 * It's usually only necessary to worry about this on real-time or embedded systems.
 */
#ifndef RDMNET_TICK_THREAD_STACK
#define RDMNET_TICK_THREAD_STACK (ETCPAL_THREAD_DEFAULT_STACK * 2)
#endif

/**
 * @}
 */

/**
 * @defgroup rdmnetopts_disc Discovery
 * @ingroup rdmnetopts
 *
 * Configuration options for RDMnet discovery using DNS-SD.
 * @{
 */

/**
 * @brief How many RDMnet scopes can be monitored simultaneously.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_MONITORED_SCOPES
#define RDMNET_MAX_MONITORED_SCOPES ((RDMNET_MAX_SCOPES_PER_CONTROLLER * RDMNET_MAX_CONTROLLERS) + RDMNET_MAX_DEVICES)
#endif

/**
 * @brief How many brokers can be discovered at the same time on a given scope.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_DISCOVERED_BROKERS_PER_SCOPE
#define RDMNET_MAX_DISCOVERED_BROKERS_PER_SCOPE 1
#endif

/**
 * @brief How many listen addresses can be resolved for each discovered broker.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. Theoretically, this should only need to
 * be a small number, since only reachable listen addresses should be advertised by registered
 * brokers.
 */
#ifndef RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER
#define RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER 2
#endif

#if !RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER
#error "RDMNET_MAX_ADDRS_PER_DISCOVERED_BROKER must be at least 1"
#endif

/**
 * @brief How many additional TXT record items can be resolved for each discovered broker.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. This is above and beyond the TXT record
 * key/value pairs that RDMnet requires (which there is always room for).
 */
#ifndef RDMNET_MAX_ADDITIONAL_TXT_ITEMS_PER_DISCOVERED_BROKER
#define RDMNET_MAX_ADDITIONAL_TXT_ITEMS_PER_DISCOVERED_BROKER 5
#endif

#if RDMNET_MAX_ADDITIONAL_TXT_ITEMS_PER_DISCOVERED_BROKER < 1
#undef RDMNET_MAX_ADDITIONAL_TXT_ITEMS_PER_DISCOVERED_BROKER
#define RDMNET_MAX_ADDITIONAL_TXT_ITEMS_PER_DISCOVERED_BROKER 1
#endif

/**
 * @}
 */

/**
 * @defgroup rdmnetopts_llrp LLRP
 * @ingroup rdmnetopts
 *
 * Configuration options for LLRP.
 * @{
 */

/**
 * @brief The maximum number of LLRP targets that can be created.
 *
 * Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_LLRP_TARGETS
#define RDMNET_MAX_LLRP_TARGETS RDMNET_MAX_CLIENTS
#endif

/** @cond internal definition */

#if RDMNET_MAX_LLRP_TARGETS
#define RC_MAX_LLRP_TARGETS RDMNET_MAX_LLRP_TARGETS
#else
#define RC_MAX_LLRP_TARGETS 1
#endif

/** @endcond */

/**
 * @}
 */

#endif /* RDMNET_CORE_OPTS_H_ */
