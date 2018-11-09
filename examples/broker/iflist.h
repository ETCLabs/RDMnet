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

// iflist.h: Get the list of network interfaces on your machine.
//
// OF COURSE-- YAAYY IPV6 isn't working on my machine right now, so this isn't
// actually tested in ipv6!!
//
//////////////////////////////////////////////////////////////////////

#ifndef IFLIST_H
#define IFLIST_H

#include <vector>
#include "lwpa/log.h"
#include "lwpa/inet.h"
#include "broker_log.h"

namespace IFList
{
enum
{
  kMACLen = 6
};

// Note that multiple addresses can be on the same interface/mac address!
struct iflist_entry
{
  LwpaIpAddr addr;       // The address field is filled in, port and net_interface are ignored
  uint8_t mac[kMACLen];  // The mac address
};

// Discover the latest set of NICs.  Empties the vector before filling it
void FindIFaces(BrokerLog &log, std::vector<iflist_entry> &ifaces);

};  // namespace IFList

#endif
