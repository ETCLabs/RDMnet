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

#include "macos_broker_log.h"
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sys/stat.h>

static const std::vector<std::string> kLogFileDirComponents = {"Library", "Logs", "ETC", "RDMnetExamples"};
static constexpr char                 kLogFileBaseName[] = "broker.log";

// Gets the full path to the log file, creating intermediate directories if necessary.
std::string GetLogFileName()
{
  std::string cur_path = getenv("HOME");
  for (const auto& directory_part : kLogFileDirComponents)
  {
    cur_path += "/" + directory_part;
    if (mkdir(cur_path.c_str(), 0755) != 0)
    {
      if (errno != EEXIST)
      {
        std::cout << "Couldn't create directory " << cur_path << ": " << strerror(errno) << ".\n";
        return std::string{};
      }
    }
  }
  return cur_path + "/" + kLogFileBaseName;
}

bool MacBrokerLog::Startup(int log_mask)
{
  const auto file_name = GetLogFileName();
  if (!file_name.empty())
  {
    file_.open(file_name.c_str(), std::fstream::out);
    if (!file_.is_open())
      std::cout << "BrokerLog couldn't open log file '" << file_name << "'.\n";
  }

  return logger_.SetLogMask(log_mask).Startup(*this);
}

void MacBrokerLog::Shutdown()
{
  logger_.Shutdown();
  file_.close();
}

etcpal::LogTimestamp MacBrokerLog::GetLogTimestamp()
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

  return etcpal::LogTimestamp(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour,
                              timeinfo->tm_min, timeinfo->tm_sec, 0, static_cast<int>(utc_offset));
}

void MacBrokerLog::HandleLogMessage(const EtcPalLogStrings& strings)
{
  std::cout << strings.human_readable << '\n';
  if (file_.is_open())
  {
    file_ << strings.human_readable << std::endl;
    file_.flush();
  }
}
