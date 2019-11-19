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

// main.cpp: The Linux console entry point for the Broker example application.

#include <cstring>
#include <csignal>
#include <iostream>
#include <map>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "broker_shell.h"
#include "linux_broker_log.h"

// Print the command-line usage details.
void PrintHelp(const char* app_name)
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

// Parse the --scope=SCOPE command line option and transfer it to the BrokerShell instance.
bool ParseAndSetScope(const char* scope_str, BrokerShell& broker_shell)
{
  if (strlen(scope_str) != 0)
  {
    broker_shell.SetInitialScope(scope_str);
    return true;
  }
  return false;
}

// Parse the --ifaces=IFACE_LIST command line option and transfer it to the BrokerShell instance.
bool ParseAndSetIfaceList(char* iface_list_str, BrokerShell& broker_shell)
{
  std::set<etcpal::IpAddr> addrs;

  if (strlen(iface_list_str) != 0)
  {
    char* context;
    for (char* p = strtok_r(iface_list_str, ",", &context); p != NULL; p = strtok_r(NULL, ",", &context))
    {
      struct addrinfo ai_hints;
      ai_hints.ai_flags = AI_NUMERICHOST;
      ai_hints.ai_family = AF_UNSPEC;
      ai_hints.ai_socktype = 0;
      ai_hints.ai_protocol = 0;
      ai_hints.ai_addrlen = 0;
      ai_hints.ai_addr = nullptr;
      ai_hints.ai_canonname = nullptr;
      ai_hints.ai_next = nullptr;

      struct addrinfo* gai_result;
      int res = getaddrinfo(p, nullptr, &ai_hints, &gai_result);
      if (res == 0)
      {
        if (gai_result->ai_addr)
        {
          ip_os_to_etcpal(gai_result->ai_addr, &addr.get());
          addrs.insert(addr);
        }
        freeaddrinfo(gai_result);
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

// Parse the --macs=MAC_LIST command line option and transfer it to the BrokerShell instance.
bool ParseAndSetMacList(char* mac_list_str, BrokerShell& broker_shell)
{
  std::set<etcpal::MacAddr> macs;

  if (strlen(mac_list_str) != 0)
  {
    char* context;
    for (char* p = strtok_r(mac_list_str, ",", &context); p != NULL; p = strtok_r(NULL, ",", &context))
    {
      auto mac = etcpal::MacAddr::FromString(p);
      if (!mac.IsNull())
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
bool ParseAndSetPort(const char* port_str, BrokerShell& broker_shell)
{
  if (strlen(port_str) != 0)
  {
    uint16_t port = static_cast<uint16_t>(atoi(port_str));
    broker_shell.SetInitialPort(port);
    return true;
  }
  return false;
}

// clang-format off
static const std::map<std::string, int> log_levels = {
  {"EMERG", ETCPAL_LOG_UPTO(ETCPAL_LOG_EMERG)},
  {"ALERT", ETCPAL_LOG_UPTO(ETCPAL_LOG_ALERT)},
  {"CRIT", ETCPAL_LOG_UPTO(ETCPAL_LOG_CRIT)},
  {"ERR", ETCPAL_LOG_UPTO(ETCPAL_LOG_ERR)},
  {"WARNING", ETCPAL_LOG_UPTO(ETCPAL_LOG_WARNING)},
  {"NOTICE", ETCPAL_LOG_UPTO(ETCPAL_LOG_NOTICE)},
  {"INFO", ETCPAL_LOG_UPTO(ETCPAL_LOG_INFO)},
  {"DEBUG", ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG)}
};
// clang-format on

// Parse the --log-level=LOG_LEVEL command line option and transfer it to the BrokerShell instance.
bool ParseAndSetLogLevel(const char* log_level_str, int& log_mask)
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
ParseResult ParseArgs(int argc, char* argv[], BrokerShell& broker_shell, int& log_mask)
{
  if (argc > 1)
  {
    bool iface_key_encountered = false;

    for (int i = 1; i < argc; ++i)
    {
      if (strncmp(argv[i], "--scope=", 8) == 0)
      {
        if (!ParseAndSetScope(argv[i] + 8, broker_shell))
        {
          return ParseResult::kParseErr;
        }
      }
      else if (strncmp(argv[i], "--ifaces=", 9) == 0)
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
      else if (strncmp(argv[i], "--macs=", 7) == 0)
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
      else if (strncmp(argv[i], "--port=", 7) == 0)
      {
        if (!ParseAndSetPort(argv[i] + 7, broker_shell))
          return ParseResult::kParseErr;
      }
      else if (strncmp(argv[i], "--log-level=", 12) == 0)
      {
        if (!ParseAndSetLogLevel(argv[i] + 12, log_mask))
          return ParseResult::kParseErr;
      }
      else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0)
      {
        return ParseResult::kPrintVersion;
      }
      else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-?") == 0)
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

// Change detection not implemented on Linux for now

// VOID NETIOAPI_API_ InterfaceChangeCallback(IN PVOID CallerContext, IN PMIB_IPINTERFACE_ROW Row,
//                                           IN MIB_NOTIFICATION_TYPE NotificationType)
//{
//  (void)Row;
//  (void)NotificationType;
//
//  BrokerShell* shell = static_cast<BrokerShell*>(CallerContext);
//  if (shell)
//  {
//    shell->NetworkChanged();
//  }
//}

BrokerShell broker_shell;

void signal_handler(int signal)
{
  std::cout << "Caught signal " << signal << ". Stopping...\n";
  broker_shell.AsyncShutdown();
}

// Linux console entry point for the example broker app.
// Parse command-line arguments and then run the platform-neutral Broker shell.
int main(int argc, char* argv[])
{
  bool should_run = true;
  int exit_code = 0;
  int log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG);

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
    // Register for network change detection. (disabled for now)

    // HANDLE change_notif_handle = nullptr;
    // NotifyIpInterfaceChange(AF_UNSPEC, InterfaceChangeCallback, &broker_shell, FALSE, &change_notif_handle);

    // Handle Ctrl+C and gracefully shutdown.
    struct sigaction sigint_handler;
    sigint_handler.sa_handler = signal_handler;
    sigemptyset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = 0;
    sigaction(SIGINT, &sigint_handler, NULL);

    // Startup and run the Broker.
    LinuxBrokerLog log;
    log.Startup("RDMnetBroker.log", log_mask);
    exit_code = broker_shell.Run(log.broker_log_instance());
    log.Shutdown();

    // Unregister/cleanup the network change detection. (Disabled for now)
    // CancelMibChangeNotify2(change_notif_handle);
  }

  return exit_code;
}
