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

#include "rdmnet/cpp/broker.h"

#include "gtest/gtest.h"

TEST(TestBrokerSettings, DefaultConstructedSettingsIsNotValid)
{
  rdmnet::Broker::Settings settings;
  EXPECT_FALSE(settings.IsValid());
}

TEST(TestBrokerSettings, ExplicitConstructedSettingsIsValid)
{
  // Constructed using dynamic UID
  rdmnet::Broker::Settings settings(etcpal::Uuid::OsPreferred(), 0x6574);
  EXPECT_TRUE(settings.IsValid());

  rdmnet::Broker::Settings settings_2(etcpal::Uuid::OsPreferred(), {0x6574, 0x00001234});
  EXPECT_TRUE(settings_2.IsValid());
}
