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

// iflist.cpp: implementation of IFList
//
//  This code is an adaptation of the documentation for GetAdaptersAddresses
//
// OF COURSE-- YAAYY IPV6 isn't working on my machine right now, so this isn't
// actually tested in ipv6!!
//
//////////////////////////////////////////////////////////////////////

#include <WinSock2.h>
#include <ws2ipdef.h>
#include <IPHlpApi.h>

#include "iflist.h"

// Discover the latest set of NICs.  Empties the vector before filling it
void IFList::FindIFaces(RDMnet::BrokerLog &log, std::vector<iflist_entry> &ifaces)
{
  // We always clear the vector
  ifaces.clear();

  const unsigned long WORKING_BUFFER_SIZE = 15000;
  const int MAX_TRIES = 3;

  DWORD dwRetVal = 0;

  // Set the flags to pass to GetAdaptersAddresses
  ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER |
                GAA_FLAG_SKIP_FRIENDLY_NAME;

  // default to unspecified address family (both)
  ULONG family = AF_UNSPEC;

  PIP_ADAPTER_ADDRESSES pAddresses = NULL;
  ULONG outBufLen = 0;
  ULONG Iterations = 0;

  PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
  PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;

  // Allocate a 15 KB buffer to start with.
  outBufLen = WORKING_BUFFER_SIZE;
  do
  {
    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
    if (pAddresses == NULL)
    {
      log.Log(LWPA_LOG_ERR, "Memory allocation failed for IP_ADAPTER_ADDRESSES struct.");
      return;
    }

    dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW)
    {
      free(pAddresses);
      pAddresses = NULL;
    }
    else
    {
      // We got the right size!
      break;
    }

    Iterations++;

  } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

  if (dwRetVal == NO_ERROR)
  {
    // Let's build the vector!

    pCurrAddresses = pAddresses;
    while (pCurrAddresses)
    {
      uint8_t mac_addr[kMACLen];

      int sum = 0;
      for (int i = 0; i < kMACLen; ++i)
      {
        mac_addr[i] = pCurrAddresses->PhysicalAddress[i];
        sum += pCurrAddresses->PhysicalAddress[i];
      }

      // We'll ignore any adapters without mac addrs
      if (sum == 0)
      {
        pCurrAddresses = pCurrAddresses->Next;
        continue;
      }

      pUnicast = pCurrAddresses->FirstUnicastAddress;
      while (pUnicast)
      {
        iflist_entry entry;
        memcpy(entry.mac, mac_addr, kMACLen);

        bool found = true;
        if (pUnicast->Address.iSockaddrLength == sizeof(struct sockaddr_in))
        {
          lwpaip_set_v4_address(&entry.addr,
                                ntohl(((struct sockaddr_in *)pUnicast->Address.lpSockaddr)->sin_addr.s_addr));
        }
        else if (pUnicast->Address.iSockaddrLength == sizeof(struct sockaddr_in6))
        {
          lwpaip_set_v6_address(&entry.addr,
                                (((struct sockaddr_in6 *)pUnicast->Address.lpSockaddr)->sin6_addr.s6_addr));
        }
        else
          found = false;

        if (found)
          ifaces.push_back(entry);

        pUnicast = pUnicast->Next;
      }

      pCurrAddresses = pCurrAddresses->Next;
    }
  }
  else
  {
    log.Log(LWPA_LOG_ERR, "Call to GetAdaptersAddresses failed with error: %d", dwRetVal);
    if (dwRetVal == ERROR_NO_DATA)
    {
      log.Log(LWPA_LOG_ERR, "\tNo addresses were found for the requested parameters");
    }
  }

  if (pAddresses)
    free(pAddresses);
}
