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

#include "rdmnet/core/llrp.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/llrp_manager.h"
#include "rdmnet/private/llrp_target.h"
#include "rdmnet/private/opts.h"

/**************************** Global variables *******************************/

LwpaSockaddr kLlrpIpv4RespAddrInternal;
LwpaSockaddr kLlrpIpv6RespAddrInternal;
LwpaSockaddr kLlrpIpv4RequestAddrInternal;
LwpaSockaddr kLlrpIpv6RequestAddrInternal;

const LwpaSockaddr *kLlrpIpv4RespAddr = &kLlrpIpv4RespAddrInternal;
const LwpaSockaddr *kLlrpIpv6RespAddr = &kLlrpIpv6RespAddrInternal;
const LwpaSockaddr *kLlrpIpv4RequestAddr = &kLlrpIpv4RequestAddrInternal;
const LwpaSockaddr *kLlrpIpv6RequestAddr = &kLlrpIpv6RequestAddrInternal;

/*********************** Private function prototypes *************************/

static LwpaIpAddr get_llrp_mcast_addr(bool manager, lwpa_iptype_t ip_type);
static lwpa_error_t create_sys_socket(lwpa_iptype_t ip_type, unsigned int netint_index, bool manager,
                                      lwpa_socket_t *socket);
static lwpa_error_t subscribe_multicast(lwpa_socket_t socket, lwpa_iptype_t ip_type, unsigned int netint_index,
                                        bool manager);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_llrp_init()
{
  lwpa_error_t res = kLwpaErrOk;
#if RDMNET_DYNAMIC_MEM
  bool manager_initted = false;
#endif
  bool target_initted = false;

  lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_RESPONSE, &kLlrpIpv4RespAddrInternal.ip);
  kLlrpIpv4RespAddrInternal.port = LLRP_PORT;
  lwpa_inet_pton(kLwpaIpTypeV6, LLRP_MULTICAST_IPV6_ADDRESS_RESPONSE, &kLlrpIpv6RespAddrInternal.ip);
  kLlrpIpv6RespAddrInternal.port = LLRP_PORT;
  lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_REQUEST, &kLlrpIpv4RequestAddrInternal.ip);
  kLlrpIpv4RequestAddrInternal.port = LLRP_PORT;
  lwpa_inet_pton(kLwpaIpTypeV6, LLRP_MULTICAST_IPV6_ADDRESS_REQUEST, &kLlrpIpv6RequestAddrInternal.ip);
  kLlrpIpv6RequestAddrInternal.port = LLRP_PORT;
  llrp_prot_init();

#if RDMNET_DYNAMIC_MEM
  manager_initted = ((res = rdmnet_llrp_manager_init()) == kLwpaErrOk);
#endif

  if (res == kLwpaErrOk)
  {
    target_initted = ((res = rdmnet_llrp_target_init()) == kLwpaErrOk);
  }

  if (res != kLwpaErrOk)
  {
    if (target_initted)
      rdmnet_llrp_target_deinit();
#if RDMNET_DYNAMIC_MEM
    if (manager_initted)
      rdmnet_llrp_manager_deinit();
#endif
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

lwpa_error_t create_llrp_socket(lwpa_iptype_t ip_type, unsigned int netint_index, bool manager, lwpa_socket_t *socket)
{
  lwpa_socket_t socket_out;

  lwpa_error_t res = create_sys_socket(ip_type, netint_index, manager, &socket_out);
  if (res == kLwpaErrOk)
    res = subscribe_multicast(socket_out, ip_type, netint_index, manager);
  if (res == kLwpaErrOk)
    *socket = socket_out;
  return res;
}

lwpa_error_t create_sys_socket(lwpa_iptype_t ip_type, unsigned int netint_index, bool manager, lwpa_socket_t *socket)
{
  lwpa_socket_t sock = LWPA_SOCKET_INVALID;
  int sockopt_ip_level = (ip_type == kLwpaIpTypeV6 ? LWPA_IPPROTO_IPV6 : LWPA_IPPROTO_IP);

  lwpa_error_t res = lwpa_socket(ip_type == kLwpaIpTypeV6 ? LWPA_AF_INET6 : LWPA_AF_INET, LWPA_DGRAM, &sock);

  if (res == kLwpaErrOk)
  {
    // SO_REUSEADDR allows multiple sockets to bind to LLRP_PORT, which is very important for our
    // multicast needs.
    int value = 1;
    res = lwpa_setsockopt(sock, LWPA_SOL_SOCKET, LWPA_SO_REUSEADDR, (const void *)(&value), sizeof(value));
  }

  if (res == kLwpaErrOk)
  {
    // We also set SO_REUSEPORT but don't check the return, because it is not applicable on all platforms
    int value = 1;
    lwpa_setsockopt(sock, LWPA_SOL_SOCKET, LWPA_SO_REUSEPORT, (const void *)(&value), sizeof(value));

    // MULTICAST_TTL controls the TTL field in outgoing multicast datagrams.
    value = LLRP_MULTICAST_TTL_VAL;
    res = lwpa_setsockopt(sock, sockopt_ip_level, LWPA_IP_MULTICAST_TTL, (const void *)(&value), sizeof(value));
  }

  if (res == kLwpaErrOk)
  {
    // MULTICAST_IF is critical for multicast sends to go over the correct interface.
    res = lwpa_setsockopt(sock, sockopt_ip_level, LWPA_IP_MULTICAST_IF, (const void *)(&netint_index),
                          sizeof netint_index);
  }

  if (res == kLwpaErrOk)
  {
    LwpaSockaddr bind_addr;
#if 0  // RDMNET_LLRP_BIND_TO_MCAST_ADDRESS
    // Bind socket to multicast address
    bind_addr.ip = get_llrp_mcast_addr(manager, ip_type);
#else
    (void)manager;
    // Bind socket to the wildcard address
    lwpa_ip_set_wildcard(ip_type, &bind_addr.ip);
#endif
    bind_addr.port = LLRP_PORT;
    res = lwpa_bind(sock, &bind_addr);
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

lwpa_error_t subscribe_multicast(lwpa_socket_t socket, lwpa_iptype_t ip_type, unsigned int netint_index, bool manager)
{
  LwpaGroupReq group_req;
  group_req.ifindex = netint_index;
  group_req.group = get_llrp_mcast_addr(manager, ip_type);
  lwpa_error_t res = lwpa_setsockopt(socket, ip_type == kLwpaIpTypeV6 ? LWPA_IPPROTO_IPV6 : LWPA_IPPROTO_IP,
                                     LWPA_MCAST_JOIN_GROUP, (const void *)&group_req, sizeof(group_req));

  return res;
}

LwpaIpAddr get_llrp_mcast_addr(bool manager, lwpa_iptype_t ip_type)
{
  if (manager)
  {
    if (ip_type == kLwpaIpTypeV6)
      return kLlrpIpv6RespAddrInternal.ip;
    else
      return kLlrpIpv4RespAddrInternal.ip;
  }
  else
  {
    if (ip_type == kLwpaIpTypeV6)
      return kLlrpIpv6RequestAddrInternal.ip;
    else
      return kLlrpIpv4RequestAddrInternal.ip;
  }
}

void rdmnet_llrp_tick()
{
#if RDMNET_DYNAMIC_MEM
  rdmnet_llrp_manager_tick();
#endif
  rdmnet_llrp_target_tick();
}
