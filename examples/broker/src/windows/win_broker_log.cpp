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
#include <cstdarg>

WindowsBrokerLog::WindowsBrokerLog(const std::string& file_name) : RDMnet::BrokerLog(), utcoffset_(0)
{
  file_.open(file_name.c_str(), std::fstream::out);
  if (file_.fail())
    std::cout << "BrokerLog couldn't open log file '" << file_name << "'." << std::endl;

  WSADATA wsdata;
  WSAStartup(MAKEWORD(2, 2), &wsdata);

  TIME_ZONE_INFORMATION tzinfo;
  switch (GetTimeZoneInformation(&tzinfo))
  {
    case TIME_ZONE_ID_UNKNOWN:
    case TIME_ZONE_ID_STANDARD:
      utcoffset_ = -(tzinfo.Bias + tzinfo.StandardBias);
      break;
    case TIME_ZONE_ID_DAYLIGHT:
      utcoffset_ = -(tzinfo.Bias + tzinfo.DaylightBias);
      break;
    default:
      std::cout << "BrokerLog couldn't get time zone info." << std::endl;
      break;
  }
}

WindowsBrokerLog::~WindowsBrokerLog()
{
  WSACleanup();
  file_.close();
}

void WindowsBrokerLog::GetTimeFromCallback(EtcPalLogTimeParams* time)
{
  SYSTEMTIME win_time;
  GetLocalTime(&win_time);
  time->year = win_time.wYear;
  time->month = win_time.wMonth;
  time->day = win_time.wDay;
  time->hour = win_time.wHour;
  time->minute = win_time.wMinute;
  time->second = win_time.wSecond;
  time->msec = win_time.wMilliseconds;
  time->utc_offset = utcoffset_;
}

void WindowsBrokerLog::OutputLogMsg(const std::string& str)
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
  std::cout << str << "\n";
  if (file_.is_open())
    file_ << str << std::endl;
}
