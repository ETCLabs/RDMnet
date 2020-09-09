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

#include "rdmnet/core/mcast.h"

#include <assert.h>
#include <stdio.h>
#include "etcpal/netint.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif

/**************************** Private constants ******************************/

#define MULTICAST_TTL_VAL 20
#define MAX_SEND_NETINT_SOURCE_PORTS 2

/****************************** Private types ********************************/

typedef struct McastSendSocket
{
  etcpal_socket_t send_sock;
  uint16_t        source_port;
  size_t          ref_count;
} McastSendSocket;

typedef struct McastNetintInfo
{
  McastSendSocket send_sockets[MAX_SEND_NETINT_SOURCE_PORTS];
} McastNetintInfo;

/**************************** Private variables ******************************/

#if RDMNET_DYNAMIC_MEM
static EtcPalMcastNetintId* mcast_netint_arr;
static McastNetintInfo*     netint_info_arr;
#else
static EtcPalMcastNetintId mcast_netint_arr[RDMNET_MAX_MCAST_NETINTS];
static McastNetintInfo     netint_info_arr[RDMNET_MAX_MCAST_NETINTS];
#endif
static size_t        num_mcast_netints;
static EtcPalMacAddr lowest_mac;

/*********************** Private function prototypes *************************/

static bool validate_netint_config(const RdmnetNetintConfig* config);
static void test_mcast_netint(const EtcPalMcastNetintId* netint_id, const char* addr_str);
static void add_mcast_netint(const EtcPalMcastNetintId* netint_id, const char* addr_str);

static etcpal_error_t create_send_socket(const EtcPalMcastNetintId* netint_id,
                                         uint16_t                   source_port,
                                         etcpal_socket_t*           socket);

static int netint_id_index_in_array(const EtcPalMcastNetintId* id, const EtcPalMcastNetintId* array, size_t array_size);
static McastNetintInfo* get_mcast_netint_info(const EtcPalMcastNetintId* id);
static McastSendSocket* get_send_socket(McastNetintInfo* netint_info, uint16_t source_port);
static McastSendSocket* get_unused_send_socket(McastNetintInfo* netint_info);

/*************************** Function definitions ****************************/

etcpal_error_t rc_mcast_module_init(const RdmnetNetintConfig* netint_config)
{
  RDMNET_ASSERT(num_mcast_netints == 0);

  if (netint_config && !validate_netint_config(netint_config))
    return kEtcPalErrInvalid;

  size_t num_sys_netints = etcpal_netint_get_num_interfaces();
  if (num_sys_netints == 0)
    return kEtcPalErrNoNetints;

#if RDMNET_DYNAMIC_MEM
  size_t num_netints_requested = (netint_config ? netint_config->num_netints : num_sys_netints);
  mcast_netint_arr = calloc(num_netints_requested, sizeof(EtcPalMcastNetintId));
  if (!mcast_netint_arr)
    return kEtcPalErrNoMem;
  netint_info_arr = calloc(num_netints_requested, sizeof(McastNetintInfo));
  if (!netint_info_arr)
  {
    free(mcast_netint_arr);
    return kEtcPalErrNoMem;
  }
#endif

  // Initialize the lowest mac address on the system.
  memset(lowest_mac.data, 0xff, ETCPAL_MAC_BYTES);

  RDMNET_LOG_INFO("Initializing multicast network interfaces...");
  const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();
  for (const EtcPalNetintInfo* netint = netint_list; netint < netint_list + num_sys_netints; ++netint)
  {
    // Update the lowest MAC, if necessary.
    if (!ETCPAL_MAC_IS_NULL(&netint->mac))
    {
      if (netint == netint_list)
        lowest_mac = netint->mac;
      else if (ETCPAL_MAC_CMP(&netint->mac, &lowest_mac) < 0)
        lowest_mac = netint->mac;
    }

    // Get the interface IP address for logging
    char addr_str[ETCPAL_IP_STRING_BYTES];
    addr_str[0] = '\0';
    if (RDMNET_CAN_LOG(ETCPAL_LOG_INFO))
      etcpal_ip_to_string(&netint->addr, addr_str);

    // Create a test send and receive socket on each network interface. If either one fails, we
    // remove that interface from the final set.
    EtcPalMcastNetintId netint_id;
    netint_id.index = netint->index;
    netint_id.ip_type = netint->addr.type;

    if (netint_config &&
        (netint_id_index_in_array(&netint_id, netint_config->netints, netint_config->num_netints) == -1))
    {
      RDMNET_LOG_DEBUG("  Skipping network interface %s as it is not present in user configuration.", addr_str);
      continue;
    }

    test_mcast_netint(&netint_id, addr_str);
  }

  if (num_mcast_netints != 0)
  {
  }
  else
  {
    RDMNET_LOG_ERR("No usable multicast network interfaces found.");
    return kEtcPalErrNoNetints;
  }
  return kEtcPalErrOk;
}

void rc_mcast_module_deinit(void)
{
  for (McastNetintInfo* netint_info = netint_info_arr; netint_info < netint_info_arr + num_mcast_netints; ++netint_info)
  {
    for (McastSendSocket* send_socket = netint_info->send_sockets;
         send_socket < netint_info->send_sockets + MAX_SEND_NETINT_SOURCE_PORTS; ++send_socket)
    {
      if (send_socket->ref_count)
        etcpal_close(send_socket->send_sock);
    }
  }
#if RDMNET_DYNAMIC_MEM
  free(mcast_netint_arr);
  free(netint_info_arr);
#endif
  num_mcast_netints = 0;
}

size_t rc_mcast_get_netint_array(const EtcPalMcastNetintId** array)
{
  RDMNET_ASSERT(array);
  *array = mcast_netint_arr;
  return num_mcast_netints;
}

bool rc_mcast_netint_is_valid(const EtcPalMcastNetintId* id)
{
  return (netint_id_index_in_array(id, mcast_netint_arr, num_mcast_netints) != -1);
}

const EtcPalMacAddr* rc_mcast_get_lowest_mac_addr(void)
{
  return &lowest_mac;
}

etcpal_error_t rc_mcast_get_send_socket(const EtcPalMcastNetintId* id, uint16_t source_port, etcpal_socket_t* socket)
{
  McastNetintInfo* netint_info = get_mcast_netint_info(id);
  if (netint_info)
  {
    McastSendSocket* send_sock = get_send_socket(netint_info, source_port);
    if (send_sock)
    {
      RDMNET_ASSERT(send_sock->ref_count > 0);
      RDMNET_ASSERT(send_sock->send_sock != ETCPAL_SOCKET_INVALID);
      ++send_sock->ref_count;
      *socket = send_sock->send_sock;
      return kEtcPalErrOk;
    }
    else
    {
      send_sock = get_unused_send_socket(netint_info);
      if (send_sock)
      {
        RDMNET_ASSERT(send_sock->ref_count == 0);
        etcpal_error_t res = create_send_socket(id, source_port, &send_sock->send_sock);
        if (res == kEtcPalErrOk)
        {
          RDMNET_ASSERT(send_sock->send_sock != ETCPAL_SOCKET_INVALID);
          *socket = send_sock->send_sock;
          send_sock->source_port = source_port;
          ++send_sock->ref_count;
        }
        return res;
      }
      else
      {
        return kEtcPalErrNoMem;
      }
    }
  }
  return kEtcPalErrNotFound;
}

void rc_mcast_release_send_socket(const EtcPalMcastNetintId* id, uint16_t source_port)
{
  McastNetintInfo* netint_info = get_mcast_netint_info(id);
  if (netint_info)
  {
    McastSendSocket* send_sock = get_send_socket(netint_info, source_port);
    if (send_sock)
    {
      if (--send_sock->ref_count == 0)
      {
        etcpal_close(send_sock->send_sock);
        send_sock->send_sock = ETCPAL_SOCKET_INVALID;
      }
    }
  }
}

etcpal_error_t rc_mcast_create_recv_socket(const EtcPalIpAddr* group, uint16_t port, etcpal_socket_t* socket)
{
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  etcpal_error_t  res =
      etcpal_socket(ETCPAL_IP_IS_V6(group) ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_SOCK_DGRAM, &sock);

  // Since we create separate sockets for IPv4 and IPv6, we don't want to receive IPv4 traffic on
  // the IPv6 socket.
  if (res == kEtcPalErrOk && ETCPAL_IP_IS_V6(group))
  {
    const int value = 1;
    etcpal_setsockopt(sock, ETCPAL_IPPROTO_IPV6, ETCPAL_IPV6_V6ONLY, &value, sizeof value);
  }

  if (res == kEtcPalErrOk)
  {
    // SO_REUSEADDR allows multiple sockets to bind to LLRP_PORT, which is very important for our
    // multicast needs.
    const int value = 1;
    res = etcpal_setsockopt(sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEADDR, &value, sizeof value);
  }

  if (res == kEtcPalErrOk)
  {
    // We also set SO_REUSEPORT but don't check the return, because it is not applicable on all platforms
    const int value = 1;
    etcpal_setsockopt(sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEPORT, &value, sizeof value);

    EtcPalSockAddr bind_addr;
#if RDMNET_BIND_MCAST_SOCKETS_TO_MCAST_ADDRESS
    // Bind socket to multicast address
    bind_addr.ip = *group;
#else
    // Bind socket to the wildcard address
    etcpal_ip_set_wildcard(group->type, &bind_addr.ip);
#endif
    bind_addr.port = port;
    res = etcpal_bind(sock, &bind_addr);
  }

  if (res == kEtcPalErrOk)
    *socket = sock;
  else if (sock != ETCPAL_SOCKET_INVALID)
    etcpal_close(sock);

  return res;
}

etcpal_error_t rc_mcast_subscribe_recv_socket(etcpal_socket_t            socket,
                                              const EtcPalMcastNetintId* netint,
                                              const EtcPalIpAddr*        group)
{
  RDMNET_ASSERT(netint);
  RDMNET_ASSERT(group);
  RDMNET_ASSERT(netint->ip_type == group->type);

  EtcPalGroupReq group_req;
  group_req.ifindex = netint->index;
  group_req.group = *group;

  return etcpal_setsockopt(socket, netint->ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP,
                           ETCPAL_MCAST_JOIN_GROUP, (const void*)&group_req, sizeof(group_req));
}

etcpal_error_t rc_mcast_unsubscribe_recv_socket(etcpal_socket_t            socket,
                                                const EtcPalMcastNetintId* netint,
                                                const EtcPalIpAddr*        group)
{
  RDMNET_ASSERT(netint);
  RDMNET_ASSERT(group);
  RDMNET_ASSERT(netint->ip_type == group->type);

  EtcPalGroupReq group_req;
  group_req.ifindex = netint->index;
  group_req.group = *group;

  return etcpal_setsockopt(socket, netint->ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP,
                           ETCPAL_MCAST_LEAVE_GROUP, (const void*)&group_req, sizeof(group_req));
}

bool validate_netint_config(const RdmnetNetintConfig* config)
{
  if (!config->netints || !config->num_netints)
    return false;

  for (const EtcPalMcastNetintId* netint_id = config->netints; netint_id < config->netints + config->num_netints;
       ++netint_id)
  {
    if (netint_id->index == 0 || (netint_id->ip_type != kEtcPalIpTypeV4 && netint_id->ip_type != kEtcPalIpTypeV6))
      return false;
  }

  return true;
}

void test_mcast_netint(const EtcPalMcastNetintId* netint_id, const char* addr_str)
{
  // create_send_socket() also tests setting the relevant send socket options and the
  // MULTICAST_IF on the relevant interface.
  etcpal_socket_t test_socket;
  etcpal_error_t  test_res = create_send_socket(netint_id, 0, &test_socket);
  if (test_res == kEtcPalErrOk)
  {
    etcpal_close(test_socket);

    // Try creating and subscribing a multicast receive socket.
    // Test receive sockets using one of the LLRP multicast addresses.
    EtcPalIpAddr test_mcast_group;
    if (netint_id->ip_type == kEtcPalIpTypeV6)
    {
      uint8_t ipv6_val[ETCPAL_IPV6_BYTES] = {0xff, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85};
      ETCPAL_IP_SET_V6_ADDRESS(&test_mcast_group, ipv6_val);
    }
    else
    {
      ETCPAL_IP_SET_V4_ADDRESS(&test_mcast_group, 0xeffffa85);
    }
    test_res = rc_mcast_create_recv_socket(&test_mcast_group, LLRP_PORT, &test_socket);
    if (test_res == kEtcPalErrOk)
    {
      test_res = rc_mcast_subscribe_recv_socket(test_socket, netint_id, &test_mcast_group);
      etcpal_close(test_socket);
    }
  }

  if (test_res == kEtcPalErrOk)
  {
    add_mcast_netint(netint_id, addr_str);
  }
  else
  {
    RDMNET_LOG_WARNING(
        "  Error creating multicast test socket on network interface %s: '%s'. This network interface will not be "
        "used for multicast.",
        addr_str, etcpal_strerror(test_res));
  }
}

void add_mcast_netint(const EtcPalMcastNetintId* netint_id, const char* addr_str)
{
#if !RDMNET_DYNAMIC_MEM
  // We've reached the configured maximum, in which case we have already given the user a stern
  // warning.
  if (num_mcast_netints >= RDMNET_MAX_MCAST_NETINTS)
  {
    RDMNET_LOG_WARNING(
        "  WARNING: SKIPPING multicast network interface %s as the configured value %d for RDMNET_MAX_MCAST_NETINTS "
        "has been reached.",
        addr_str, RDMNET_MAX_MCAST_NETINTS);
    RDMNET_LOG_WARNING(
        "To fix this likely error, recompile RDMnet with a higher value for RDMNET_MAX_MCAST_NETINTS or with "
        "RDMNET_DYNAMIC_MEM defined to 1.");
    return;
  }
#endif

  if (!rc_mcast_netint_is_valid(netint_id))
  {
    mcast_netint_arr[num_mcast_netints] = *netint_id;

    McastNetintInfo* netint_info = &netint_info_arr[num_mcast_netints];
    for (McastSendSocket* send_socket = netint_info->send_sockets;
         send_socket < netint_info->send_sockets + MAX_SEND_NETINT_SOURCE_PORTS; ++send_socket)
    {
      send_socket->ref_count = 0;
      send_socket->send_sock = ETCPAL_SOCKET_INVALID;
    }
    num_mcast_netints++;
    RDMNET_LOG_DEBUG("  Set up multicast network interface %s for listening.", addr_str);
  }
  // Else already added - don't add it again
}

etcpal_error_t create_send_socket(const EtcPalMcastNetintId* netint, uint16_t source_port, etcpal_socket_t* socket)
{
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  int             sockopt_ip_level = (netint->ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP);

  etcpal_error_t res =
      etcpal_socket(netint->ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_SOCK_DGRAM, &sock);

  if (res == kEtcPalErrOk)
  {
    // MULTICAST_TTL controls the TTL field in outgoing multicast datagrams.
    const int value = MULTICAST_TTL_VAL;
    res = etcpal_setsockopt(sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_TTL, &value, sizeof value);
  }

  if (res == kEtcPalErrOk)
  {
    // MULTICAST_IF is critical for multicast sends to go over the correct interface.
    res = etcpal_setsockopt(sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_IF, &netint->index, sizeof netint->index);
  }

  if (res == kEtcPalErrOk && source_port != 0)
  {
    // SO_REUSEADDR allows multiple sockets to bind to a single source port, which is often
    // important for multicast.
    const int value = 1;
    res = etcpal_setsockopt(sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEADDR, &value, sizeof value);
  }

  if (res == kEtcPalErrOk && source_port != 0)
  {
    // We also set SO_REUSEPORT but don't check the return, because it is not applicable on all platforms
    const int value = 1;
    etcpal_setsockopt(sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEPORT, &value, sizeof value);

    EtcPalSockAddr bind_addr;
    etcpal_ip_set_wildcard(netint->ip_type, &bind_addr.ip);
    bind_addr.port = source_port;
    res = etcpal_bind(sock, &bind_addr);
  }

  if (res == kEtcPalErrOk)
    *socket = sock;
  else if (sock != ETCPAL_SOCKET_INVALID)
    etcpal_close(sock);

  return res;
}

int netint_id_index_in_array(const EtcPalMcastNetintId* id, const EtcPalMcastNetintId* array, size_t array_size)
{
  for (size_t i = 0; i < array_size; ++i)
  {
    if (array[i].index == id->index && array[i].ip_type == id->ip_type)
      return (int)i;
  }
  return -1;
}

McastNetintInfo* get_mcast_netint_info(const EtcPalMcastNetintId* id)
{
  RDMNET_ASSERT(id);

  int index = netint_id_index_in_array(id, mcast_netint_arr, num_mcast_netints);
  return (index >= 0 ? &netint_info_arr[index] : NULL);
}

McastSendSocket* get_send_socket(McastNetintInfo* netint_info, uint16_t source_port)
{
  for (McastSendSocket* send_socket = netint_info->send_sockets;
       send_socket < netint_info->send_sockets + MAX_SEND_NETINT_SOURCE_PORTS; ++send_socket)
  {
    if (send_socket->ref_count != 0 && send_socket->source_port == source_port)
      return send_socket;
  }
  return NULL;
}

McastSendSocket* get_unused_send_socket(McastNetintInfo* netint_info)
{
  for (McastSendSocket* send_socket = netint_info->send_sockets;
       send_socket < netint_info->send_sockets + MAX_SEND_NETINT_SOURCE_PORTS; ++send_socket)
  {
    if (send_socket->ref_count == 0)
      return send_socket;
  }
  return NULL;
}
