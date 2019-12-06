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

#ifndef RDMNET_PRIVATE_MCAST_H_
#define RDMNET_PRIVATE_MCAST_H_

#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/socket.h"
#include "rdmnet/core.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t rdmnet_mcast_init(const RdmnetNetintConfig* netint_config);
void rdmnet_mcast_deinit(void);

size_t rdmnet_get_mcast_netint_array(const RdmnetMcastNetintId** array);
bool rdmnet_mcast_netint_is_valid(const RdmnetMcastNetintId* id);
const EtcPalMacAddr* rdmnet_get_lowest_mac_addr(void);

etcpal_error_t rdmnet_get_mcast_send_socket(const RdmnetMcastNetintId* id, etcpal_socket_t* socket);
void rdmnet_release_mcast_send_socket(const RdmnetMcastNetintId* id);
etcpal_error_t rdmnet_create_mcast_recv_socket(const EtcPalIpAddr* group, uint16_t port, etcpal_socket_t* socket);
etcpal_error_t rdmnet_subscribe_mcast_recv_socket(etcpal_socket_t socket, const RdmnetMcastNetintId* netint,
                                                  const EtcPalIpAddr* group);
etcpal_error_t rdmnet_unsubscribe_mcast_recv_socket(etcpal_socket_t socket, const RdmnetMcastNetintId* netint,
                                                    const EtcPalIpAddr* group);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_MCAST_H_ */
