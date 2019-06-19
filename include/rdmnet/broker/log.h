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

/// \file rdmnet/broker/log.h
#ifndef _RDMNET_BROKER_LOG_H_
#define _RDMNET_BROKER_LOG_H_

#include <queue>
#include <string>

#include "lwpa/lock.h"
#include "lwpa/log.h"
#include "lwpa/thread.h"

namespace RDMnet
{
/// \brief A class for logging messages from the %Broker.
class BrokerLog
{
public:
  BrokerLog();
  virtual ~BrokerLog();
  bool Startup(int log_mask);
  void Shutdown();

  const LwpaLogParams *GetLogParams() const { return &log_params_; }
  void Log(int pri, const char *format, ...);
  bool CanLog(int pri) const { return LWPA_CAN_LOG(&log_params_, pri); }

  void LogFromCallback(const std::string &str);
  void LogThreadRun();

  virtual void GetTimeFromCallback(LwpaLogTimeParams *time) = 0;
  virtual void OutputLogMsg(const std::string &str) = 0;

protected:
  LwpaLogParams log_params_{};

  std::queue<std::string> msg_q_;
  lwpa_signal_t signal_;
  lwpa_thread_t thread_;
  lwpa_mutex_t lock_;
  bool keep_running_{false};
};

};  // namespace RDMnet

#endif  // _RDMNET_BROKER_LOG_H_
