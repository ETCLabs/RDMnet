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

#include <cwchar>
#include <iostream>

#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>

#include "lwpa/log.h"
#include "lwpa/uuid.h"
#include "manager.h"

static int s_utc_offset;

extern "C" {
static void manager_log_callback(void *context, const LwpaLogStrings *strings)
{
  (void)context;
  std::cout << strings->human_readable << "\n";
}

static void manager_time_callback(void *context, LwpaLogTimeParams *time)
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

std::string ConsoleInputToUtf8(const std::wstring &input)
{
  if (!input.empty())
  {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
    if (size_needed > 0)
    {
      std::string str_res(size_needed, '\0');
      int convert_res = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, &str_res[0], size_needed, NULL, NULL);
      if (convert_res > 0)
      {
        return str_res;
      }
    }
  }

  return std::string();
}

int wmain(int /*argc*/, wchar_t * /*argv*/ [])
{
  LwpaUuid manager_cid;

  UUID uuid;
  UuidCreate(&uuid);
  memcpy(manager_cid.data, &uuid, LWPA_UUID_BYTES);

  TIME_ZONE_INFORMATION tzinfo;
  switch(GetTimeZoneInformation(&tzinfo))
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

  LwpaLogParams params;
  params.action = kLwpaLogCreateHumanReadableLog;
  params.log_fn = manager_log_callback;
  params.log_mask = LWPA_LOG_UPTO(LWPA_LOG_INFO);
  params.time_fn = manager_time_callback;
  params.context = nullptr;
  lwpa_validate_log_params(&params);

  LLRPManager mgr(manager_cid, &params);
  printf("Discovered network interfaces:\n");
  mgr.PrintNetints();
  mgr.PrintCommandList();

  std::wstring input;
  do
  {
    std::getline(std::wcin, input);
  } while (mgr.ParseCommand(ConsoleInputToUtf8(input)));

  return 0;
}
