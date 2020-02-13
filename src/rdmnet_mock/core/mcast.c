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

#include "rdmnet_mock/private/mcast.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_mcast_init, const RdmnetNetintConfig*);
DEFINE_FAKE_VOID_FUNC(rdmnet_mcast_deinit);

DEFINE_FAKE_VALUE_FUNC(size_t, rdmnet_get_mcast_netint_array, const RdmnetMcastNetintId**);
DEFINE_FAKE_VALUE_FUNC(bool, rdmnet_mcast_netint_is_valid, const RdmnetMcastNetintId*);
DEFINE_FAKE_VALUE_FUNC(const EtcPalMacAddr*, rdmnet_get_lowest_mac_addr);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_get_mcast_send_socket, const RdmnetMcastNetintId*, etcpal_socket_t*);
DEFINE_FAKE_VOID_FUNC(rdmnet_release_mcast_send_socket, const RdmnetMcastNetintId*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_create_mcast_recv_socket, const EtcPalIpAddr*, uint16_t,
                       etcpal_socket_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_subscribe_mcast_recv_socket, etcpal_socket_t, const RdmnetMcastNetintId*,
                       const EtcPalIpAddr*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_unsubscribe_mcast_recv_socket, etcpal_socket_t,
                       const RdmnetMcastNetintId*, const EtcPalIpAddr*);
