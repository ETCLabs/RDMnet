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

/// \file broker_util.h

#ifndef _BROKER_UTIL_H_
#define _BROKER_UTIL_H_

#include <stdexcept>
#include <queue>

#include "etcpal/lock.h"
#include "rdmnet/core/rpt_prot.h"

// Utility functions for manipulating messages
RptHeader SwapHeaderData(const RptHeader& source);
std::vector<RdmBuffer> RdmBufListToVect(const RdmBufListEntry* list_head);

#endif  // _BROKER_UTIL_H_
