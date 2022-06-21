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

#include "rdmnet_mock/core/common.h"

#include "rdmnet_mock/core/broker_prot.h"
#include "rdmnet_mock/core/client.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/llrp_target.h"
#include "rdmnet_mock/core/mcast.h"
#include "rdmnet_mock/core/message.h"
#include "rdmnet_mock/core/msg_buf.h"
#include "rdmnet_mock/core/rpt_prot.h"

static etcpal_error_t fake_init(const EtcPalLogParams*, const RdmnetNetintConfig*);
static void           fake_deinit(void);

// public mocks
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rc_init, const EtcPalLogParams*, const RdmnetNetintConfig*);
DEFINE_FAKE_VOID_FUNC(rc_deinit);
DEFINE_FAKE_VALUE_FUNC(bool, rc_initialized);
DEFINE_FAKE_VOID_FUNC(rc_tick);
DEFINE_FAKE_VALUE_FUNC(bool, rdmnet_readlock);
DEFINE_FAKE_VOID_FUNC(rdmnet_readunlock);
DEFINE_FAKE_VALUE_FUNC(bool, rdmnet_writelock);
DEFINE_FAKE_VOID_FUNC(rdmnet_writeunlock);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_add_polled_socket,
                       etcpal_socket_t,
                       etcpal_poll_events_t,
                       RCPolledSocketInfo*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t,
                       rc_modify_polled_socket,
                       etcpal_socket_t,
                       etcpal_poll_events_t,
                       RCPolledSocketInfo*);
DEFINE_FAKE_VOID_FUNC(rc_remove_polled_socket, etcpal_socket_t);

DEFINE_FAKE_VALUE_FUNC(int, rc_send, etcpal_socket_t, const void*, size_t, int);

const EtcPalLogParams* rdmnet_log_params = NULL;

void rdmnet_mock_core_reset(void)
{
  RESET_FAKE(rc_init);
  RESET_FAKE(rc_deinit);
  RESET_FAKE(rc_initialized);
  RESET_FAKE(rc_tick);
  RESET_FAKE(rdmnet_readlock);
  RESET_FAKE(rdmnet_readunlock);
  RESET_FAKE(rdmnet_writelock);
  RESET_FAKE(rdmnet_writeunlock);

  RESET_FAKE(rc_add_polled_socket);
  RESET_FAKE(rc_modify_polled_socket);
  RESET_FAKE(rc_remove_polled_socket);

  RESET_FAKE(rc_send);

#if RDMNET_BUILDING_FULL_MOCK_CORE_LIB
  rc_broker_prot_reset_all_fakes();
  rc_client_reset_all_fakes();
  rc_connection_reset_all_fakes();
  rc_llrp_target_reset_all_fakes();
  rc_mcast_reset_all_fakes();
  rc_message_reset_all_fakes();
  rc_msg_buf_reset_all_fakes();
  rc_rpt_prot_reset_all_fakes();
#endif

  rc_init_fake.custom_fake = fake_init;
  rc_deinit_fake.custom_fake = fake_deinit;
}

void rdmnet_mock_core_reset_and_init(void)
{
  rdmnet_mock_core_reset();

  rdmnet_readlock_fake.return_val = true;
  rdmnet_writelock_fake.return_val = true;
  rc_initialized_fake.return_val = true;
}

etcpal_error_t fake_init(const EtcPalLogParams* params, const RdmnetNetintConfig* config)
{
  (void)config;
  rdmnet_log_params = params;
  rdmnet_readlock_fake.return_val = true;
  rdmnet_writelock_fake.return_val = true;
  rc_initialized_fake.return_val = true;
  return kEtcPalErrOk;
}

void fake_deinit(void)
{
  rdmnet_log_params = NULL;

  rdmnet_readlock_fake.return_val = false;
  rdmnet_writelock_fake.return_val = false;
  rc_initialized_fake.return_val = false;
}
