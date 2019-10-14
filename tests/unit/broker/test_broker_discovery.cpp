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
#include "gmock/gmock.h"
#include "fff.h"
#include "broker_discovery.h"
#include "rdmnet_mock/core/discovery.h"

class MockBrokerDiscoveryNotify : public BrokerDiscoveryNotify
{
  MOCK_METHOD(void, BrokerRegistered,
              (const std::string& scope, const std::string& requested_service_name,
               const std::string& assigned_service_name),
              (override));
  MOCK_METHOD(void, OtherBrokerFound, (const RdmnetBrokerDiscInfo& broker_info), (override));
  MOCK_METHOD(void, OtherBrokerLost, (const std::string& scope, const std::string& service_name), (override));
  MOCK_METHOD(void, BrokerRegisterError,
              (const std::string& scope, const std::string& requested_service_name, int platform_error), (override));
};

class TestBrokerDiscovery : public testing::Test
{
protected:
  MockBrokerDiscoveryNotify notify_;
  BrokerDiscoveryManager disc_mgr_;
  rdmnet::BrokerSettings settings_;

  void SetUp() override
  {
    RDMNET_CORE_DISCOVERY_DO_FOR_ALL_FAKES(RESET_FAKE);

    disc_mgr_.SetNotify(&notify_);

    settings_.cid = etcpal::Uuid::FromString("22672657-407a-4a83-b34c-0929ec6d0bfb");
    settings_.dns.manufacturer = "Test";
    settings_.dns.model = "Test Broker";
    settings_.dns.service_instance_name = "Test Broker Service Instance";
    settings_.scope = "Test Scope";
  }
};

TEST_F(TestBrokerDiscovery, RegisterWorksWithNoErrors)
{
}
