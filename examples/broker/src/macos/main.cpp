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

// main.cpp: The macOS console entry point for the Broker example application.

#include <cstdlib>
#include <cstring>
#include <csignal>
#include <iostream>
#include <map>
#include <netdb.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include "etcpal/common.h"
#include "etcpal/netint.h"
#include "broker_shell.h"
#include "macos_broker_log.h"

// Print the command-line usage details.
void PrintHelp(const char* app_name)
{
  std::cout << "Usage: " << app_name << " [OPTION]...\n";
  std::cout << "\n";
  std::cout << "Options:\n";
  std::cout << "  --scope=SCOPE         Configures the RDMnet Scope this Broker runs on to\n";
  std::cout << "                        SCOPE. By default, the default RDMnet scope is used.\n";
  std::cout << "  --ifaces=IFACE_LIST   A comma-separated list of local network interface names\n";
  std::cout << "                        to use, e.g. 'en0,en1'. By default, all available\n";
  std::cout << "                        interfaces are used.\n";
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
  std::vector<std::string> netint_names;

  size_t                        num_netints = 4u;  // Start with estimate
  std::vector<EtcPalNetintInfo> netints(num_netints);
  while (etcpal_netint_get_interfaces(netints.data(), &num_netints) == kEtcPalErrBufSize)
    netints.resize(num_netints);

  netints.resize(num_netints);

  if (strlen(iface_list_str) != 0)
  {
    char* context;
    for (char* p = strtok_r(iface_list_str, ",", &context); p != NULL; p = strtok_r(NULL, ",", &context))
    {
      std::string interface_name = p;
      bool        found = false;
      for (const auto& netint : netints)
      {
        if (interface_name == netint.id)
        {
          found = true;
          if (std::find(netint_names.begin(), netint_names.end(), interface_name) == netint_names.end())
            netint_names.push_back(interface_name);
          else
            std::cout << "Skipping duplicate specified network interface '" << interface_name << "'.\n";
          break;
        }
      }
      if (!found)
        std::cout << "Specified network interface '" << interface_name << "' not found.\n";
    }
  }

  if (!netint_names.empty())
  {
    broker_shell.SetInitialNetintList(netint_names);
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
    for (int i = 1; i < argc; ++i)
    {
      if (strncmp(argv[i], "--scope=", 8) == 0)
      {
        if (!ParseAndSetScope(argv[i] + 8, broker_shell))
          return ParseResult::kParseErr;
      }
      else if (strncmp(argv[i], "--ifaces=", 9) == 0)
      {
        if (!ParseAndSetIfaceList(argv[i] + 9, broker_shell))
          return ParseResult::kParseErr;
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

// Change detection not implemented on macOS for now

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

// macOS console entry point for the example broker app.
// Parse command-line arguments and then run the platform-neutral Broker shell.
int main(int argc, char* argv[])
{
  etcpal_error_t res = etcpal_init(ETCPAL_FEATURE_NETINTS);
  if (res != kEtcPalErrOk)
  {
    std::cout << "Couldn't get system network information: '" << etcpal_strerror(res) << "'.\n";
    return 1;
  }

  bool should_run = true;
  int  exit_code = 0;
  int  log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_INFO);

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
    MacBrokerLog log;
    log.Startup(log_mask);
    exit_code = broker_shell.Run(log.log_instance());
    log.Shutdown();

    // Unregister/cleanup the network change detection. (Disabled for now)
    // CancelMibChangeNotify2(change_notif_handle);
  }

  etcpal_deinit(ETCPAL_FEATURE_NETINTS);
  return exit_code;
}
