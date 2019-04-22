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

#include "rdmnet/core/llrp.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/llrp_manager.h"
#include "rdmnet/private/llrp_target.h"
#include "rdmnet/private/opts.h"

/**************************** Global variables *******************************/

const LwpaIpAddr kLlrpIpv4RespAddr;
const LwpaIpAddr kLlrpIpv6RespAddr;
const LwpaIpAddr kLlrpIpv4RequestAddr;
const LwpaIpAddr kLlrpIpv6RequestAddr;

/*********************** Private function prototypes *************************/

lwpa_error_t create_sys_socket(const LwpaIpAddr *netint, bool manager, lwpa_socket_t *socket);
lwpa_error_t subscribe_multicast(lwpa_socket_t socket, const LwpaIpAddr *netint, bool manager);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_llrp_init()
{
  lwpa_error_t res = kLwpaErrOk;
  bool manager_initted = false;
  bool target_initted = false;

#if RDMNET_DYNAMIC_MEM
  manager_initted = ((res = rdmnet_llrp_manager_init()) == kLwpaErrOk);
#endif

  if (res == kLwpaErrOk)
  {
    target_initted = ((res = rdmnet_llrp_target_init()) == kLwpaErrOk);
  }

  if (res == kLwpaErrOk)
  {
    lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_RESPONSE, &kLlrpIpv4RespAddr);
    lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV6_ADDRESS_RESPONSE, &kLlrpIpv6RespAddr);
    lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_REQUEST, &kLlrpIpv4RequestAddr);
    lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV6_ADDRESS_REQUEST, &kLlrpIpv6RequestAddr);
    llrp_prot_init();
  }
  else
  {
    if (manager_initted)
      rdmnet_llrp_manager_deinit();
    if (target_initted)
      rdmnet_llrp_target_deinit();
  }

  return res;
}

void rdmnet_llrp_deinit()
{
  rdmnet_llrp_target_deinit();
#if RDMNET_DYNAMIC_MEM
  rdmnet_llrp_manager_deinit();
#endif
}

lwpa_error_t create_llrp_socket(const LwpaIpAddr *netint, bool manager, lwpa_socket_t *socket)
{
  lwpa_socket_t socket_out;

  lwpa_error_t res = create_sys_socket(netint, manager, &socket_out);
  if (res == kLwpaErrOk)
    res = subscribe_multicast(socket_out, netint, manager);
  if (res == kLwpaErrOk)
    *socket = socket_out;
  return res;
}

lwpa_error_t create_sys_socket(const LwpaIpAddr *netint, bool manager, lwpa_socket_t *socket)
{
  lwpa_socket_t sock = LWPA_SOCKET_INVALID;
  lwpa_error_t res = lwpa_socket(lwpaip_is_v6(netint) ? LWPA_AF_INET6 : LWPA_AF_INET, LWPA_DGRAM, &sock);

  if (res == kLwpaErrOk)
  {
    // SO_REUSEADDR allows multiple sockets to bind to LLRP_PORT, which is very important for our
    // multicast needs.
    int option = 1;
    res = lwpa_setsockopt(sock, LWPA_SOL_SOCKET, LWPA_SO_REUSEADDR, (const void *)(&option), sizeof(option));
  }

  if (res == kLwpaErrOk)
  {
    // MULTICAST_TTL controls the TTL field in outgoing multicast datagrams.
    if (lwpaip_is_v4(netint))
    {
      int value = LLRP_MULTICAST_TTL_VAL;
      res = lwpa_setsockopt(sock, LWPA_IPPROTO_IP, LWPA_IP_MULTICAST_TTL, (const void *)(&value), sizeof(value));
    }
    else
    {
      // TODO: add Ipv6 support
    }
  }

  if (res == kLwpaErrOk)
  {
    // MULTICAST_IF is critical for multicast sends to go over the correct interface.
    if (lwpaip_is_v4(netint))
    {
      res = lwpa_setsockopt(sock, LWPA_IPPROTO_IP, LWPA_IP_MULTICAST_IF, (const void *)(netint), sizeof(LwpaIpAddr));
    }
    else
    {
      // TODO: add Ipv6 support
    }
  }

  if (res == kLwpaErrOk)
  {
    if (lwpaip_is_v4(netint))
    {
      LwpaSockaddr bind_addr;
#if RDMNET_LLRP_BIND_TO_MCAST_ADDRESS
      // Bind socket to multicast address for IPv4
      bind_addr.ip = (manager ? kLlrpIpv4RespAddr : kLlrpIpv4RequestAddr);
#else
      (void)manager;
      // Bind socket to INADDR_ANY
      lwpaip_make_any_v4(&bind_addr.ip);
#endif
      bind_addr.port = LLRP_PORT;
      res = lwpa_bind(sock, &bind_addr);
    }
    else
    {
      // TODO: add Ipv6 support
    }
  }

  if (res == kLwpaErrOk)
  {
    *socket = sock;
  }
  else
  {
    lwpa_close(sock);
  }

  return res;
}

lwpa_error_t subscribe_multicast(lwpa_socket_t socket, const LwpaIpAddr *netint, bool manager)
{
  lwpa_error_t res = kLwpaErrNotImpl;

  if (lwpaip_is_v4(netint))
  {
    LwpaMreq multireq;

    multireq.group = (manager ? kLlrpIpv4RespAddr : kLlrpIpv4RequestAddr);
    multireq.netint = *netint;
    res = lwpa_setsockopt(socket, LWPA_IPPROTO_IP, LWPA_MCAST_JOIN_GROUP, (const void *)&multireq, sizeof(multireq));
  }
  else
  {
    // TODO: add Ipv6 support
  }

  return res;
}
