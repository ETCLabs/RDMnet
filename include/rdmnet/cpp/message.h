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

/// @file rdmnet/cpp/message.h
/// @brief RDMnet C++ message type definitions

#ifndef RDMNET_CPP_MESSAGE_H_
#define RDMNET_CPP_MESSAGE_H_

// This monolithic header includes all of the individual message types. This one is used from the
// other API headers.

#include "rdmnet/cpp/message_types/dynamic_uid.h"
#include "rdmnet/cpp/message_types/ept_client.h"
#include "rdmnet/cpp/message_types/ept_data.h"
#include "rdmnet/cpp/message_types/ept_status.h"
#include "rdmnet/cpp/message_types/llrp_rdm_command.h"
#include "rdmnet/cpp/message_types/llrp_rdm_response.h"
#include "rdmnet/cpp/message_types/rdm_command.h"
#include "rdmnet/cpp/message_types/rdm_response.h"
#include "rdmnet/cpp/message_types/rpt_client.h"
#include "rdmnet/cpp/message_types/rpt_status.h"

#endif  // RDMNET_CPP_MESSAGE_H_
