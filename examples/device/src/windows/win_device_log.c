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

#include "win_device_log.h"

#include <WinSock2.h>
#include <Windows.h>
#include <stdio.h>

static LwpaLogParams s_device_log_params;
static FILE* s_log_file;
static int s_utc_offset;

static void device_log_callback(void* context, const LwpaLogStrings* strings)
{
  (void)context;
  printf("%s\n", strings->human_readable);
  if (s_log_file)
    fprintf(s_log_file, "%s\n", strings->human_readable);
}

static void device_time_callback(void* context, LwpaLogTimeParams* time)
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

void device_log_init(const char* file_name)
{
  s_log_file = fopen(file_name, "w");
  if (!s_log_file)
    printf("Device Log: Couldn't open log file %s\n", file_name);

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

  s_device_log_params.action = kLwpaLogCreateHumanReadableLog;
  s_device_log_params.log_fn = device_log_callback;
  s_device_log_params.log_mask = LWPA_LOG_UPTO(LWPA_LOG_DEBUG);
  s_device_log_params.time_fn = device_time_callback;
  s_device_log_params.context = NULL;

  lwpa_validate_log_params(&s_device_log_params);
}

const LwpaLogParams* device_get_log_params()
{
  return &s_device_log_params;
}

void device_log_deinit()
{
  fclose(s_log_file);
}
