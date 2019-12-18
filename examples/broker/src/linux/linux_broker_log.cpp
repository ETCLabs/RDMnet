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

#include "linux_broker_log.h"
#include <ctime>
#include <iostream>
#include <cstdarg>

bool LinuxBrokerLog::Startup(const std::string& file_name, int log_mask)
{
  file_.open(file_name.c_str(), std::fstream::out);
  if (!file_.is_open())
    std::cout << "BrokerLog couldn't open log file '" << file_name << "'." << std::endl;

  return logger_.SetLogMask(log_mask).Startup(*this);
}

void LinuxBrokerLog::Shutdown()
{
  logger_.Shutdown();
  file_.close();
}

EtcPalLogTimeParams LinuxBrokerLog::GetLogTimestamp()
{
  time_t cur_time;
  time(&cur_time);

  // Determine the UTC offset
  // A bit of naive time zone code that probably misses tons of edge cases.
  // After all, it's just an example app...
  struct tm* timeinfo = gmtime(&cur_time);
  time_t utc = mktime(timeinfo);
  timeinfo = localtime(&cur_time);
  time_t local = mktime(timeinfo);
  double utc_offset = difftime(local, utc) / 60.0;
  if (timeinfo->tm_isdst)
    utc_offset += 60;

  return EtcPalLogTimeParams{timeinfo->tm_year + 1900,
                             timeinfo->tm_mon + 1,
                             timeinfo->tm_mday,
                             timeinfo->tm_hour,
                             timeinfo->tm_min,
                             timeinfo->tm_sec,
                             0,
                             static_cast<int>(utc_offset)};
}

void LinuxBrokerLog::HandleLogMessage(const EtcPalLogStrings& strings)
{
  std::cout << strings.human_readable << '\n';
  if (file_.is_open())
    file_ << strings.human_readable << std::endl;
}
