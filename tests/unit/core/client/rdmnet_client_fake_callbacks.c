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

#include "rdmnet_client_fake_callbacks.h"

DEFINE_FAKE_VOID_FUNC(rc_client_connected, RCClient*, rdmnet_client_scope_t, const RdmnetClientConnectedInfo*);
DEFINE_FAKE_VOID_FUNC(rc_client_connect_failed, RCClient*, rdmnet_client_scope_t, const RdmnetClientConnectFailedInfo*);
DEFINE_FAKE_VOID_FUNC(rc_client_disconnected, RCClient*, rdmnet_client_scope_t, const RdmnetClientDisconnectedInfo*);
DEFINE_FAKE_VOID_FUNC(rc_client_broker_msg_received, RCClient*, rdmnet_client_scope_t, const BrokerMessage*);
DEFINE_FAKE_VOID_FUNC(rc_client_llrp_msg_received, RCClient*, const LlrpRdmCommand*, RdmnetSyncRdmResponse*, bool*);
DEFINE_FAKE_VOID_FUNC(rc_client_rpt_msg_received,
                      RCClient*,
                      rdmnet_client_scope_t,
                      const RptClientMessage*,
                      RdmnetSyncRdmResponse*,
                      bool*);
DEFINE_FAKE_VOID_FUNC(rc_client_ept_msg_received,
                      RCClient*,
                      rdmnet_client_scope_t,
                      const EptClientMessage*,
                      RdmnetSyncRdmResponse*,
                      bool*);

void rc_client_callbacks_reset_all_fakes(void)
{
  RESET_FAKE(rc_client_connected);
  RESET_FAKE(rc_client_connect_failed);
  RESET_FAKE(rc_client_disconnected);
  RESET_FAKE(rc_client_broker_msg_received);
  RESET_FAKE(rc_client_llrp_msg_received);
  RESET_FAKE(rc_client_rpt_msg_received);
  RESET_FAKE(rc_client_ept_msg_received);
}
