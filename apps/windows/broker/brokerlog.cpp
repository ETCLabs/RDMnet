/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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

#include "brokerlog.h"
#include <iostream>
#include <cstdarg>
#include "serviceshell.h"

WindowsBrokerLog::WindowsBrokerLog(bool debug, const std::string &file_name) : BrokerLog(), debug_(debug), utcoffset_(0)
{
  file_.open(file_name.c_str(), std::fstream::out);
  if (file_.fail())
  {
    std::cout << "BrokerLog couldn't open log file '" << file_name << "'." << std::endl;
  }

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

  DWORD procid = GetCurrentProcessId();
  snprintf(log_params_.syslog_params.procid, LWPA_LOG_PROCID_MAX_LEN, "%d", procid);

  char hostname[LWPA_LOG_HOSTNAME_MAX_LEN];
  if (0 != gethostname(hostname, LWPA_LOG_HOSTNAME_MAX_LEN))
  {
    WCHAR error_text[128];
    char error_text_utf8[256] = "Unknown Error";

    GetLastErrorMessage(error_text, 128);
    WideCharToMultiByte(CP_UTF8, 0, error_text, -1, error_text_utf8, 256, NULL, NULL);
    LogFromCallback("BrokerLog couldn't get hostname: Error '" + std::string(error_text_utf8) + "'.");
    hostname[0] = '\0';
  }

  InitializeLogParams(hostname, "RDMnet Broker", std::to_string(procid), LWPA_LOG_LOCAL1,
                      debug ? LWPA_LOG_UPTO(LWPA_LOG_DEBUG) : LWPA_LOG_UPTO(LWPA_LOG_INFO));
}

WindowsBrokerLog::~WindowsBrokerLog()
{
  WSACleanup();
  file_.close();
}

void WindowsBrokerLog::GetTimeFromCallback(LwpaLogTimeParams *time)
{
  SYSTEMTIME win_time;
  GetLocalTime(&win_time);
  time->cur_time.tm_year = win_time.wYear - 1900;
  time->cur_time.tm_mon = win_time.wMonth - 1;
  time->cur_time.tm_mday = win_time.wDay;
  time->cur_time.tm_hour = win_time.wHour;
  time->cur_time.tm_min = win_time.wMinute;
  time->cur_time.tm_sec = win_time.wSecond;
  time->msec = win_time.wMilliseconds;
  time->utc_offset = utcoffset_;
}

void WindowsBrokerLog::OutputLogMsg(const std::string &str)
{
  if (debug_)
  {
    // Haven't figured out the secret recipe for UTF-8 -> Windows Console yet.
    //    WCHAR wmsg[LWPA_LOG_MSG_MAX_LEN + 1];
    //    if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wmsg,
    //                            LWPA_LOG_MSG_MAX_LEN + 1) != 0)
    //    {
    //      char final_msg[LWPA_LOG_MSG_MAX_LEN + 1];
    //      if (WideCharToMultiByte(CP_OEMCP, 0, wmsg, -1, final_msg,
    //                              LWPA_LOG_MSG_MAX_LEN + 1, NULL, NULL) != 0)
    //      {
    //        std::cout << final_msg << std::endl;
    //      }
    //    }
    std::cout << str << std::endl;
  }
  if (file_.is_open())
    file_ << str << std::endl;
}
