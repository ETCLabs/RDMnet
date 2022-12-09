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

#include "rdmnet_mock/disc/common.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, rdmnet_disc_module_init, const RdmnetNetintConfig*);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_module_deinit);
DEFINE_FAKE_VOID_FUNC(rdmnet_disc_module_tick);
DEFINE_FAKE_VALUE_FUNC(bool, rdmnet_disc_broker_should_deregister, const EtcPalUuid*, const EtcPalUuid*);

void rdmnet_disc_common_reset_all_fakes(void)
{
  RESET_FAKE(rdmnet_disc_module_init);
  RESET_FAKE(rdmnet_disc_module_deinit);
  RESET_FAKE(rdmnet_disc_module_tick);
  RESET_FAKE(rdmnet_disc_broker_should_deregister);
}
