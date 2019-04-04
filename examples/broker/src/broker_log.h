/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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

// A class for logging messages from the Broker.
#ifndef _BROKERLOG_H_
#define _BROKERLOG_H_

#include <WinSock2.h>
#include <Windows.h>
#include <fstream>
#include <string>
#include <queue>
#include "lwpa/log.h"
#include "lwpa/thread.h"
#include "lwpa/lock.h"
#include "rdmnet/broker/log.h"

class WindowsBrokerLog : public RDMnet::BrokerLog
{
public:
  WindowsBrokerLog(bool debug, const std::string &file_name);
  virtual ~WindowsBrokerLog();

  void OutputLogMsg(const std::string &str) override;
  virtual void GetTimeFromCallback(LwpaLogTimeParams *time) override;

private:
  bool debug_;
  std::fstream file_;
  long utcoffset_;
};

#endif  // _BROKERLOG_H_
