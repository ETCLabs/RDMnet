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

#include <string>
#include <iostream>
#include <ctime>

#include "etcpal/uuid.h"
#include "manager.h"

extern "C" {
static void manager_log_callback(void* context, const EtcPalLogStrings* strings)
{
  (void)context;
  std::cout << strings->human_readable << "\n";
}

static void manager_time_callback(void* context, EtcPalLogTimeParams* time_params)
{
  time_t t = time(NULL);
  struct tm* local_time = localtime(&t);
  time_params->year = local_time->tm_year + 1900;
  time_params->month = local_time->tm_mon + 1;
  time_params->day = local_time->tm_mday;
  time_params->hour = local_time->tm_hour;
  time_params->minute = local_time->tm_min;
  time_params->second = local_time->tm_sec;
  time_params->msec = 0;
  time_params->utc_offset = (int)(local_time->tm_gmtoff / 60);
}
}

int main(int argc, char* argv[])
{
  LLRPManager mgr;

  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 0; i < argc; ++i)
    args.push_back(std::string(argv[i]));
  switch (mgr.ParseCommandLineArgs(args))
  {
    case LLRPManager::ParseResult::kParseErr:
      mgr.PrintUsage(argv[0]);
      return 1;
    case LLRPManager::ParseResult::kPrintHelp:
      mgr.PrintUsage(argv[0]);
      return 0;
    case LLRPManager::ParseResult::kPrintVersion:
      mgr.PrintVersion();
      return 0;
    default:
      break;
  }

  auto manager_cid = etcpal::Uuid::OsPreferred();

  EtcPalLogParams params;
  params.action = kEtcPalLogCreateHumanReadableLog;
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

  std::string input;
  do
  {
    std::getline(std::cin, input);
  } while (mgr.ParseCommand(input));

  mgr.Shutdown();
  return 0;
}
