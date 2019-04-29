/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "gtest/gtest.h"
#include "rdmnet/core/connection.h"
#include "rdmnet_mock/core.h"

FAKE_VOID_FUNC(conncb_connected, rdmnet_conn_t, const RdmnetConnectedInfo *, void *);
FAKE_VOID_FUNC(conncb_connect_failed, rdmnet_conn_t, const RdmnetConnectFailedInfo *, void *);
FAKE_VOID_FUNC(conncb_disconnected, rdmnet_conn_t, const RdmnetDisconnectedInfo *, void *);
FAKE_VOID_FUNC(conncb_msg_received, rdmnet_conn_t, const RdmnetMessage *, void *);

class TestConnection : public testing::Test
{
protected:
  TestConnection()
  {
    RESET_FAKE(conncb_connected);
    RESET_FAKE(conncb_connect_failed);
    RESET_FAKE(conncb_disconnected);
    RESET_FAKE(conncb_msg_received);

    RDMNET_CORE_DO_FOR_ALL_FAKES(RESET_FAKE);

    rdmnet_core_initialized_fake.return_val = true;
  }

  RdmnetConnectionConfig default_config_{
      {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
      {conncb_connected, conncb_connect_failed, conncb_disconnected, conncb_msg_received},
      nullptr};
};
