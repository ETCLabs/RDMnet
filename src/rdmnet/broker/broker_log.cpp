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

#include "rdmnet/broker/log.h"

#include "rdmnet/private/opts.h"
#include "broker_util.h"

extern "C" {
static void broker_log_callback(void* context, const EtcPalLogStrings* strings)
{
  RDMNET_ASSERT(strings);
  RDMNET_ASSERT(strings->human_readable);
  rdmnet::BrokerLog* bl = static_cast<rdmnet::BrokerLog*>(context);
  if (bl)
    bl->LogFromCallback(strings->human_readable);
}

static void broker_time_callback(void* context, EtcPalLogTimestamp* time)
{
  rdmnet::BrokerLog* bl = static_cast<rdmnet::BrokerLog*>(context);
  if (bl && time)
    bl->GetTimeFromCallback(*time);
}

static void log_thread_fn(void* arg)
{
  rdmnet::BrokerLog* bl = static_cast<rdmnet::BrokerLog*>(arg);
  if (bl)
    bl->LogThreadRun();
}
}  // extern "C"

rdmnet::BrokerLog::BrokerLog(DispatchPolicy dispatch_policy) : dispatch_policy_(dispatch_policy), keep_running_(false)
{
  // Set up the log params
  log_params_.action = kEtcPalLogCreateHumanReadable;
  log_params_.log_fn = broker_log_callback;
  log_params_.log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG);
  log_params_.time_fn = broker_time_callback;
  log_params_.context = this;
}

rdmnet::BrokerLog::~BrokerLog()
{
  Shutdown();
}

bool rdmnet::BrokerLog::Startup(BrokerLogInterface& log_interface)
{
  if (etcpal_init(ETCPAL_FEATURE_LOGGING) != kEtcPalErrOk)
    return false;

  log_interface_ = &log_interface;

  etcpal_validate_log_params(&log_params_);

  if (dispatch_policy_ == DispatchPolicy::kQueued)
  {
    // Start the log dispatch thread
    keep_running_ = true;
    // clang-format off
    EtcPalThreadParams tparams =
    {
      ETCPAL_THREAD_DEFAULT_PRIORITY,
      ETCPAL_THREAD_DEFAULT_STACK,
      "RDMnetBrokerLogThread",
      NULL
    };
    // clang-format on
    return (etcpal_thread_create(&thread_, &tparams, log_thread_fn, this) == kEtcPalErrOk);
  }
  else
  {
    return true;
  }
}

void rdmnet::BrokerLog::Shutdown()
{
  if (dispatch_policy_ == DispatchPolicy::kQueued)
  {
    if (keep_running_)
    {
      keep_running_ = false;
      signal_.Notify();
      etcpal_thread_join(&thread_);
    }
  }

  etcpal_deinit(ETCPAL_FEATURE_LOGGING);
  log_interface_ = nullptr;
}

void rdmnet::BrokerLog::Log(int pri, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, pri, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::Debug(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, ETCPAL_LOG_DEBUG, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::Info(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, ETCPAL_LOG_INFO, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::Notice(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, ETCPAL_LOG_NOTICE, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::Warning(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, ETCPAL_LOG_WARNING, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::Error(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, ETCPAL_LOG_ERR, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::Critical(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, ETCPAL_LOG_CRIT, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::Alert(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, ETCPAL_LOG_ALERT, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::Emergency(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, ETCPAL_LOG_EMERG, format, args);
  va_end(args);
}

void rdmnet::BrokerLog::LogFromCallback(const std::string& str)
{
  if (log_interface_)
  {
    if (dispatch_policy_ == DispatchPolicy::kDirect)
    {
      log_interface_->OutputLogMsg(str);
    }
    else if (dispatch_policy_ == DispatchPolicy::kQueued)
    {
      {
        etcpal::MutexGuard guard(lock_);
        msg_q_.push(str);
      }
      signal_.Notify();
    }
  }
}

void rdmnet::BrokerLog::GetTimeFromCallback(EtcPalLogTimestamp& time)
{
  if (log_interface_)
    log_interface_->GetLogTime(time);
}

void rdmnet::BrokerLog::LogThreadRun()
{
  while (keep_running_)
  {
    signal_.Wait();

    std::queue<std::string> to_log;
    {
      etcpal::MutexGuard guard(lock_);
      to_log.swap(msg_q_);
    }

    while (!to_log.empty())
    {
      if (log_interface_)
        log_interface_->OutputLogMsg(to_log.front());
      to_log.pop();
    }
  }
}
