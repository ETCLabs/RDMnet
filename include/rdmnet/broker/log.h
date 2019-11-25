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

/// \file rdmnet/broker/log.h

#ifndef RDMNET_BROKER_LOG_H_
#define RDMNET_BROKER_LOG_H_

#include <queue>
#include <string>

#include "etcpal/cpp/lock.h"
#include "etcpal/log.h"
#include "etcpal/thread.h"

namespace rdmnet
{
/// This interface connects the BrokerLog class to an application that handles logs.
class BrokerLogInterface
{
public:
  /// Used by the BrokerLog class to get a timestamp to prepend to a log message.
  virtual void GetLogTime(EtcPalLogTimeParams& time) = 0;
  /// Called from the BrokerLog's dispatch context to output a log message.
  virtual void OutputLogMsg(const std::string& str) = 0;
};

/// \brief A class for logging messages from the Broker.
class BrokerLog
{
public:
  enum class DispatchPolicy
  {
    kDirect,  ///< Log messages propagate directly from calls to Log() to being output (normally only used for testing)
    kQueued   ///< Log messages are queued and dispatched from another thread (recommended)
  };

  BrokerLog(DispatchPolicy dispatch_policy = DispatchPolicy::kQueued);
  virtual ~BrokerLog();
  bool Startup(BrokerLogInterface& log_interface);
  void Shutdown();

  const EtcPalLogParams* GetLogParams() const { return &log_params_; }
  bool CanLog(int pri) const { return etcpal_can_log(&log_params_, pri); }
  void SetLogMask(int log_mask) { log_params_.log_mask = log_mask; }

  // Log a message
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
  void GetTimeFromCallback(EtcPalLogTimeParams& time);
  void LogThreadRun();

private:
  BrokerLogInterface* log_interface_{nullptr};

  EtcPalLogParams log_params_{};

  const DispatchPolicy dispatch_policy_{DispatchPolicy::kQueued};

  std::queue<std::string> msg_q_;
  etcpal::Signal signal_;
  etcpal_thread_t thread_{};
  etcpal::Mutex lock_;
  bool keep_running_{false};
};

};  // namespace rdmnet

#endif  // RDMNET_BROKER_LOG_H_
