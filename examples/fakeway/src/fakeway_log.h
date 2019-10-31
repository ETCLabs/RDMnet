#ifndef _FAKEWAY_LOG_H_
#define _FAKEWAY_LOG_H_

#include <string>
#include <fstream>
#include "lwpa/log.h"

class FakewayLog
{
public:
  FakewayLog(const std::string &file_name);
  virtual ~FakewayLog();

  void Log(int pri, const char *format, ...);
  bool CanLog(int pri) const { return LWPA_CAN_LOG(&params_, pri); }
  const LwpaLogParams *params() const { return &params_; }

  void LogFromCallback(const std::string &str);
  void GetTimeFromCallback(LwpaLogTimeParams *time);

protected:
  std::fstream file_;
  LwpaLogParams params_;
  int utc_offset_;
};

#endif  // _FAKEWAY_LOG_H_
