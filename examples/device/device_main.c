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
#include "lwpa/pack.h"
#include "lwpa/uuid.h"
#include "lwpa/socket.h"
#include "rdmnet/version.h"
#include "device.h"
#include "device_log.h"
#include "device_llrp.h"

/* DEBUG */
#include <stdio.h>

void print_version()
{
  printf("ETC Prototype RDMnet Device\n");
  printf("Version %s\n\n", RDMNET_VERSION_STRING);
  printf("Copyright (c) 2018 ETC Inc.\n");
  printf("License: Apache License v2.0 <http://www.apache.org/licenses/LICENSE-2.0>\n");
  printf("Unless required by applicable law or agreed to in writing, this software is\n");
  printf("provided \"AS IS\", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express\n");
  printf("or implied.\n");
}

void print_help(wchar_t *app_name)
{
  printf("Usage: %ls [OPTION]...\n\n", app_name);
  printf("  --scope=SCOPE     Configures the RDMnet Scope to SCOPE. Enter nothing after\n");
  printf("                    '=' to set the scope to the default.\n");
  printf("  --broker=IP:PORT  Connect to a Broker at address IP:PORT instead of\n");
  printf("                    performing discovery.\n");
  printf("  --help            Display this help and exit.\n");
  printf("  --version         Output version information and exit.\n");
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

static bool device_keep_running = true;

BOOL WINAPI console_handler(DWORD signal)
{
  if (signal == CTRL_C_EVENT)
  {
    printf("Stopping Device...\n");
    device_keep_running = false;
    device_deinit();
  }

  return TRUE;
}

int wmain(int argc, wchar_t *argv[])
{
  lwpa_error_t res = LWPA_OK;
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  bool should_exit = false;
  UUID uuid;
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
        if (set_scope(&argv[i][8], scope))
        {
          settings.scope = scope;
        }
        else
        {
          print_help(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (_wcsnicmp(argv[i], L"--broker=", 9) == 0)
      {
        if (!set_static_broker(&argv[i][9], &settings.static_broker_addr))
        {
          print_help(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (_wcsicmp(argv[i], L"--version") == 0)
      {
        print_version();
        should_exit = true;
        break;
      }
      else
      {
        print_help(argv[0]);
        should_exit = true;
        break;
      }
    }
  }
  if (should_exit)
    return 1;

  device_log_init("RDMnetDevice.log");
  lparams = device_get_log_params();
  lwpa_log(lparams, LWPA_LOG_INFO, "ETC Prototype RDMnet Device Version " RDMNET_VERSION_STRING);

  /* Create the Device's CID */
  /* Normally we would use lwpa_cid's generate_cid() function to lock a CID to
   * the local MAC address. This conforms more closely to the CID requirements
   * in E1.17 (and by extension E1.33). But we want to be able to create many
   * ephemeral Devices on the same system. So we will just generate UUIDs on
   * the fly. */
  // generate_cid(&my_cid, "ETC Prototype RDMnet Device", macaddr, 1);
  UuidCreate(&uuid);
  memcpy(settings.cid.data, &uuid, UUID_BYTES);

  settings.uid.manu = 0xe574;
  /* Slight hack - using the last 32 bits of the CID as the UID. */
  settings.uid.id = upack_32b(&settings.cid.data[12]);

  /* Initialize LLRP */
  device_llrp_init(&settings.cid, &settings.uid, lparams);

  /* Handle console signals */
  if (!SetConsoleCtrlHandler(console_handler, TRUE))
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Could not set console signal handler.");
    return 1;
  }

  /* Startup the device */
  res = device_init(&settings, lparams);
  if (res != LWPA_OK)
  {
    lwpa_log(lparams, LWPA_LOG_ERR, "Device failed to initialize: '%s'", lwpa_strerror(res));
    return 1;
  }

  while (device_keep_running)
    device_run();

  device_llrp_deinit();
  device_log_deinit();
  return 0;
}
