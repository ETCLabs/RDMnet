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

#include "ControllerLog.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QDateTime>
#include <QTimeZone>
END_INCLUDE_QT_HEADERS()

static void LogCallback(void *context, const char * /*syslog_str*/, const char *human_str, const char * /*raw_str*/)
{
  ControllerLog *log = static_cast<ControllerLog *>(context);
  if (log)
    log->LogFromCallback(human_str);
}

static void TimeCallback(void * /*context*/, LwpaLogTimeParams *time)
{
  QDateTime now = QDateTime::currentDateTime();
  QDate qdate = now.date();
  QTime qtime = now.time();
  time->second = qtime.second();
  time->minute = qtime.minute();
  time->hour = qtime.hour();
  time->day = qdate.day();
  time->month = qdate.month();
  time->year = qdate.year();
  time->msec = qtime.msec();
  time->utc_offset = (QTimeZone::systemTimeZone().offsetFromUtc(now) / 60);
}

ControllerLog::ControllerLog(const std::string &file_name) : file_name_(file_name)
{
  file_.open(file_name.c_str(), std::fstream::out);

  params_.action = kLwpaLogCreateHumanReadableLog;
  params_.log_fn = LogCallback;
  params_.syslog_params.facility = LWPA_LOG_LOCAL1;
  params_.syslog_params.app_name[0] = '\0';
  params_.syslog_params.procid[0] = '\0';
  params_.syslog_params.hostname[0] = '\0';
  params_.log_mask = LWPA_LOG_UPTO(LWPA_LOG_DEBUG);
  params_.time_fn = TimeCallback;
  params_.context = this;
  lwpa_validate_log_params(&params_);

  Log(LWPA_LOG_INFO, "Starting RDMnet Controller...");
}

ControllerLog::~ControllerLog()
{
  file_.close();
}

void ControllerLog::Log(int pri, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  lwpa_vlog(&params_, pri, format, args);
  va_end(args);
}

void ControllerLog::LogFromCallback(const std::string &str)
{
  if (file_.is_open())
    file_ << str << std::endl;

  for (LogOutputStream *stream : customOutputStreams)
  {
    if (stream != NULL)
    {
      (*stream) << str << "\n";
    }
  }
}

void ControllerLog::addCustomOutputStream(LogOutputStream *stream)
{
  if (stream != NULL)
  {
    if (std::find(customOutputStreams.begin(), customOutputStreams.end(), stream) == customOutputStreams.end())
    {
      // Reinitialize the stream's contents to the log file's contents.
      stream->clear();

      std::ifstream ifs(file_name_, std::ifstream::in);

      std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

      (*stream) << str;

      ifs.close();

      customOutputStreams.push_back(stream);
    }
  }
}

void ControllerLog::removeCustomOutputStream(LogOutputStream *stream)
{
  for (size_t i = 0; i < customOutputStreams.size(); ++i)
  {
    if (customOutputStreams.at(i) == stream)
    {
      customOutputStreams.erase(customOutputStreams.begin() + i);
    }
  }
}

size_t ControllerLog::getNumberOfCustomLogOutputStreams()
{
  return customOutputStreams.size();
}
