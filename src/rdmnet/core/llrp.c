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

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "lwpa/mempool.h"
#endif
#include "lwpa/netint.h"
#include "lwpa/rbtree.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/llrp_manager.h"
#include "rdmnet/private/llrp_target.h"
#include "rdmnet/private/opts.h"

/**************************** Global variables *******************************/

LwpaSockaddr kLlrpIpv4RespAddrInternal;
LwpaSockaddr kLlrpIpv6RespAddrInternal;
LwpaSockaddr kLlrpIpv4RequestAddrInternal;
LwpaSockaddr kLlrpIpv6RequestAddrInternal;

const LwpaSockaddr* kLlrpIpv4RespAddr = &kLlrpIpv4RespAddrInternal;
const LwpaSockaddr* kLlrpIpv6RespAddr = &kLlrpIpv6RespAddrInternal;
const LwpaSockaddr* kLlrpIpv4RequestAddr = &kLlrpIpv4RequestAddrInternal;
const LwpaSockaddr* kLlrpIpv6RequestAddr = &kLlrpIpv6RequestAddrInternal;

uint8_t kLlrpLowestHardwareAddr[6];

/**************************** Private constants ******************************/

#define LLRP_MAX_RB_NODES (RDMNET_LLRP_MAX_NETINTS_PER_TARGET)

/***************************** Private macros ********************************/

#if RDMNET_DYNAMIC_MEM
#define llrp_netint_alloc() malloc(sizeof(LlrpNetint))
#define llrp_netint_dealloc(ptr) free(ptr)
#else
#define llrp_netint_alloc() lwpa_mempool_alloc(llrp_netints)
#define llrp_netint_dealloc(ptr) lwpa_mempool_free(llrp_netints, ptr)
#endif

/****************************** Private types ********************************/

typedef struct LlrpRecvSocket
{
  bool initted;
  lwpa_socket_t socket;
  PolledSocketInfo poll_info;
} LlrpRecvSocket;

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(llrp_netints, LlrpNetint, RDMNET_LLRP_MAX_NETINTS_PER_TARGET);
LWPA_MEMPOOL_DEFINE(llrp_rb_nodes, LwpaRbNode, LLRP_MAX_RB_NODES);
#endif

static struct LlrpState
{
  LlrpRecvSocket manager_recvsock_ipv4;
  LlrpRecvSocket manager_recvsock_ipv6;
  LlrpRecvSocket target_recvsock_ipv4;
  LlrpRecvSocket target_recvsock_ipv6;

  LwpaRbTree sys_netints;
} state;

/*********************** Private function prototypes *************************/

static lwpa_error_t init_sys_netints();
static lwpa_error_t init_sys_netint(const LlrpNetintId* id);
static void deinit_sys_netints();
static void deinit_sys_netint(const LwpaRbTree* self, LwpaRbNode* node);

static LwpaIpAddr get_llrp_mcast_addr(llrp_socket_t llrp_type, lwpa_iptype_t ip_type);
static LlrpRecvSocket* get_llrp_recv_sock(llrp_socket_t llrp_type, lwpa_iptype_t ip_type);
static lwpa_error_t create_send_socket(const LlrpNetintId* netint, lwpa_socket_t* socket);
static lwpa_error_t create_recv_socket(llrp_socket_t llrp_type, lwpa_iptype_t ip_type, LlrpRecvSocket* sock_struct);
static lwpa_error_t subscribe_recv_socket(const LlrpNetintId* netint, llrp_socket_t llrp_type,
                                          LlrpRecvSocket* sock_struct);
static lwpa_error_t unsubscribe_recv_socket(const LlrpNetintId* netint, llrp_socket_t llrp_type,
                                            LlrpRecvSocket* sock_struct);
static void destroy_recv_socket(LlrpRecvSocket* sock_struct);

static void llrp_socket_activity(const LwpaPollEvent* event, PolledSocketOpaqueData data);
static void llrp_socket_error(lwpa_error_t err);

static int netint_cmp(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b);
static LwpaRbNode* llrp_node_alloc();
static void llrp_node_free(LwpaRbNode* node);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_llrp_init()
{
  bool target_initted = false;
  bool netints_initted = false;
#if RDMNET_DYNAMIC_MEM
  bool manager_initted = false;
#endif

  lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_RESPONSE, &kLlrpIpv4RespAddrInternal.ip);
  kLlrpIpv4RespAddrInternal.port = LLRP_PORT;
  lwpa_inet_pton(kLwpaIpTypeV6, LLRP_MULTICAST_IPV6_ADDRESS_RESPONSE, &kLlrpIpv6RespAddrInternal.ip);
  kLlrpIpv6RespAddrInternal.port = LLRP_PORT;
  lwpa_inet_pton(kLwpaIpTypeV4, LLRP_MULTICAST_IPV4_ADDRESS_REQUEST, &kLlrpIpv4RequestAddrInternal.ip);
  kLlrpIpv4RequestAddrInternal.port = LLRP_PORT;
  lwpa_inet_pton(kLwpaIpTypeV6, LLRP_MULTICAST_IPV6_ADDRESS_REQUEST, &kLlrpIpv6RequestAddrInternal.ip);
  kLlrpIpv6RequestAddrInternal.port = LLRP_PORT;
  llrp_prot_init();
  lwpa_rbtree_init(&state.sys_netints, netint_cmp, llrp_node_alloc, llrp_node_free);

  lwpa_error_t res;
  netints_initted = ((res = init_sys_netints()) == kLwpaErrOk);

#if RDMNET_DYNAMIC_MEM
  if (res == kLwpaErrOk)
    manager_initted = ((res = rdmnet_llrp_manager_init()) == kLwpaErrOk);
#endif

  if (res == kLwpaErrOk)
    target_initted = ((res = rdmnet_llrp_target_init()) == kLwpaErrOk);

  if (res != kLwpaErrOk)
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

lwpa_error_t init_sys_netints()
{
  size_t num_sys_netints = lwpa_netint_get_num_interfaces();
  if (num_sys_netints == 0)
    return kLwpaErrNoNetints;

  const LwpaNetintInfo* netint_list = lwpa_netint_get_interfaces();

  uint8_t null_mac[6];
  memset(null_mac, 0, sizeof null_mac);
  memset(kLlrpLowestHardwareAddr, 0xff, sizeof kLlrpLowestHardwareAddr);

  lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, RDMNET_LOG_MSG("Initializing network interfaces for LLRP..."));
  for (const LwpaNetintInfo* netint = netint_list; netint < netint_list + num_sys_netints; ++netint)
  {
    char addr_str[LWPA_INET6_ADDRSTRLEN];
    addr_str[0] = '\0';
    if (LWPA_CAN_LOG(rdmnet_log_params, LWPA_LOG_WARNING))
    {
      lwpa_inet_ntop(&netint->addr, addr_str, LWPA_INET6_ADDRSTRLEN);
    }

    // Create a test send and receive socket on each network interface. If the either one fails, we
    // remove that interface from the set that LLRP targets listen on.
    LlrpNetintId netint_id;
    netint_id.index = netint->index;
    netint_id.ip_type = netint->addr.type;

    lwpa_socket_t test_socket;
    lwpa_error_t test_res = create_send_socket(&netint_id, &test_socket);
    if (test_res == kLwpaErrOk)
    {
      lwpa_close(test_socket);
      test_res = init_sys_netint(&netint_id);
    }

    if (test_res == kLwpaErrOk)
    {
      lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, RDMNET_LOG_MSG("  Set up LLRP network interface %s for listening."),
               addr_str);

      // Modify the lowest hardware address, if necessary
      if (memcmp(netint->mac, null_mac, 6) != 0)
      {
        if (netint == netint_list)
        {
          memcpy(kLlrpLowestHardwareAddr, netint->mac, 6);
        }
        else
        {
          if (memcmp(netint->mac, kLlrpLowestHardwareAddr, 6) < 0)
            memcpy(kLlrpLowestHardwareAddr, netint->mac, 6);
        }
      }
    }
    else
    {
      lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
               RDMNET_LOG_MSG("  Error creating test socket on LLRP network interface %s: '%s'. Skipping!"), addr_str,
               lwpa_strerror(test_res));
    }
  }

  if (lwpa_rbtree_size(&state.sys_netints) == 0)
  {
    lwpa_log(rdmnet_log_params, LWPA_LOG_ERR, RDMNET_LOG_MSG("No usable LLRP interfaces found."));
    return kLwpaErrNoNetints;
  }
  return kLwpaErrOk;
}

lwpa_error_t init_sys_netint(const LlrpNetintId* id)
{
  lwpa_error_t res = kLwpaErrNoMem;
  LlrpNetint* new_netint = (LlrpNetint*)llrp_netint_alloc();
  if (new_netint)
  {
    // Member initialization
    new_netint->id = *id;
    new_netint->recv_ref_count = 0;
    new_netint->send_ref_count = 0;
    new_netint->send_sock = LWPA_SOCKET_INVALID;

    res = lwpa_rbtree_insert(&state.sys_netints, new_netint);
    if (res == kLwpaErrOk || res == kLwpaErrExists)
    {
      // We still indicate success when we find a duplicate - effectively making one entry
      // for any multi-homed interfaces while still logging meaningful messages.
      if (res == kLwpaErrExists)
      {
        // But make sure it is deallocated in this case to avoid leaks.
        llrp_netint_dealloc(new_netint);
        res = kLwpaErrOk;
      }
    }
  }
  return res;
}

void deinit_sys_netints()
{
  lwpa_rbtree_clear_with_cb(&state.sys_netints, deinit_sys_netint);
}

void deinit_sys_netint(const LwpaRbTree* self, LwpaRbNode* node)
{
  (void)self;

  LlrpNetint* netint = (LlrpNetint*)node->value;
  if (netint->send_ref_count)
    lwpa_close(netint->send_sock);
  llrp_netint_dealloc(netint);
  llrp_node_free(node);
}

void get_llrp_netint_list(LwpaRbIter* list_iter)
{
  lwpa_rbiter_init(list_iter);
  lwpa_rbiter_first(list_iter, &state.sys_netints);
}

lwpa_error_t get_llrp_send_socket(const LlrpNetintId* netint, lwpa_socket_t* socket)
{
  lwpa_error_t res = kLwpaErrNotFound;

  LlrpNetint* netint_info = (LlrpNetint*)lwpa_rbtree_find(&state.sys_netints, (void*)netint);
  if (netint_info)
  {
    res = kLwpaErrOk;
    if (netint_info->send_ref_count == 0)
    {
      res = create_send_socket(netint, &netint_info->send_sock);
    }

    if (res == kLwpaErrOk)
    {
      ++netint_info->send_ref_count;
      *socket = netint_info->send_sock;
    }
  }

  return res;
}

void release_llrp_send_socket(const LlrpNetintId* netint)
{
  LlrpNetint* netint_info = (LlrpNetint*)lwpa_rbtree_find(&state.sys_netints, (void*)netint);
  if (netint_info && netint_info->send_ref_count > 0)
  {
    if (--netint_info->send_ref_count == 0)
    {
      lwpa_close(netint_info->send_sock);
      netint_info->send_sock = LWPA_SOCKET_INVALID;
    }
  }
}

lwpa_error_t llrp_recv_netint_add(const LlrpNetintId* netint, llrp_socket_t llrp_type)
{
  lwpa_error_t res = kLwpaErrNotFound;

  LlrpNetint* netint_info = (LlrpNetint*)lwpa_rbtree_find(&state.sys_netints, (void*)netint);
  if (netint_info)
  {
    res = kLwpaErrOk;
    LlrpRecvSocket* recv_sock = get_llrp_recv_sock(llrp_type, netint->ip_type);
    bool sock_created = false;

    if (!recv_sock->initted)
    {
      sock_created = ((res = create_recv_socket(llrp_type, netint->ip_type, recv_sock)) == kLwpaErrOk);
    }

    if (res == kLwpaErrOk)
    {
      res = subscribe_recv_socket(netint, llrp_type, recv_sock);
    }

    if (res == kLwpaErrOk)
    {
      ++netint_info->recv_ref_count;
    }
    else if (sock_created)
    {
      lwpa_close(recv_sock->socket);
      recv_sock->initted = false;
    }
  }

  return res;
}

void llrp_recv_netint_remove(const LlrpNetintId* netint, llrp_socket_t llrp_type)
{
  LlrpNetint* netint_info = (LlrpNetint*)lwpa_rbtree_find(&state.sys_netints, (void*)netint);
  if (netint_info && netint_info->recv_ref_count > 0)
  {
    if (--netint_info->send_ref_count == 0)
    {
      LlrpRecvSocket* recv_sock = get_llrp_recv_sock(llrp_type, netint->ip_type);
      unsubscribe_recv_socket(netint, llrp_type, recv_sock);
    }
  }
}

lwpa_error_t create_send_socket(const LlrpNetintId* netint, lwpa_socket_t* socket)
{
  lwpa_socket_t sock = LWPA_SOCKET_INVALID;
  int sockopt_ip_level = (netint->ip_type == kLwpaIpTypeV6 ? LWPA_IPPROTO_IPV6 : LWPA_IPPROTO_IP);

  lwpa_error_t res = lwpa_socket(netint->ip_type == kLwpaIpTypeV6 ? LWPA_AF_INET6 : LWPA_AF_INET, LWPA_DGRAM, &sock);

  if (res == kLwpaErrOk)
  {
    // MULTICAST_TTL controls the TTL field in outgoing multicast datagrams.
    const int value = LLRP_MULTICAST_TTL_VAL;
    res = lwpa_setsockopt(sock, sockopt_ip_level, LWPA_IP_MULTICAST_TTL, &value, sizeof value);
  }

  if (res == kLwpaErrOk)
  {
    // MULTICAST_IF is critical for multicast sends to go over the correct interface.
    res = lwpa_setsockopt(sock, sockopt_ip_level, LWPA_IP_MULTICAST_IF, &netint->index, sizeof netint->index);
  }

  if (res == kLwpaErrOk)
  {
    *socket = sock;
  }
  else if (sock != LWPA_SOCKET_INVALID)
  {
    lwpa_close(sock);
  }

  return res;
}

lwpa_error_t create_recv_socket(llrp_socket_t llrp_type, lwpa_iptype_t ip_type, LlrpRecvSocket* sock_struct)
{
  lwpa_error_t res =
      lwpa_socket(ip_type == kLwpaIpTypeV6 ? LWPA_AF_INET6 : LWPA_AF_INET, LWPA_DGRAM, &sock_struct->socket);

  // Since we create separate sockets for IPv4 and IPv6, we don't want to receive IPv4 traffic on
  // the IPv6 socket.
  if (res == kLwpaErrOk && ip_type == kLwpaIpTypeV6)
  {
    const int value = 1;
    lwpa_setsockopt(sock_struct->socket, LWPA_IPPROTO_IPV6, LWPA_IPV6_V6ONLY, &value, sizeof value);
  }

  if (res == kLwpaErrOk)
  {
    // SO_REUSEADDR allows multiple sockets to bind to LLRP_PORT, which is very important for our
    // multicast needs.
    const int value = 1;
    res = lwpa_setsockopt(sock_struct->socket, LWPA_SOL_SOCKET, LWPA_SO_REUSEADDR, &value, sizeof value);
  }

  if (res == kLwpaErrOk)
  {
    // We also set SO_REUSEPORT but don't check the return, because it is not applicable on all platforms
    const int value = 1;
    lwpa_setsockopt(sock_struct->socket, LWPA_SOL_SOCKET, LWPA_SO_REUSEPORT, &value, sizeof value);

    LwpaSockaddr bind_addr;
#if RDMNET_LLRP_BIND_TO_MCAST_ADDRESS
    // Bind socket to multicast address
    bind_addr.ip = get_llrp_mcast_addr(llrp_type, ip_type);
#else
    // Bind socket to the wildcard address
    lwpa_ip_set_wildcard(ip_type, &bind_addr.ip);
#endif
    bind_addr.port = LLRP_PORT;
    res = lwpa_bind(sock_struct->socket, &bind_addr);
  }

  if (res == kLwpaErrOk)
  {
    sock_struct->poll_info.callback = llrp_socket_activity;
    sock_struct->poll_info.data.int_val = (int)llrp_type;
    res = rdmnet_core_add_polled_socket(sock_struct->socket, LWPA_POLL_IN, &sock_struct->poll_info);
  }

  if (res == kLwpaErrOk)
  {
    sock_struct->initted = true;
  }
  else if (sock_struct->socket != LWPA_SOCKET_INVALID)
  {
    lwpa_close(sock_struct->socket);
  }

  return res;
}

lwpa_error_t subscribe_recv_socket(const LlrpNetintId* netint, llrp_socket_t llrp_type, LlrpRecvSocket* sock_struct)
{
  LwpaGroupReq group_req;
  group_req.ifindex = netint->index;
  group_req.group = get_llrp_mcast_addr(llrp_type, netint->ip_type);

  return lwpa_setsockopt(sock_struct->socket, netint->ip_type == kLwpaIpTypeV6 ? LWPA_IPPROTO_IPV6 : LWPA_IPPROTO_IP,
                         LWPA_MCAST_JOIN_GROUP, (const void*)&group_req, sizeof(group_req));
}

lwpa_error_t unsubscribe_recv_socket(const LlrpNetintId* netint, llrp_socket_t llrp_type, LlrpRecvSocket* sock_struct)
{
  LwpaGroupReq group_req;
  group_req.ifindex = netint->index;
  group_req.group = get_llrp_mcast_addr(llrp_type, netint->ip_type);

  return lwpa_setsockopt(sock_struct->socket, netint->ip_type == kLwpaIpTypeV6 ? LWPA_IPPROTO_IPV6 : LWPA_IPPROTO_IP,
                         LWPA_MCAST_LEAVE_GROUP, (const void*)&group_req, sizeof(group_req));
}

void destroy_recv_socket(LlrpRecvSocket* sock_struct)
{
  if (sock_struct->initted)
  {
    rdmnet_core_remove_polled_socket(sock_struct->socket);
    lwpa_close(sock_struct->socket);
    sock_struct->initted = false;
  }
}

void llrp_socket_activity(const LwpaPollEvent* event, PolledSocketOpaqueData data)
{
  static uint8_t llrp_recv_buf[LLRP_MAX_MESSAGE_SIZE];

  if (event->events & LWPA_POLL_ERR)
  {
    llrp_socket_error(event->err);
  }
  else if (event->events & LWPA_POLL_IN)
  {
    LwpaSockaddr from_addr;
    int recv_res = lwpa_recvfrom(event->socket, llrp_recv_buf, LLRP_MAX_MESSAGE_SIZE, 0, &from_addr);
    if (recv_res <= 0)
    {
      if (recv_res != kLwpaErrMsgSize)
      {
        llrp_socket_error(recv_res);
      }
    }
    else
    {
      LwpaNetintInfo reply_netint;
      if (kLwpaErrOk == lwpa_netint_get_interface_for_dest(&from_addr.ip, &reply_netint))
      {
        LlrpNetintId netint_id;
        netint_id.index = reply_netint.index;
        netint_id.ip_type = reply_netint.addr.type;

        if ((llrp_socket_t)data.int_val == kLlrpSocketTypeManager)
          manager_data_received(llrp_recv_buf, (size_t)recv_res, &netint_id);
        else
          target_data_received(llrp_recv_buf, (size_t)recv_res, &netint_id);
      }
      else if (LWPA_CAN_LOG(rdmnet_log_params, LWPA_LOG_WARNING))
      {
        char addr_str[LWPA_INET6_ADDRSTRLEN];
        lwpa_inet_ntop(&from_addr.ip, addr_str, LWPA_INET6_ADDRSTRLEN);

        lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING,
                 RDMNET_LOG_MSG("Couldn't reply to LLRP message from %s:%u because no reply route could be found."),
                 addr_str, from_addr.port);
      }
    }
  }
}

void llrp_socket_error(lwpa_error_t err)
{
  lwpa_log(rdmnet_log_params, LWPA_LOG_WARNING, RDMNET_LOG_MSG("Error receiving on an LLRP socket: '%s'"),
           lwpa_strerror(err));
}

LwpaIpAddr get_llrp_mcast_addr(llrp_socket_t llrp_type, lwpa_iptype_t ip_type)
{
  if (llrp_type == kLlrpSocketTypeManager)
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

LlrpRecvSocket* get_llrp_recv_sock(llrp_socket_t llrp_type, lwpa_iptype_t ip_type)
{
  if (llrp_type == kLlrpSocketTypeManager)
  {
    if (ip_type == kLwpaIpTypeV6)
      return &state.manager_recvsock_ipv6;
    else
      return &state.manager_recvsock_ipv4;
  }
  else
  {
    if (ip_type == kLwpaIpTypeV6)
      return &state.target_recvsock_ipv6;
    else
      return &state.target_recvsock_ipv4;
  }
}

int netint_cmp(const LwpaRbTree* self, const LwpaRbNode* node_a, const LwpaRbNode* node_b)
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
    return (a->id.ip_type == kLwpaIpTypeV6);
  }
}

LwpaRbNode* llrp_node_alloc()
{
#if RDMNET_DYNAMIC_MEM
  return (LwpaRbNode*)malloc(sizeof(LwpaRbNode));
#else
  return lwpa_mempool_alloc(llrp_rb_nodes);
#endif
}

void llrp_node_free(LwpaRbNode* node)
{
#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  lwpa_mempool_free(llrp_rb_nodes, node);
#endif
}
