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
