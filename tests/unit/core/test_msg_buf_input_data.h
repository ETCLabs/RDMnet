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
#ifndef _TEST_MSG_BUF_INPUT_DATA_H_
#define _TEST_MSG_BUF_INPUT_DATA_H_

#include "rdmnet/core/message.h"

// A full, valid RPT Notification PDU
namespace RptNotificationPduFullValid
{
// Root Layer PDU fields
constexpr LwpaUuid sender_cid = {
    {0xde, 0xad, 0xbe, 0xef, 0xba, 0xad, 0xf0, 0x0d, 0xfa, 0xce, 0xb0, 0x0c, 0xd1, 0x5e, 0xea, 0x5e}};
constexpr uint32_t root_vector = ACN_VECTOR_ROOT_RPT;

// RPT Header fields
constexpr uint32_t rpt_vector = VECTOR_RPT_NOTIFICATION;
constexpr RdmUid rpt_src_uid = {0x1234, 0x5678aaaa};
constexpr uint16_t rpt_src_endpoint = 0x0004;
constexpr RdmUid rpt_dest_uid = {0xfffc, 0xffffffff};
constexpr uint16_t rpt_dest_endpoint = 0x0000;
constexpr uint32_t seq_num = 0x12345678;

// Notification Header fields
constexpr uint32_t notification_vector = VECTOR_NOTIFICATION_RDM_CMD;

// RDM SET Command
constexpr RdmBuffer first_cmd = {{0xcc, 0x01, 0x1a, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xcb, 0xa9, 0x87, 0x65, 0x43,
                                  0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0xf0, 0x02, 0x00, 0x10, 0x07, 0x47},
                                 0x1c};
constexpr RdmBuffer second_cmd = {{0xcc, 0x01, 0x18, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21, 0x12, 0x34, 0x56, 0x78,
                                   0x9a, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0xf0, 0x00, 0x07, 0x34},
                                  0x1a};

// clang-format off
constexpr uint8_t buf[] = {
  // ACN packet identifier
  0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00,
  // Total length
  0x00, 0x00, 0x00, 0x76,
  // Root layer PDU flags & length & vector
  0xf0, 0x00, 0x76, 0x00, 0x00, 0x00, 0x05,
  // Sender CID
  0xde, 0xad, 0xbe, 0xef, 0xba, 0xad, 0xf0, 0x0d, 0xfa, 0xce, 0xb0, 0x0c, 0xd1, 0x5e, 0xea, 0x5e,
  // RPT PDU flags & length & vector
  0xf0, 0x00, 0x5f, 0x00, 0x00, 0x00, 0x03,
  // RPT PDU header
  0x12, 0x34, 0x56, 0x78, 0xaa, 0xaa, // Source UID
  0x00, 0x04, // Source Endpoint ID
  0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, // Destination UID
  0x00, 0x00, // Destination Endpoint ID
  0x12, 0x34, 0x56, 0x78, // Sequence number
  0x00, // Reserved
  // Notification PDU flags & length & vector
  0xf0, 0x00, 0x43, 0x00, 0x00, 0x00, 0x01,
  // RDM Command PDU flags & length & vector
  0xf0, 0x00, 0x1f, 0xcc,
  // RDM SET Command
  0x01, 0x1a, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0xf0, 0x02, 0x00, 0x10, 0x07, 0x47,
  // RDM Command PDU flags & length & vector
  0xf0, 0x00, 0x1d, 0xcc,
  // RDM SET_RESPONSE Command
  0x01, 0x18, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0xf0, 0x00, 0x07, 0x34
};
// clang-format on
};  // namespace RptNotificationPduFullValid

#endif  // _TEST_MSG_BUF_INPUT_DATA_H_