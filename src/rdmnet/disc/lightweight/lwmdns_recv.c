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

#include "lwmdns_recv.h"

#include "etcpal/common.h"
#include "etcpal/inet.h"
#include "etcpal/pack.h"
#include "etcpal/socket.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/mcast.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/disc/common.h"
#include "rdmnet/disc/monitored_scope.h"
#include "rdmnet/disc/discovered_broker.h"
#include "lwmdns_common.h"

/******************************************************************************
 * Private Macros
 *****************************************************************************/

#define MDNS_IP_FOR_TYPE(ip_type) (type == kEtcPalIpTypeV6 ? kMdnsIpv6Address.ip : kMdnsIpv4Address.ip)

/******************************************************************************
 * Private Types
 *****************************************************************************/

typedef struct MdnsRecvSocket
{
  etcpal_socket_t    socket;
  RCPolledSocketInfo poll_info;
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

static MdnsRecvSocket recv_sock_ipv4;
static MdnsRecvSocket recv_sock_ipv6;

#define MDNS_RECV_BUF_SIZE 1400
static uint8_t mdns_recv_buf[MDNS_RECV_BUF_SIZE];

/******************************************************************************
 * Private function prototypes
 *****************************************************************************/

static etcpal_error_t init_recv_socket(MdnsRecvSocket*           sock_struct,
                                       const EtcPalIpAddr*       mcast_group,
                                       const RdmnetNetintConfig* netint_config);
static etcpal_error_t setup_recv_netints(MdnsRecvSocket*            sock_struct,
                                         const EtcPalIpAddr*        mcast_group,
                                         const RdmnetMcastNetintId* mcast_netint_arr,
                                         size_t                     mcast_netint_arr_size,
                                         const RdmnetNetintConfig*  netint_config);
static void           deinit_recv_socket(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group);
static void           cleanup_recv_netints(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group);
static void           mdns_socket_activity(const EtcPalPollEvent* event, RCPolledSocketOpaqueData data);
static void           handle_mdns_message(int message_size);

static const uint8_t* bypass_mdns_query(const uint8_t* offset, int remaining_length);
static const uint8_t* handle_resource_record(const uint8_t* offset, int remaining_length);
static void           handle_ptr_record(const DnsResourceRecord* rr);
static void           handle_srv_record(const DnsResourceRecord* rr);
static void           handle_address_record(const DnsResourceRecord* rr);
static void           handle_txt_record(const DnsResourceRecord* rr);

static bool service_instance_name_matches(const RdmnetScopeMonitorRef* ref,
                                          const DiscoveredBroker*      db,
                                          const void*                  context);

/******************************************************************************
 * Function Definitions
 *****************************************************************************/

etcpal_error_t lwmdns_recv_module_init(const RdmnetNetintConfig* netint_config)
{
  etcpal_error_t v4_res = init_recv_socket(&recv_sock_ipv4, kMdnsIpv4Address, netint_config);
  etcpal_error_t v6_res = init_recv_socket(&recv_sock_ipv6, kMdnsIpv6Address, netint_config);

  if (v4_res == kEtcPalErrOk && v6_res == kEtcPalErrOk)
  {
    return kEtcPalErrOk;
  }
  else if (v4_res == kEtcPalErrOk && v6_res != kEtcPalErrOk)
  {
    RDMNET_LOG_INFO("mDNS operating with IPv4 only (IPv6 initialization failed with error '%s')",
                    etcpal_strerror(v6_res));
    return kEtcPalErrOk;
  }
  else if (v4_res != kEtcPalErrOk && v6_res == kEtcPalErrOk)
  {
    RDMNET_LOG_INFO("mDNS operating with IPv6 only (IPv4 initialization failed with error '%s')",
                    etcpal_strerror(v4_res));
    return kEtcPalErrOk;
  }
  else
  {
    // Neither v4 or v6 worked
    return v4_res;
  }
}

void lwmdns_recv_module_deinit(void)
{
  deinit_recv_socket(&recv_sock_ipv4, kMdnsIpv4Address);
  deinit_recv_socket(&recv_sock_ipv6, kMdnsIpv6Address);
}

etcpal_error_t init_recv_socket(MdnsRecvSocket*           sock_struct,
                                const EtcPalIpAddr*       mcast_group,
                                const RdmnetNetintConfig* netint_config)
{
  // Initialize the network interface array from the global multicast network interface array.
  const RdmnetMcastNetintId* mcast_array;
  size_t                     mcast_array_size = rc_mcast_get_netint_array(&mcast_array);

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

  etcpal_error_t res = rc_mcast_create_recv_socket(mcast_group, E133_MDNS_PORT, &sock_struct->socket);
  if (res != kEtcPalErrOk)
  {
    DEALLOC_NETINTS();
    return res;
  }

  sock_struct->poll_info.callback = mdns_socket_activity;
  sock_struct->poll_info.data.int_val = mcast_group->type;
  res = rc_add_polled_socket(sock_struct->socket, ETCPAL_POLL_IN, &sock_struct->poll_info);
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

  rc_remove_polled_socket(sock_struct->socket);
  etcpal_close(sock_struct->socket);
}

etcpal_error_t setup_recv_netints(MdnsRecvSocket*            sock_struct,
                                  const EtcPalIpAddr*        mcast_group,
                                  const RdmnetMcastNetintId* mcast_netint_arr,
                                  size_t                     mcast_netint_arr_size,
                                  const RdmnetNetintConfig*  netint_config)
{
  etcpal_error_t res = kEtcPalErrNoNetints;
  sock_struct->num_netints = 0;
  if (netint_config)
  {
    for (const RdmnetMcastNetintId* netint = netint_config->netints;
         netint < netint_config->netints + netint_config->num_netints; ++netint)
    {
      if (netint->ip_type == mcast_group->type)
      {
        res = rc_mcast_subscribe_recv_socket(sock_struct->socket, netint, mcast_group);
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
        res = rc_mcast_subscribe_recv_socket(sock_struct->socket, netint, mcast_group);
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
    rc_mcast_unsubscribe_recv_socket(sock_struct->socket, netint, mcast_group);
  }
#if RDMNET_DYNAMIC_MEM
  free(sock_struct->netints);
#endif
}

void mdns_socket_activity(const EtcPalPollEvent* event, RCPolledSocketOpaqueData data)
{
  ETCPAL_UNUSED_ARG(data);

  if (event->events & ETCPAL_POLL_ERR)
  {
    RDMNET_LOG_ERR("Error occurred on mDNS receive socket: '%s'", etcpal_strerror(event->err));
  }
  else if (event->events & ETCPAL_POLL_IN)
  {
    EtcPalSockAddr from_addr;
    int            recv_res = etcpal_recvfrom(event->socket, mdns_recv_buf, MDNS_RECV_BUF_SIZE, 0, &from_addr);
    if (recv_res > 0)
    {
      handle_mdns_message(recv_res);
    }
  }
}

void handle_mdns_message(int message_size)
{
  DnsHeader      header;
  const uint8_t* cur_ptr = lwmdns_parse_dns_header(mdns_recv_buf, message_size, &header);
  if (!cur_ptr)
    return;

  int remaining_message_size = message_size - (int)(cur_ptr - mdns_recv_buf);
  for (uint16_t i = 0; i < header.query_count; ++i)
  {
    if (remaining_message_size <= 0)
      break;

    const uint8_t* next_ptr = bypass_mdns_query(cur_ptr, remaining_message_size);
    if (cur_ptr)
      remaining_message_size -= (int)(next_ptr - cur_ptr);
    else
      return;
  }
}

const uint8_t* bypass_mdns_query(const uint8_t* offset, int remaining_length)
{
  DnsDomainName  name;
  const uint8_t* cur_ptr = lwmdns_parse_domain_name(mdns_recv_buf, offset, remaining_length, &name);
  if (cur_ptr)
  {
    remaining_length -= (int)(cur_ptr - offset);
    if (remaining_length >= 4)
      cur_ptr += 4;
    else
      cur_ptr = NULL;
  }
  return cur_ptr;
}

const uint8_t* handle_resource_record(const uint8_t* offset, int remaining_length)
{
  DnsResourceRecord rr;
  const uint8_t*    next_ptr = lwmdns_parse_resource_record(mdns_recv_buf, offset, remaining_length, &rr);
  if (next_ptr)
  {
    switch (rr.record_type)
    {
      case kDnsRecordTypePTR:
        handle_ptr_record(&rr);
        break;
      case kDnsRecordTypeSRV:
        handle_srv_record(&rr);
        break;
      case kDnsRecordTypeA:
      case kDnsRecordTypeAAAA:
        handle_address_record(&rr);
        break;
      case kDnsRecordTypeTXT:
        handle_txt_record(&rr);
        break;
      default:
        break;
    }
  }
  return next_ptr;
}

void handle_ptr_record(const DnsResourceRecord* rr)
{
  ETCPAL_UNUSED_ARG(rr);
}

void handle_srv_record(const DnsResourceRecord* rr)
{
  ETCPAL_UNUSED_ARG(rr);
}

void handle_address_record(const DnsResourceRecord* rr)
{
  ETCPAL_UNUSED_ARG(rr);
}

void handle_txt_record(const DnsResourceRecord* rr)
{
  ETCPAL_UNUSED_ARG(rr);
  /*
  RdmnetScopeMonitorRef* ref;
  DiscoveredBroker*      db;
  if (scope_monitor_and_discovered_broker_find(service_instance_name_matches, rr, &ref, &db))
  {
    bool data_changed = lwmdns_txt_record_to_broker_info(rr->data_ptr, rr->data_len, db);
  }
  */
}

bool service_instance_name_matches(const RdmnetScopeMonitorRef* ref, const DiscoveredBroker* db, const void* context)
{
  ETCPAL_UNUSED_ARG(ref);
  const DnsResourceRecord* rr = (const DnsResourceRecord*)context;
  return lwmdns_domain_name_matches_service(&rr->name, db->service_instance_name, E133_DNSSD_SRV_TYPE,
                                            E133_DEFAULT_DOMAIN);
}
