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

#ifndef FAKEWAY_LOG_H_
#define FAKEWAY_LOG_H_

#include <string>
#include <fstream>
#include "etcpal/log.h"

class FakewayLog
{
public:
  FakewayLog(const std::string& file_name);
  virtual ~FakewayLog();

  bool CanLog(int pri) const { return etcpal_can_log(&params_, pri); }
  const EtcPalLogParams& params() const { return params_; }

  void Log(int pri, const char* format, ...);

  // Shortcuts to log at a specific priority level
  void Debug(const char* format, ...);
  void Info(const char* format, ...);
  void Notice(const char* format, ...);
  void Warning(const char* format, ...);
  void Error(const char* format, ...);
  void Critical(const char* format, ...);
  void Alert(const char* format, ...);
  void Emergency(const char* format, ...);

  void LogFromCallback(const std::string& str);
  void GetTimeFromCallback(EtcPalLogTimestamp& time);

protected:
  std::fstream file_;
  EtcPalLogParams params_;
  int utc_offset_;
};

#endif  // FAKEWAY_LOG_H_
