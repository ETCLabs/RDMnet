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

#include <string>
#include <iostream>
#include <ctime>

#include "etcpal/cpp/log.h"
#include "manager.h"

class ManagerLogHandler : public etcpal::LogMessageHandler
{
public:
  void                 HandleLogMessage(const EtcPalLogStrings& strings) override;
  etcpal::LogTimestamp GetLogTimestamp() override;
};

void ManagerLogHandler::HandleLogMessage(const EtcPalLogStrings& strings)
{
  std::cout << strings.human_readable << '\n';
}

etcpal::LogTimestamp ManagerLogHandler::GetLogTimestamp()
{
  time_t     t = time(NULL);
  struct tm* local_time = localtime(&t);

  return etcpal::LogTimestamp(local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
                              local_time->tm_hour, local_time->tm_min, local_time->tm_sec, 0,
                              (int)(local_time->tm_gmtoff / 60));
}

int main(int argc, char* argv[])
{
  LlrpManagerExample mgr;

  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 0; i < argc; ++i)
    args.push_back(std::string(argv[i]));
  switch (mgr.ParseCommandLineArgs(args))
  {
    case LlrpManagerExample::ParseResult::kParseErr:
      mgr.PrintUsage(argv[0]);
      return 1;
    case LlrpManagerExample::ParseResult::kPrintHelp:
      mgr.PrintUsage(argv[0]);
      return 0;
    case LlrpManagerExample::ParseResult::kPrintVersion:
      mgr.PrintVersion();
      return 0;
    default:
      break;
  }

  ManagerLogHandler log_handler;
  etcpal::Logger    logger;
  logger.SetLogAction(kEtcPalLogCreateHumanReadable).SetLogMask(ETCPAL_LOG_UPTO(ETCPAL_LOG_INFO)).Startup(log_handler);

  if (!mgr.Startup(logger))
  {
    logger.Shutdown();
    return 1;
  }

  printf("Discovered network interfaces:\n");
  mgr.PrintNetints();
  mgr.PrintCommandList();

  std::string input;
  do
  {
    std::getline(std::cin, input);
  } while (mgr.ParseCommand(input));

  mgr.Shutdown();
  logger.Shutdown();
  return 0;
}
