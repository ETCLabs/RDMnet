#include "fakeway_log.h"

#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <stdio.h>

static void log_callback(void *context, const LwpaLogStrings *strings)
{
  FakewayLog *log = static_cast<FakewayLog *>(context);
  if (log)
    log->LogFromCallback(strings->human_readable);
}

static void time_callback(void *context, LwpaLogTimeParams *time)
{
  FakewayLog *log = static_cast<FakewayLog *>(context);
  if (log)
    log->GetTimeFromCallback(time);
}

FakewayLog::FakewayLog(const std::string &file_name)
{
  file_.open(file_name.c_str(), std::fstream::out);
  if (file_.fail())
    std::cout << "Fakeway Log: Couldn't open log file '" << file_name << "'." << std::endl;

  TIME_ZONE_INFORMATION tzinfo;
  switch (GetTimeZoneInformation(&tzinfo))
  {
    case TIME_ZONE_ID_UNKNOWN:
    case TIME_ZONE_ID_STANDARD:
      utc_offset_ = -(tzinfo.Bias + tzinfo.StandardBias);
      break;
    case TIME_ZONE_ID_DAYLIGHT:
      utc_offset_ = -(tzinfo.Bias + tzinfo.DaylightBias);
      break;
    default:
      std::cout << "Fakeway Log: Couldn't get time zone info." << std::endl;
      break;
  }

  params_.action = kLwpaLogCreateHumanReadableLog;
  params_.log_fn = log_callback;
  params_.log_mask = LWPA_LOG_UPTO(LWPA_LOG_DEBUG);
  params_.time_fn = time_callback;
  params_.context = this;
  lwpa_validate_log_params(&params_);
}

FakewayLog::~FakewayLog()
{
  file_.close();
}

void FakewayLog::Log(int pri, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  lwpa_vlog(&params_, pri, format, args);
  va_end(args);
}

void FakewayLog::LogFromCallback(const std::string &str)
{
  std::cout << str << std::endl;
  if (file_.is_open())
    file_ << str << std::endl;
}

void FakewayLog::GetTimeFromCallback(LwpaLogTimeParams *time)
{
  SYSTEMTIME win_time;
  GetLocalTime(&win_time);
  time->year = win_time.wYear - 1900;
  time->month = win_time.wMonth - 1;
  time->day = win_time.wDay;
  time->hour = win_time.wHour;
  time->minute = win_time.wMinute;
  time->second = win_time.wSecond;
  time->msec = win_time.wMilliseconds;
  time->utc_offset = utc_offset_;
}
