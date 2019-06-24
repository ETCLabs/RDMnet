/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

/*! \file rdmnet/private/opts.h
 *  \brief RDMnet configuration options.
 *
 *  Default values for all of RDMnet's \ref rdmnetopts "compile-time configuration options".
 *
 *  \author Sam Kearney
 */
#ifndef _RDMNET_PRIVATE_OPTS_H_
#define _RDMNET_PRIVATE_OPTS_H_

/*! \defgroup rdmnetopts RDMnet Configuration Options
 *  \brief Compile-time configuration options for RDMnet.
 *
 *  Default values can be overriden in the compiler settings, or by defining RDMNET_HAVE_CONFIG_H in
 *  the compiler settings and providing a file called rdmnet_config.h with overridden definitions in
 *  it.
 */

#if RDMNET_HAVE_CONFIG_H
#include "rdmnet_config.h"
#endif

#include "lwpa/thread.h"

/* Some option hints based on well-known compile definitions */

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

/********************************* Global ************************************/

/*! \defgroup rdmnetopts_global Global
 *  \ingroup rdmnetopts
 *
 *  Global or library-wide configuration options. Any options with *_MAX_* in the name are
 *  applicable only to compilations with dynamic memory disabled (#RDMNET_DYNAMIC_MEM = 0, most
 *  common in embedded toolchains).
 *
 *  @{
 */

/*! \brief Use dynamic memory allocation.
 *
 *  If defined nonzero, RDMnet manages memory dynamically using malloc() and free() from stdlib.h.
 *  Otherwise, RDMnet uses static arrays and fixed-size pools through \ref lwpa_mempool. The size of
 *  the pools is controlled with other config options.
 *
 *  If not defined in rdmnet_config.h, the library attempts to guess using standard OS predefined
 *  macros whether it is being compiled on a full-featured OS, in which case this option is defined
 *  to 1 (otherwise an embedded application is assumed and it is defined to 0).
 */
#ifndef RDMNET_DYNAMIC_MEM
#define RDMNET_DYNAMIC_MEM RDMNET_FULL_OS_AVAILABLE_HINT
#endif

/*! \brief A string which will be prepended to all log messages from the RDMnet library.
 */
#ifndef RDMNET_LOG_MSG_PREFIX
#define RDMNET_LOG_MSG_PREFIX "RDMnet: "
#endif

/*! @} */

/*! \defgroup rdmnetopts_client Client
 *  \ingroup rdmnetopts
 *
 *  Options that affect the RDMnet Client APIs. Any options with *_MAX_* in the name are applicable
 *  only to compilations with dynamic memory disabled (#RDMNET_DYNAMIC_MEM = 0, most common in
 *  embedded toolchains).
 *
 *  @{
 */

/*! \brief The maximum number of RDMnet Controller instances that an application can create.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. A typical application will only need one
 *  controller instance (which can communicate on an arbitrary number of scopes).
 */
#ifndef RDMNET_MAX_CONTROLLERS
#define RDMNET_MAX_CONTROLLERS 0
#endif

/*! \brief The maximum number of RDMnet Device instances that an application can create.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. A typical application will only need one
 *  device instance.
 */
#ifndef RDMNET_MAX_DEVICES
#define RDMNET_MAX_DEVICES 1
#endif

/*! \brief The maximum number of EPT Client instances that an application can create.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_EPT_CLIENTS
#define RDMNET_MAX_EPT_CLIENTS 0
#endif

/*! \brief The maximum number of scopes on which each controller instance can communicate.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_SCOPES_PER_CONTROLLER
#define RDMNET_MAX_SCOPES_PER_CONTROLLER 1
#endif

/*! \brief The maximum number of RDM responses that can be sent from an RPT Client at once in an
 *         ACK_OVERFLOW response.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. For applications which desire static
 *  memory, this parameter should be set to the maximum number of RDM ACK_OVERFLOW responses the
 *  application ever anticipates generating in response to an RDMnet request, based on the client's
 *  parameter data. Since RDMnet gateways cannot anticipate how many ACK_OVERFLOW responses will be
 *  received from a downstream RDM responder, a reasonable guess may need to be made based on the
 *  RDM standard.
 */
#ifndef RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES
#define RDMNET_MAX_SENT_ACK_OVERFLOW_RESPONSES 5
#endif

/*! @} */

#ifdef RDMNET_MAX_RPT_CLIENTS
#undef RDMNET_MAX_RPT_CLIENTS
#endif
#define RDMNET_MAX_RPT_CLIENTS (RDMNET_MAX_CONTROLLERS + RDMNET_MAX_DEVICES)

#ifdef RDMNET_MAX_CLIENTS
#undef RDMNET_MAX_CLIENTS
#endif
#define RDMNET_MAX_CLIENTS (RDMNET_MAX_RPT_CLIENTS + RDMNET_MAX_EPT_CLIENTS)

/*! \defgroup rdmnetopts_core Core
 *  \ingroup rdmnetopts
 *
 *  Options that affect the RDMnet core library. Any options with *_MAX_* in the name are applicable
 *  only to compilations with dynamic memory disabled (#RDMNET_DYNAMIC_MEM = 0, most common in
 *  embedded toolchains).
 *
 *  @{
 */

/*! \brief The maximum number of RDMnet connections that can be created.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. This setting should be left at default
 *  if you have configured #RDMNET_MAX_CONTROLLERS, #RDMNET_MAX_DEVICES and/or
 *  #RDMNET_MAX_EPT_CLIENTS, as those settings will propagate to this one appropriately.
 */
#ifndef RDMNET_MAX_CONNECTIONS
#define RDMNET_MAX_CONNECTIONS RDMNET_MAX_CLIENTS
#endif

/*! \brief The maximum number of ClientEntryData structures that can be returned with a parsed
 *         message.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_CLIENT_ENTRIES
#define RDMNET_MAX_CLIENT_ENTRIES 5
#endif

/*! \brief The maximum number of EptSubProtocol structures that can be returned with a parsed
 *         message.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_EPT_SUBPROTS
#define RDMNET_MAX_EPT_SUBPROTS 5
#endif

/*! \brief The maximum number of Dynamic-UID-related structures that can be returned with a parsed
 *         message.
 *
 *  This option applies to DynamicUidRequestListEntry, DynamicUidMapping, and
 *  FetchUidAssignmentListEntry structures. Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_MAX_DYNAMIC_UID_ENTRIES
#define RDMNET_MAX_DYNAMIC_UID_ENTRIES 5
#endif

/*! \brief The maximum number of RdmCmdListEntry structures that can be returned with a parsed
 *         ACK_OVERFLOW response (e.g. from an RPT Notification message).
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0. If an RDMnet response is received with
 *  more ACK_OVERFLOW responses than this number, they will be delivered in batches of this number
 *  with the "partial" flag set to true on all but the last batch.
 */
#ifndef RDMNET_MAX_RECEIVED_ACK_OVERFLOW_RESPONSES
#define RDMNET_MAX_RECEIVED_ACK_OVERFLOW_RESPONSES 10
#endif

/*! \brief Whether to allow sockets associated with connections to be polled externally.
 *
 *  If this option is defined to 0, the externally-managed-socket functions in the
 *  \ref rdmnet_conn "Connection API" will not be implemented. Most applications will want the
 *  default behavior, unless scaling the number of connections is a concern. Broker applications
 *  will set this to 1.
 */
#ifndef RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS
#define RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS RDMNET_FULL_OS_AVAILABLE_HINT
#endif

/* The library has some limitations around static memory allocation and how many message structures
 * can be allocated at a time. If RDMNET_POLL_CONNECTIONS_INTERNALLY is false, the library has no
 * guarantee as to how many threads could be receiving and allocating messages simultaneously;
 * therefore, in this case, RDMNET_DYNAMIC_MEM must be enabled.
 */
#if (RDMNET_ALLOW_EXTERNALLY_MANAGED_SOCKETS && !RDMNET_DYNAMIC_MEM)
#error "RDMNET_POLL_CONNECTIONS_INTERNALLY=0 requires RDMNET_DYNAMIC_MEM=1"
#endif

/*! \brief Spawn a thread internally to call rdmnet_tick().
 *
 *  If defined nonzero, rdmnet_init() will create a thread using \ref lwpa_thread which calls
 *  rdmnet_tick() periodically. The thread will be created using #RDMNET_TICK_THREAD_PRIORITY and
 *  #RDMNET_TICK_THREAD_STACK. The thread will be stopped by rdmnet_deinit().
 *
 *  If defined zero, the function declaration of rdmnet_tick() will be exposed in
 *  rdmnet/connection.h and it must be called by the application as specified in that function's
 *  documentation. */
#ifndef RDMNET_USE_TICK_THREAD
#define RDMNET_USE_TICK_THREAD 1
#endif

/*! \brief The priority of the tick thread.
 *
 *  This is usually only meaningful on real-time systems. */
#ifndef RDMNET_TICK_THREAD_PRIORITY
#define RDMNET_TICK_THREAD_PRIORITY LWPA_THREAD_DEFAULT_PRIORITY
#endif

/*! \brief The stack size of the tick thread.
 *
 *  It's usually only necessary to worry about this on real-time or embedded systems. */
#ifndef RDMNET_TICK_THREAD_STACK
#define RDMNET_TICK_THREAD_STACK LWPA_THREAD_DEFAULT_STACK
#endif

/*! @} */

/*! \defgroup rdmnetopts_llrp LLRP
 *  \ingroup rdmnetopts
 *
 *  Configuration options for LLRP.
 *  @{
 */

/*! \brief The maximum number of LLRP targets that can be created.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_LLRP_MAX_TARGETS
#define RDMNET_LLRP_MAX_TARGETS RDMNET_MAX_CLIENTS
#endif

/*! \brief The maximum number of network interfaces on which each LLRP Target can operate.
 *
 *  Meaningful only if #RDMNET_DYNAMIC_MEM is defined to 0.
 */
#ifndef RDMNET_LLRP_MAX_TARGET_NETINTS
#define RDMNET_LLRP_MAX_TARGET_NETINTS 1
#endif

/*! \brief In LLRP, whether to bind the underlying network socket directly to the LLRP multicast
 *         address.
 *
 *  Otherwise, the socket is bound to INADDR_ANY. On some systems, binding directly to a multicast
 *  address decreases traffic duplication. On other systems, it's not even allowed. Leave this
 *  option at its default value unless you REALLY know what you're doing.
 */
#ifndef RDMNET_LLRP_BIND_TO_MCAST_ADDRESS
#define RDMNET_LLRP_BIND_TO_MCAST_ADDRESS !RDMNET_WINDOWS_HINT
#endif

/*! @} */

#endif /* _RDMNET_CORE_OPTS_H_ */
