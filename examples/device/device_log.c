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

#include "device_log.h"

#include <WinSock2.h>
#include <Windows.h>
#include <stdio.h>

static LwpaLogParams s_device_log_params;
static FILE *s_log_file;
static int s_utc_offset;

/*-----------------------------------------------------------------
  Function: GetLastErrorMessage

    Input:  - Pointer to the buffer where the descriptive message about
      the last error is returned.
      - Maximum number of characters that can be copied into
      the error message buffer (including the terminating
      NULL character)
-----------------------------------------------------------------*/
static void GetLastErrorMessage(LPWSTR lpszerr, unsigned int nSize)
{
  LPWSTR lpMsgBuf;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                 GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, NULL);

  if (wcslen(lpMsgBuf) >= nSize)
  {
    // truncate the message if it is longer than the supplied
    // size
    lpMsgBuf[nSize] = 0;
  }
  wcsncpy_s(lpszerr, nSize, lpMsgBuf, _TRUNCATE);
  LocalFree(lpMsgBuf);
}

static void device_log_callback(void *context, const char *syslog_str, const char *human_str, const char *raw_str)
{
  (void)context;
  (void)syslog_str;
  (void)raw_str;
  printf("%s\n", human_str);
  if (s_log_file)
    fprintf(s_log_file, "%s\n", human_str);
}

static void device_time_callback(void *context, LwpaLogTimeParams *time)
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

void device_log_init(const char *file_name)
{
  s_log_file = fopen(file_name, "w");
  if (!s_log_file)
    printf("Device Log: Couldn't open log file %s\n", file_name);

  WSADATA wsdata;
  WSAStartup(MAKEWORD(2, 2), &wsdata);

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
      device_log_callback(NULL, NULL, "Device Log: Couldn't get time zone info.\n", NULL);
      break;
  }

  s_device_log_params.action = kLwpaLogCreateHumanReadableLog;
  s_device_log_params.log_fn = device_log_callback;
  strncpy_s(s_device_log_params.syslog_params.app_name, LWPA_LOG_APP_NAME_MAX_LEN, "RDMnet Device", _TRUNCATE);
  s_device_log_params.syslog_params.facility = LWPA_LOG_LOCAL1;
  s_device_log_params.log_mask = LWPA_LOG_UPTO(LWPA_LOG_DEBUG);
  s_device_log_params.time_fn = device_time_callback;
  s_device_log_params.context = NULL;

  DWORD procid = GetCurrentProcessId();
  snprintf(s_device_log_params.syslog_params.procid, LWPA_LOG_PROCID_MAX_LEN, "%d", procid);

  if (0 != gethostname(s_device_log_params.syslog_params.hostname, LWPA_LOG_HOSTNAME_MAX_LEN))
  {
    WCHAR error_text[128];
    char error_text_utf8[256] = "Unknown Error";

    GetLastErrorMessage(error_text, 128);
    WideCharToMultiByte(CP_UTF8, 0, error_text, -1, error_text_utf8, 256, NULL, NULL);
    device_log_callback(NULL, NULL, "Device Log: Couldn't get hostname due to error:\n", NULL);
    device_log_callback(NULL, NULL, error_text_utf8, NULL);
    s_device_log_params.syslog_params.hostname[0] = '\0';
  }

  lwpa_validate_log_params(&s_device_log_params);
}

const LwpaLogParams *device_get_log_params()
{
  return &s_device_log_params;
}

void device_log_deinit()
{
  WSACleanup();
  fclose(s_log_file);
}
