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

#include "win_broker_log.h"

#include <iostream>
#include <string>
#include <ShlObj.h>
#include <WinSock2.h>
#include <Windows.h>

static const std::vector<std::wstring> kLogFileDirComponents = {L"ETC", L"RDMnet Examples"};
static constexpr WCHAR                 kLogFileBaseName[] = L"broker.log";

// Gets the full path to the log file, creating intermediate directories if necessary.
std::wstring GetLogFileName()
{
  PWSTR app_data_path;
  if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &app_data_path) == S_OK)
  {
    std::wstring cur_path = app_data_path;
    for (const auto& directory_part : kLogFileDirComponents)
    {
      cur_path += L"\\" + directory_part;
      if (!CreateDirectoryW(cur_path.c_str(), NULL))
      {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        {
          // Something went wrong creating an intermediate directory.
          std::wcout << L"Couldn't create directory " << cur_path << L": Error " << error << ".\n";
          return std::wstring{};
        }
      }
    }
    return cur_path + L"\\" + kLogFileBaseName;
  }
  return std::wstring{};
}

bool WindowsBrokerLog::Startup(int log_mask)
{
  const auto file_name = GetLogFileName();
  if (!file_name.empty())
  {
    file_.open(file_name.c_str(), std::fstream::out);
    if (!file_)
      std::wcout << L"BrokerLog couldn't open log file '" << file_name << L"'.\n";
  }

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
  return etcpal::LogTimestamp(win_time.wYear, win_time.wMonth, win_time.wDay, win_time.wHour, win_time.wMinute,
                              win_time.wSecond, win_time.wMilliseconds, utcoffset_);
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
  {
    file_ << strings.human_readable << std::endl;
    file_.flush();
  }
}
