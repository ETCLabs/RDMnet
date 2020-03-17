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

#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include "etcpal/socket.h"
#include "rdmnet/device.h"
#include "example_device.h"
#include "win_device_log.h"

void print_help(wchar_t* app_name)
{
  printf("Usage: %ls [OPTION]...\n\n", app_name);
  printf("  --scope=SCOPE     Configures the RDMnet Scope to SCOPE. Enter nothing after\n");
  printf("                    '=' to set the scope to the default.\n");
  printf("  --broker=IP:PORT  Connect to a Broker at address IP:PORT instead of\n");
  printf("                    performing discovery.\n");
  printf("  --help            Display this help and exit.\n");
  printf("  --version         Output version information and exit.\n");
}

bool set_scope(wchar_t* arg, char* scope_buf)
{
  if (WideCharToMultiByte(CP_UTF8, 0, arg, -1, scope_buf, E133_SCOPE_STRING_PADDED_LENGTH, NULL, NULL) > 0)
    return true;
  return false;
}

bool set_static_broker(wchar_t* arg, EtcPalSockAddr* static_broker_addr)
{
  wchar_t* sep = wcschr(arg, ':');
  if (sep != NULL && sep - arg < ETCPAL_INET6_ADDRSTRLEN)
  {
    wchar_t ip_str[ETCPAL_INET6_ADDRSTRLEN];
    ptrdiff_t ip_str_len = sep - arg;
    struct sockaddr_storage tst_addr;

    //    struct in_addr tst_addr;
    //    struct in6_addr tst_addr6;
    INT convert_res;

    wmemcpy(ip_str, arg, ip_str_len);
    ip_str[ip_str_len] = '\0';

    /* Try to convert the address in both IPv4 and IPv6 forms. */
    convert_res = InetPtonW(AF_INET, ip_str, &((struct sockaddr_in*)&tst_addr)->sin_addr);
    if (convert_res == 1)
    {
      tst_addr.ss_family = AF_INET;
      ip_os_to_etcpal((etcpal_os_ipaddr_t*)&tst_addr, &static_broker_addr->ip);
    }
    else
    {
      convert_res = InetPtonW(AF_INET6, ip_str, &((struct sockaddr_in6*)&tst_addr)->sin6_addr);
      if (convert_res == 1)
      {
        tst_addr.ss_family = AF_INET6;
        ip_os_to_etcpal((etcpal_os_ipaddr_t*)&tst_addr, &static_broker_addr->ip);
      }
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
  }

  return TRUE;
}

int wmain(int argc, wchar_t* argv[])
{
  etcpal_error_t res = kEtcPalErrOk;
  bool should_exit = false;
  RdmnetScopeConfig scope_config;
  const EtcPalLogParams* lparams;

  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  srand(counter.LowPart);

  RDMNET_CLIENT_SET_DEFAULT_SCOPE(&scope_config);

  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
    {
      if (_wcsnicmp(argv[i], L"--scope=", 8) == 0)
      {
        if (!set_scope(&argv[i][8], scope_config.scope))
        {
          print_help(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (_wcsnicmp(argv[i], L"--broker=", 9) == 0)
      {
        if (set_static_broker(&argv[i][9], &scope_config.static_broker_addr))
        {
          scope_config.has_static_broker_addr = true;
        }
        else
        {
          print_help(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (_wcsicmp(argv[i], L"--version") == 0)
      {
        device_print_version();
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

  /* Handle console signals */
  if (!SetConsoleCtrlHandler(console_handler, TRUE))
  {
    etcpal_log(lparams, ETCPAL_LOG_ERR, "Could not set console signal handler.");
    return 1;
  }

  /* Startup the device */
  res = device_init(&scope_config, lparams);
  if (res != kEtcPalErrOk)
  {
    etcpal_log(lparams, ETCPAL_LOG_ERR, "Device failed to initialize: '%s'", etcpal_strerror(res));
    return 1;
  }

  etcpal_log(lparams, ETCPAL_LOG_INFO, "Device initialized.");

  while (device_keep_running)
  {
    Sleep(100);
  }

  device_deinit();
  device_log_deinit();
  return 0;
}
