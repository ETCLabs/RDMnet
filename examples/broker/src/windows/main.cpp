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

#include <iostream>
#include <windows.h>
#include "broker_shell.h"
#include "win_socket_manager.h"
#include "win_broker_log.h"

void PrintHelp(wchar_t *app_name)
{
  std::cout << "Usage: " << app_name << " [OPTION]...\n";
  std::cout << "\n";
  std::cout << "Options:\n";
  std::cout << "  --scope=SCOPE         Configures the RDMnet Scope this Broker runs on to\n";
  std::cout << "                        SCOPE. By default, the default RDMnet scope is used.\n";
  std::cout << "  --ifaces=IFACE_LIST   Mutually exclusive with --macs. A comma-separated list\n";
  std::cout << "                        of local network interface IP addresses to use, e.g.\n";
  std::cout << "                        10.101.50.60. Note: using --macs is preferred; with\n";
  std::cout << "                        --ifaces, if an interface IP changes, the broker will\n";
  std::cout << "                        likely stop functioning. --ifaces is fine for quick\n";
  std::cout << "                        tests. By default, all available interfaces are used.\n";
  std::cout << "  --macs=MAC_LIST       Mutually exclusive with --ifaces. A comma-separated list\n";
  std::cout << "                        of local network interface mac addresses to use, e.g.\n";
  std::cout << "                        00:c0:16:11:da:b3. By default, all available interfaces\n";
  std::cout << "                        are used.\n";
  std::cout << "  --port=PORT           The port that this broker instance should use. By\n";
  std::cout << "                        default, an ephemeral port is used.\n";
  std::cout << "  --log-level=LOG_LEVEL Set the logging output level mask, using standard syslog\n";
  std::cout << "                        names from EMERG to DEBUG. Default is INFO.\n";
  std::cout << "  --help                Display this help and exit.\n";
  std::cout << "  --version             Output version information and exit.\n";
}

void ParseAndSetScope(const LPWSTR scope_str, BrokerShell &broker_shell)
{
  if (wcslen(scope_str) != 0)
  {
    char val_utf8[256];
    if (0 != WideCharToMultiByte(CP_UTF8, 0, scope_str, -1, val_utf8, 256, NULL, NULL))
    {
      broker_shell.SetInitialScope(val_utf8);
    }
  }
}

void ParseAndSetIfaceList(const LPWSTR iface_list_str, BrokerShell &broker_shell)
{
}

// Given a pointer to a string, parses out a mac addr
void ParseMac(WCHAR *s, BrokerShell::MacAddr &mac_buf)
{
  WCHAR *p = s;

  for (int index = 0; index < LWPA_NETINTINFO_MAC_LEN; ++index)
  {
    mac_buf[index] = (uint8_t)wcstol(p, &p, 16);
    ++p;  // P points at the :
  }
}

void ParseAndSetMacList(const LPWSTR mac_list_str, BrokerShell &broker_shell)
{
  std::vector<BrokerShell::MacAddr> macs;

  if (wcslen(mac_list_str) != 0)
  {
    WCHAR *context;
    for (WCHAR *p = wcstok_s(mac_list_str, L",", &context); p != NULL; p = wcstok_s(NULL, L",", &context))
    {
      BrokerShell::MacAddr mac_buf;
      ParseMac(p, mac_buf);
      macs.push_back(mac_buf);
    }
  }
}

void ParseAndSetPort(const LPWSTR port_str, BrokerShell &broker_shell)
{
  if (wcslen(port_str) != 0)
  {
    uint16_t port = static_cast<uint16_t>(_wtoi(port_str));
    broker_shell.SetInitialPort(port);
  }
}

void ParseAndSetLogLevel(const LPWSTR log_level_str, BrokerShell &broker_shell)
{
}

void ParseArgs(int argc, wchar_t *argv[], BrokerShell &broker_shell, bool &should_run, int &exit_code)
{
  if (argc > 1)
  {
    bool iface_key_encountered = false;

    for (int i = 1; i < argc; ++i)
    {
      if (_wcsnicmp(argv[i], L"--scope=", 8) == 0)
      {
        ParseAndSetScope(argv[i] + 8, broker_shell);
      }
      else if (_wcsnicmp(argv[i], L"--ifaces=", 9) == 0)
      {
        if (iface_key_encountered)
        {
          PrintHelp(argv[0]);
          should_run = false;
          exit_code = 1;
        }
        else
        {
          ParseAndSetIfaceList(argv[i] + 9, broker_shell);
          iface_key_encountered = true;
        }
      }
      else if (_wcsnicmp(argv[i], L"--macs=", 7) == 0)
      {
        if (iface_key_encountered)
        {
          PrintHelp(argv[0]);
          should_run = false;
          exit_code = 1;
        }
        else
        {
          ParseAndSetMacList(argv[i] + 9, broker_shell);
          iface_key_encountered = true;
        }
      }
      else if (_wcsnicmp(argv[i], L"--port=", 7) == 0)
      {
        ParseAndSetPort(argv[i] + 7, broker_shell);
      }
      else if (_wcsnicmp(argv[i], L"--log-level=", 12) == 0)
      {
        ParseAndSetLogLevel(argv[i] + 12, broker_shell);
      }
      else if (_wcsicmp(argv[i], L"--version") == 0)
      {
        BrokerShell::PrintVersion();
        should_run = false;
        break;
      }
      else
      {
        PrintHelp(argv[0]);
        should_run = false;
        exit_code = 1;
        break;
      }
    }
  }
}

// Windows console entry point for the example broker app.
// Parse command-line arguments and then run the platform-neutral Broker shell.
int wmain(int argc, wchar_t *argv[])
{
  BrokerShell broker_shell;
  bool should_run = true;
  int exit_code = 0;

  ParseArgs(argc, argv, broker_shell, should_run, exit_code);

  if (should_run)
  {
    // We want to detect network change as well
    OVERLAPPED net_overlap;
    memset(&net_overlap, 0, sizeof(OVERLAPPED));
    HANDLE net_handle = NULL;
    NotifyAddrChange(&net_handle, &net_overlap);

    WinBrokerSocketManager socket_mgr;
    WindowsBrokerLog log("RDMnetBroker.log");
    broker_shell.Run(&log, &socket_mgr);
  }

  return exit_code;
}
