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

#pragma once

#include <stdexcept>
#include <cstddef>
#include "etcpal/int.h"
#include "etcpal/cpp/inet.h"

// Macros to suppress warnings inside of Qt headers.
#if defined(_MSC_VER)

#define BEGIN_INCLUDE_QT_HEADERS() \
  __pragma(warning(push)) __pragma(warning(disable : 4127)) __pragma(warning(disable : 4251))

#define END_INCLUDE_QT_HEADERS() __pragma(warning(pop))

#else

#define BEGIN_INCLUDE_QT_HEADERS()
#define END_INCLUDE_QT_HEADERS()

#endif

// A representation of an optional static Broker configuration.
struct StaticBrokerConfig
{
  StaticBrokerConfig() {}
  StaticBrokerConfig(const EtcPalSockaddr& address)
  {
    memset(&addr, 0, sizeof(EtcPalSockaddr));
    addr.ip.type = kEtcPalIpTypeInvalid;
    valid = (address.ip.type != kEtcPalIpTypeInvalid) && (address.port != 0);
    if (valid)
    {
      addr = address;
    }
  }

  bool valid{false};
  etcpal::SockAddr addr;
};

// Some definitions that aren't provided elsewhere
constexpr uint16_t kRdmnetMaxScopeSlotNumber = 0xFFFF;
constexpr size_t kRdmDeviceLabelMaxLength = 32u;
