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

// main.cpp: The Windows console entry point for the Broker example application.

#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#include <cassert>
#include <iostream>
#include <map>
#include "broker_shell.h"
#include "win_broker_log.h"

// Print the command-line usage details.
void PrintHelp(wchar_t* app_name)
{
  std::wcout << L"Usage: " << app_name << L" [OPTION]...\n";
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

// Parse the --scope=SCOPE command line option and transfer it to the BrokerShell instance.
bool ParseAndSetScope(const LPWSTR scope_str, BrokerShell& broker_shell)
{
  if (wcslen(scope_str) != 0)
  {
    char val_utf8[256];
    if (0 != WideCharToMultiByte(CP_UTF8, 0, scope_str, -1, val_utf8, 256, NULL, NULL))
    {
      broker_shell.SetInitialScope(val_utf8);
      return true;
    }
  }
  return false;
}

// Parse the --ifaces=IFACE_LIST command line option and transfer it to the BrokerShell instance.
bool ParseAndSetIfaceList(const LPWSTR iface_list_str, BrokerShell& broker_shell)
{
  std::set<etcpal::IpAddr> addrs;

  if (wcslen(iface_list_str) != 0)
  {
    WCHAR* context;
    for (WCHAR* p = wcstok_s(iface_list_str, L",", &context); p != NULL; p = wcstok_s(NULL, L",", &context))
    {
      etcpal::IpAddr addr;

      ADDRINFOW ai_hints;
      ai_hints.ai_flags = AI_NUMERICHOST;
      ai_hints.ai_family = AF_UNSPEC;
      ai_hints.ai_socktype = 0;
      ai_hints.ai_protocol = 0;
      ai_hints.ai_addrlen = 0;
      ai_hints.ai_addr = nullptr;
      ai_hints.ai_canonname = nullptr;
      ai_hints.ai_next = nullptr;

      ADDRINFOW* gai_result;
      INT res = GetAddrInfoW(p, nullptr, &ai_hints, &gai_result);
      if (res == 0)
      {
        if (gai_result->ai_addr)
        {
          ip_os_to_etcpal(gai_result->ai_addr, &addr.get());
          addrs.insert(addr);
        }
        FreeAddrInfoW(gai_result);
      }
    }
  }

  if (!addrs.empty())
  {
    broker_shell.SetInitialIfaceList(addrs);
    return true;
  }
  return false;
}

// Given a pointer to a string, parses out a mac addr
void ParseMac(WCHAR* s, etcpal::MacAddr& mac)
{
  WCHAR* p = s;

  for (int index = 0; index < ETCPAL_MAC_BYTES; ++index)
  {
    mac.get().data[index] = (uint8_t)wcstol(p, &p, 16);
    ++p;  // P points at the :
  }
}

// Parse the --macs=MAC_LIST command line option and transfer it to the BrokerShell instance.
bool ParseAndSetMacList(const LPWSTR mac_list_str, BrokerShell& broker_shell)
{
  std::set<etcpal::MacAddr> macs;

  if (wcslen(mac_list_str) != 0)
  {
    WCHAR* context;
    for (WCHAR* p = wcstok_s(mac_list_str, L",", &context); p != NULL; p = wcstok_s(NULL, L",", &context))
    {
      etcpal::MacAddr mac;
      ParseMac(p, mac);
      macs.insert(mac);
    }
  }

  if (!macs.empty())
  {
    broker_shell.SetInitialMacList(macs);
    return true;
  }
  return false;
}

// Parse the --port=PORT command line option and transfer it to the BrokerShell instance.
bool ParseAndSetPort(const LPWSTR port_str, BrokerShell& broker_shell)
{
  if (wcslen(port_str) != 0)
  {
    uint16_t port = static_cast<uint16_t>(_wtoi(port_str));
    broker_shell.SetInitialPort(port);
    return true;
  }
  return false;
}

// clang-format off
static const std::map<std::wstring, int> log_levels = {
  {L"EMERG", ETCPAL_LOG_UPTO(ETCPAL_LOG_EMERG)},
  {L"ALERT", ETCPAL_LOG_UPTO(ETCPAL_LOG_ALERT)},
  {L"CRIT", ETCPAL_LOG_UPTO(ETCPAL_LOG_CRIT)},
  {L"ERR", ETCPAL_LOG_UPTO(ETCPAL_LOG_ERR)},
  {L"WARNING", ETCPAL_LOG_UPTO(ETCPAL_LOG_WARNING)},
  {L"NOTICE", ETCPAL_LOG_UPTO(ETCPAL_LOG_NOTICE)},
  {L"INFO", ETCPAL_LOG_UPTO(ETCPAL_LOG_INFO)},
  {L"DEBUG", ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG)}
};
// clang-format on

// Parse the --log-level=LOG_LEVEL command line option and convert it to a logging mask
bool ParseAndSetLogLevel(const LPWSTR log_level_str, int& log_mask)
{
  auto level = log_levels.find(log_level_str);
  if (level != log_levels.end())
  {
    log_mask = level->second;
    return true;
  }
  return false;
}

// Possible results of parsing the command-line arguments.
enum class ParseResult
{
  kGoodParse,    // Arguments were parsed OK.
  kParseErr,     // Error while parsing arguments - should print usage and exit error.
  kPrintHelp,    // A help argument was passed - should print usage and exit success.
  kPrintVersion  // A version argument was passed - should print version and exit success.
};

// Parse the command-line arguments.
ParseResult ParseArgs(int argc, wchar_t* argv[], BrokerShell& broker_shell, int& log_mask)
{
  if (argc > 1)
  {
    bool iface_key_encountered = false;

    for (int i = 1; i < argc; ++i)
    {
      if (_wcsnicmp(argv[i], L"--scope=", 8) == 0)
      {
        if (!ParseAndSetScope(argv[i] + 8, broker_shell))
        {
          return ParseResult::kParseErr;
        }
      }
      else if (_wcsnicmp(argv[i], L"--ifaces=", 9) == 0)
      {
        if (iface_key_encountered)
        {
          return ParseResult::kParseErr;
        }
        else
        {
          if (ParseAndSetIfaceList(argv[i] + 9, broker_shell))
            iface_key_encountered = true;
          else
            return ParseResult::kParseErr;
        }
      }
      else if (_wcsnicmp(argv[i], L"--macs=", 7) == 0)
      {
        if (iface_key_encountered)
        {
          return ParseResult::kParseErr;
        }
        else
        {
          if (ParseAndSetMacList(argv[i] + 9, broker_shell))
            iface_key_encountered = true;
          else
            return ParseResult::kParseErr;
        }
      }
      else if (_wcsnicmp(argv[i], L"--port=", 7) == 0)
      {
        if (!ParseAndSetPort(argv[i] + 7, broker_shell))
          return ParseResult::kParseErr;
      }
      else if (_wcsnicmp(argv[i], L"--log-level=", 12) == 0)
      {
        if (!ParseAndSetLogLevel(argv[i] + 12, log_mask))
          return ParseResult::kParseErr;
      }
      else if (_wcsicmp(argv[i], L"--version") == 0 || _wcsicmp(argv[i], L"-v") == 0)
      {
        return ParseResult::kPrintVersion;
      }
      else if (_wcsicmp(argv[i], L"--help") == 0 || _wcsicmp(argv[i], L"-?") == 0)
      {
        return ParseResult::kPrintHelp;
      }
      else
      {
        return ParseResult::kParseErr;
      }
    }
  }

  // Handles the (valid) case of no args, or all args parsed successfully.
  return ParseResult::kGoodParse;
}

// The system will deliver this callback when an IPv4 or IPv6 network adapter changes state. This
// event is passed along to the BrokerShell instance, which restarts the broker.
VOID NETIOAPI_API_ InterfaceChangeCallback(IN PVOID CallerContext, IN PMIB_IPINTERFACE_ROW Row,
                                           IN MIB_NOTIFICATION_TYPE NotificationType)
{
  (void)Row;
  (void)NotificationType;

  BrokerShell* shell = static_cast<BrokerShell*>(CallerContext);
  if (shell)
  {
    shell->NetworkChanged();
  }
}

BrokerShell broker_shell;
bool handled_ctrl_c_once = false;

BOOL WINAPI ConsoleSignalHandler(DWORD signal)
{
  // We only gracefully shut down on the first Ctrl+C
  if (signal == CTRL_C_EVENT && !handled_ctrl_c_once)
  {
    broker_shell.AsyncShutdown();
    handled_ctrl_c_once = true;
    return TRUE;
  }
  return FALSE;
}

// Windows console entry point for the example broker app.
// Parse command-line arguments and then run the platform-neutral Broker shell.
int wmain(int argc, wchar_t* argv[])
{
  bool should_run = true;
  int exit_code = 0;
  int log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_INFO);

  WSADATA wsa_data;
  int ws_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  assert(ws_err == 0);

  switch (ParseArgs(argc, argv, broker_shell, log_mask))
  {
    case ParseResult::kParseErr:
      PrintHelp(argv[0]);
      should_run = false;
      exit_code = 1;
      break;
    case ParseResult::kPrintHelp:
      PrintHelp(argv[0]);
      should_run = false;
      break;
    case ParseResult::kPrintVersion:
      BrokerShell::PrintVersion();
      should_run = false;
      break;
    case ParseResult::kGoodParse:
    default:
      break;
  }

  if (should_run)
  {
    // Register with Windows for network change detection.
    HANDLE change_notif_handle = nullptr;
    NotifyIpInterfaceChange(AF_UNSPEC, InterfaceChangeCallback, &broker_shell, FALSE, &change_notif_handle);

    // Handle Ctrl+C and gracefully shutdown.
    SetConsoleCtrlHandler(ConsoleSignalHandler, TRUE);

    // Startup and run the Broker.
    WindowsBrokerLog log;
    log.Startup("RDMnetBroker.log", log_mask);
    exit_code = broker_shell.Run(log.broker_log_instance());
    log.Shutdown();

    // Unregister/cleanup the network change detection.
    CancelMibChangeNotify2(change_notif_handle);
  }

  WSACleanup();

  return exit_code;
}
