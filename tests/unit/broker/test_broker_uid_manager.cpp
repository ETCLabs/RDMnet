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
#include "broker_uid_manager.h"

class TestBrokerUidManager : public ::testing::Test
{
protected:
  BrokerUidManager manager_;
};

TEST_F(TestBrokerUidManager, static_uid)
{
  // Test adding static UIDs
  RdmUid test_1 = {0, 1};
  RdmUid test_2 = {0, 2};
  RdmUid test_3 = {10, 20};

  auto res = manager_.AddStaticUid(1, test_1);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  res = manager_.AddStaticUid(2, test_2);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  res = manager_.AddStaticUid(3, test_3);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);

  int handle_out;
  ASSERT_TRUE(manager_.UidToHandle(test_1, handle_out));
  ASSERT_EQ(handle_out, 1);
  ASSERT_TRUE(manager_.UidToHandle(test_2, handle_out));
  ASSERT_EQ(handle_out, 2);
  ASSERT_TRUE(manager_.UidToHandle(test_3, handle_out));
  ASSERT_EQ(handle_out, 3);

  // Static UID conflict
  res = manager_.AddStaticUid(4, test_1);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kDuplicateId);

  // Remove Static UID
  manager_.RemoveUid(test_1);
  ASSERT_FALSE(manager_.UidToHandle(test_1, handle_out));

  // Add the same Static UID again with a different connection
  res = manager_.AddStaticUid(5, test_1);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_TRUE(manager_.UidToHandle(test_1, handle_out));
  ASSERT_EQ(handle_out, 5);
}

TEST_F(TestBrokerUidManager, dynamic_uid)
{
  manager_.SetNextDeviceId(1000);

  EtcPalUuid cid_1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  RdmUid uid_1 = {0xe574, 0};

  auto res = manager_.AddDynamicUid(1, cid_1, uid_1);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  // Should not change the manufacturer portion
  ASSERT_EQ(uid_1.manu, 0xe574u);
  // Should have gotten the next device ID
  ASSERT_EQ(uid_1.id, 1000u);

  // Can't add the same CID again
  res = manager_.AddDynamicUid(2, cid_1, uid_1);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kDuplicateId);

  // Add another one
  EtcPalUuid cid_2 = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  RdmUid uid_2 = {0x8001, 0};
  res = manager_.AddDynamicUid(3, cid_2, uid_2);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_EQ(uid_2.manu, 0x8001u);
  ASSERT_EQ(uid_2.id, 1001u);

  // Find them both
  int handle_out;
  ASSERT_TRUE(manager_.UidToHandle(uid_1, handle_out));
  ASSERT_EQ(handle_out, 1);
  ASSERT_TRUE(manager_.UidToHandle(uid_2, handle_out));
  ASSERT_EQ(handle_out, 3);

  // Remove the first one
  manager_.RemoveUid(uid_1);
  ASSERT_FALSE(manager_.UidToHandle(uid_1, handle_out));

  // Re-add the first one - it should get its reservation
  uid_1.manu = 0xe574;
  uid_1.id = 0;
  res = manager_.AddDynamicUid(4, cid_1, uid_1);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_EQ(uid_1.manu, 0xe574u);
  ASSERT_EQ(uid_1.id, 1000u);
}

TEST_F(TestBrokerUidManager, wraparound)
{
  manager_.SetNextDeviceId(1);

  RdmUid test_uid = {0x8001, 0};
  EtcPalUuid test_cid = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

  // Generate the first 3 dynamic UIDs in the range
  auto res = manager_.AddDynamicUid(1, test_cid, test_uid);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_EQ(test_uid.id, 1u);

  test_cid.data[15] = 1;  // Each new UID generated needs a different CID.
  test_uid.id = 0;
  res = manager_.AddDynamicUid(2, test_cid, test_uid);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_EQ(test_uid.id, 2u);

  test_cid.data[15] = 2;
  test_uid.id = 0;
  res = manager_.AddDynamicUid(3, test_cid, test_uid);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_EQ(test_uid.id, 3u);

  // Remove the one with ID 2
  test_uid.id = 2;
  manager_.RemoveUid(test_uid);

  // Now for the wraparound case - pretend we've assigned everything in the 32-bit range
  manager_.SetNextDeviceId(0xffffffff);

  // Assign the highest possible Device Id of 0xffffffff
  test_cid.data[15] = 3;
  test_uid.id = 0;
  res = manager_.AddDynamicUid(4, test_cid, test_uid);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_EQ(test_uid.id, 0xffffffffu);

  // Next one should wrap around, skip over 1 which is already assigned, and be assigned Device ID
  // 2 (Device ID 0 is reserved).
  test_cid.data[15] = 4;
  test_uid.id = 0;
  res = manager_.AddDynamicUid(5, test_cid, test_uid);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_EQ(test_uid.id, 2u);

  // Next one should skip over 3 which is already assigned and be assigned 4.
  test_cid.data[15] = 5;
  test_uid.id = 0;
  res = manager_.AddDynamicUid(6, test_cid, test_uid);
  ASSERT_EQ(res, BrokerUidManager::AddResult::kOk);
  ASSERT_EQ(test_uid.id, 4u);
}
