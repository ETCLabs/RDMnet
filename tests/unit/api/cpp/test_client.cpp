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

#include "rdmnet/cpp/client.h"

#include "gtest/gtest.h"

TEST(TestDestinationAddr, ToDefaultResponderWorks)
{
  auto addr = rdmnet::DestinationAddr::ToDefaultResponder(rdm::Uid(0x1234, 0x56789abc));

  auto c_addr = addr.get();
  EXPECT_EQ(c_addr.rdmnet_uid.manu, 0x1234);
  EXPECT_EQ(c_addr.rdmnet_uid.id, 0x56789abcu);
  EXPECT_EQ(c_addr.endpoint, 0);
  EXPECT_EQ(c_addr.rdm_uid.manu, 0x1234);
  EXPECT_EQ(c_addr.rdm_uid.id, 0x56789abcu);
  EXPECT_EQ(c_addr.subdevice, 0);
}

// The default constructor of a scope config should create the default scope with dynamic discovery
TEST(TestScope, DefaultConstructorWorks)
{
  rdmnet::Scope scope;

  EXPECT_TRUE(scope.IsDefault());
  EXPECT_FALSE(scope.IsStatic());
  EXPECT_EQ(scope.id_string(), E133_DEFAULT_SCOPE);
}
