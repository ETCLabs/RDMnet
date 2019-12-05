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

#include "lwmdns_recv.h"

#include "etcpal/inet.h"
#include "etcpal/socket.h"
#include "rdmnet/defs.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/mcast.h"
#include "rdmnet/private/opts.h"

/******************************************************************************
 * Private Macros
 *****************************************************************************/

#define MDNS_IP_FOR_TYPE(ip_type) (type == kEtcPalIpTypeV6 ? kMdnsIpv6Address.ip : kMdnsIpv4Address.ip)

/******************************************************************************
 * Private Types
 *****************************************************************************/

typedef struct MdnsRecvSocket
{
  etcpal_socket_t socket;
  PolledSocketInfo poll_info;
#if RDMNET_DYNAMIC_MEM
  RdmnetMcastNetintId* netints;
#else
  RdmnetMcastNetintId netints[RDMNET_MAX_MCAST_NETINTS];
#endif
  size_t num_netints;
} MdnsRecvSocket;

/******************************************************************************
 * Private Variables
 *****************************************************************************/

EtcPalIpAddr kMdnsIpv4Address;
EtcPalIpAddr kMdnsIpv6Address;

MdnsRecvSocket recv_sock_ipv4;
MdnsRecvSocket recv_sock_ipv6;

/******************************************************************************
 * Private function prototypes
 *****************************************************************************/

static etcpal_error_t init_recv_socket(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group,
                                       const RdmnetNetintConfig* netint_config);
static etcpal_error_t setup_recv_netints(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group,
                                         const RdmnetMcastNetintId* mcast_netint_arr, size_t mcast_netint_arr_size,
                                         const RdmnetNetintConfig* netint_config);
static void deinit_recv_socket(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group);
static void cleanup_recv_netints(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group);
static void mdns_socket_activity(const EtcPalPollEvent* event, PolledSocketOpaqueData data);

/******************************************************************************
 * Function Definitions
 *****************************************************************************/

etcpal_error_t mdns_recv_init(const RdmnetNetintConfig* netint_config)
{
  etcpal_inet_pton(kEtcPalIpTypeV4, E133_MDNS_IPV4_MULTICAST_ADDRESS, &kMdnsIpv4Address);
  etcpal_inet_pton(kEtcPalIpTypeV6, E133_MDNS_IPV6_MULTICAST_ADDRESS, &kMdnsIpv6Address);

  etcpal_error_t res = init_recv_socket(&recv_sock_ipv4, &kMdnsIpv4Address, netint_config);
  if (res == kEtcPalErrOk)
  {
    res = init_recv_socket(&recv_sock_ipv6, &kMdnsIpv6Address, netint_config);
    if (res != kEtcPalErrOk)
      deinit_recv_socket(&recv_sock_ipv4, &kMdnsIpv4Address);
  }
  return res;
}

etcpal_error_t init_recv_socket(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group,
                                const RdmnetNetintConfig* netint_config)
{
  // Initialize the network interface array from the global multicast network interface array.
  const RdmnetMcastNetintId* mcast_array;
  size_t mcast_array_size = rdmnet_get_mcast_netint_array(&mcast_array);

  size_t num_netints_requested = (netint_config ? netint_config->num_netints : mcast_array_size);
#if RDMNET_DYNAMIC_MEM
#define DEALLOC_NETINTS() free(sock_struct->netints)
  sock_struct->netints = (RdmnetMcastNetintId*)calloc(num_netints_requested, sizeof(RdmnetMcastNetintId));
  if (!sock_struct->netints)
    return kEtcPalErrNoMem;
#else
#define DEALLOC_NETINTS()
  if (num_netints_requested > RDMNET_MAX_MCAST_NETINTS)
    return kEtcPalErrNoMem;
#endif

  etcpal_error_t res = rdmnet_create_mcast_recv_socket(mcast_group, E133_MDNS_PORT, &sock_struct->socket);
  if (res != kEtcPalErrOk)
  {
    DEALLOC_NETINTS();
    return res;
  }

  sock_struct->poll_info.callback = mdns_socket_activity;
  sock_struct->poll_info.data.int_val = mcast_group->type;
  res = rdmnet_core_add_polled_socket(sock_struct->socket, ETCPAL_POLL_IN, &sock_struct->poll_info);
  if (res != kEtcPalErrOk)
  {
    etcpal_close(sock_struct->socket);
    DEALLOC_NETINTS();
    return res;
  }

  res = setup_recv_netints(sock_struct, mcast_group, mcast_array, mcast_array_size, netint_config);
  if (res != kEtcPalErrOk)
  {
    cleanup_recv_netints(sock_struct, mcast_group);
  }
  return res;
}

void deinit_recv_socket(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group)
{
  cleanup_recv_netints(sock_struct, mcast_group);

  rdmnet_core_remove_polled_socket(sock_struct->socket);
  etcpal_close(sock_struct->socket);
}

etcpal_error_t setup_recv_netints(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group,
                                  const RdmnetMcastNetintId* mcast_netint_arr, size_t mcast_netint_arr_size,
                                  const RdmnetNetintConfig* netint_config)
{
  etcpal_error_t res = kEtcPalErrNoNetints;
  sock_struct->num_netints = 0;
  if (netint_config)
  {
    for (const RdmnetMcastNetintId* netint = netint_config->netint_arr;
         netint < netint_config->netint_arr + netint_config->num_netints; ++netint)
    {
      if (netint->ip_type == mcast_group->type)
      {
        res = rdmnet_subscribe_mcast_recv_socket(sock_struct->socket, netint, mcast_group);
        if (res != kEtcPalErrOk)
          break;
        sock_struct->netints[sock_struct->num_netints++] = *netint;
      }
    }
  }
  else
  {
    for (const RdmnetMcastNetintId* netint = mcast_netint_arr; netint < mcast_netint_arr + mcast_netint_arr_size;
         ++netint)
    {
      if (netint->ip_type == mcast_group->type)
      {
        res = rdmnet_subscribe_mcast_recv_socket(sock_struct->socket, netint, mcast_group);
        if (res != kEtcPalErrOk)
          break;
        sock_struct->netints[sock_struct->num_netints++] = *netint;
      }
    }
  }
  return res;
}

void cleanup_recv_netints(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group)
{
  for (RdmnetMcastNetintId* netint = sock_struct->netints; netint < sock_struct->netints + sock_struct->num_netints;
       ++netint)
  {
    rdmnet_unsubscribe_mcast_recv_socket(sock_struct->socket, netint, mcast_group);
  }
#if RDMNET_DYNAMIC_MEM
  free(sock_struct->netints);
#endif
}

void mdns_socket_activity(const EtcPalPollEvent* event, PolledSocketOpaqueData data)
{
}
