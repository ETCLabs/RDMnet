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
#define DNS_TTL_TO_MS(ttl) (ttl * 1000)

/******************************************************************************
 * Private Types
 *****************************************************************************/

typedef struct MdnsRecvSocket
{
  etcpal_socket_t    socket;
  RCPolledSocketInfo poll_info;
#if RDMNET_DYNAMIC_MEM
  EtcPalMcastNetintId* netints;
#else
  EtcPalMcastNetintId netints[RDMNET_MAX_MCAST_NETINTS];
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

// Initialization and deinitialization
static etcpal_error_t init_recv_socket(MdnsRecvSocket*           sock_struct,
                                       const EtcPalIpAddr*       mcast_group,
                                       const RdmnetNetintConfig* netint_config);
static etcpal_error_t setup_recv_netints(MdnsRecvSocket*            sock_struct,
                                         const EtcPalIpAddr*        mcast_group,
                                         const EtcPalMcastNetintId* mcast_netint_arr,
                                         size_t                     mcast_netint_arr_size,
                                         const RdmnetNetintConfig*  netint_config);
static void           deinit_recv_socket(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group);
static void           cleanup_recv_netints(MdnsRecvSocket* sock_struct, const EtcPalIpAddr* mcast_group);

// Incoming message handling
static void           mdns_socket_activity(const EtcPalPollEvent* event, RCPolledSocketOpaqueData data);
static void           handle_mdns_message(int message_size);
static const uint8_t* bypass_mdns_query(const uint8_t* offset, int remaining_length);
static const uint8_t* handle_resource_record(const uint8_t* offset, int remaining_length);
static void           handle_ptr_record(const DnsResourceRecord* rr);
static void           handle_srv_record(const DnsResourceRecord* rr);
static void           handle_address_record(const DnsResourceRecord* rr);
static void           handle_txt_record(const DnsResourceRecord* rr);

// Predicates for use with find functions
static bool scope_monitor_matches_subtype(const RdmnetScopeMonitorRef* ref, const void* context);
static bool db_matches_service_instance(const DiscoveredBroker* db, const void* context);
static bool scope_and_db_matches_service_instance(const RdmnetScopeMonitorRef* ref,
                                                  const DiscoveredBroker*      db,
                                                  const void*                  context);
static bool scope_and_db_matches_hostname(const RdmnetScopeMonitorRef* ref,
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
  const EtcPalMcastNetintId* mcast_array;
  size_t                     mcast_array_size = rc_mcast_get_netint_array(&mcast_array);

  size_t num_netints_requested = (netint_config ? netint_config->num_netints : mcast_array_size);
#if RDMNET_DYNAMIC_MEM
#define DEALLOC_NETINTS() free(sock_struct->netints)
  sock_struct->netints = (EtcPalMcastNetintId*)calloc(num_netints_requested, sizeof(EtcPalMcastNetintId));
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
                                  const EtcPalMcastNetintId* mcast_netint_arr,
                                  size_t                     mcast_netint_arr_size,
                                  const RdmnetNetintConfig*  netint_config)
{
  etcpal_error_t res = kEtcPalErrNoNetints;
  sock_struct->num_netints = 0;
  if (netint_config)
  {
    for (const EtcPalMcastNetintId* netint = netint_config->netints;
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
    for (const EtcPalMcastNetintId* netint = mcast_netint_arr; netint < mcast_netint_arr + mcast_netint_arr_size;
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
  for (EtcPalMcastNetintId* netint = sock_struct->netints; netint < sock_struct->netints + sock_struct->num_netints;
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
    else if (recv_res < 0)
    {
      RDMNET_LOG_ERR("Error occurred when receiving on mDNS receive socket: '%s'", etcpal_strerror(recv_res));
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
    if (next_ptr)
    {
      remaining_message_size -= (int)(next_ptr - cur_ptr);
      cur_ptr = next_ptr;
    }
    else
    {
      return;
    }
  }

  if (RDMNET_DISC_LOCK())
  {
    for (uint16_t i = 0; i < (header.answer_count + header.authority_count + header.additional_count); ++i)
    {
      if (remaining_message_size <= 0)
        break;

      const uint8_t* next_ptr = handle_resource_record(cur_ptr, remaining_message_size);
      if (next_ptr)
      {
        remaining_message_size -= (int)(next_ptr - cur_ptr);
        cur_ptr = next_ptr;
      }
      else
      {
        break;
      }
    }
    RDMNET_DISC_UNLOCK();
  }
}

const uint8_t* bypass_mdns_query(const uint8_t* offset, int remaining_length)
{
  const uint8_t* cur_ptr = lwmdns_parse_domain_name(mdns_recv_buf, offset, remaining_length);
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
  if (lwmdns_parse_domain_name(mdns_recv_buf, rr->data_ptr, rr->data_len) != NULL)
  {
    RdmnetScopeMonitorRef* ref = scope_monitor_find(scope_monitor_matches_subtype, rr->name);
    if (ref)
    {
      DiscoveredBroker* db = discovered_broker_find(ref->broker_list, db_matches_service_instance, rr->data_ptr);
      if (db && !db->platform_data.destruction_pending)
      {
        // Another PTR record received for a broker we already knew about.
        if (rr->ttl == 0)
        {
          // This broker is going away
          db->platform_data.destruction_pending = true;
        }
        else
        {
          // Reset the TTL timer.
          etcpal_timer_start(&db->platform_data.ttl_timer, DNS_TTL_TO_MS(rr->ttl));
        }
      }
      else if (rr->ttl != 0)
      {
        db = discovered_broker_new(ref, "", "");
        if (db && lwmdns_domain_label_to_string(mdns_recv_buf, rr->data_ptr, db->service_instance_name))
        {
          discovered_broker_insert(&ref->broker_list, db);
          etcpal_timer_start(&db->platform_data.ttl_timer, DNS_TTL_TO_MS(rr->ttl));
        }
      }
    }
  }
}

void handle_srv_record(const DnsResourceRecord* rr)
{
  if (rr->data_len > 7 && (lwmdns_parse_domain_name(mdns_recv_buf, &rr->data_ptr[6], rr->data_len - 6) != NULL))
  {
    RdmnetScopeMonitorRef* ref;
    DiscoveredBroker*      db;
    if (scope_monitor_and_discovered_broker_find(scope_and_db_matches_service_instance, rr->name, &ref, &db) &&
        !db->platform_data.destruction_pending)
    {
      // uint16_t priority = etcpal_unpack_u16b(rr->data_ptr);
      // uint16_t weight = etcpal_unpack_u16b(&rr->data_ptr[2]);
      uint16_t port = etcpal_unpack_u16b(&rr->data_ptr[4]);
      if (!db->platform_data.srv_record_received ||
          (port != db->port ||
           !lwmdns_domain_names_equal(mdns_recv_buf, &rr->data_ptr[6], db->platform_data.wire_host_name,
                                      db->platform_data.wire_host_name)))
      {
        if (lwmdns_copy_domain_name(mdns_recv_buf, &rr->data_ptr[6], db->platform_data.wire_host_name) > 0)
        {
          if (db->platform_data.srv_record_received)
          {
            if (db->platform_data.initial_notification_sent)
              db->platform_data.update_pending = true;
          }
          db->port = port;
          db->platform_data.srv_record_received = true;
        }
      }
    }
  }
}

void handle_address_record(const DnsResourceRecord* rr)
{
  if ((rr->record_type == kDnsRecordTypeA && rr->data_len == 4) ||
      (rr->record_type == kDnsRecordTypeAAAA && rr->data_len == 16))
  {
    RdmnetScopeMonitorRef* ref;
    DiscoveredBroker*      db;
    if (scope_monitor_and_discovered_broker_find(scope_and_db_matches_hostname, rr->name, &ref, &db) &&
        !db->platform_data.destruction_pending)
    {
      if (rr->record_type == kDnsRecordTypeA)
      {
        uint32_t v4_addr = etcpal_unpack_u32b(rr->data_ptr);
        for (const EtcPalIpAddr* addr = db->listen_addr_array; addr < db->listen_addr_array + db->num_listen_addrs;
             ++addr)
        {
          if (ETCPAL_IP_IS_V4(addr) && ETCPAL_IP_V4_ADDRESS(addr) == v4_addr)
          {
            // Already know about this address.
            return;
          }
        }
        EtcPalIpAddr addr;
        ETCPAL_IP_SET_V4_ADDRESS(&addr, v4_addr);
        if (discovered_broker_add_listen_addr(db, &addr, 0))  // TODO - pass along interface instead of 0
        {
          if (db->platform_data.initial_notification_sent)
            db->platform_data.update_pending = true;
        }
      }
      else  // AAAA
      {
        for (const EtcPalIpAddr* addr = db->listen_addr_array; addr < db->listen_addr_array + db->num_listen_addrs;
             ++addr)
        {
          if (ETCPAL_IP_IS_V6(addr) && memcmp(ETCPAL_IP_V6_ADDRESS(addr), rr->data_ptr, ETCPAL_IPV6_BYTES) == 0)
          {
            // Already know about this address.
            return;
          }
        }
        EtcPalIpAddr addr;
        ETCPAL_IP_SET_V6_ADDRESS(&addr, rr->data_ptr);
        if (discovered_broker_add_listen_addr(db, &addr, 0))  // TODO - pass along interface instead of 0
        {
          if (db->platform_data.initial_notification_sent)
            db->platform_data.update_pending = true;
        }
      }
    }
  }
}

void handle_txt_record(const DnsResourceRecord* rr)
{
  RdmnetScopeMonitorRef* ref;
  DiscoveredBroker*      db;
  if (scope_monitor_and_discovered_broker_find(scope_and_db_matches_service_instance, rr->name, &ref, &db) &&
      !db->platform_data.destruction_pending)
  {
    txt_record_parse_result_t parse_result = lwmdns_txt_record_to_broker_info(rr->data_ptr, rr->data_len, db);
    if (parse_result != kTxtRecordParseError)
    {
      if (strcmp(db->scope, ref->scope) != 0)
      {
        db->platform_data.destruction_pending = true;
      }
      else
      {
        db->platform_data.txt_record_received = true;
        if (parse_result == kTxtRecordParseOkDataChanged && db->platform_data.initial_notification_sent)
          db->platform_data.update_pending = true;
      }
    }
  }
}

bool scope_monitor_matches_subtype(const RdmnetScopeMonitorRef* ref, const void* context)
{
  const uint8_t* name = (const uint8_t*)context;
  return lwmdns_domain_name_matches_service_subtype(mdns_recv_buf, name, ref->scope);
}

bool db_matches_service_instance(const DiscoveredBroker* db, const void* context)
{
  const uint8_t* name = (const uint8_t*)context;
  return lwmdns_domain_name_matches_service_instance(mdns_recv_buf, name, db->service_instance_name);
}

bool scope_and_db_matches_service_instance(const RdmnetScopeMonitorRef* ref,
                                           const DiscoveredBroker*      db,
                                           const void*                  context)
{
  ETCPAL_UNUSED_ARG(ref);
  const uint8_t* name = (const uint8_t*)context;
  return lwmdns_domain_name_matches_service_instance(mdns_recv_buf, name, db->service_instance_name);
}

bool scope_and_db_matches_hostname(const RdmnetScopeMonitorRef* ref, const DiscoveredBroker* db, const void* context)
{
  ETCPAL_UNUSED_ARG(ref);
  const uint8_t* name = (const uint8_t*)context;
  return lwmdns_domain_names_equal(mdns_recv_buf, name, db->platform_data.wire_host_name,
                                   db->platform_data.wire_host_name);
}
