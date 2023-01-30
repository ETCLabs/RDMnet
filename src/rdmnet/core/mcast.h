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

#ifndef RDMNET_PRIVATE_MCAST_H_
#define RDMNET_PRIVATE_MCAST_H_

#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/socket.h"
#include "rdmnet/common.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t rc_mcast_module_init(const RdmnetNetintConfig* netint_config);
etcpal_error_t rc_mcast_module_net_reset(const RdmnetNetintConfig* netint_config);
void           rc_mcast_module_deinit(void);

size_t               rc_mcast_get_netint_array(const EtcPalMcastNetintId** array);
bool                 rc_mcast_netint_is_valid(const EtcPalMcastNetintId* id);
const EtcPalMacAddr* rc_mcast_get_lowest_mac_addr(void);

etcpal_error_t rc_mcast_get_send_socket(const EtcPalMcastNetintId* id, uint16_t source_port, etcpal_socket_t* socket);
void           rc_mcast_release_send_socket(const EtcPalMcastNetintId* id, uint16_t source_port);
etcpal_error_t rc_mcast_create_recv_socket(const EtcPalIpAddr* group, uint16_t port, etcpal_socket_t* socket);
etcpal_error_t rc_mcast_subscribe_recv_socket(etcpal_socket_t            socket,
                                              const EtcPalMcastNetintId* netint,
                                              const EtcPalIpAddr*        group);
etcpal_error_t rc_mcast_unsubscribe_recv_socket(etcpal_socket_t            socket,
                                                const EtcPalMcastNetintId* netint,
                                                const EtcPalIpAddr*        group);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_PRIVATE_MCAST_H_ */
