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

// A class for logging messages from the Broker on Linux.
#ifndef _LINUX_BROKER_LOG_H_
#define _LINUX_BROKER_LOG_H_

#include <fstream>
#include <string>
#include <queue>
#include "etcpal/log.h"
#include "etcpal/thread.h"
#include "etcpal/lock.h"
#include "rdmnet/broker/log.h"

class LinuxBrokerLog : public RDMnet::BrokerLog
{
public:
  LinuxBrokerLog(const std::string& file_name);
  virtual ~LinuxBrokerLog();

  void OutputLogMsg(const std::string& str) override;
  virtual void GetTimeFromCallback(EtcPalLogTimeParams* time) override;

private:
  std::fstream file_;
  int log_level_{ETCPAL_LOG_INFO};
};

#endif  // _LINUX_BROKER_LOG_
