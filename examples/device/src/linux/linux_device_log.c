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

#include "linux_device_log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char* kLogFileDirComponents[] = {".local", "share", "rdmnet-examples"};
#define LOG_FILE_BASENAME "device.log"

static EtcPalLogParams s_device_log_params;
static FILE*           s_log_file;
#define LOG_FILE_NAME_BUF_SIZE 1024
static char s_log_file_name[LOG_FILE_NAME_BUF_SIZE];

const char* get_log_file_name(void)
{
  const char* home_dir = getenv("HOME");
  if (!home_dir)
  {
    printf("Error: couldn't get home directory reference to open log file.\n");
    return NULL;
  }

  strcpy(s_log_file_name, home_dir);
  for (size_t i = 0; i < sizeof(kLogFileDirComponents) / sizeof(kLogFileDirComponents[0]); ++i)
  {
    strcat(s_log_file_name, "/");
    strcat(s_log_file_name, kLogFileDirComponents[i]);
    if (mkdir(s_log_file_name, 0755) != 0)
    {
      if (errno != EEXIST)
      {
        // Something went wrong creating an intermediate directory.
        printf("Couldn't create directory %s: %s.\n", s_log_file_name, strerror(errno));
        return NULL;
      }
    }
  }
  strcat(s_log_file_name, "/");
  strcat(s_log_file_name, LOG_FILE_BASENAME);
  return s_log_file_name;
}

static void device_log_callback(void* context, const EtcPalLogStrings* strings)
{
  (void)context;
  printf("%s\n", strings->human_readable);
  if (s_log_file)
    fprintf(s_log_file, "%s\n", strings->human_readable);
}

static void device_time_callback(void* context, EtcPalLogTimestamp* time_params)
{
  time_t cur_time;
  time(&cur_time);

  // Determine the UTC offset
  // A bit of naive time zone code that probably misses tons of edge cases.
  // After all, it's just an example app...
  struct tm* timeinfo = gmtime(&cur_time);
  time_t     utc = mktime(timeinfo);
  timeinfo = localtime(&cur_time);
  time_t local = mktime(timeinfo);
  double utc_offset = difftime(local, utc) / 60.0;
  if (timeinfo->tm_isdst)
    utc_offset += 60;

  time_params->year = timeinfo->tm_year + 1900;
  time_params->month = timeinfo->tm_mon + 1;
  time_params->day = timeinfo->tm_mday;
  time_params->hour = timeinfo->tm_hour;
  time_params->minute = timeinfo->tm_min;
  time_params->second = timeinfo->tm_sec;
  time_params->msec = 0;
  time_params->utc_offset = (int)utc_offset;
}

void device_log_init(void)
{
  etcpal_init(ETCPAL_FEATURE_LOGGING);

  const char* file_name = get_log_file_name();
  if (file_name)
  {
    s_log_file = fopen(file_name, "w");
    if (!s_log_file)
      printf("Device Log: Couldn't open log file %s\n", file_name);
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
