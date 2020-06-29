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

#include "rdmnet/core/llrp.h"

#include "etcpal/netint.h"
#include "etcpal/rbtree.h"
#include "rdmnet/core/llrp_manager.h"
#include "rdmnet/core/llrp_target.h"
#include "rdmnet/core/mcast.h"
#include "rdmnet/core/util.h"
#include "rdmnet/core/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif

/**************************** Global variables *******************************/

EtcPalSockAddr kLlrpIpv4RespAddrInternal;
EtcPalSockAddr kLlrpIpv6RespAddrInternal;
EtcPalSockAddr kLlrpIpv4RequestAddrInternal;
EtcPalSockAddr kLlrpIpv6RequestAddrInternal;
EtcPalUuid     kLlrpBroadcastCidInternal;

const EtcPalSockAddr* kLlrpIpv4RespAddr = &kLlrpIpv4RespAddrInternal;
const EtcPalSockAddr* kLlrpIpv6RespAddr = &kLlrpIpv6RespAddrInternal;
const EtcPalSockAddr* kLlrpIpv4RequestAddr = &kLlrpIpv4RequestAddrInternal;
const EtcPalSockAddr* kLlrpIpv6RequestAddr = &kLlrpIpv6RequestAddrInternal;
const EtcPalUuid*     kLlrpBroadcastCid = &kLlrpBroadcastCidInternal;

EtcPalMacAddr kLlrpLowestHardwareAddr;

/****************************** Private types ********************************/

typedef struct LlrpRecvNetint
{
  RdmnetMcastNetintId id;
  size_t              ref_count;
} LlrpRecvNetint;

typedef struct LlrpRecvSocket
{
  bool created;

  llrp_socket_t      llrp_type;
  etcpal_socket_t    socket;
  RCPolledSocketInfo poll_info;

#if RDMNET_DYNAMIC_MEM
  LlrpRecvNetint* netints;
#else
  LlrpRecvNetint netints[RDMNET_MAX_MCAST_NETINTS];
#endif
  size_t num_netints;
} LlrpRecvSocket;

/**************************** Private variables ******************************/

static struct LlrpState
{
  LlrpRecvSocket manager_recvsock_ipv4;
  LlrpRecvSocket manager_recvsock_ipv6;
  LlrpRecvSocket target_recvsock_ipv4;
  LlrpRecvSocket target_recvsock_ipv6;
} state;

/*********************** Private function prototypes *************************/

static void init_recv_socket(LlrpRecvSocket* sock_struct, llrp_socket_t llrp_type);
static void deinit_recv_socket(LlrpRecvSocket* sock_struct);

static const EtcPalIpAddr* get_llrp_mcast_addr(llrp_socket_t llrp_type, etcpal_iptype_t ip_type);
static LlrpRecvSocket*     get_llrp_recv_sock(llrp_socket_t llrp_type, etcpal_iptype_t ip_type);
static etcpal_error_t create_recv_socket(llrp_socket_t llrp_type, etcpal_iptype_t ip_type, LlrpRecvSocket* sock_struct);

static void llrp_socket_activity(const EtcPalPollEvent* event, RCPolledSocketOpaqueData data);
static void llrp_socket_error(etcpal_error_t err);

/*************************** Function definitions ****************************/

etcpal_error_t rc_llrp_module_init(void)
{
  etcpal_string_to_ip(kEtcPalIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_RESPONSE, &kLlrpIpv4RespAddrInternal.ip);
  kLlrpIpv4RespAddrInternal.port = LLRP_PORT;
  init_recv_socket(&state.manager_recvsock_ipv4, kLlrpSocketTypeManager);

  etcpal_string_to_ip(kEtcPalIpTypeV6, LLRP_MULTICAST_IPV6_ADDRESS_RESPONSE, &kLlrpIpv6RespAddrInternal.ip);
  kLlrpIpv6RespAddrInternal.port = LLRP_PORT;
  init_recv_socket(&state.manager_recvsock_ipv6, kLlrpSocketTypeManager);

  etcpal_string_to_ip(kEtcPalIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_REQUEST, &kLlrpIpv4RequestAddrInternal.ip);
  kLlrpIpv4RequestAddrInternal.port = LLRP_PORT;
  init_recv_socket(&state.target_recvsock_ipv4, kLlrpSocketTypeTarget);

  etcpal_string_to_ip(kEtcPalIpTypeV6, LLRP_MULTICAST_IPV6_ADDRESS_REQUEST, &kLlrpIpv6RequestAddrInternal.ip);
  kLlrpIpv6RequestAddrInternal.port = LLRP_PORT;
  init_recv_socket(&state.target_recvsock_ipv6, kLlrpSocketTypeTarget);

  etcpal_string_to_uuid(LLRP_BROADCAST_CID, &kLlrpBroadcastCidInternal);

  return kEtcPalErrOk;
}

void rc_llrp_module_deinit(void)
{
  deinit_recv_socket(&state.manager_recvsock_ipv4);
  deinit_recv_socket(&state.manager_recvsock_ipv6);
  deinit_recv_socket(&state.target_recvsock_ipv4);
  deinit_recv_socket(&state.target_recvsock_ipv6);
}

etcpal_error_t rc_llrp_recv_netint_add(const RdmnetMcastNetintId* id, llrp_socket_t llrp_type)
{
  etcpal_error_t  res = kEtcPalErrNotFound;
  LlrpRecvSocket* recv_sock = get_llrp_recv_sock(llrp_type, id->ip_type);
  LlrpRecvNetint* netint = recv_sock->netints;

  // Find the requested network interface in the list
  for (; netint < recv_sock->netints + recv_sock->num_netints; ++netint)
  {
    if (netint->id.index == id->index && netint->id.ip_type == id->ip_type)
      break;
  }

  if (netint < recv_sock->netints + recv_sock->num_netints)
  {
    res = kEtcPalErrOk;
    bool sock_created = false;

    if (!recv_sock->created)
    {
      sock_created = ((res = create_recv_socket(llrp_type, id->ip_type, recv_sock)) == kEtcPalErrOk);
    }

    if (res == kEtcPalErrOk && netint->ref_count == 0)
    {
      res = rc_mcast_subscribe_recv_socket(recv_sock->socket, &netint->id, get_llrp_mcast_addr(llrp_type, id->ip_type));
    }

    if (res == kEtcPalErrOk)
    {
      ++netint->ref_count;
    }
    else if (sock_created)
    {
      etcpal_close(recv_sock->socket);
      recv_sock->created = false;
    }
  }

  return res;
}

void rc_llrp_recv_netint_remove(const RdmnetMcastNetintId* id, llrp_socket_t llrp_type)
{
  LlrpRecvSocket* recv_sock = get_llrp_recv_sock(llrp_type, id->ip_type);
  LlrpRecvNetint* netint = recv_sock->netints;

  // Find the requested network interface in the list
  for (; netint < recv_sock->netints + recv_sock->num_netints; ++netint)
  {
    if (netint->id.index == id->index && netint->id.ip_type == id->ip_type)
      break;
  }

  if (netint < recv_sock->netints + recv_sock->num_netints && netint->ref_count > 0)
  {
    if (--netint->ref_count == 0)
    {
      rc_mcast_unsubscribe_recv_socket(recv_sock->socket, &netint->id, get_llrp_mcast_addr(llrp_type, id->ip_type));
    }
  }
}

void init_recv_socket(LlrpRecvSocket* sock_struct, llrp_socket_t llrp_type)
{
  // Initialize the network interface array from the global multicast network interface array.
  const RdmnetMcastNetintId* mcast_array;
  size_t                     array_size = rc_mcast_get_netint_array(&mcast_array);

  sock_struct->num_netints = array_size;
#if RDMNET_DYNAMIC_MEM
  sock_struct->netints = (LlrpRecvNetint*)calloc(sock_struct->num_netints, sizeof(LlrpRecvNetint));
  RDMNET_ASSERT(sock_struct->netints);
#endif

  for (size_t i = 0; i < array_size; ++i)
  {
    sock_struct->netints[i].id = mcast_array[i];
    sock_struct->netints[i].ref_count = 0;
  }

  // Initialize the rest of the structure.
  sock_struct->created = false;
  sock_struct->socket = ETCPAL_SOCKET_INVALID;
  sock_struct->llrp_type = llrp_type;
}

void deinit_recv_socket(LlrpRecvSocket* sock_struct)
{
  if (sock_struct->created)
  {
    for (LlrpRecvNetint* netint = sock_struct->netints; netint < sock_struct->netints + sock_struct->num_netints;
         ++netint)
    {
      if (netint->ref_count > 0)
      {
        rc_mcast_unsubscribe_recv_socket(sock_struct->socket, &netint->id,
                                         get_llrp_mcast_addr(sock_struct->llrp_type, netint->id.ip_type));
        netint->ref_count = 0;
      }
    }
#if RDMNET_DYNAMIC_MEM
    free(sock_struct->netints);
#endif

    rc_remove_polled_socket(sock_struct->socket);
    etcpal_close(sock_struct->socket);
    sock_struct->created = false;
  }
}

etcpal_error_t create_recv_socket(llrp_socket_t llrp_type, etcpal_iptype_t ip_type, LlrpRecvSocket* sock_struct)
{
  etcpal_error_t res =
      rc_mcast_create_recv_socket(get_llrp_mcast_addr(llrp_type, ip_type), LLRP_PORT, &sock_struct->socket);

  if (res == kEtcPalErrOk)
  {
    sock_struct->poll_info.callback = llrp_socket_activity;
    sock_struct->poll_info.data.int_val = (int)llrp_type;
    res = rc_add_polled_socket(sock_struct->socket, ETCPAL_POLL_IN, &sock_struct->poll_info);
  }

  if (res == kEtcPalErrOk)
  {
    sock_struct->created = true;
  }
  else if (sock_struct->socket != ETCPAL_SOCKET_INVALID)
  {
    etcpal_close(sock_struct->socket);
  }

  return res;
}

void llrp_socket_activity(const EtcPalPollEvent* event, RCPolledSocketOpaqueData data)
{
  static uint8_t llrp_recv_buf[LLRP_MAX_MESSAGE_SIZE];

  if (event->events & ETCPAL_POLL_ERR)
  {
    llrp_socket_error(event->err);
  }
  else if (event->events & ETCPAL_POLL_IN)
  {
    EtcPalSockAddr from_addr;
    int            recv_res = etcpal_recvfrom(event->socket, llrp_recv_buf, LLRP_MAX_MESSAGE_SIZE, 0, &from_addr);
    if (recv_res <= 0)
    {
      if (recv_res != kEtcPalErrMsgSize)
      {
        llrp_socket_error((etcpal_error_t)recv_res);
      }
    }
    else
    {
      RdmnetMcastNetintId netint_id;
      if (kEtcPalErrOk == etcpal_netint_get_interface_for_dest(&from_addr.ip, &netint_id.index))
      {
        netint_id.ip_type = from_addr.ip.type;

        if ((llrp_socket_t)data.int_val == kLlrpSocketTypeManager)
          rc_llrp_manager_data_received(llrp_recv_buf, (size_t)recv_res, &netint_id);
        else
          rc_llrp_target_data_received(llrp_recv_buf, (size_t)recv_res, &netint_id);
      }
      else if (RDMNET_CAN_LOG(ETCPAL_LOG_WARNING))
      {
        char addr_str[ETCPAL_IP_STRING_BYTES];
        etcpal_ip_to_string(&from_addr.ip, addr_str);
        RDMNET_LOG_WARNING("Couldn't reply to LLRP message from %s:%u because no reply route could be found.", addr_str,
                           from_addr.port);
      }
    }
  }
}

void llrp_socket_error(etcpal_error_t err)
{
  RDMNET_LOG_WARNING("Error receiving on an LLRP socket: '%s'", etcpal_strerror(err));
}

const EtcPalIpAddr* get_llrp_mcast_addr(llrp_socket_t llrp_type, etcpal_iptype_t ip_type)
{
  if (llrp_type == kLlrpSocketTypeManager)
  {
    if (ip_type == kEtcPalIpTypeV6)
      return &kLlrpIpv6RespAddrInternal.ip;
    else
      return &kLlrpIpv4RespAddrInternal.ip;
  }
  else
  {
    if (ip_type == kEtcPalIpTypeV6)
      return &kLlrpIpv6RequestAddrInternal.ip;
    else
      return &kLlrpIpv4RequestAddrInternal.ip;
  }
}

LlrpRecvSocket* get_llrp_recv_sock(llrp_socket_t llrp_type, etcpal_iptype_t ip_type)
{
  if (llrp_type == kLlrpSocketTypeManager)
  {
    if (ip_type == kEtcPalIpTypeV6)
      return &state.manager_recvsock_ipv6;
    else
      return &state.manager_recvsock_ipv4;
  }
  else
  {
    if (ip_type == kEtcPalIpTypeV6)
      return &state.target_recvsock_ipv6;
    else
      return &state.target_recvsock_ipv4;
  }
}
