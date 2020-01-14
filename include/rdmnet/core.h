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
 * \file rdmnet/core.h
 * \brief Functions to init, deinit and drive the rdmnet/core modules.
 */

#ifndef RDMNET_CORE_H_
#define RDMNET_CORE_H_

#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \addtogroup rdmnet_conn
 * @{
 */

/*! A handle to an RDMnet connection. */
typedef int rdmnet_conn_t;

/*! An invalid RDMnet connection handle value. */
#define RDMNET_CONN_INVALID -1

/*!
 * @}
 */

/*!
 * \defgroup rdmnet_core_lib RDMnet Core Library
 * \brief Implementation of the core functions of RDMnet.
 *
 * The core library sits underneath the higher-level \ref rdmnet_api "RDMnet APIs" and contains the
 * functionality that every component of RDMnet needs. This includes discovery, connections, and
 * LLRP, as well as message packing and unpacking.
 *
 * Most applications will not need to interact with the API functions in the core library directly,
 * although it does define types that are exposed through the higher-level APIs.
 *
 * @{
 */

/*!
 * A set of identifying information for a network interface, for multicast purposes. RDMnet uses
 * two multicast protocols, LLRP and mDNS. When creating sockets to use with these protocols, the
 * interface IP addresses don't matter and the primary key for a network interface is simply a
 * combination of the interface index and the IP protocol used.
 */
typedef struct RdmnetMcastNetintId
{
  /*! The IP protocol used on the network interface. */
  etcpal_iptype_t ip_type;
  /*! The index of the network interface. See \ref interface_indexes for more information. */
  unsigned int index;
} RdmnetMcastNetintId;

typedef struct RdmnetNetintConfig
{
  const RdmnetMcastNetintId* netint_arr;
  size_t num_netints;
} RdmnetNetintConfig;

etcpal_error_t rdmnet_core_init(const EtcPalLogParams* log_params, const RdmnetNetintConfig* netint_config);
void rdmnet_core_deinit();

void rdmnet_core_tick();

/*!
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_CORE_H_ */
