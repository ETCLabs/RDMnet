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

// ControllerLog: A class for logging messages from the Controller and underlying RDMnet library.

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include "etcpal/cpp/log.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QString>
END_INCLUDE_QT_HEADERS()

class LogOutputStream
{
public:
  virtual LogOutputStream& operator<<(const std::string& str) = 0;
  virtual void             clear() = 0;
};

class ControllerLog : public etcpal::LogMessageHandler
{
public:
  ControllerLog();
  virtual ~ControllerLog();

  etcpal::Logger& logger() noexcept { return logger_; }
  QString         file_name() { return file_name_; }
  bool            HasFileError() { return !file_.is_open(); }

  // etcpal::LogMessageHandler overrides
  etcpal::LogTimestamp GetLogTimestamp() override;
  void                 HandleLogMessage(const EtcPalLogStrings& strings) override;

  void addCustomOutputStream(LogOutputStream* stream);
  void removeCustomOutputStream(LogOutputStream* stream);

  size_t getNumberOfCustomLogOutputStreams();

protected:
  std::fstream                  file_;
  QString                       file_name_;
  etcpal::Logger                logger_;
  std::vector<LogOutputStream*> customOutputStreams;
};
