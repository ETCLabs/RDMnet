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

#include <cwchar>
#include <iostream>

#include "manager.h"

#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>

class ManagerLogHandler : public etcpal::LogMessageHandler
{
public:
  ManagerLogHandler();

  void                 HandleLogMessage(const EtcPalLogStrings& strings) override;
  etcpal::LogTimestamp GetLogTimestamp() override;

private:
  int utc_offset_{0};
};

ManagerLogHandler::ManagerLogHandler()
{
  TIME_ZONE_INFORMATION tzinfo;
  switch (GetTimeZoneInformation(&tzinfo))
  {
    case TIME_ZONE_ID_UNKNOWN:
    case TIME_ZONE_ID_STANDARD:
      utc_offset_ = -(tzinfo.Bias + tzinfo.StandardBias);
      break;
    case TIME_ZONE_ID_DAYLIGHT:
      utc_offset_ = -(tzinfo.Bias + tzinfo.DaylightBias);
      break;
    default:
      break;
  }
}

void ManagerLogHandler::HandleLogMessage(const EtcPalLogStrings& strings)
{
  std::cout << strings.human_readable << '\n';
}

etcpal::LogTimestamp ManagerLogHandler::GetLogTimestamp()
{
  SYSTEMTIME win_time;
  GetLocalTime(&win_time);
  return etcpal::LogTimestamp(win_time.wYear, win_time.wMonth, win_time.wDay, win_time.wHour, win_time.wMinute,
                              win_time.wSecond, win_time.wMilliseconds, utc_offset_);
}

static std::string WCharToUtf8String(const wchar_t* str_in)
{
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, str_in, -1, NULL, 0, NULL, NULL);
  if (size_needed > 0)
  {
    std::string str_res(size_needed, '\0');
    int         convert_res = WideCharToMultiByte(CP_UTF8, 0, str_in, -1, &str_res[0], size_needed, NULL, NULL);
    if (convert_res > 0)
    {
      // Remove the NULL from being included in the string's count
      str_res.resize(size_needed - 1);
      return str_res;
    }
  }

  return std::string();
}

void ConvertArgsToUtf8(int argc, wchar_t* argv[], std::vector<std::string>& args_out)
{
  for (int i = 0; i < argc; ++i)
  {
    args_out.push_back(WCharToUtf8String(argv[i]));
  }
}

std::string ConsoleInputToUtf8(const std::wstring& input)
{
  if (!input.empty())
  {
    return WCharToUtf8String(input.c_str());
  }

  return std::string();
}

int wmain(int argc, wchar_t* argv[])
{
  LlrpManagerExample mgr;

  std::vector<std::string> args_utf8;
  args_utf8.reserve(argc);
  ConvertArgsToUtf8(argc, argv, args_utf8);
  switch (mgr.ParseCommandLineArgs(args_utf8))
  {
    case LlrpManagerExample::ParseResult::kParseErr:
      mgr.PrintUsage(args_utf8[0]);
      return 1;
    case LlrpManagerExample::ParseResult::kPrintHelp:
      mgr.PrintUsage(args_utf8[0]);
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

  std::wstring input;
  do
  {
    std::getline(std::wcin, input);
  } while (mgr.ParseCommand(ConsoleInputToUtf8(input)));

  mgr.Shutdown();
  logger.Shutdown();
  return 0;
}
