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

#include "win_device_log.h"

#include <WinSock2.h>
#include <Windows.h>
#include <ShlObj.h>
#include <stdio.h>

static const WCHAR* kLogFileDirComponents[] = {L"ETC", L"RDMnet Examples"};
#define LOG_FILE_BASENAME L"device.log"

static EtcPalLogParams s_device_log_params;
static FILE*           s_log_file;
static int             s_utc_offset;
static WCHAR           s_log_file_name[MAX_PATH];

const WCHAR* get_log_file_name(void)
{
  PWSTR app_data_path;
  if (SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &app_data_path) == S_OK)
  {
    wcscpy_s(s_log_file_name, MAX_PATH, app_data_path);
    for (size_t i = 0; i < sizeof(kLogFileDirComponents) / sizeof(kLogFileDirComponents[0]); ++i)
    {
      wcscat_s(s_log_file_name, MAX_PATH, L"\\");
      wcscat_s(s_log_file_name, MAX_PATH, kLogFileDirComponents[i]);
      if (!CreateDirectoryW(s_log_file_name, NULL))
      {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        {
          // Something went wrong creating an intermediate directory.
          wprintf(L"Couldn't create directory %s: Error %d.\n", s_log_file_name, error);
          return NULL;
        }
      }
    }
    wcscat_s(s_log_file_name, MAX_PATH, L"\\");
    wcscat_s(s_log_file_name, MAX_PATH, LOG_FILE_BASENAME);
    return s_log_file_name;
  }
  return NULL;
}

static void device_log_callback(void* context, const EtcPalLogStrings* strings)
{
  (void)context;
  printf("%s\n", strings->human_readable);
  if (s_log_file)
    fprintf(s_log_file, "%s\n", strings->human_readable);
}

static void device_time_callback(void* context, EtcPalLogTimestamp* time)
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

void device_log_init(void)
{
  etcpal_init(ETCPAL_FEATURE_LOGGING);

  const WCHAR* file_name = get_log_file_name();
  if (file_name)
  {
    s_log_file = _wfopen(file_name, L"w");
    if (!s_log_file)
      wprintf(L"Device Log: Couldn't open log file %s\n", file_name);
  }

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
      printf("Device Log: Couldn't get time zone info.\n");
      break;
  }

  s_device_log_params.action = kEtcPalLogCreateHumanReadable;
  s_device_log_params.log_fn = device_log_callback;
  s_device_log_params.log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG);
  s_device_log_params.time_fn = device_time_callback;
  s_device_log_params.context = NULL;

  etcpal_validate_log_params(&s_device_log_params);
}

const EtcPalLogParams* device_get_log_params(void)
{
  return &s_device_log_params;
}

void device_log_deinit(void)
{
  if (s_log_file)
    fclose(s_log_file);
  etcpal_deinit(ETCPAL_FEATURE_LOGGING);
}
