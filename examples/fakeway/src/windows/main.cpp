// RDMnetDevice.cpp : Defines the entry point for the console application.

#include "etcpal/inet.h"
#include "fakeway.h"

#include <WinSock2.h>  //MUST ALWAYS BE INCLUDED before windows.h!
#include <windows.h>
#include <WS2tcpip.h>
#include <conio.h>

#include <cwchar>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <deque>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

void PrintHelp(wchar_t* app_name)
{
  std::wcout << L"Usage: " << app_name << L"[OPTION]...\n\n";
  std::cout << "  --scope=SCOPE     Configures the RDMnet Scope to SCOPE. Enter nothing after\n";
  std::cout << "                    '=' to set the scope to the default.\n";
  std::cout << "  --broker=IP:PORT  Connect to a Broker at address IP:PORT instead of\n";
  std::cout << "                    performing discovery.\n";
  std::cout << "  --help            Display this help and exit.\n";
  std::cout << "  --version         Output version information and exit.\n";
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
    std::cout << "Stopping Fakeway...\n";
    fakeway_keep_running = false;
  }

  return TRUE;
}

class WindowsLog : public etcpal::LogMessageHandler
{
public:
  WindowsLog();
  ~WindowsLog();

  etcpal::Logger& logger() { return logger_; }

  etcpal::LogTimestamp GetLogTimestamp() override;
  void HandleLogMessage(const EtcPalLogStrings& strings) override;

private:
  etcpal::Logger logger_;
  std::fstream file_;
  int utc_offset_{0};
};

WindowsLog::WindowsLog()
{
  etcpal_init(ETCPAL_FEATURE_LOGGING);

  file_.open(file_name.c_str(), std::fstream::out);
  if (file_.fail())
    std::cout << "Fakeway Log: Couldn't open log file '" << file_name << "'." << std::endl;

  TIME_ZONE_INFORMATION tzinfo;
  switch (GetTimeZoneInformation(&tzinfo))
  {
    case TIME_ZONE_ID_UNKNOWN:
    case TIME_ZONE_ID_STANDARD:
      utc_offset_ = -(tzinfo.Bias + tzinfo.StandardBias);
      break;
    case TIME_ZONE_ID_DAYLIGHT:
      utc_offset_ = -(tzinfo.Bias + tzinfo.DaylightBias);
      break;
    default:
      std::cout << "Fakeway Log: Couldn't get time zone info.\n";
      break;
  }

  logger_.SetLogAction(kEtcPalLogCreateHumanReadable).SetLogMask(ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG)).Startup(*this);
}

WindowsLog::~WindowsLog()
{
  logger_.Shutdown();
  etcpal_deinit(ETCPAL_FEATURE_LOGGING);
}

etcpal::LogTimestamp WindowsLog::GetLogTimestamp()
{
  SYSTEMTIME time;
  GetLocalTime(&time);
  return etcpal::LogTimestamp(time.wYear - 1900, time.wMonth - 1, time.wDay, time.wHour, time.wMinute, time.wSecond,
                              time.wMilliseconds, utc_offset_);
}

void WindowsLog::HandleLogMessage(const EtcPalLogStrings& strings)
{
  std::cout << strings.human_readable << '\n';
  if (file_.is_open())
    file_ << strings.human_readable << '\n';
}

int wmain(int argc, wchar_t* argv[])
{
  bool should_exit = false;
  rdmnet::Scope scope_config;

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
    std::cout << "Could not set console signal handler.\n";
    return 1;
  }

  std::cout << "Starting Fakeway...\n";
  WindowsLog log;
  fakeway.Startup(scope_config, log.logger());

  while (fakeway_keep_running)
  {
    Sleep(100);
  }

  fakeway.Shutdown();

  return 0;
}
