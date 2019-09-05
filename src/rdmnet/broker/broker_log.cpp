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

#include "rdmnet/broker/log.h"

#include <cassert>
#include "broker_util.h"

extern "C" {
static void broker_log_callback(void* context, const EtcPalLogStrings* strings)
{
  assert(strings);
  assert(strings->human_readable);
  RDMnet::BrokerLog* bl = static_cast<RDMnet::BrokerLog*>(context);
  if (bl)
    bl->LogFromCallback(strings->human_readable);
}

static void broker_time_callback(void* context, EtcPalLogTimeParams* time)
{
  RDMnet::BrokerLog* bl = static_cast<RDMnet::BrokerLog*>(context);
  if (bl)
    bl->GetTimeFromCallback(time);
}

static void log_thread_fn(void* arg)
{
  RDMnet::BrokerLog* bl = static_cast<RDMnet::BrokerLog*>(arg);
  if (bl)
    bl->LogThreadRun();
}
}  // extern "C"

RDMnet::BrokerLog::BrokerLog() : keep_running_(false)
{
  etcpal_signal_create(&signal_);
  etcpal_mutex_create(&lock_);
}

RDMnet::BrokerLog::~BrokerLog()
{
  Shutdown();
  etcpal_mutex_destroy(&lock_);
  etcpal_signal_destroy(&signal_);
}

bool RDMnet::BrokerLog::Startup(int log_mask)
{
  // Set up the log params
  log_params_.action = kEtcPalLogCreateHumanReadableLog;
  log_params_.log_fn = broker_log_callback;
  log_params_.log_mask = log_mask;
  log_params_.time_fn = broker_time_callback;
  log_params_.context = this;

  etcpal_validate_log_params(&log_params_);

  // Start the log dispatch thread
  keep_running_ = true;
  EtcPalThreadParams tparams = {ETCPAL_THREAD_DEFAULT_PRIORITY, ETCPAL_THREAD_DEFAULT_STACK, "RDMnet::BrokerLogThread", NULL};
  return etcpal_thread_create(&thread_, &tparams, log_thread_fn, this);
}

void RDMnet::BrokerLog::Shutdown()
{
  if (keep_running_)
  {
    keep_running_ = false;
    etcpal_signal_post(&signal_);
    etcpal_thread_join(&thread_);
  }
}

void RDMnet::BrokerLog::Log(int pri, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  etcpal_vlog(&log_params_, pri, format, args);
  va_end(args);
}

void RDMnet::BrokerLog::LogFromCallback(const std::string& str)
{
  {
    etcpal::MutexGuard guard(lock_);
    msg_q_.push(str);
  }
  etcpal_signal_post(&signal_);
}

void RDMnet::BrokerLog::LogThreadRun()
{
  while (keep_running_)
  {
    etcpal_signal_wait(&signal_);
    if (keep_running_)
    {
      std::vector<std::string> to_log;

      {
        etcpal::MutexGuard guard(lock_);
        to_log.reserve(msg_q_.size());
        while (!msg_q_.empty())
        {
          to_log.push_back(msg_q_.front());
          msg_q_.pop();
        }
      }
      for (auto log_msg : to_log)
      {
        OutputLogMsg(log_msg);
      }
    }
  }
}
