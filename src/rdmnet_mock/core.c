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

#include "rdmnet_mock/core.h"
#include "rdmnet_mock/private/core.h"

#include "rdmnet_mock/core/broker_prot.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/discovery.h"
#include "rdmnet_mock/core/rpt_prot.h"
#include "rdmnet_mock/core/llrp_target.h"

static etcpal_error_t fake_init(const EtcPalLogParams*);
static void fake_deinit(void);

// public mocks
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_core_init, const EtcPalLogParams*);
DEFINE_FAKE_VOID_FUNC(rdmnet_core_deinit);
DEFINE_FAKE_VOID_FUNC(rdmnet_core_tick);
DEFINE_FAKE_VALUE_FUNC(bool, rdmnet_core_initialized);

DEFINE_FAKE_VALUE_FUNC(bool, rdmnet_readlock);
DEFINE_FAKE_VOID_FUNC(rdmnet_readunlock);
DEFINE_FAKE_VALUE_FUNC(bool, rdmnet_writelock);
DEFINE_FAKE_VOID_FUNC(rdmnet_writeunlock);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_core_add_polled_socket, etcpal_socket_t, etcpal_poll_events_t,
                       PolledSocketInfo*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_core_modify_polled_socket, etcpal_socket_t, etcpal_poll_events_t,
                       PolledSocketInfo*);
DEFINE_FAKE_VOID_FUNC(rdmnet_core_remove_polled_socket, etcpal_socket_t);

const EtcPalLogParams* rdmnet_log_params = NULL;

void rdmnet_mock_core_reset(void)
{
  RESET_FAKE(rdmnet_core_init);
  RESET_FAKE(rdmnet_core_deinit);
  RESET_FAKE(rdmnet_core_tick);
  RESET_FAKE(rdmnet_core_initialized);

  RESET_FAKE(rdmnet_readlock);
  RESET_FAKE(rdmnet_readunlock);
  RESET_FAKE(rdmnet_writelock);
  RESET_FAKE(rdmnet_writeunlock);

  RESET_FAKE(rdmnet_core_add_polled_socket);
  RESET_FAKE(rdmnet_core_modify_polled_socket);
  RESET_FAKE(rdmnet_core_remove_polled_socket);

#if RDMNET_BUILDING_FULL_MOCK_CORE_LIB
  RDMNET_CORE_BROKER_PROT_DO_FOR_ALL_FAKES(RESET_FAKE);
  rdmnet_connection_reset_all_fakes();
  RDMNET_CORE_DISCOVERY_DO_FOR_ALL_FAKES(RESET_FAKE);
  llrp_target_reset_all_fakes();
  RDMNET_CORE_RPT_PROT_DO_FOR_ALL_FAKES(RESET_FAKE);
#endif

  rdmnet_core_init_fake.custom_fake = fake_init;
  rdmnet_core_deinit_fake.custom_fake = fake_deinit;
}

void rdmnet_mock_core_reset_and_init(void)
{
  rdmnet_mock_core_reset();

  rdmnet_readlock_fake.return_val = true;
  rdmnet_writelock_fake.return_val = true;
  rdmnet_core_initialized_fake.return_val = true;
}

etcpal_error_t fake_init(const EtcPalLogParams* params)
{
  rdmnet_log_params = params;
  rdmnet_readlock_fake.return_val = true;
  rdmnet_writelock_fake.return_val = true;
  rdmnet_core_initialized_fake.return_val = true;
  return kEtcPalErrOk;
}

void fake_deinit(void)
{
  rdmnet_log_params = NULL;

  rdmnet_readlock_fake.return_val = false;
  rdmnet_writelock_fake.return_val = false;
  rdmnet_core_initialized_fake.return_val = false;
}
