/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "gtest/gtest.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/private/connection.h"
#include "rdmnet_mock/core.h"
#include "rdmnet_mock/private/core.h"

FAKE_VOID_FUNC(conncb_connected, rdmnet_conn_t, const RdmnetConnectedInfo *, void *);
FAKE_VOID_FUNC(conncb_connect_failed, rdmnet_conn_t, const RdmnetConnectFailedInfo *, void *);
FAKE_VOID_FUNC(conncb_disconnected, rdmnet_conn_t, const RdmnetDisconnectedInfo *, void *);
FAKE_VOID_FUNC(conncb_msg_received, rdmnet_conn_t, const RdmnetMessage *, void *);

class TestConnection : public testing::Test
{
protected:
  void SetUp() override
  {
    RESET_FAKE(conncb_connected);
    RESET_FAKE(conncb_connect_failed);
    RESET_FAKE(conncb_disconnected);
    RESET_FAKE(conncb_msg_received);

    rdmnet_mock_core_reset_and_init();

    ASSERT_EQ(kLwpaErrOk, rdmnet_conn_init());
  }

  void TearDown() override { rdmnet_conn_deinit(); }

  RdmnetConnectionConfig default_config_{
      {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
      {conncb_connected, conncb_connect_failed, conncb_disconnected, conncb_msg_received},
      nullptr};
};

TEST_F(TestConnection, socket_error)
{
  rdmnet_conn_t conn;
  ASSERT_EQ(kLwpaErrOk, rdmnet_connection_create(&default_config_, &conn));

  // This allows us to skip the connection process and go straight to a connected state.
  lwpa_socket_t fake_socket = 0;
  LwpaSockaddr remote_addr{};
  ASSERT_EQ(kLwpaErrOk, rdmnet_attach_existing_socket(conn, fake_socket, &remote_addr));

  // Simulate an error on a socket, make sure it is marked disconnected.
  rdmnet_socket_error(conn, kLwpaErrConnReset);

  ASSERT_EQ(conncb_disconnected_fake.call_count, 1u);
  ASSERT_EQ(conncb_disconnected_fake.arg0_val, conn);
  ASSERT_EQ(conncb_disconnected_fake.arg1_val->socket_err, kLwpaErrConnReset);
  ASSERT_EQ(conncb_disconnected_fake.arg1_val->event, kRdmnetDisconnectAbruptClose);
}
