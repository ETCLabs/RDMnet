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

#ifndef TEST_RDM_COMMANDS_H_
#define TEST_RDM_COMMANDS_H_

#include <vector>
#include "rdmnet/core/client.h"
#include "rdmnet/core/message.h"
#include "rdm/message.h"
#include "gtest/gtest.h"

constexpr RdmUid   kTestRdmCmdsSrcUid{42, 84};
constexpr uint32_t kTestRdmCmdsSeqNum{42};
constexpr uint8_t  kTestRdmCmdsTransactionNum{static_cast<uint8_t>(kTestRdmCmdsSeqNum)};

class TestRdmCommand
{
public:
  RdmnetMessage msg{};
  RdmBuffer     buf;

  static TestRdmCommand Get(const RCClient& client,
                            uint16_t        param_id,
                            const uint8_t*  data = nullptr,
                            uint8_t         data_len = 0);
  static TestRdmCommand Set(const RCClient& client,
                            uint16_t        param_id,
                            const uint8_t*  data = nullptr,
                            uint8_t         data_len = 0);

private:
  TestRdmCommand(const RCClient&     client,
                 rdm_command_class_t command_class,
                 uint16_t            param_id,
                 const uint8_t*      data,
                 uint8_t             data_len);
};

inline TestRdmCommand TestRdmCommand::Get(const RCClient& client,
                                          uint16_t        param_id,
                                          const uint8_t*  data,
                                          uint8_t         data_len)
{
  return TestRdmCommand(client, kRdmCCGetCommand, param_id, data, data_len);
}

inline TestRdmCommand TestRdmCommand::Set(const RCClient& client,
                                          uint16_t        param_id,
                                          const uint8_t*  data,
                                          uint8_t         data_len)
{
  return TestRdmCommand(client, kRdmCCSetCommand, param_id, data, data_len);
}

inline TestRdmCommand::TestRdmCommand(const RCClient&     client,
                                      rdm_command_class_t command_class,
                                      uint16_t            param_id,
                                      const uint8_t*      data,
                                      uint8_t             data_len)
{
  msg.vector = ACN_VECTOR_ROOT_RPT;
  msg.sender_cid = etcpal::Uuid::FromString("5ca2221f-b177-492d-9896-9270710249c3").get();

  RptMessage* rpt_msg = RDMNET_GET_RPT_MSG(&msg);
  rpt_msg->vector = VECTOR_RPT_REQUEST;
  rpt_msg->header.source_uid = kTestRdmCmdsSrcUid;
  rpt_msg->header.source_endpoint_id = E133_NULL_ENDPOINT;
  rpt_msg->header.dest_uid = RC_RPT_CLIENT_DATA(&client)->uid;
  rpt_msg->header.dest_endpoint_id = E133_NULL_ENDPOINT;
  rpt_msg->header.seqnum = kTestRdmCmdsSeqNum;

  RdmCommandHeader rdm_header;
  rdm_header.source_uid = kTestRdmCmdsSrcUid;
  rdm_header.dest_uid = RC_RPT_CLIENT_DATA(&client)->uid;
  rdm_header.transaction_num = kTestRdmCmdsTransactionNum;
  rdm_header.port_id = 1;
  rdm_header.subdevice = 0;
  rdm_header.command_class = command_class;
  rdm_header.param_id = param_id;

  EXPECT_EQ(rdm_pack_command(&rdm_header, data, data_len, &buf), kEtcPalErrOk);
  RptRdmBufList* buf_list = RPT_GET_RDM_BUF_LIST(rpt_msg);
  buf_list->rdm_buffers = &buf;
  buf_list->num_rdm_buffers = 1;
  buf_list->more_coming = false;
}

class TestRdmResponse
{
public:
  RdmnetMessage          msg{};
  std::vector<RdmBuffer> bufs;

  static TestRdmResponse GetResponse(const RCClient& client,
                                     uint16_t        param_id,
                                     const uint8_t*  data = nullptr,
                                     size_t          data_len = 0,
                                     const uint8_t*  cmd_data = nullptr,
                                     uint8_t         cmd_data_len = 0);
  static TestRdmResponse GetResponseBroadcast(const RCClient& client,
                                              uint16_t        param_id,
                                              const uint8_t*  data = nullptr,
                                              size_t          data_len = 0,
                                              const uint8_t*  cmd_data = nullptr,
                                              uint8_t         cmd_data_len = 0);
  static TestRdmResponse SetResponse(const RCClient& client,
                                     uint16_t        param_id,
                                     const uint8_t*  data = nullptr,
                                     size_t          data_len = 0,
                                     const uint8_t*  cmd_data = nullptr,
                                     uint8_t         cmd_data_len = 0);

private:
  TestRdmResponse(const RCClient&     client,
                  rdm_command_class_t command_class,
                  uint16_t            param_id,
                  const uint8_t*      data,
                  size_t              data_len,
                  bool                broadcast,
                  const uint8_t*      cmd_data,
                  uint8_t             cmd_data_len);
};

inline TestRdmResponse TestRdmResponse::GetResponse(const RCClient& client,
                                                    uint16_t        param_id,
                                                    const uint8_t*  data,
                                                    size_t          data_len,
                                                    const uint8_t*  cmd_data,
                                                    uint8_t         cmd_data_len)
{
  return TestRdmResponse(client, kRdmCCGetCommandResponse, param_id, data, data_len, false, cmd_data, cmd_data_len);
}

inline TestRdmResponse TestRdmResponse::GetResponseBroadcast(const RCClient& client,
                                                             uint16_t        param_id,
                                                             const uint8_t*  data,
                                                             size_t          data_len,
                                                             const uint8_t*  cmd_data,
                                                             uint8_t         cmd_data_len)
{
  return TestRdmResponse(client, kRdmCCGetCommandResponse, param_id, data, data_len, true, cmd_data, cmd_data_len);
}

inline TestRdmResponse TestRdmResponse::SetResponse(const RCClient& client,
                                                    uint16_t        param_id,
                                                    const uint8_t*  data,
                                                    size_t          data_len,
                                                    const uint8_t*  cmd_data,
                                                    uint8_t         cmd_data_len)
{
  return TestRdmResponse(client, kRdmCCSetCommandResponse, param_id, data, data_len, false, cmd_data, cmd_data_len);
}

inline TestRdmResponse::TestRdmResponse(const RCClient&     client,
                                        rdm_command_class_t command_class,
                                        uint16_t            param_id,
                                        const uint8_t*      data,
                                        size_t              data_len,
                                        bool                broadcast,
                                        const uint8_t*      cmd_data,
                                        uint8_t             cmd_data_len)
{
  msg.vector = ACN_VECTOR_ROOT_RPT;
  msg.sender_cid = etcpal::Uuid::FromString("36199247-a44e-4eb7-b43f-d6374b783a7a").get();

  RptMessage* rpt_msg = RDMNET_GET_RPT_MSG(&msg);
  rpt_msg->vector = VECTOR_RPT_NOTIFICATION;
  rpt_msg->header.source_uid = kTestRdmCmdsSrcUid;
  rpt_msg->header.source_endpoint_id = E133_NULL_ENDPOINT;
  rpt_msg->header.dest_uid = broadcast ? kRdmnetControllerBroadcastUid : RC_RPT_CLIENT_DATA(&client)->uid;
  rpt_msg->header.dest_endpoint_id = E133_NULL_ENDPOINT;
  rpt_msg->header.seqnum = kTestRdmCmdsSeqNum;

  RdmCommandHeader rdm_header;
  rdm_header.source_uid = RC_RPT_CLIENT_DATA(&client)->uid;
  rdm_header.dest_uid = kTestRdmCmdsSrcUid;
  rdm_header.transaction_num = kTestRdmCmdsTransactionNum;
  rdm_header.port_id = 1;
  rdm_header.subdevice = 0;
  rdm_header.command_class = static_cast<rdm_command_class_t>(static_cast<int>(command_class) - 1);
  rdm_header.param_id = param_id;

  bufs.resize(rdm_get_num_responses_needed(param_id, data_len) + 1);

  EXPECT_EQ(rdm_pack_command(&rdm_header, cmd_data, cmd_data_len, &bufs[0]), kEtcPalErrOk);
  EXPECT_EQ(rdm_pack_full_response(&rdm_header, data, data_len, &bufs[1], bufs.size() - 1), kEtcPalErrOk);

  RptRdmBufList* buf_list = RPT_GET_RDM_BUF_LIST(rpt_msg);
  buf_list->rdm_buffers = bufs.data();
  buf_list->num_rdm_buffers = bufs.size();
  buf_list->more_coming = false;
}

#endif  // TEST_RDM_COMMANDS_H_
