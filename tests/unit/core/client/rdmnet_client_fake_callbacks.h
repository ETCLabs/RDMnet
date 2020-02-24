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
#ifndef RDMNET_CLIENT_FAKE_CALLBACKS_H
#define RDMNET_CLIENT_FAKE_CALLBACKS_H

#include "fff.h"
#include "rdmnet/core/client.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VOID_FUNC(rdmnet_client_connected, rdmnet_client_t, rdmnet_client_scope_t,
                       const RdmnetClientConnectedInfo*, void*);
DECLARE_FAKE_VOID_FUNC(rdmnet_client_connect_failed, rdmnet_client_t, rdmnet_client_scope_t,
                       const RdmnetClientConnectFailedInfo*, void*);
DECLARE_FAKE_VOID_FUNC(rdmnet_client_disconnected, rdmnet_client_t, rdmnet_client_scope_t,
                       const RdmnetClientDisconnectedInfo*, void*);
DECLARE_FAKE_VOID_FUNC(rdmnet_client_broker_msg_received, rdmnet_client_t, rdmnet_client_scope_t, const BrokerMessage*,
                       void*);
DECLARE_FAKE_VOID_FUNC(rpt_client_msg_received, rdmnet_client_t, rdmnet_client_scope_t, const RptClientMessage*, void*);
DECLARE_FAKE_VOID_FUNC(ept_client_msg_received, rdmnet_client_t, rdmnet_client_scope_t, const EptClientMessage*, void*);

#ifdef __cplusplus
}
#endif

#define RDMNET_CLIENT_CALLBACKS_DO_FOR_ALL_FAKES(operation) \
  operation(rdmnet_client_connected);                       \
  operation(rdmnet_client_connect_failed);                  \
  operation(rdmnet_client_disconnected);                    \
  operation(rdmnet_client_broker_msg_received);             \
  operation(rpt_client_msg_received);                       \
  operation(ept_client_msg_received)

#endif  // RDMNET_CLIENT_FAKE_CALLBACKS_H
