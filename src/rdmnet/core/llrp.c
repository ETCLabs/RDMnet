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

#include "rdmnet/core/llrp.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif
#include "etcpal/netint.h"
#include "etcpal/rbtree.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/llrp_manager.h"
#include "rdmnet/private/llrp_target.h"
#include "rdmnet/private/opts.h"

/**************************** Global variables *******************************/

EtcPalSockAddr kLlrpIpv4RespAddrInternal;
EtcPalSockAddr kLlrpIpv6RespAddrInternal;
EtcPalSockAddr kLlrpIpv4RequestAddrInternal;
EtcPalSockAddr kLlrpIpv6RequestAddrInternal;

const EtcPalSockAddr* kLlrpIpv4RespAddr = &kLlrpIpv4RespAddrInternal;
const EtcPalSockAddr* kLlrpIpv6RespAddr = &kLlrpIpv6RespAddrInternal;
const EtcPalSockAddr* kLlrpIpv4RequestAddr = &kLlrpIpv4RequestAddrInternal;
const EtcPalSockAddr* kLlrpIpv6RequestAddr = &kLlrpIpv6RequestAddrInternal;

EtcPalMacAddr kLlrpLowestHardwareAddr;

/**************************** Private constants ******************************/

#define LLRP_MAX_RB_NODES (RDMNET_LLRP_MAX_NETINTS_PER_TARGET)

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define llrp_netint_alloc() malloc(sizeof(LlrpNetint))
#define llrp_netint_dealloc(ptr) free(ptr)
#else
#define llrp_netint_alloc() etcpal_mempool_alloc(llrp_netints)
#define llrp_netint_dealloc(ptr) etcpal_mempool_free(llrp_netints, ptr)
#endif

/****************************** Private types ********************************/

typedef struct LlrpRecvSocket
{
  bool initted;
  etcpal_socket_t socket;
  PolledSocketInfo poll_info;
} LlrpRecvSocket;

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(llrp_netints, LlrpNetint, RDMNET_LLRP_MAX_NETINTS_PER_TARGET);
ETCPAL_MEMPOOL_DEFINE(llrp_rb_nodes, EtcPalRbNode, LLRP_MAX_RB_NODES);
#endif

static struct LlrpState
{
  LlrpRecvSocket manager_recvsock_ipv4;
  LlrpRecvSocket manager_recvsock_ipv6;
  LlrpRecvSocket target_recvsock_ipv4;
  LlrpRecvSocket target_recvsock_ipv6;

  EtcPalRbTree sys_netints;
} state;

/*********************** Private function prototypes *************************/

static etcpal_error_t init_sys_netints();
static etcpal_error_t init_sys_netint(const LlrpNetintId* id);
static void deinit_sys_netints();
static void deinit_sys_netint(const EtcPalRbTree* self, EtcPalRbNode* node);

static EtcPalIpAddr get_llrp_mcast_addr(llrp_socket_t llrp_type, etcpal_iptype_t ip_type);
static LlrpRecvSocket* get_llrp_recv_sock(llrp_socket_t llrp_type, etcpal_iptype_t ip_type);
static etcpal_error_t create_send_socket(const LlrpNetintId* netint, etcpal_socket_t* socket);
static etcpal_error_t create_recv_socket(llrp_socket_t llrp_type, etcpal_iptype_t ip_type, LlrpRecvSocket* sock_struct);
static etcpal_error_t subscribe_recv_socket(const LlrpNetintId* netint, llrp_socket_t llrp_type,
                                            LlrpRecvSocket* sock_struct);
static etcpal_error_t unsubscribe_recv_socket(const LlrpNetintId* netint, llrp_socket_t llrp_type,
                                              LlrpRecvSocket* sock_struct);
static void destroy_recv_socket(LlrpRecvSocket* sock_struct);

static void llrp_socket_activity(const EtcPalPollEvent* event, PolledSocketOpaqueData data);
static void llrp_socket_error(etcpal_error_t err);

static int netint_cmp(const EtcPalRbTree* self, const EtcPalRbNode* node_a, const EtcPalRbNode* node_b);
static EtcPalRbNode* llrp_node_alloc();
static void llrp_node_free(EtcPalRbNode* node);

/*************************** Function definitions ****************************/

etcpal_error_t rdmnet_llrp_init()
{
  bool target_initted = false;
  bool netints_initted = false;
#if RDMNET_DYNAMIC_MEM
  bool manager_initted = false;
#endif

  etcpal_inet_pton(kEtcPalIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_RESPONSE, &kLlrpIpv4RespAddrInternal.ip);
  kLlrpIpv4RespAddrInternal.port = LLRP_PORT;
  etcpal_inet_pton(kEtcPalIpTypeV6, LLRP_MULTICAST_IPV6_ADDRESS_RESPONSE, &kLlrpIpv6RespAddrInternal.ip);
  kLlrpIpv6RespAddrInternal.port = LLRP_PORT;
  etcpal_inet_pton(kEtcPalIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_REQUEST, &kLlrpIpv4RequestAddrInternal.ip);
  kLlrpIpv4RequestAddrInternal.port = LLRP_PORT;
  etcpal_inet_pton(kEtcPalIpTypeV6, LLRP_MULTICAST_IPV6_ADDRESS_REQUEST, &kLlrpIpv6RequestAddrInternal.ip);
  kLlrpIpv6RequestAddrInternal.port = LLRP_PORT;
  llrp_prot_init();
  etcpal_rbtree_init(&state.sys_netints, netint_cmp, llrp_node_alloc, llrp_node_free);

  etcpal_error_t res;
  netints_initted = ((res = init_sys_netints()) == kEtcPalErrOk);

#if RDMNET_DYNAMIC_MEM
  if (res == kEtcPalErrOk)
    manager_initted = ((res = rdmnet_llrp_manager_init()) == kEtcPalErrOk);
#endif

  if (res == kEtcPalErrOk)
    target_initted = ((res = rdmnet_llrp_target_init()) == kEtcPalErrOk);

  if (res != kEtcPalErrOk)
  {
    if (target_initted)
      rdmnet_llrp_target_deinit();
#if RDMNET_DYNAMIC_MEM
    if (manager_initted)
      rdmnet_llrp_manager_deinit();
#endif
    if (netints_initted)
      deinit_sys_netints();
  }

  return res;
}

void rdmnet_llrp_deinit()
{
  rdmnet_llrp_target_deinit();
#if RDMNET_DYNAMIC_MEM
  rdmnet_llrp_manager_deinit();
#endif

  deinit_sys_netints();

  destroy_recv_socket(&state.manager_recvsock_ipv4);
  destroy_recv_socket(&state.manager_recvsock_ipv6);
  destroy_recv_socket(&state.target_recvsock_ipv4);
  destroy_recv_socket(&state.target_recvsock_ipv6);
}

void rdmnet_llrp_tick()
{
#if RDMNET_DYNAMIC_MEM
  rdmnet_llrp_manager_tick();
#endif
  rdmnet_llrp_target_tick();
}

etcpal_error_t init_sys_netints()
{
  size_t num_sys_netints = etcpal_netint_get_num_interfaces();
  if (num_sys_netints == 0)
    return kEtcPalErrNoNetints;

  const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();

  memset(kLlrpLowestHardwareAddr.data, 0xff, ETCPAL_MAC_BYTES);

  etcpal_log(rdmnet_log_params, ETCPAL_LOG_INFO, RDMNET_LOG_MSG("Initializing network interfaces for LLRP..."));
  for (const EtcPalNetintInfo* netint = netint_list; netint < netint_list + num_sys_netints; ++netint)
  {
    char addr_str[ETCPAL_INET6_ADDRSTRLEN];
    addr_str[0] = '\0';
    if (etcpal_can_log(rdmnet_log_params, ETCPAL_LOG_INFO))
    {
      etcpal_inet_ntop(&netint->addr, addr_str, ETCPAL_INET6_ADDRSTRLEN);
    }

    // Create a test send and receive socket on each network interface. If the either one fails, we
    // remove that interface from the set that LLRP targets listen on.
    LlrpNetintId netint_id;
    netint_id.index = netint->index;
    netint_id.ip_type = netint->addr.type;

    // create_send_socket() also tests setting the relevant send socket options and the
    // MULTICAST_IF on the relevant interface.
    etcpal_socket_t test_socket;
    etcpal_error_t test_res = create_send_socket(&netint_id, &test_socket);
    if (test_res == kEtcPalErrOk)
    {
      etcpal_close(test_socket);

      LlrpRecvSocket test_recv_sock;
      test_res = create_recv_socket(kLlrpSocketTypeTarget, netint_id.ip_type, &test_recv_sock);
      if (test_res == kEtcPalErrOk)
      {
        test_res = subscribe_recv_socket(&netint_id, kLlrpSocketTypeTarget, &test_recv_sock);
        destroy_recv_socket(&test_recv_sock);
      }

      if (test_res == kEtcPalErrOk)
        test_res = init_sys_netint(&netint_id);
    }

    if (test_res == kEtcPalErrOk)
    {
      etcpal_log(rdmnet_log_params, ETCPAL_LOG_INFO,
                 RDMNET_LOG_MSG("  Set up LLRP network interface %s for listening."), addr_str);

      // Modify the lowest hardware address, if necessary
      if (!ETCPAL_MAC_IS_NULL(&netint->mac))
      {
        if (netint == netint_list)
        {
          kLlrpLowestHardwareAddr = netint->mac;
        }
        else
        {
          if (ETCPAL_MAC_CMP(&netint->mac, &kLlrpLowestHardwareAddr) < 0)
            kLlrpLowestHardwareAddr = netint->mac;
        }
      }
    }
    else
    {
      etcpal_log(rdmnet_log_params, ETCPAL_LOG_WARNING,
                 RDMNET_LOG_MSG("  Error creating test socket on LLRP network interface %s: '%s'. Skipping!"), addr_str,
                 etcpal_strerror(test_res));
    }
  }

  if (etcpal_rbtree_size(&state.sys_netints) == 0)
  {
    etcpal_log(rdmnet_log_params, ETCPAL_LOG_ERR, RDMNET_LOG_MSG("No usable LLRP interfaces found."));
    return kEtcPalErrNoNetints;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_sys_netint(const LlrpNetintId* id)
{
  etcpal_error_t res = kEtcPalErrNoMem;
  LlrpNetint* new_netint = (LlrpNetint*)llrp_netint_alloc();
  if (new_netint)
  {
    // Member initialization
    new_netint->id = *id;
    new_netint->recv_ref_count = 0;
    new_netint->send_ref_count = 0;
    new_netint->send_sock = ETCPAL_SOCKET_INVALID;

    res = etcpal_rbtree_insert(&state.sys_netints, new_netint);
    if (res == kEtcPalErrOk || res == kEtcPalErrExists)
    {
      // We still indicate success when we find a duplicate - effectively making one entry
      // for any multi-homed interfaces while still logging meaningful messages.
      if (res == kEtcPalErrExists)
      {
        // But make sure it is deallocated in this case to avoid leaks.
        llrp_netint_dealloc(new_netint);
        res = kEtcPalErrOk;
      }
    }
  }
  return res;
}

void deinit_sys_netints()
{
  etcpal_rbtree_clear_with_cb(&state.sys_netints, deinit_sys_netint);
}

void deinit_sys_netint(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  (void)self;

  LlrpNetint* netint = (LlrpNetint*)node->value;
  if (netint->send_ref_count)
    etcpal_close(netint->send_sock);
  llrp_netint_dealloc(netint);
  llrp_node_free(node);
}

void get_llrp_netint_list(EtcPalRbIter* list_iter)
{
  etcpal_rbiter_init(list_iter);
  etcpal_rbiter_first(list_iter, &state.sys_netints);
}

etcpal_error_t get_llrp_send_socket(const LlrpNetintId* netint, etcpal_socket_t* socket)
{
  etcpal_error_t res = kEtcPalErrNotFound;

  LlrpNetint* netint_info = (LlrpNetint*)etcpal_rbtree_find(&state.sys_netints, (void*)netint);
  if (netint_info)
  {
    res = kEtcPalErrOk;
    if (netint_info->send_ref_count == 0)
    {
      res = create_send_socket(netint, &netint_info->send_sock);
    }

    if (res == kEtcPalErrOk)
    {
      ++netint_info->send_ref_count;
      *socket = netint_info->send_sock;
    }
  }

  return res;
}

void release_llrp_send_socket(const LlrpNetintId* netint)
{
  LlrpNetint* netint_info = (LlrpNetint*)etcpal_rbtree_find(&state.sys_netints, (void*)netint);
  if (netint_info && netint_info->send_ref_count > 0)
  {
    if (--netint_info->send_ref_count == 0)
    {
      etcpal_close(netint_info->send_sock);
      netint_info->send_sock = ETCPAL_SOCKET_INVALID;
    }
  }
}

etcpal_error_t llrp_recv_netint_add(const LlrpNetintId* netint, llrp_socket_t llrp_type)
{
  etcpal_error_t res = kEtcPalErrNotFound;

  LlrpNetint* netint_info = (LlrpNetint*)etcpal_rbtree_find(&state.sys_netints, (void*)netint);
  if (netint_info)
  {
    res = kEtcPalErrOk;
    LlrpRecvSocket* recv_sock = get_llrp_recv_sock(llrp_type, netint->ip_type);
    bool sock_created = false;

    if (!recv_sock->initted)
    {
      sock_created = ((res = create_recv_socket(llrp_type, netint->ip_type, recv_sock)) == kEtcPalErrOk);
    }

    if (res == kEtcPalErrOk)
    {
      res = subscribe_recv_socket(netint, llrp_type, recv_sock);
    }

    if (res == kEtcPalErrOk)
    {
      ++netint_info->recv_ref_count;
    }
    else if (sock_created)
    {
      etcpal_close(recv_sock->socket);
      recv_sock->initted = false;
    }
  }

  return res;
}

void llrp_recv_netint_remove(const LlrpNetintId* netint, llrp_socket_t llrp_type)
{
  LlrpNetint* netint_info = (LlrpNetint*)etcpal_rbtree_find(&state.sys_netints, (void*)netint);
  if (netint_info && netint_info->recv_ref_count > 0)
  {
    if (--netint_info->send_ref_count == 0)
    {
      LlrpRecvSocket* recv_sock = get_llrp_recv_sock(llrp_type, netint->ip_type);
      unsubscribe_recv_socket(netint, llrp_type, recv_sock);
    }
  }
}

etcpal_error_t create_send_socket(const LlrpNetintId* netint, etcpal_socket_t* socket)
{
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  int sockopt_ip_level = (netint->ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP);

  etcpal_error_t res =
      etcpal_socket(netint->ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_DGRAM, &sock);

  if (res == kEtcPalErrOk)
  {
    // MULTICAST_TTL controls the TTL field in outgoing multicast datagrams.
    const int value = LLRP_MULTICAST_TTL_VAL;
    res = etcpal_setsockopt(sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_TTL, &value, sizeof value);
  }

  if (res == kEtcPalErrOk)
  {
    // MULTICAST_IF is critical for multicast sends to go over the correct interface.
    res = etcpal_setsockopt(sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_IF, &netint->index, sizeof netint->index);
  }

  if (res == kEtcPalErrOk)
  {
    *socket = sock;
  }
  else if (sock != ETCPAL_SOCKET_INVALID)
  {
    etcpal_close(sock);
  }

  return res;
}

etcpal_error_t create_recv_socket(llrp_socket_t llrp_type, etcpal_iptype_t ip_type, LlrpRecvSocket* sock_struct)
{
  etcpal_error_t res =
      etcpal_socket(ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_DGRAM, &sock_struct->socket);

  // Since we create separate sockets for IPv4 and IPv6, we don't want to receive IPv4 traffic on
  // the IPv6 socket.
  if (res == kEtcPalErrOk && ip_type == kEtcPalIpTypeV6)
  {
    const int value = 1;
    etcpal_setsockopt(sock_struct->socket, ETCPAL_IPPROTO_IPV6, ETCPAL_IPV6_V6ONLY, &value, sizeof value);
  }

  if (res == kEtcPalErrOk)
  {
    // SO_REUSEADDR allows multiple sockets to bind to LLRP_PORT, which is very important for our
    // multicast needs.
    const int value = 1;
    res = etcpal_setsockopt(sock_struct->socket, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEADDR, &value, sizeof value);
  }

  if (res == kEtcPalErrOk)
  {
    // We also set SO_REUSEPORT but don't check the return, because it is not applicable on all platforms
    const int value = 1;
    etcpal_setsockopt(sock_struct->socket, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEPORT, &value, sizeof value);

    EtcPalSockAddr bind_addr;
#if RDMNET_LLRP_BIND_TO_MCAST_ADDRESS
    // Bind socket to multicast address
    bind_addr.ip = get_llrp_mcast_addr(llrp_type, ip_type);
#else
    // Bind socket to the wildcard address
    etcpal_ip_set_wildcard(ip_type, &bind_addr.ip);
#endif
    bind_addr.port = LLRP_PORT;
    res = etcpal_bind(sock_struct->socket, &bind_addr);
  }

  if (res == kEtcPalErrOk)
  {
    sock_struct->poll_info.callback = llrp_socket_activity;
    sock_struct->poll_info.data.int_val = (int)llrp_type;
    res = rdmnet_core_add_polled_socket(sock_struct->socket, ETCPAL_POLL_IN, &sock_struct->poll_info);
  }

  if (res == kEtcPalErrOk)
  {
    sock_struct->initted = true;
  }
  else if (sock_struct->socket != ETCPAL_SOCKET_INVALID)
  {
    etcpal_close(sock_struct->socket);
  }

  return res;
}

etcpal_error_t subscribe_recv_socket(const LlrpNetintId* netint, llrp_socket_t llrp_type, LlrpRecvSocket* sock_struct)
{
  EtcPalGroupReq group_req;
  group_req.ifindex = netint->index;
  group_req.group = get_llrp_mcast_addr(llrp_type, netint->ip_type);

  return etcpal_setsockopt(sock_struct->socket,
                           netint->ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP,
                           ETCPAL_MCAST_JOIN_GROUP, (const void*)&group_req, sizeof(group_req));
}

etcpal_error_t unsubscribe_recv_socket(const LlrpNetintId* netint, llrp_socket_t llrp_type, LlrpRecvSocket* sock_struct)
{
  EtcPalGroupReq group_req;
  group_req.ifindex = netint->index;
  group_req.group = get_llrp_mcast_addr(llrp_type, netint->ip_type);

  return etcpal_setsockopt(sock_struct->socket,
                           netint->ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP,
                           ETCPAL_MCAST_LEAVE_GROUP, (const void*)&group_req, sizeof(group_req));
}

void destroy_recv_socket(LlrpRecvSocket* sock_struct)
{
  if (sock_struct->initted)
  {
    rdmnet_core_remove_polled_socket(sock_struct->socket);
    etcpal_close(sock_struct->socket);
    sock_struct->initted = false;
  }
}

void llrp_socket_activity(const EtcPalPollEvent* event, PolledSocketOpaqueData data)
{
  static uint8_t llrp_recv_buf[LLRP_MAX_MESSAGE_SIZE];

  if (event->events & ETCPAL_POLL_ERR)
  {
    llrp_socket_error(event->err);
  }
  else if (event->events & ETCPAL_POLL_IN)
  {
    EtcPalSockAddr from_addr;
    int recv_res = etcpal_recvfrom(event->socket, llrp_recv_buf, LLRP_MAX_MESSAGE_SIZE, 0, &from_addr);
    if (recv_res <= 0)
    {
      if (recv_res != kEtcPalErrMsgSize)
      {
        llrp_socket_error((etcpal_error_t)recv_res);
      }
    }
    else
    {
      LlrpNetintId netint_id;
      if (kEtcPalErrOk == etcpal_netint_get_interface_for_dest(&from_addr.ip, &netint_id.index))
      {
        netint_id.ip_type = from_addr.ip.type;

        if ((llrp_socket_t)data.int_val == kLlrpSocketTypeManager)
          manager_data_received(llrp_recv_buf, (size_t)recv_res, &netint_id);
        else
          target_data_received(llrp_recv_buf, (size_t)recv_res, &netint_id);
      }
      else if (etcpal_can_log(rdmnet_log_params, ETCPAL_LOG_WARNING))
      {
        char addr_str[ETCPAL_INET6_ADDRSTRLEN];
        etcpal_inet_ntop(&from_addr.ip, addr_str, ETCPAL_INET6_ADDRSTRLEN);

        etcpal_log(rdmnet_log_params, ETCPAL_LOG_WARNING,
                   RDMNET_LOG_MSG("Couldn't reply to LLRP message from %s:%u because no reply route could be found."),
                   addr_str, from_addr.port);
      }
    }
  }
}

void llrp_socket_error(etcpal_error_t err)
{
  etcpal_log(rdmnet_log_params, ETCPAL_LOG_WARNING, RDMNET_LOG_MSG("Error receiving on an LLRP socket: '%s'"),
             etcpal_strerror(err));
}

EtcPalIpAddr get_llrp_mcast_addr(llrp_socket_t llrp_type, etcpal_iptype_t ip_type)
{
  if (llrp_type == kLlrpSocketTypeManager)
  {
    if (ip_type == kEtcPalIpTypeV6)
      return kLlrpIpv6RespAddrInternal.ip;
    else
      return kLlrpIpv4RespAddrInternal.ip;
  }
  else
  {
    if (ip_type == kEtcPalIpTypeV6)
      return kLlrpIpv6RequestAddrInternal.ip;
    else
      return kLlrpIpv4RequestAddrInternal.ip;
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

int netint_cmp(const EtcPalRbTree* self, const EtcPalRbNode* node_a, const EtcPalRbNode* node_b)
{
  (void)self;
  const LlrpNetint* a = (const LlrpNetint*)node_a->value;
  const LlrpNetint* b = (const LlrpNetint*)node_b->value;
  if (a->id.ip_type == b->id.ip_type)
  {
    return (a->id.index > b->id.index) - (a->id.index < b->id.index);
  }
  else
  {
    return (a->id.ip_type == kEtcPalIpTypeV6 ? 1 : -1);
  }
}

EtcPalRbNode* llrp_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(llrp_rb_nodes);
#endif
}

void llrp_node_free(EtcPalRbNode* node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(llrp_rb_nodes, node);
#endif
}
