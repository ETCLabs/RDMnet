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

#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#include <rpc.h>
#include <stdbool.h>
#include <wchar.h>
#include <string.h>
#include "lwpa_pack.h"
#include "lwpa_cid.h"
#include "lwpa_socket.h"
#include "rdmnet/version.h"
#include "rdmnet/connection.h"
#include "device.h"
#include "devicelog.h"
#include "devicellrp.h"
#include "rdmnet/discovery.h"

/* DEBUG */
#include <stdio.h>

/****** mdns / dns-sd ********************************************************/

LwpaSockaddr mdns_broker_addr;

void broker_found(const char *scope, const BrokerDiscInfo *broker_info, void *context)
{
  (void)scope;
  (void)context;

  size_t ip_index;
  for (ip_index = 0; ip_index < broker_info->listen_addrs_count; ip_index++)
  {
    if (lwpaip_is_v4(&broker_info->listen_addrs[ip_index].ip))
    {
      mdns_broker_addr = broker_info->listen_addrs[ip_index];
      break;
    }
  }

  char broker_info_string[48];
  strcpy(broker_info_string, "Found Broker \"");
  strcat(broker_info_string, broker_info->service_name);
  strcat(broker_info_string, "\" ");

  const LwpaLogParams *lparams = device_get_log_params();
  lwpa_log(lparams, LWPA_LOG_INFO, broker_info_string);
}

void broker_lost(const char *service_name, void *context)
{
  (void)service_name;
  (void)context;
}

void scope_monitor_error(const ScopeMonitorInfo *scope_info, int platform_error, void *context)
{
  (void)scope_info;
  (void)platform_error;
  (void)context;
}

void broker_registered(const BrokerDiscInfo *broker_info, const char *assigned_service_name, void *context)
{
  (void)broker_info;
  (void)assigned_service_name;
  (void)context;
}

void broker_register_error(const BrokerDiscInfo *broker_info, int platform_error, void *context)
{
  (void)broker_info;
  (void)platform_error;
  (void)context;
}

void set_callback_functions(RdmnetDiscCallbacks *callbacks)
{
  callbacks->broker_found = &broker_found;
  callbacks->broker_lost = &broker_lost;
  callbacks->scope_monitor_error = &scope_monitor_error;
  callbacks->broker_registered = &broker_registered;
  callbacks->broker_register_error = &broker_register_error;
}

void mdns_dnssd_resolve_addr(RdmnetConnectParams *connect_params)
{
  int platform_specific_error;
  ScopeMonitorInfo scope_monitor_info;
  fill_default_scope_info(&scope_monitor_info);

  strncpy(scope_monitor_info.scope, connect_params->scope, E133_SCOPE_STRING_PADDED_LENGTH);
  strncpy(scope_monitor_info.domain, connect_params->search_domain, E133_DOMAIN_STRING_PADDED_LENGTH);

  rdmnetdisc_startmonitoring(&scope_monitor_info, &platform_specific_error, NULL);

  while (mdns_broker_addr.ip.type == LWPA_IP_INVALID)
  {
    rdmnetdisc_tick(NULL);
    Sleep(100);
  }
}
/****************************************************************************/

void try_connecting_until_connected(int broker_conn, LwpaSockaddr *broker_addr, const ClientConnectMsg *connect_msg,
                                    const LwpaLogParams *lparams)
{
  static RdmnetData connect_data;

  /* Attempt to connect. */
  lwpa_error_t res = rdmnet_connect(broker_conn, broker_addr, connect_msg, &connect_data);
  while (res != LWPA_OK)
  {
    if (lwpa_canlog(lparams, LWPA_LOG_WARNING))
    {
      char addr_str[LWPA_INET6_ADDRSTRLEN];
      lwpa_inet_ntop(&broker_addr->ip, addr_str, LWPA_INET6_ADDRSTRLEN);
      lwpa_log(lparams, LWPA_LOG_WARNING, "Connection to Broker at address %s:%d failed with error: '%s'. Retrying...",
               addr_str, broker_addr->port, lwpa_strerror(res));
    }
    res = rdmnet_connect(broker_conn, broker_addr, connect_msg, &connect_data);
  }

  /* If we were redirected, the data structure will tell us the new address. */
  if (rdmnet_data_is_addr(&connect_data))
    *broker_addr = *(rdmnet_data_addr(&connect_data));
}

void connect_to_broker(int conn, const LwpaCid *my_cid, const LwpaUid *my_uid, const LwpaLogParams *lparams)
{
  RdmnetConnectParams my_connect_params;
  ClientConnectMsg connect_msg;

  default_responder_get_e133_params(&my_connect_params);

  /* Fill in the information used in the initial connection handshake. */
  connect_msg.scope = my_connect_params.scope;
  connect_msg.search_domain = my_connect_params.search_domain;
  connect_msg.e133_version = E133_VERSION;
  connect_msg.connect_flags = 0;
  create_rpt_client_entry(my_cid, my_uid, kRPTClientTypeDevice, NULL, &connect_msg.client_entry);

  LwpaSockaddr broker_addr;
  /* If we have a static configuration, use it to connect to the Broker. */
  if (lwpaip_is_invalid(&my_connect_params.broker_static_addr.ip))
  {
    mdns_dnssd_resolve_addr(&my_connect_params);
    broker_addr = mdns_broker_addr;
  }
  else
  {
    broker_addr = my_connect_params.broker_static_addr;
  }

  try_connecting_until_connected(conn, &broker_addr, &connect_msg, lparams);
  default_responder_set_tcp_status(&broker_addr);
}

void print_help(wchar_t *app_name)
{
  printf("ETC Prototype RDMnet Device\n");
  printf("Version %s\n\n", RDMNET_VERSION_STRING);

  printf("Usage: %ls [--scope=SCOPE] [--broker=IPV4:PORT]\n", app_name);
  printf("   --scope=SCOPE: Configures the RDMnet Scope to SCOPE. Enter nothing\n");
  printf("                  after = to set the scope to the default.\n");
  printf("   --broker=IP:PORT: Connect to a Broker at address IP:PORT instead of\n");
  printf("                     performing discovery.\n");
}

bool set_scope(wchar_t *arg, char *scope_buf)
{
  if (WideCharToMultiByte(CP_UTF8, 0, arg, -1, scope_buf, E133_SCOPE_STRING_PADDED_LENGTH, NULL, NULL) > 0)
    return true;
  return false;
}

bool set_static_broker(wchar_t *arg, LwpaSockaddr *static_broker_addr)
{
  wchar_t *sep = wcschr(arg, ':');
  if (sep != NULL && sep - arg < LWPA_INET6_ADDRSTRLEN)
  {
    wchar_t ip_str[LWPA_INET6_ADDRSTRLEN];
    ptrdiff_t ip_str_len = sep - arg;
    struct in_addr tst_addr;
    struct in6_addr tst_addr6;
    INT convert_res;

    wmemcpy(ip_str, arg, ip_str_len);
    ip_str[ip_str_len] = '\0';

    /* Try to convert the address in both IPv4 and IPv6 forms. */
    convert_res = InetPtonW(AF_INET, ip_str, &tst_addr);
    if (convert_res == 1)
    {
      ip_plat_to_lwpa_v4(&static_broker_addr->ip, &tst_addr);
    }
    else
    {
      convert_res = InetPtonW(AF_INET6, ip_str, &tst_addr6);
      if (convert_res == 1)
        ip_plat_to_lwpa_v6(&static_broker_addr->ip, &tst_addr6);
    }
    if (convert_res == 1 && 1 == swscanf(sep + 1, L"%hu", &static_broker_addr->port))
      return true;
  }
  return false;
}

int wmain(int argc, wchar_t *argv[])
{
  lwpa_error_t res = LWPA_OK;
  LwpaCid my_cid;
  LwpaUid my_uid;
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  bool print_usage_and_exit = false;
  UUID uuid;
  RdmnetDiscCallbacks callbacks;
  DeviceSettings settings;
  const LwpaLogParams *lparams;

  lwpaip_set_invalid(&settings.static_broker_addr.ip);
  settings.scope = E133_DEFAULT_SCOPE;

  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
    {
      if (_wcsnicmp(argv[i], L"--scope=", 8) == 0)
      {
        print_usage_and_exit = !set_scope(&argv[i][8], scope);
        settings.scope = scope;
      }
      else if (_wcsnicmp(argv[i], L"--broker=", 9) == 0)
      {
        print_usage_and_exit = !set_static_broker(&argv[i][9], &settings.static_broker_addr);
      }
      else
      {
        print_usage_and_exit = true;
        break;
      }
    }
  }
  if (print_usage_and_exit)
  {
    print_help(argv[0]);
    return 1;
  }

  device_log_init("RDMnetDevice.log");
  lparams = device_get_log_params();
  lwpa_log(lparams, LWPA_LOG_INFO, "ETC Prototype RDMnet Device Version " RDMNET_VERSION_STRING);

  set_callback_functions(&callbacks);
  rdmnetdisc_init(&callbacks);

  /* Create the Device's CID */
  /* Normally we would use lwpa_cid's generate_cid() function to lock a CID to
   * the local MAC address. This conforms more closely to the CID requirements
   * in E1.17 (and by extension E1.33). But we want to be able to create many
   * ephemeral Devices on the same system. So we will just generate UUIDs on
   * the fly. */
  // generate_cid(&my_cid, "ETC Prototype RDMnet Device", macaddr, 1);
  UuidCreate(&uuid);
  memcpy(settings.cid.data, &uuid, CID_BYTES);

  settings.uid.manu = 0xe574;
  /* Slight hack - using the last 32 bits of the CID as the UID. */
  settings.uid.id = upack_32b(&settings.cid.data[12]);

  /* Initialize the RDMnet library */
  res = rdmnet_init(lparams);
  if (res != LWPA_OK)
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Couldn't initialize RDMnet library due to error: '%s'. Stopping.",
             lwpa_strerror(res));
  }

  /* Initialize the device settings */
  device_init(&settings);

  /* Initialize LLRP */
  device_llrp_init(&my_cid, &my_uid, lparams);

  /* Create a new connection handle */
  int broker_conn = -1;
  if (res == LWPA_OK)
  {
    broker_conn = rdmnet_new_connection(&my_cid);
    if (broker_conn < 0)
    {
      res = broker_conn;
      lwpa_log(lparams, LWPA_LOG_ERR, "Couldn't create a new RDMnet Connection due to error: '%s'. Stopping.",
               lwpa_strerror(res));
    }
  }

  /* Try to connect to a static broker. */
  if (res == LWPA_OK)
  {
    connect_to_broker(broker_conn, &my_cid, &my_uid, lparams);
    lwpa_log(lparams, LWPA_LOG_INFO, "Connected to Broker. Entering main run loop...");
    while (1)
    {
      static RdmnetData recv_data;

      res = rdmnet_recv(broker_conn, &recv_data);
      if (res == LWPA_OK)
      {
        bool reconnect_required = false;
        device_handle_message(broker_conn, rdmnet_data_msg(&recv_data), lparams, &reconnect_required);
        if (reconnect_required)
        {
          lwpa_log(lparams, LWPA_LOG_INFO,
                   "Device received configuration message that requires re-connection to Broker. Disconnecting...");
          /* Standard TODO, this needs a better reason */
          rdmnet_disconnect(broker_conn, true, E133_DISCONNECT_LLRP_RECONFIGURE);
          connect_to_broker(broker_conn, &my_cid, &my_uid, lparams);
          lwpa_log(lparams, LWPA_LOG_INFO, "Re-connected to Broker.");
        }
      }
      else if (res != LWPA_NODATA)
      {
        /* Disconnected from Broker. */
        lwpa_log(lparams, LWPA_LOG_INFO, "Disconnected from Broker with error: '%s'. Attempting to reconnect...",
                 lwpa_strerror(res));

        /* On an unhealthy TCP event, increment our internal counter. */
        if (res == LWPA_TIMEDOUT)
          default_responder_incr_unhealthy_count();

        /* Attempt to reconnect to the Broker using our most current connect parameters. */
        connect_to_broker(broker_conn, &my_cid, &my_uid, lparams);
        lwpa_log(lparams, LWPA_LOG_INFO, "Re-connected to Broker.");
      }
    }
  }

  device_deinit();
  rdmnet_deinit();
  device_log_deinit();
  return (res == LWPA_OK ? 0 : 1);
}
