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

// Test how an RPT client handles RDM commands internally.

#include "rdmnet/core/client.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <random>
#include <set>
#include <vector>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/mutex.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/pack.h"
#include "etcpal_mock/common.h"
#include "rdm/defs.h"
#include "rdm/message.h"
#include "rdmnet_mock/core/broker_prot.h"
#include "rdmnet_mock/core/common.h"
#include "rdmnet_mock/core/connection.h"
#include "rdmnet_mock/core/llrp_target.h"
#include "rdmnet_mock/core/rpt_prot.h"
#include "rdmnet_mock/discovery.h"
#include "rdmnet_client_fake_callbacks.h"
#include "test_rdm_commands.h"
#include "gtest/gtest.h"

extern "C" {
static RCConnection* last_conn;
static RCLlrpTarget* last_llrp_target;

static etcpal_error_t register_and_save_conn(RCConnection* conn)
{
  last_conn = conn;
  return kEtcPalErrOk;
}

static etcpal_error_t register_and_save_llrp_target(RCLlrpTarget* target)
{
  last_llrp_target = target;
  return kEtcPalErrOk;
}
}

RptHeader              last_sent_header;
std::vector<RdmBuffer> last_sent_buf_list;

static constexpr RdmUid kClientUid{0x6574, 0x1234};

class TestRptClientRdmHandling : public testing::Test
{
protected:
  RCClient              client_{};
  etcpal::Mutex         client_lock_;
  rdmnet_client_scope_t scope_handle_{RDMNET_CLIENT_SCOPE_INVALID};
  RdmnetScopeConfig     default_static_scope_{};

  static constexpr char kTestScope[] = "test scope";

  void SetUp() override
  {
    // Reset the fakes
    rc_client_callbacks_reset_all_fakes();
    rdmnet_mock_core_reset_and_init();
    rc_broker_prot_reset_all_fakes();
    rc_rpt_prot_reset_all_fakes();
    rc_connection_reset_all_fakes();
    rdmnet_disc_reset_all_fakes();
    etcpal_reset_all_fakes();

    rc_conn_register_fake.custom_fake = register_and_save_conn;
    rc_llrp_target_register_fake.custom_fake = register_and_save_llrp_target;

    // Capture the RdmBuffer lists sent by rc_rpt_send_notification()
    last_sent_buf_list.clear();
    rc_rpt_send_notification_fake.custom_fake = [](RCConnection*, const EtcPalUuid*, const RptHeader* header,
                                                   const RdmBuffer* cmd_arr, size_t cmd_arr_size) {
      last_sent_header = *header;
      last_sent_buf_list.assign(cmd_arr, cmd_arr + cmd_arr_size);
      return kEtcPalErrOk;
    };

    client_.lock = &client_lock_.get();
    client_.type = kClientProtocolRPT;
    client_.cid = etcpal::Uuid::FromString("01b638ac-be34-40a7-988c-cc62d2fbb3b0").get();
    client_.callbacks = kClientFakeCommonCallbacks;
    RC_RPT_CLIENT_DATA(&client_)->type = kRPTClientTypeController;
    RC_RPT_CLIENT_DATA(&client_)->uid = kClientUid;
    RC_RPT_CLIENT_DATA(&client_)->callbacks = kClientFakeRptCallbacks;

    auto static_broker = etcpal::SockAddr(etcpal::IpAddr::FromString("10.101.1.1"), 8888);
    RDMNET_CLIENT_SET_STATIC_SCOPE(&default_static_scope_, kTestScope, static_broker.get());

    // Create client
    ASSERT_EQ(kEtcPalErrOk, rc_client_module_init());
    ASSERT_EQ(kEtcPalErrOk, rc_rpt_client_register(&client_, true));

    ConnectAndVerify();
  }

  void TearDown() override
  {
    if (!rc_client_unregister(&client_, kRdmnetDisconnectShutdown))
    {
      last_conn->callbacks.destroyed(last_conn);
      last_llrp_target->callbacks.destroyed(last_llrp_target);
    }
    rc_client_module_deinit();
  }

  void ConnectAndVerify()
  {
    ASSERT_EQ(kEtcPalErrOk, rc_client_add_scope(&client_, &default_static_scope_, &scope_handle_));

    EXPECT_EQ(rc_conn_register_fake.call_count, 1u);
    EXPECT_EQ(rc_conn_connect_fake.call_count, 1u);

    RCConnectedInfo connected_info{};
    connected_info.broker_cid = etcpal::Uuid::FromString("500a4ae0-527d-45db-a37c-7fecd0c01f81").get();
    connected_info.broker_uid = {20, 40};
    connected_info.client_uid = RC_RPT_CLIENT_DATA(&client_)->uid;
    connected_info.connected_addr = default_static_scope_.static_broker_addr;
    last_conn->callbacks.connected(last_conn, &connected_info);

    EXPECT_EQ(rc_client_connected_fake.call_count, 1u);
  }
};

TEST_F(TestRptClientRdmHandling, AcksGetTcpCommsStatus)
{
  auto test_cmd = TestRdmCommand::Get(client_, E133_TCP_COMMS_STATUS);

  last_conn->callbacks.message_received(last_conn, &test_cmd.msg);

  EXPECT_EQ(rc_client_rpt_msg_received_fake.call_count, 0u);
  EXPECT_EQ(rc_rpt_send_notification_fake.call_count, 1u);
}

TEST_F(TestRptClientRdmHandling, AcksSetTcpCommsStatus)
{
  std::vector<uint8_t> scope_data(std::begin(kTestScope), std::end(kTestScope));
  std::fill_n(std::back_inserter(scope_data), E133_SCOPE_STRING_PADDED_LENGTH - sizeof(kTestScope),
              static_cast<uint8_t>(0));

  auto test_cmd =
      TestRdmCommand::Set(client_, E133_TCP_COMMS_STATUS, scope_data.data(), static_cast<uint8_t>(scope_data.size()));

  last_conn->callbacks.message_received(last_conn, &test_cmd.msg);

  EXPECT_EQ(rc_client_rpt_msg_received_fake.call_count, 0u);
  EXPECT_EQ(rc_rpt_send_notification_fake.call_count, 1u);
}

// For use by the AppendToSupportedParams tests

// clang-format off
const RdmnetSavedRdmCommand kGetSupportedParamsSavedCmd{
  {1, 2},
  E133_NULL_ENDPOINT,
  20,
  {
    {1, 2},
    kClientUid,
    20,
    1,
    0,
    kRdmCCGetCommand,
    E120_SUPPORTED_PARAMETERS
  },
  {},
  0
};

static const std::vector<uint16_t> kSupportedParamsAll = {
  E120_SUPPORTED_PARAMETERS,
  E120_DEVICE_MODEL_DESCRIPTION,
  E120_MANUFACTURER_LABEL,
  E120_DEVICE_LABEL,
  E120_SOFTWARE_VERSION_LABEL,
  E133_COMPONENT_SCOPE,
  E133_SEARCH_DOMAIN,
  E133_TCP_COMMS_STATUS,
  E120_IDENTIFY_DEVICE,
};

static const std::vector<uint16_t> kSupportedParamsDevice = {
  E137_7_ENDPOINT_LIST,
  E137_7_ENDPOINT_LIST_CHANGE,
  E137_7_ENDPOINT_RESPONDERS,
  E137_7_ENDPOINT_RESPONDER_LIST_CHANGE
};
// clang-format on

TEST_F(TestRptClientRdmHandling, AppendsRequiredSupportedParams)
{
  uint8_t data_buf[2];
  etcpal_pack_u16b(data_buf, E120_DEVICE_INFO);

  ASSERT_EQ(rc_client_send_rdm_ack(&client_, scope_handle_, &kGetSupportedParamsSavedCmd, data_buf, 2), kEtcPalErrOk);
  ASSERT_EQ(rc_rpt_send_notification_fake.call_count, 1u);

  EXPECT_EQ(last_sent_buf_list.size(), 2u);
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[0]));
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[1]));

  const RdmBuffer&   response = last_sent_buf_list[1];
  uint8_t            pdl = response.data[RDM_OFFSET_PARAM_DATA_LEN];
  std::set<uint16_t> params_found;
  for (const uint8_t* cur_ptr = &response.data[RDM_OFFSET_PARAM_DATA];
       cur_ptr < &response.data[RDM_OFFSET_PARAM_DATA] + pdl; cur_ptr += 2)
  {
    EXPECT_TRUE(params_found.insert(etcpal_unpack_u16b(cur_ptr)).second);
  }

  EXPECT_EQ(params_found.size(), kSupportedParamsAll.size() + 1);
  EXPECT_NE(params_found.find(E120_DEVICE_INFO), params_found.end());
  for (const auto& param : kSupportedParamsAll)
    EXPECT_NE(params_found.find(param), params_found.end()) << "Parameter value: " << param;
}

TEST_F(TestRptClientRdmHandling, AppendsRequiredSupportedParamsDevice)
{
  RC_RPT_CLIENT_DATA(&client_)->type = kRPTClientTypeDevice;

  uint8_t data_buf[2];
  etcpal_pack_u16b(data_buf, E120_DEVICE_INFO);

  ASSERT_EQ(rc_client_send_rdm_ack(&client_, scope_handle_, &kGetSupportedParamsSavedCmd, data_buf, 2), kEtcPalErrOk);
  ASSERT_EQ(rc_rpt_send_notification_fake.call_count, 1u);

  EXPECT_EQ(last_sent_buf_list.size(), 2u);
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[0]));
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[1]));

  const RdmBuffer&   response = last_sent_buf_list[1];
  uint8_t            pdl = response.data[RDM_OFFSET_PARAM_DATA_LEN];
  std::set<uint16_t> params_found;
  for (const uint8_t* cur_ptr = &response.data[RDM_OFFSET_PARAM_DATA];
       cur_ptr < &response.data[RDM_OFFSET_PARAM_DATA] + pdl; cur_ptr += 2)
  {
    EXPECT_TRUE(params_found.insert(etcpal_unpack_u16b(cur_ptr)).second);
  }

  EXPECT_EQ(params_found.size(), kSupportedParamsAll.size() + kSupportedParamsDevice.size() + 1);
  EXPECT_NE(params_found.find(E120_DEVICE_INFO), params_found.end());
  for (const auto& param : kSupportedParamsAll)
    EXPECT_NE(params_found.find(param), params_found.end()) << "Parameter value: " << param;
  for (const auto& param : kSupportedParamsDevice)
    EXPECT_NE(params_found.find(param), params_found.end()) << "Parameter value: " << param;
}

TEST_F(TestRptClientRdmHandling, AppendsSupportedParamsToUpdate)
{
  uint8_t data_buf[2];
  etcpal_pack_u16b(data_buf, E120_DEVICE_INFO);

  ASSERT_EQ(rc_client_send_rdm_update(&client_, scope_handle_, 0, E120_SUPPORTED_PARAMETERS, data_buf, 2),
            kEtcPalErrOk);
  ASSERT_EQ(rc_rpt_send_notification_fake.call_count, 1u);

  EXPECT_EQ(last_sent_buf_list.size(), 1u);
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[0]));

  const RdmBuffer&   response = last_sent_buf_list[0];
  uint8_t            pdl = response.data[RDM_OFFSET_PARAM_DATA_LEN];
  std::set<uint16_t> params_found;
  for (const uint8_t* cur_ptr = &response.data[RDM_OFFSET_PARAM_DATA];
       cur_ptr < &response.data[RDM_OFFSET_PARAM_DATA] + pdl; cur_ptr += 2)
  {
    EXPECT_TRUE(params_found.insert(etcpal_unpack_u16b(cur_ptr)).second);
  }

  EXPECT_EQ(params_found.size(), kSupportedParamsAll.size() + 1);
  EXPECT_NE(params_found.find(E120_DEVICE_INFO), params_found.end());
  for (const auto& param : kSupportedParamsAll)
    EXPECT_NE(params_found.find(param), params_found.end()) << "Parameter value: " << param;
}

TEST_F(TestRptClientRdmHandling, DoesNotAppendDuplicateSupportedParams)
{
  auto supported_params = kSupportedParamsAll;

  std::random_device         rd;
  std::default_random_engine random_gen(rd());

  // Remove a few random parameters from the set to be appended
  for (int i = 0; i < 4; ++i)
  {
    std::uniform_int_distribution<size_t> distrib(0, supported_params.size() - 1);
    supported_params.erase(supported_params.begin() + distrib(random_gen));
  }

  std::vector<uint8_t> param_data;
  for (const auto& param : supported_params)
  {
    param_data.resize(param_data.size() + 2);
    etcpal_pack_u16b(&param_data[param_data.size() - 2], param);
  }

  ASSERT_EQ(rc_client_send_rdm_ack(&client_, scope_handle_, &kGetSupportedParamsSavedCmd, param_data.data(),
                                   param_data.size()),
            kEtcPalErrOk);
  ASSERT_EQ(rc_rpt_send_notification_fake.call_count, 1u);

  EXPECT_EQ(last_sent_buf_list.size(), 2u);
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[0]));
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[1]));

  const RdmBuffer&   response = last_sent_buf_list[1];
  uint8_t            pdl = response.data[RDM_OFFSET_PARAM_DATA_LEN];
  std::set<uint16_t> params_found;
  for (const uint8_t* cur_ptr = &response.data[RDM_OFFSET_PARAM_DATA];
       cur_ptr < &response.data[RDM_OFFSET_PARAM_DATA] + pdl; cur_ptr += 2)
  {
    EXPECT_TRUE(params_found.insert(etcpal_unpack_u16b(cur_ptr)).second);
  }

  EXPECT_EQ(params_found.size(), kSupportedParamsAll.size());
  for (const auto& param : kSupportedParamsAll)
    EXPECT_NE(params_found.find(param), params_found.end()) << "Parameter value: " << param;
}

TEST_F(TestRptClientRdmHandling, AppendsSplitIntoSecondResponse)
{
  size_t space_to_leave = kSupportedParamsAll.size() / 2;

  // Fill with a bunch of manufacturer-specific parameters, leaving room for about half the
  // standard PIDs at the end.
  std::vector<uint16_t> supported_params;
  uint16_t              first_param = 0x8001;
  std::generate_n(std::back_inserter(supported_params), 115 - space_to_leave, [&]() { return first_param++; });

  // Pack into wire format
  std::vector<uint8_t> param_data;
  for (const auto& param : supported_params)
  {
    param_data.resize(param_data.size() + 2);
    etcpal_pack_u16b(&param_data[param_data.size() - 2], param);
  }

  ASSERT_EQ(rc_client_send_rdm_ack(&client_, scope_handle_, &kGetSupportedParamsSavedCmd, param_data.data(),
                                   param_data.size()),
            kEtcPalErrOk);
  ASSERT_EQ(rc_rpt_send_notification_fake.call_count, 1u);

  EXPECT_EQ(last_sent_buf_list.size(), 3u);
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[0]));
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[1]));
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[2]));

  std::set<uint16_t> params_found;

  for (auto buf_iter = last_sent_buf_list.begin() + 1; buf_iter != last_sent_buf_list.end(); ++buf_iter)
  {
    const RdmBuffer& response = *buf_iter;
    uint8_t          pdl = response.data[RDM_OFFSET_PARAM_DATA_LEN];
    for (const uint8_t* cur_ptr = &response.data[RDM_OFFSET_PARAM_DATA];
         cur_ptr < &response.data[RDM_OFFSET_PARAM_DATA] + pdl; cur_ptr += 2)
    {
      EXPECT_TRUE(params_found.insert(etcpal_unpack_u16b(cur_ptr)).second);
    }
  }

  EXPECT_EQ(params_found.size(), kSupportedParamsAll.size() + supported_params.size());
  for (const auto& param : kSupportedParamsAll)
    EXPECT_NE(params_found.find(param), params_found.end()) << "Parameter value: " << param;
  for (const auto& param : supported_params)
    EXPECT_NE(params_found.find(param), params_found.end()) << "Parameter value: " << param;
}

TEST_F(TestRptClientRdmHandling, ParsesNotificationWithCommand)
{
  static constexpr char kDeviceLabel[] = "Test Device";

  auto test_resp = TestRdmResponse::GetResponse(
      client_, E120_DEVICE_LABEL, reinterpret_cast<const uint8_t*>(kDeviceLabel), sizeof(kDeviceLabel) - 1);

  rc_client_rpt_msg_received_fake.custom_fake = [](RCClient* client, rdmnet_client_scope_t, const RptClientMessage* msg,
                                                   RdmnetSyncRdmResponse*, bool*) {
    EXPECT_EQ(msg->type, kRptClientMsgRdmResp);
    const RdmnetRdmResponse* resp = RDMNET_GET_RDM_RESPONSE(msg);
    EXPECT_EQ(resp->rdmnet_source_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->source_endpoint, 0u);
    EXPECT_EQ(resp->seq_num, kTestRdmCmdsSeqNum);
    EXPECT_TRUE(resp->is_response_to_me);

    EXPECT_EQ(resp->original_cmd_header.source_uid, RC_RPT_CLIENT_DATA(client)->uid);
    EXPECT_EQ(resp->original_cmd_header.dest_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->original_cmd_header.transaction_num, kTestRdmCmdsTransactionNum);
    EXPECT_EQ(resp->original_cmd_header.command_class, kRdmCCGetCommand);
    EXPECT_EQ(resp->original_cmd_header.param_id, E120_DEVICE_LABEL);
    EXPECT_EQ(resp->original_cmd_data, nullptr);
    EXPECT_EQ(resp->original_cmd_data_len, 0);

    EXPECT_EQ(resp->rdm_header.source_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->rdm_header.dest_uid, RC_RPT_CLIENT_DATA(client)->uid);
    EXPECT_EQ(resp->rdm_header.transaction_num, kTestRdmCmdsTransactionNum);
    EXPECT_EQ(resp->rdm_header.resp_type, kRdmResponseTypeAck);
    EXPECT_EQ(resp->rdm_header.command_class, kRdmCCGetCommandResponse);
    EXPECT_EQ(resp->rdm_header.param_id, E120_DEVICE_LABEL);
    EXPECT_EQ(resp->rdm_data_len, sizeof(kDeviceLabel) - 1);
    EXPECT_EQ(std::memcmp(resp->rdm_data, kDeviceLabel, sizeof(kDeviceLabel) - 1), 0);
  };
  last_conn->callbacks.message_received(last_conn, &test_resp.msg);
  EXPECT_EQ(rc_client_rpt_msg_received_fake.call_count, 1u);
}

TEST_F(TestRptClientRdmHandling, ParsesNotificationWithoutCommand)
{
  static const std::array<uint8_t, 12> kEndpointListResponse = {0, 0, 0, 1, 0, 1, 0, 2, 0, 3, 0, 4};

  auto test_resp = TestRdmResponse::GetResponseBroadcast(client_, E137_7_ENDPOINT_LIST, kEndpointListResponse.data(),
                                                         kEndpointListResponse.size());

  RDMNET_GET_RPT_MSG(&test_resp.msg)->header.seqnum = 0;
  RPT_GET_RDM_BUF_LIST(RDMNET_GET_RPT_MSG(&test_resp.msg))->rdm_buffers = &test_resp.bufs[1];
  RPT_GET_RDM_BUF_LIST(RDMNET_GET_RPT_MSG(&test_resp.msg))->num_rdm_buffers--;

  rc_client_rpt_msg_received_fake.custom_fake = [](RCClient* client, rdmnet_client_scope_t, const RptClientMessage* msg,
                                                   RdmnetSyncRdmResponse*, bool*) {
    EXPECT_EQ(msg->type, kRptClientMsgRdmResp);
    const RdmnetRdmResponse* resp = RDMNET_GET_RDM_RESPONSE(msg);
    EXPECT_EQ(resp->rdmnet_source_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->source_endpoint, 0u);
    EXPECT_EQ(resp->seq_num, 0u);
    EXPECT_FALSE(resp->is_response_to_me);

    EXPECT_EQ(resp->original_cmd_data, nullptr);
    EXPECT_EQ(resp->original_cmd_data_len, 0);

    EXPECT_EQ(resp->rdm_header.source_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->rdm_header.dest_uid, RC_RPT_CLIENT_DATA(client)->uid);
    EXPECT_EQ(resp->rdm_header.transaction_num, kTestRdmCmdsTransactionNum);
    EXPECT_EQ(resp->rdm_header.resp_type, kRdmResponseTypeAck);
    EXPECT_EQ(resp->rdm_header.command_class, kRdmCCGetCommandResponse);
    EXPECT_EQ(resp->rdm_header.param_id, E137_7_ENDPOINT_LIST);
    EXPECT_EQ(resp->rdm_data_len, kEndpointListResponse.size());
    EXPECT_EQ(std::memcmp(resp->rdm_data, kEndpointListResponse.data(), kEndpointListResponse.size()), 0);
  };
  last_conn->callbacks.message_received(last_conn, &test_resp.msg);
  EXPECT_EQ(rc_client_rpt_msg_received_fake.call_count, 1u);
}

TEST_F(TestRptClientRdmHandling, ParsesOverflowNotificationWithCommand)
{
  // clang-format off
  static const std::array<uint8_t, 2> kEndpointRespondersCommand = { 0, 1 };
  static const std::array<uint8_t, 306> kEndpointRespondersResponse = {
    0, 1, 0, 0, 0, 42,
    0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 2, 0, 1, 0, 0, 0, 3, 0, 1, 0, 0, 0, 4, 0, 1, 0, 0, 0, 5, 0, 1, 0, 0, 0, 6, 0, 1, 0, 0, 0, 7, 0, 1, 0, 0, 0, 8, 0, 1, 0, 0, 0, 9, 0, 1, 0, 0, 0, 10,
    0, 1, 0, 0, 0, 11, 0, 1, 0, 0, 0, 12, 0, 1, 0, 0, 0, 13, 0, 1, 0, 0, 0, 14, 0, 1, 0, 0, 0, 15, 0, 1, 0, 0, 0, 16, 0, 1, 0, 0, 0, 17, 0, 1, 0, 0, 0, 18, 0, 1, 0, 0, 0, 19, 0, 1, 0, 0, 0, 20,
    0, 1, 0, 0, 0, 21, 0, 1, 0, 0, 0, 22, 0, 1, 0, 0, 0, 23, 0, 1, 0, 0, 0, 24, 0, 1, 0, 0, 0, 25, 0, 1, 0, 0, 0, 26, 0, 1, 0, 0, 0, 27, 0, 1, 0, 0, 0, 28, 0, 1, 0, 0, 0, 29, 0, 1, 0, 0, 0, 30,
    0, 1, 0, 0, 0, 31, 0, 1, 0, 0, 0, 32, 0, 1, 0, 0, 0, 33, 0, 1, 0, 0, 0, 34, 0, 1, 0, 0, 0, 35, 0, 1, 0, 0, 0, 36, 0, 1, 0, 0, 0, 37, 0, 1, 0, 0, 0, 38, 0, 1, 0, 0, 0, 39, 0, 1, 0, 0, 0, 40,
    0, 1, 0, 0, 0, 41, 0, 1, 0, 0, 0, 42, 0, 1, 0, 0, 0, 43, 0, 1, 0, 0, 0, 44, 0, 1, 0, 0, 0, 45, 0, 1, 0, 0, 0, 46, 0, 1, 0, 0, 0, 47, 0, 1, 0, 0, 0, 48, 0, 1, 0, 0, 0, 49, 0, 1, 0, 0, 0, 50,
  };
  // clang-format on

  auto test_resp = TestRdmResponse::GetResponse(client_, E137_7_ENDPOINT_RESPONDERS, kEndpointRespondersResponse.data(),
                                                kEndpointRespondersResponse.size(), kEndpointRespondersCommand.data(),
                                                static_cast<uint8_t>(kEndpointRespondersCommand.size()));
  rc_client_rpt_msg_received_fake.custom_fake = [](RCClient* client, rdmnet_client_scope_t, const RptClientMessage* msg,
                                                   RdmnetSyncRdmResponse*, bool*) {
    EXPECT_EQ(msg->type, kRptClientMsgRdmResp);
    const RdmnetRdmResponse* resp = RDMNET_GET_RDM_RESPONSE(msg);
    EXPECT_EQ(resp->rdmnet_source_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->source_endpoint, 0u);
    EXPECT_EQ(resp->seq_num, kTestRdmCmdsSeqNum);
    EXPECT_TRUE(resp->is_response_to_me);

    EXPECT_EQ(resp->original_cmd_header.source_uid, RC_RPT_CLIENT_DATA(client)->uid);
    EXPECT_EQ(resp->original_cmd_header.dest_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->original_cmd_header.transaction_num, kTestRdmCmdsTransactionNum);
    EXPECT_EQ(resp->original_cmd_header.command_class, kRdmCCGetCommand);
    EXPECT_EQ(resp->original_cmd_header.param_id, E137_7_ENDPOINT_RESPONDERS);
    EXPECT_EQ(resp->original_cmd_data_len, kEndpointRespondersCommand.size());
    EXPECT_EQ(
        std::memcmp(resp->original_cmd_data, kEndpointRespondersCommand.data(), kEndpointRespondersCommand.size()), 0);

    EXPECT_EQ(resp->rdm_header.source_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->rdm_header.dest_uid, RC_RPT_CLIENT_DATA(client)->uid);
    EXPECT_EQ(resp->rdm_header.transaction_num, kTestRdmCmdsTransactionNum);
    EXPECT_EQ(resp->rdm_header.resp_type, kRdmResponseTypeAck);
    EXPECT_EQ(resp->rdm_header.command_class, kRdmCCGetCommandResponse);
    EXPECT_EQ(resp->rdm_header.param_id, E137_7_ENDPOINT_RESPONDERS);
    EXPECT_EQ(resp->rdm_data_len, kEndpointRespondersResponse.size());
    EXPECT_EQ(std::memcmp(resp->rdm_data, kEndpointRespondersResponse.data(), kEndpointRespondersResponse.size()), 0);
  };
  last_conn->callbacks.message_received(last_conn, &test_resp.msg);
  EXPECT_EQ(rc_client_rpt_msg_received_fake.call_count, 1u);
}

TEST_F(TestRptClientRdmHandling, ParsesOverflowNotificationWithoutCommand)
{
  // clang-format off
  static const std::array<uint8_t, 174> kTcpCommsStatusResponse = {
    100, 101, 102, 97, 117, 108, 116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    192, 168, 1, 22,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    191, 104,
    0, 2,
    110, 111, 116, 32, 100, 101, 102, 97, 117, 108, 116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
    32, 1, 13, 184, 0, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0, 13,
    191, 105,
    0, 50
  };
  // clang-format on

  auto test_resp = TestRdmResponse::GetResponseBroadcast(client_, E133_TCP_COMMS_STATUS, kTcpCommsStatusResponse.data(),
                                                         kTcpCommsStatusResponse.size());
  RDMNET_GET_RPT_MSG(&test_resp.msg)->header.seqnum = 0;
  RPT_GET_RDM_BUF_LIST(RDMNET_GET_RPT_MSG(&test_resp.msg))->rdm_buffers = &test_resp.bufs[1];
  RPT_GET_RDM_BUF_LIST(RDMNET_GET_RPT_MSG(&test_resp.msg))->num_rdm_buffers--;

  rc_client_rpt_msg_received_fake.custom_fake = [](RCClient* client, rdmnet_client_scope_t, const RptClientMessage* msg,
                                                   RdmnetSyncRdmResponse*, bool*) {
    EXPECT_EQ(msg->type, kRptClientMsgRdmResp);
    const RdmnetRdmResponse* resp = RDMNET_GET_RDM_RESPONSE(msg);
    EXPECT_EQ(resp->rdmnet_source_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->source_endpoint, 0u);
    EXPECT_EQ(resp->seq_num, 0u);
    EXPECT_FALSE(resp->is_response_to_me);

    EXPECT_EQ(resp->original_cmd_data, nullptr);
    EXPECT_EQ(resp->original_cmd_data_len, 0);

    EXPECT_EQ(resp->rdm_header.source_uid, kTestRdmCmdsSrcUid);
    EXPECT_EQ(resp->rdm_header.dest_uid, RC_RPT_CLIENT_DATA(client)->uid);
    EXPECT_EQ(resp->rdm_header.transaction_num, kTestRdmCmdsTransactionNum);
    EXPECT_EQ(resp->rdm_header.resp_type, kRdmResponseTypeAck);
    EXPECT_EQ(resp->rdm_header.command_class, kRdmCCGetCommandResponse);
    EXPECT_EQ(resp->rdm_header.param_id, E133_TCP_COMMS_STATUS);
    EXPECT_EQ(resp->rdm_data_len, kTcpCommsStatusResponse.size());
    EXPECT_EQ(std::memcmp(resp->rdm_data, kTcpCommsStatusResponse.data(), kTcpCommsStatusResponse.size()), 0);
  };
  last_conn->callbacks.message_received(last_conn, &test_resp.msg);
  EXPECT_EQ(rc_client_rpt_msg_received_fake.call_count, 1u);
}

// clang-format off
const RdmnetSavedRdmCommand kSetDeviceInfoSavedCmd{
  {1, 2},
  E133_NULL_ENDPOINT,
  20,
  {
    {1, 2},
    kClientUid,
    20,
    1,
    0,
    kRdmCCSetCommand,
    E120_DEVICE_LABEL
  },
  { 0x64, 0x65, 0x76, 0x69, 0x63, 0x65 },
  6
};
// clang-format on

TEST_F(TestRptClientRdmHandling, RespondsBroadcastToSetCommands)
{
  ASSERT_EQ(rc_client_send_rdm_ack(&client_, scope_handle_, &kSetDeviceInfoSavedCmd, nullptr, 0), kEtcPalErrOk);
  ASSERT_EQ(rc_rpt_send_notification_fake.call_count, 1u);

  EXPECT_EQ(last_sent_buf_list.size(), 2u);
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[0]));
  EXPECT_TRUE(rdm_validate_msg(&last_sent_buf_list[1]));

  EXPECT_EQ(last_sent_header.dest_uid, kRdmnetControllerBroadcastUid);

  const RdmBuffer& response = last_sent_buf_list[1];
  RdmUid           rdm_dest_uid;
  rdm_dest_uid.manu = etcpal_unpack_u16b(&response.data[RDM_OFFSET_DEST_MANUFACTURER]);
  rdm_dest_uid.id = etcpal_unpack_u32b(&response.data[RDM_OFFSET_DEST_DEVICE]);
  EXPECT_EQ(rdm_dest_uid, kRdmBroadcastUid);
}
