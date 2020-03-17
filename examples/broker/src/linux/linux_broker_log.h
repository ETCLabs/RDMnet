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
#ifndef LINUX_BROKER_LOG_H_
#define LINUX_BROKER_LOG_H_

#include <fstream>
#include <string>
#include <queue>
#include "etcpal/log.h"
#include "etcpal/thread.h"
#include "etcpal/lock.h"
#include "rdmnet/broker/log.h"

class LinuxBrokerLog : public rdmnet::BrokerLog
{
public:
  bool Startup(const std::string& file_name, int log_mask);
  void Shutdown();

  void OutputLogMsg(const std::string& str) override;
  void GetLogTime(EtcPalLogTimestamp& time) override;

  rdmnet::BrokerLog& broker_log_instance() { return log_; }

private:
  rdmnet::BrokerLog log_;

  std::fstream file_;
};

#endif  // LINUX_BROKER_LOG_
