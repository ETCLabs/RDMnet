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

#include "win_broker_log.h"

#include <iostream>
#include <WinSock2.h>
#include <Windows.h>

bool WindowsBrokerLog::Startup(const std::string& file_name, int log_mask)
{
  file_.open(file_name.c_str(), std::fstream::out);
  if (!file_.is_open())
    std::cout << "BrokerLog couldn't open log file '" << file_name << "'.\n";

  WSADATA wsdata;
  WSAStartup(MAKEWORD(2, 2), &wsdata);

  TIME_ZONE_INFORMATION tzinfo;
  switch (GetTimeZoneInformation(&tzinfo))
  {
    case TIME_ZONE_ID_UNKNOWN:
    case TIME_ZONE_ID_STANDARD:
      utcoffset_ = static_cast<int>(-(tzinfo.Bias + tzinfo.StandardBias));
      break;
    case TIME_ZONE_ID_DAYLIGHT:
      utcoffset_ = static_cast<int>(-(tzinfo.Bias + tzinfo.DaylightBias));
      break;
    default:
      std::cout << "BrokerLog couldn't get time zone info." << std::endl;
      break;
  }

  logger_.SetLogMask(log_mask);
  return logger_.Startup(*this);
}

void WindowsBrokerLog::Shutdown()
{
  logger_.Shutdown();
  WSACleanup();
  file_.close();
}

etcpal::LogTimestamp WindowsBrokerLog::GetLogTimestamp()
{
  SYSTEMTIME win_time;
  GetLocalTime(&win_time);
  return etcpal::LogTimestamp(win_time.wYear,   win_time.wMonth,  win_time.wDay,          win_time.wHour,
                              win_time.wMinute, win_time.wSecond, win_time.wMilliseconds, utcoffset_);
}

void WindowsBrokerLog::HandleLogMessage(const EtcPalLogStrings& strings)
{
  // Haven't figured out the secret recipe for UTF-8 -> Windows Console yet.
  //    WCHAR wmsg[ETCPAL_LOG_MSG_MAX_LEN + 1];
  //    if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wmsg,
  //                            ETCPAL_LOG_MSG_MAX_LEN + 1) != 0)
  //    {
  //      char final_msg[ETCPAL_LOG_MSG_MAX_LEN + 1];
  //      if (WideCharToMultiByte(CP_OEMCP, 0, wmsg, -1, final_msg,
  //                              ETCPAL_LOG_MSG_MAX_LEN + 1, NULL, NULL) != 0)
  //      {
  //        std::cout << final_msg << std::endl;
  //      }
  //    }

  std::cout << strings.human_readable << '\n';
  if (file_.is_open())
    file_ << strings.human_readable << std::endl;
}
