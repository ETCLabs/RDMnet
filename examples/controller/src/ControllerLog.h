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

// ControllerLog: A class for logging messages from the Controller and underlying RDMnet library.

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include "lwpa/log.h"

class LogOutputStream
{
public:
  virtual LogOutputStream &operator<<(const std::string &str) = 0;
  virtual void clear() = 0;
};

class ControllerLog
{
public:
  explicit ControllerLog(const std::string &file_name);
  virtual ~ControllerLog();

  void Log(int pri, const char *format, ...);
  bool CanLog(int pri) const { return LWPA_CAN_LOG(&params_, pri); }
  const LwpaLogParams *GetLogParams() const { return &params_; }

  void LogFromCallback(const std::string &str);

  void addCustomOutputStream(LogOutputStream *stream);
  void removeCustomOutputStream(LogOutputStream *stream);

  size_t getNumberOfCustomLogOutputStreams();

protected:
  std::fstream file_;
  std::string file_name_;
  LwpaLogParams params_;
  std::vector<LogOutputStream *> customOutputStreams;
};
