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

#include <cwchar>
#include <iostream>

#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>

#include "etcpal/log.h"
#include "etcpal/uuid.h"
#include "manager.h"

static int s_utc_offset;

extern "C" {
static void manager_log_callback(void* context, const EtcPalLogStrings* strings)
{
  (void)context;
  std::cout << strings->human_readable << "\n";
}

static void manager_time_callback(void* context, EtcPalLogTimeParams* time)
{
  SYSTEMTIME win_time;
  (void)context;
  GetLocalTime(&win_time);
  time->year = win_time.wYear;
  time->month = win_time.wMonth;
  time->day = win_time.wDay;
  time->hour = win_time.wHour;
  time->minute = win_time.wMinute;
  time->second = win_time.wSecond;
  time->msec = win_time.wMilliseconds;
  time->utc_offset = s_utc_offset;
}
}

static std::string WCharToUtf8String(const wchar_t* str_in)
{
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, str_in, -1, NULL, 0, NULL, NULL);
  if (size_needed > 0)
  {
    std::string str_res(size_needed, '\0');
    int convert_res = WideCharToMultiByte(CP_UTF8, 0, str_in, -1, &str_res[0], size_needed, NULL, NULL);
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
  LLRPManager mgr;

  std::vector<std::string> args_utf8;
  args_utf8.reserve(argc);
  ConvertArgsToUtf8(argc, argv, args_utf8);
  switch (mgr.ParseCommandLineArgs(args_utf8))
  {
    case LLRPManager::ParseResult::kParseErr:
      mgr.PrintUsage(args_utf8[0]);
      return 1;
    case LLRPManager::ParseResult::kPrintHelp:
      mgr.PrintUsage(args_utf8[0]);
      return 0;
    case LLRPManager::ParseResult::kPrintVersion:
      mgr.PrintVersion();
      return 0;
    default:
      break;
  }

  auto manager_cid = etcpal::Uuid::OsPreferred();

  TIME_ZONE_INFORMATION tzinfo;
  switch (GetTimeZoneInformation(&tzinfo))
  {
    case TIME_ZONE_ID_UNKNOWN:
    case TIME_ZONE_ID_STANDARD:
      s_utc_offset = -(tzinfo.Bias + tzinfo.StandardBias);
      break;
    case TIME_ZONE_ID_DAYLIGHT:
      s_utc_offset = -(tzinfo.Bias + tzinfo.DaylightBias);
      break;
    default:
      break;
  }

  EtcPalLogParams params;
  params.action = kEtcPalLogCreateHumanReadable;
  params.log_fn = manager_log_callback;
  params.log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_INFO);
  params.time_fn = manager_time_callback;
  params.context = nullptr;
  etcpal_validate_log_params(&params);

  if (!mgr.Startup(manager_cid, &params))
    return 1;

  printf("Discovered network interfaces:\n");
  mgr.PrintNetints();
  mgr.PrintCommandList();

  std::wstring input;
  do
  {
    std::getline(std::wcin, input);
  } while (mgr.ParseCommand(ConsoleInputToUtf8(input)));

  mgr.Shutdown();
  return 0;
}
