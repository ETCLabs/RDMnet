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
#include "rdmnet/broker/log.h"

#include <string>

class MockBrokerLog : public rdmnet::BrokerLog
{
public:
  MockBrokerLog() : BrokerLog(DispatchPolicy::kDirect) {}

  MOCK_METHOD(void, GetTimeFromCallback, (EtcPalLogTimestamp & time), (override));
  MOCK_METHOD(void, OutputLogMsg, (const std::string& str), (override));
};

using testing::_;
using testing::HasSubstr;
using testing::SetArgReferee;

class TestBrokerLog : public testing::Test
{
public:
  TestBrokerLog()
  {
    etcpal_init(ETCPAL_FEATURE_LOGGING);
    log_.Startup(ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG));
  }
  ~TestBrokerLog()
  {
    log_.Shutdown();
    etcpal_deinit(ETCPAL_FEATURE_LOGGING);
  }

protected:
  MockBrokerLog log_;

  const EtcPalLogTimestamp test_time_ = {1970, 1, 1, 0, 0, 0, 0, 0};

  template <typename... FormatArgs>
  void TestLogFormat(const std::string& expected, const std::string& format, FormatArgs... args)
  {
    EXPECT_CALL(log_, OutputLogMsg(HasSubstr(expected)));
    EXPECT_CALL(log_, GetTimeFromCallback(_)).WillOnce(SetArgReferee<0>(test_time_));
    log_.Log(ETCPAL_LOG_INFO, format.c_str(), args...);
  }
};

TEST_F(TestBrokerLog, LogMessagesFormattedCorrectly)
{
  TestLogFormat("Test strings: string 1 string 2", "Test strings: %s %s", "string 1", "string 2");
  TestLogFormat("Test ints: 1 2 -3", "Test ints: %u %d %d", 1u, 2, -3);
  TestLogFormat("Test floats: 1.3 27.2 1111.1111", "Test floats: %.1f %.1f %.4f", 1.3, 27.2, 1111.1111);
  TestLogFormat("Test hex: 1a 3c AAAA", "Test hex: %x %x %X", 0x1a, 0x3c, 0xaaaa);
  TestLogFormat("Test octal: 23 45 1234", "Test octal: %o %o %o", 023, 045, 01234);
  TestLogFormat("Test chars: a B -", "Test chars: %c %c %c", 'a', 'B', '-');
  TestLogFormat("Test mixed: String 20 AA 1.33 /", "Test mixed: %s %d %X %.2f %c", "String", 20, 0xaa, 1.33, '/');
}

#define TEST_PRIORITY_SHORTCUT(name, priority)     \
  log_.SetLogMask(ETCPAL_LOG_UPTO(priority));      \
  EXPECT_CALL(log_, GetTimeFromCallback(_));       \
  EXPECT_CALL(log_, OutputLogMsg("Test message")); \
  log_.name("Test message");                       \
  log_.SetLogMask(ETCPAL_LOG_UPTO(priority - 1));  \
  log_.name("Test message")  // Expect no call

TEST_F(TestBrokerLog, PriorityShortcutsWorkCorrectly)
{
  TEST_PRIORITY_SHORTCUT(Debug, ETCPAL_LOG_DEBUG);
  TEST_PRIORITY_SHORTCUT(Info, ETCPAL_LOG_INFO);
  TEST_PRIORITY_SHORTCUT(Notice, ETCPAL_LOG_NOTICE);
  TEST_PRIORITY_SHORTCUT(Warning, ETCPAL_LOG_WARNING);
  TEST_PRIORITY_SHORTCUT(Error, ETCPAL_LOG_ERR);
  TEST_PRIORITY_SHORTCUT(Critical, ETCPAL_LOG_CRIT);
  TEST_PRIORITY_SHORTCUT(Alert, ETCPAL_LOG_ALERT);
  TEST_PRIORITY_SHORTCUT(Emergency, ETCPAL_LOG_EMERG);
}
