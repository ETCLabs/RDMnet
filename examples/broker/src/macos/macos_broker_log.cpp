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

#include "macos_broker_log.h"
#include <ctime>
#include <iostream>
#include <cstdarg>

MacBrokerLog::MacBrokerLog(const std::string& file_name) : RDMnet::BrokerLog()
{
  file_.open(file_name.c_str(), std::fstream::out);
  if (file_.fail())
    std::cout << "BrokerLog couldn't open log file '" << file_name << "'." << std::endl;
}

MacBrokerLog::~MacBrokerLog()
{
  file_.close();
}

void MacBrokerLog::GetTimeFromCallback(LwpaLogTimeParams* time_params)
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

  time_params->year = timeinfo->tm_year + 1900;
  time_params->month = timeinfo->tm_mon + 1;
  time_params->day = timeinfo->tm_mday;
  time_params->hour = timeinfo->tm_hour;
  time_params->minute = timeinfo->tm_min;
  time_params->second = timeinfo->tm_sec;
  time_params->msec = 0;
  time_params->utc_offset = static_cast<int>(utc_offset);
}

void MacBrokerLog::OutputLogMsg(const std::string& str)
{
  std::cout << str << "\n";
  if (file_.is_open())
    file_ << str << std::endl;
}
