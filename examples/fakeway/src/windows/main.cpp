// RDMnetDevice.cpp : Defines the entry point for the console application.

#include "etcpal/inet.h"
#include "fakeway.h"

#include <WinSock2.h>
#include <windows.h>
#include <ShlObj.h>
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
  std::cout << "  --cid=CID         Configures the CID (CID should follow the format\n";
  std::cout << "                    xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (not case sensitive)).\n";
  std::cout << "                    If this isn't specified, a V4 UUID will be generated.\n";
  std::cout << "  --help            Display this help and exit.\n";
  std::cout << "  --version         Output version information and exit.\n";
}

bool SetScope(const wchar_t* arg, rdmnet::Scope& scope_config)
{
  char scope_buf[E133_SCOPE_STRING_PADDED_LENGTH];
  if (WideCharToMultiByte(CP_UTF8, 0, arg, -1, scope_buf, E133_SCOPE_STRING_PADDED_LENGTH, NULL, NULL) > 0)
  {
    scope_config.SetIdString(scope_buf);
    return true;
  }
  return false;
}

bool SetStaticBroker(const wchar_t* arg, rdmnet::Scope& scope_config)
{
  EtcPalSockAddr static_broker_addr;

  const wchar_t* sep = wcschr(arg, ':');
  if (sep != NULL && sep - arg < ETCPAL_IP_STRING_BYTES)
  {
    wchar_t             ip_str[ETCPAL_IP_STRING_BYTES];
    ptrdiff_t           ip_str_len = sep - arg;
    struct sockaddr_in  tst_addr;
    struct sockaddr_in6 tst_addr6;
    INT                 convert_res;

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
      scope_config.SetStaticBrokerAddr(static_broker_addr);
      return true;
    }
  }
  return false;
}

bool SetCid(const wchar_t* arg, etcpal::Uuid& cid)
{
  char cid_str[ETCPAL_UUID_STRING_BYTES];
  if (WideCharToMultiByte(CP_UTF8, 0, arg, -1, cid_str, ETCPAL_UUID_STRING_BYTES, NULL, NULL) > 0)
  {
    cid = etcpal::Uuid::FromString(cid_str);
    if (!cid.IsNull())
      return true;
  }
  return false;
}

static bool    fakeway_keep_running = true;
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
  void                 HandleLogMessage(const EtcPalLogStrings& strings) override;

private:
  std::wstring GetLogFileName();

  etcpal::Logger logger_;
  std::fstream   file_;
  int            utc_offset_{0};
};

WindowsLog::WindowsLog()
{
  const auto file_name = GetLogFileName();
  if (!file_name.empty())
  {
    file_.open(file_name, std::fstream::out);
    if (!file_)
      std::wcout << L"Fakeway Log: Couldn't open log file '" << file_name << L"'.\n";
  }

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

  logger_.SetLogAction(ETCPAL_LOG_CREATE_HUMAN_READABLE).SetLogMask(ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG)).Startup(*this);
}

WindowsLog::~WindowsLog()
{
  logger_.Shutdown();
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
  {
    file_ << strings.human_readable << '\n';
    file_.flush();
  }
}

static const std::vector<std::wstring> kLogFileDirComponents = {L"ETC", L"RDMnet Examples"};
static constexpr WCHAR                 kLogFileBaseName[] = L"fakeway.log";

std::wstring WindowsLog::GetLogFileName()
{
  PWSTR app_data_path;
  if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &app_data_path) == S_OK)
  {
    std::wstring cur_path = app_data_path;
    for (const auto& directory_part : kLogFileDirComponents)
    {
      cur_path += L"\\" + directory_part;
      if (!CreateDirectoryW(cur_path.c_str(), NULL))
      {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        {
          // Something went wrong creating an intermediate directory.
          std::wcout << L"Couldn't create directory " << cur_path << L": Error " << error << ".\n";
          return std::wstring{};
        }
      }
    }
    return cur_path + L"\\" + kLogFileBaseName;
  }
  return std::wstring{};
}

int wmain(int argc, wchar_t* argv[])
{
  bool          should_exit = false;
  rdmnet::Scope scope_config;
  etcpal::Uuid  cid;

  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
    {
      if (_wcsnicmp(argv[i], L"--scope=", 8) == 0)
      {
        if (!SetScope(&argv[i][8], scope_config))
        {
          PrintHelp(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (_wcsnicmp(argv[i], L"--broker=", 9) == 0)
      {
        if (!SetStaticBroker(&argv[i][9], scope_config))
        {
          PrintHelp(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (_wcsnicmp(argv[i], L"--cid=", 6) == 0)
      {
        if (!SetCid(&argv[i][6], cid))
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

  if (cid.IsNull())
    cid = etcpal::Uuid::OsPreferred();

  std::cout << "Starting Fakeway...\n";
  WindowsLog log;
  if (!fakeway.Startup(scope_config, log.logger(), cid))
    return 1;

  while (fakeway_keep_running)
  {
    Sleep(100);
  }

  fakeway.Shutdown();

  return 0;
}
