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

/*
 * rdmnet_mock/core/mcast.h
 * Mocking the functions of rdmnet/core/mcast.h
 */

#ifndef RDMNET_MOCK_CORE_MCAST_H_
#define RDMNET_MOCK_CORE_MCAST_H_

#include "rdmnet/core/mcast.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_mcast_module_init, const RdmnetNetintConfig*);
DECLARE_FAKE_VOID_FUNC(rc_mcast_module_deinit);

DECLARE_FAKE_VALUE_FUNC(size_t, rc_mcast_get_netint_array, const RdmnetMcastNetintId**);
DECLARE_FAKE_VALUE_FUNC(bool, rc_mcast_netint_is_valid, const RdmnetMcastNetintId*);
DECLARE_FAKE_VALUE_FUNC(const EtcPalMacAddr*, rc_mcast_get_lowest_mac_addr);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_mcast_get_send_socket,
                        const RdmnetMcastNetintId*,
                        uint16_t,
                        etcpal_socket_t*);
DECLARE_FAKE_VOID_FUNC(rc_mcast_release_send_socket, const RdmnetMcastNetintId*, uint16_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, rc_mcast_create_recv_socket, const EtcPalIpAddr*, uint16_t, etcpal_socket_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_mcast_subscribe_recv_socket,
                        etcpal_socket_t,
                        const RdmnetMcastNetintId*,
                        const EtcPalIpAddr*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t,
                        rc_mcast_unsubscribe_recv_socket,
                        etcpal_socket_t,
                        const RdmnetMcastNetintId*,
                        const EtcPalIpAddr*);

void rc_mcast_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* RDMNET_MOCK_CORE_MCAST_H_ */
