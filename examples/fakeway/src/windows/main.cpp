// RDMnetDevice.cpp : Defines the entry point for the console application.

#include <WinSock2.h>  //MUST ALWAYS BE INCLUDED before windows.h!
#include <windows.h>
#include <WS2tcpip.h>
#include <conio.h>

#include <cwchar>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <deque>

#include "etcpal/inet.h"
#include "fakeway.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

void PrintHelp(wchar_t* app_name)
{
  printf("Usage: %ls [OPTION]...\n\n", app_name);
  printf("  --scope=SCOPE     Configures the RDMnet Scope to SCOPE. Enter nothing after\n");
  printf("                    '=' to set the scope to the default.\n");
  printf("  --broker=IP:PORT  Connect to a Broker at address IP:PORT instead of\n");
  printf("                    performing discovery.\n");
  printf("  --help            Display this help and exit.\n");
  printf("  --version         Output version information and exit.\n");
}

bool SetScope(wchar_t* arg, char* scope_buf)
{
  if (WideCharToMultiByte(CP_UTF8, 0, arg, -1, scope_buf, E133_SCOPE_STRING_PADDED_LENGTH, NULL, NULL) > 0)
    return true;
  return false;
}

bool SetStaticBroker(wchar_t* arg, EtcPalSockAddr& static_broker_addr)
{
  wchar_t* sep = wcschr(arg, ':');
  if (sep != NULL && sep - arg < ETCPAL_INET6_ADDRSTRLEN)
  {
    wchar_t ip_str[ETCPAL_INET6_ADDRSTRLEN];
    ptrdiff_t ip_str_len = sep - arg;
    struct sockaddr_in tst_addr;
    struct sockaddr_in6 tst_addr6;
    INT convert_res;

    wmemcpy(ip_str, arg, ip_str_len);
    ip_str[ip_str_len] = '\0';

    /* Try to convert the address in both IPv4 and IPv6 forms. */
    convert_res = InetPtonW(AF_INET, ip_str, &tst_addr.sin_addr);
    if (convert_res == 1)
    {
      tst_addr.sin_family = AF_INET;
      ip_os_to_etcpal((const etcpal_os_ipaddr_t*)&tst_addr, &static_broker_addr.ip);
    }
    else
    {
      convert_res = InetPtonW(AF_INET6, ip_str, &tst_addr6);
      if (convert_res == 1)
      {
        tst_addr6.sin6_family = AF_INET6;
        ip_os_to_etcpal((const etcpal_os_ipaddr_t*)&tst_addr6, &static_broker_addr.ip);
      }
    }
    if (convert_res == 1 && 1 == swscanf(sep + 1, L"%hu", &static_broker_addr.port))
    {
      return true;
    }
  }
  return false;
}

static bool fakeway_keep_running = true;
static Fakeway fakeway;

bool handling_ctrl_c = false;

BOOL WINAPI ConsoleHandler(DWORD signal)
{
  if (signal == CTRL_C_EVENT)
  {
    printf("Stopping Fakeway...\n");
    fakeway_keep_running = false;
  }

  return TRUE;
}

int wmain(int argc, wchar_t* argv[])
{
  bool should_exit = false;
  RdmnetScopeConfig scope_config;

  RDMNET_CLIENT_SET_DEFAULT_SCOPE(&scope_config);

  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
    {
      if (_wcsnicmp(argv[i], L"--scope=", 8) == 0)
      {
        if (!SetScope(&argv[i][8], scope_config.scope))
        {
          PrintHelp(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (_wcsnicmp(argv[i], L"--broker=", 9) == 0)
      {
        if (SetStaticBroker(&argv[i][9], scope_config.static_broker_addr))
        {
          scope_config.has_static_broker_addr = true;
        }
        else
        {
          PrintHelp(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (_wcsicmp(argv[i], L"--version") == 0)
      {
        Fakeway::PrintVersion();
        should_exit = true;
        break;
      }
      else
      {
        PrintHelp(argv[0]);
        should_exit = true;
        break;
      }
    }
  }
  if (should_exit)
    return 1;

  // Handle console signals
  if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
  {
    printf("Could not set console signal handler.\n");
    return 1;
  }

  printf("Starting Fakeway...\n");
  fakeway.Startup(scope_config);

  while (fakeway_keep_running)
  {
    Sleep(100);
  }

  fakeway.Shutdown();

  return 0;
}
